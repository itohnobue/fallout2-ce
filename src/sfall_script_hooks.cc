#include "sfall_script_hooks.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "actions.h"
#include "animation.h"
#include "critter.h"
#include "db.h"
#include "debug.h"
#include "game.h"
#include "interface.h"
#include "interpreter_extra.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "sfall_opcodes.h"
#include "skill.h"

#include <assert.h>

namespace fallout {

static int normalizeGameTimeForScript(unsigned int gameTime)
{
    // Fallout saves ticks as uint32 but scripts expect int32.  Wrapping at INT_MAX means
    // that scripts will at least see positive ticks between 6.8y and 13y of game time.
    // There isn't an elegant solution to this; scripts should use get_year instead of relying
    // on absolute tick count
    return static_cast<int>(gameTime % INT_MAX);
}

struct ScriptHook {
    Program* program = nullptr;
    int procedureIndex = -1;
};

static std::vector<ScriptHook> scriptHooks[HOOK_COUNT];

constexpr size_t MAX_HOOK_CALL_DEPTH = 8;

// RAII scope guard for the _gameModeChangeInProgress reentrancy flag.
// Sets the flag on construction, clears it on destruction.  The destructor
// may be skipped by longjmp (I2-M35); recovery from a stuck flag is handled
// in scriptHooks_GameModeChange() using I2-M16's proactive stale-drain as
// a liveness signal (current()==nullptr after drain = flag is stuck).
struct GameModeChangeGuard {
    GameModeChangeGuard() { ScriptHookCall::_gameModeChangeInProgress = true; }
    ~GameModeChangeGuard() { ScriptHookCall::_gameModeChangeInProgress = false; }
};

std::vector<ScriptHookCall*> ScriptHookCall::_callStack;
std::array<int, HOOK_COUNT> ScriptHookCall::_callStackPerType = {};
bool ScriptHookCall::_gameModeChangeInProgress = false;

ScriptHookCall* ScriptHookCall::current()
{
    return !_callStack.empty() ? _callStack.back() : nullptr;
}

ScriptHookCall::ScriptHookCall(HookType hookType, int maxReturnValues, std::initializer_list<ProgramValue> args)
    : _hookType(static_cast<HookType>(std::clamp(static_cast<int>(hookType), 0, static_cast<int>(HOOK_COUNT) - 1)))
    , _maxRetVals(std::clamp(maxReturnValues, 0, static_cast<int>(HOOKS_MAX_RETURN_VALUES)))
{
    assert(maxReturnValues >= 0 && maxReturnValues <= HOOKS_MAX_RETURN_VALUES && args.size() <= HOOKS_MAX_ARGUMENTS);
    for (auto arg : args) {
        if (_numArgs >= HOOKS_MAX_ARGUMENTS) break;
        _args[_numArgs++] = arg;
    }
}

void ScriptHookCall::setArgAt(int idx, ProgramValue value)
{
    if (idx < 0 || idx >= _numArgs) return;
    _args[idx] = value;
}

void ScriptHookCall::addReturnValueFromScript(ProgramValue value)
{
    if (_scriptRetVals >= HOOKS_MAX_RETURN_VALUES) return;
    if (_scriptRetVals >= _maxRetVals)
        return;

    _retVals[_scriptRetVals] = value;
    _retValPrograms[_scriptRetVals] = _lastProgram;
    _scriptRetVals++;

    if (_scriptRetVals > _numRetVals) {
        _numRetVals = _scriptRetVals;
    }
}

ProgramValue ScriptHookCall::getArgAt(int idx) const
{
    if (idx < 0 || idx >= _numArgs) return {};
    return _args[idx];
}

ProgramValue ScriptHookCall::getReturnValueAt(int idx) const
{
    if (idx < 0 || idx >= _numRetVals) return {};
    return _retVals[idx];
}

Program* ScriptHookCall::programForReturnValueAt(int idx) const
{
    if (idx < 0 || idx >= _numRetVals) return _lastProgram;
    Program* prog = _retValPrograms[idx];
    return prog != nullptr ? prog : _lastProgram;
}

int ScriptHookCall::numArgs() const { return _numArgs; }
int ScriptHookCall::maxReturnValues() const { return _maxRetVals; }
int ScriptHookCall::numReturnValues() const { return _numRetVals; }
int ScriptHookCall::numScriptReturnValues() const { return _scriptRetVals; }

void ScriptHookCall::drainStaleEntries(uintptr_t currentStackAddr)
{
    for (size_t i = 0; i < _callStack.size();) {
        uintptr_t entryAddr = reinterpret_cast<uintptr_t>(_callStack[i]);
        if (entryAddr < currentStackAddr) {
            // Entry is from a deeper (unwound) frame: stale.
            // Erasing shifts subsequent elements, so do NOT
            // increment i — the next element moves into position i.
            _callStack.erase(_callStack.begin() + i);
        } else {
            ++i;
        }
    }

    // I2-M08: Recalculate per-type counters from the live entries in
    // _callStack.  When stale entries are drained (removed), their per-type
    // counter increments (set before the longjmp) were never matched by
    // decrements (normally done in call()).  Recomputing from scratch after
    // drain guarantees the counters mirror reality.
    _callStackPerType = {};
    for (const auto* entry : _callStack) {
        _callStackPerType[entry->_hookType]++;
    }
}

void ScriptHookCall::call()
{
    // I2-M16: Drain stale entries left by longjmp'd ScriptHookCall frames.
    //
    // programFatalError uses longjmp to abort script execution, which
    // skips _callStack.pop_back() and leaves stale entries permanently.
    // The old code tried to detect staleness by reading _callStack[i]->_active,
    // which is UB when the ScriptHookCall object has been destroyed
    // (freed stack memory).  This replacement uses lifetime-safe address
    // comparison: stale entries from unwound frames have stack addresses
    // below the current call frame (stack grows downward on all target
    // platforms: x86, ARM64, and WASM/Emscripten).
    //
    // This runs PROACTIVELY on every call() entry — not just at the depth
    // cap — so a single stale entry is cleaned up immediately rather than
    // accumulating until MAX_HOOK_CALL_DEPTH is reached.
    {
        int stackAnchor = 0;
        drainStaleEntries(reinterpret_cast<uintptr_t>(&stackAnchor));
    }

    // Depth-cap check (after drain — stale entries have been removed).
    constexpr size_t MAX_PER_TYPE_DEPTH = 4;
    if (_callStackPerType[_hookType] >= MAX_PER_TYPE_DEPTH) {
        // Per-type reentrancy cap reached — this hook type has exhausted its
        // guaranteed portion of the global budget.  This prevents one hook
        // type from starving critical hooks like HOOK_ONDEATH.
        debugPrint("HOOK_DEPTH: type %d exceeded per-type cap %zu\n",
                   static_cast<int>(_hookType), MAX_PER_TYPE_DEPTH);
        return;
    }

    if (_callStack.size() >= MAX_HOOK_CALL_DEPTH) {
        // Genuine deep recursion beyond the cap (no stale entries to drain).
        // Reject the call — hook scripts with deeper recursion than
        // MAX_HOOK_CALL_DEPTH are not supported.
        debugPrint("HOOK_DEPTH: %zu entries at global cap %zu, rejecting call (type=%d)\n",
                   _callStack.size(), MAX_HOOK_CALL_DEPTH, static_cast<int>(_hookType));
        return;
    }

    _active = true;
    _callStack.push_back(this);
    _callStackPerType[_hookType]++;

    // Copy the hook list to protect against vector invalidation during
    // iteration. A hook script may call register_hook_proc for the same type
    // during callback, which modifies scriptHooks[_hookType] via push_back()
    // or emplace(begin()). push_back() may reallocate the vector, invalidating
    // any reference; emplace(begin()) shifts existing elements, causing index
    // mismatch under reverse iteration. A value copy isolates our iteration
    // from concurrent mutations to the live hook list.
    auto hooksOfType = scriptHooks[_hookType];

    // Iterate in reverse order. In case current hook is unregistered inside
    // the call, we can just continue iteration.
    // _scriptArgs, _scriptRetVals, and _numRetVals are all reset per-handler
    // so each handler starts with a clean return-value slate.  The LAST
    // handler's return values are what the caller sees, with no cross-handler
    // leakage (R-05: if _numRetVals were only reset once before the loop,
    // a later handler setting fewer return values than an earlier handler
    // would leave stale values at higher indices, creating mixed return sets).
    for (int i = hooksOfType.size() - 1; i >= 0; --i) {
        const auto& hook = hooksOfType[i];
        _scriptArgs = 0;
        _scriptRetVals = 0;
        _numRetVals = 0;
        _lastProgram = hook.program;
        programExecuteProcedure(hook.program, hook.procedureIndex);
    }

    assert(_callStack.back() == this);
    _callStack.pop_back();
    _callStackPerType[_hookType]--;
    _active = false;
}

ProgramValue ScriptHookCall::getNextArgFromScript()
{
    if (_scriptArgs >= _numArgs) {
        return { 0 };
    }
    return _args[_scriptArgs++];
}

void scriptHooksUnregisterProgram(Program* program)
{
    if (program == nullptr) {
        return;
    }

    for (int i = 0; i < HOOK_COUNT; i++) {
        auto& hooks = scriptHooks[i];
        for (auto it = hooks.begin(); it != hooks.end();) {
            if (it->program == program) {
                it = hooks.erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool scriptHooksRegister(Program* program, const HookType hookType, const int procedureIndex, const bool atEnd)
{
    if (program == nullptr) return false;
    if (static_cast<int>(hookType) < 0 || hookType >= HOOK_COUNT) return false;
    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) return false;

    auto& hooksByType = scriptHooks[hookType];
    // Use -1 as unregister sentinel to avoid collision with valid procedure index 0.
    // The assert above guarantees procedureIndex >= 0, so -1 is unreachable from
    // public API and exists only as infrastructure for a future explicit unregister path.
    const bool isUnregisterRequest = procedureIndex == -1;
    // Check for existing registration.
    for (auto it = hooksByType.begin(); it != hooksByType.end(); ++it) {
        if (it->program == program) {
            if (isUnregisterRequest) {
                hooksByType.erase(it);
                return true; // unregister success
            }
            // Skip: no more than 1 procedure in a script for a given hook type.
            return false; // register fail
        }
    }
    if (isUnregisterRequest) {
        return false; // unregister fail
    }

    // register_hook_proc (atEnd=false): insert at end (highest index) — last registered = first executed.
    // register_hook_proc_spec (atEnd=true): insert at beginning (index 0) — last registered = last executed.
    // Hook iteration in ScriptHookCall::call() is reverse (size-1 down to 0),
    // so highest-index hooks execute FIRST, index-0 hooks execute LAST.
    // register_hook_proc_spec hooks must be at index 0 to serve as final overrides.
    if (atEnd) {
        hooksByType.emplace(hooksByType.begin(), ScriptHook { program, procedureIndex });
    } else {
        hooksByType.push_back(ScriptHook { program, procedureIndex });
    }
    return true; // register success
}

/*
Runs before/after Fallout executes a standard procedure (handler) in any script
of any object. This hook will not be executed for `start`, `critter_p_proc`,
`timed_event_p_proc`, or `map_update_p_proc`.

int     arg0 - the number of the standard script handler (see *_proc in define.h)
Obj     arg1 - the object that owns this handler (self_obj)
Obj     arg2 - the object that called this handler (source_obj, can be 0)
int     arg3 - always 0 for HOOK_STDPROCEDURE, always 1 for HOOK_STDPROCEDURE_END
Obj     arg4 - the object that is acted upon by this handler (target_obj, can be 0)
int     arg5 - the parameter of this call (fixed_param), useful for combat_proc

int     ret0 - pass -1 to cancel the execution of the handler
*/
bool scriptHooks_StdProcedure(int procedureNumber, Object* self, Object* source, Object* target, int fixedParam, bool after)
{
    if (procedureNumber == SCRIPT_PROC_START
        || procedureNumber == SCRIPT_PROC_CRITTER
        || procedureNumber == SCRIPT_PROC_TIMED
        || procedureNumber == SCRIPT_PROC_MAP_UPDATE) {
        return false;
    }

    const HookType hookType = after ? HOOK_STDPROCEDURE_END : HOOK_STDPROCEDURE;
    if (scriptHooks[hookType].empty()) {
        return false;
    }

    ScriptHookCall hook(hookType, after ? 0 : 1,
        { procedureNumber, self, source, after ? 1 : 0, target, fixedParam });
    hook.call();

    if (after || hook.numReturnValues() <= 0) {
        return false;
    }

    return hook.getReturnValueAt(0).asInt() == -1;
}

static void scriptHooksClear()
{
    for (auto& hooks : scriptHooks) {
        hooks.clear();
    }
}

bool scriptHooksInit()
{
    return true;
}

void scriptHooksReset()
{
    scriptHooksClear();
    // Reset reentrancy guard — a longjmp from programFatalError during
    // a HOOK_GAMEMODECHANGE dispatch leaves _gameModeChangeInProgress
    // stuck at true, permanently disabling the hook.  scriptHooksReset()
    // runs on gameReset / new game, providing a guaranteed cleanup point.
    ScriptHookCall::_gameModeChangeInProgress = false;
    // Reset animation callback pointer to prevent stale pointer
    // after game reset / new game cycle. Without this, sfallAnimCallbackProgram
    // could reference freed Program memory from a previous game session.
    sfallAnimCallbackReset();
}

void scriptHooksExit()
{
    scriptHooksClear();
}

/*
Runs once every time when the game mode was changed, like opening/closing the inventory, character screen, pipboy, etc.

int arg0 - event type: 1 - when the player exits the game, 0 - otherwise
int arg1 - the previous game mode
*/
void scriptHooks_GameModeChange(int exit, int previousGameMode)
{
    // F-26: Do not fire the hook before the game is fully loaded.
    // Early-init mode changes (e.g., during gameReset) should not
    // trigger script callbacks — scripts are not yet available.
    if (!gGameLoaded) {
        return;
    }

    // I2-M35: Drain stale entries from _callStack BEFORE the
    // reentrancy check.  A longjmp from programFatalError during
    // a previous HOOK_GAMEMODECHANGE dispatch leaves stale entries
    // that make current() return a non-null stale pointer.  Without
    // proactive draining here, the reentrancy check below would see
    // the stale pointer and incorrectly block as "reentrancy."
    //
    // The drain uses the same address-based detection as
    // ScriptHookCall::call() (I2-M16): stale entries from unwound
    // frames have stack addresses below the current frame.
    {
        int stackAnchor = 0;
        ScriptHookCall::drainStaleEntries(reinterpret_cast<uintptr_t>(&stackAnchor));
    }

    // I2-M35/I2-52: Reentrancy guard with longjmp recovery.
    //
    // A longjmp from programFatalError during a HOOK_GAMEMODECHANGE
    // dispatch skips the GameModeChangeGuard destructor, permanently
    // disabling the hook (the flag stays true forever).
    //
    // Recovery: the stale-drain above removes stale entries from
    // _callStack, so current() returns nullptr when no hook is truly
    // dispatching.  We can therefore distinguish stuck-from-longjmp
    // (flag=true, current()=nullptr) from legitimate reentrancy
    // (flag=true, current()=non-null).
    if (ScriptHookCall::_gameModeChangeInProgress) {
        if (ScriptHookCall::current() != nullptr) {
            // Legitimate reentrancy: a GAMEMODECHANGE hook dispatch
            // is actively in progress.  Block recursive mode change.
            return;
        }
        // Flag is stuck from a previous longjmp (I2-M35).
        // No hook is currently dispatching — reset and proceed.
        ScriptHookCall::_gameModeChangeInProgress = false;
    }

    if (scriptHooks[HOOK_GAMEMODECHANGE].empty()) {
        return;
    }

    GameModeChangeGuard guard;
    ScriptHookCall(HOOK_GAMEMODECHANGE, 0, { exit, previousGameMode }).call();
}

/*
Runs continuously while the player is resting (using pipboy alarm clock).

int arg0 - the game time in ticks
int arg1 - event type: 1 - when the resting ends normally, -1 - when pressing ESC to cancel the timer, 0 - otherwise
int arg2 - the hour part of the length of resting time
int arg3 - the minute part of the length of resting time

int ret0 - pass 1 to interrupt the resting, pass 0 to continue the rest if it was interrupted by pressing ESC key
*/
bool scriptHooks_RestTimer(unsigned int gameTime, RestEventType eventType, int hours, int minutes)
{
    if (eventType != REST_EVENT_TYPE_CANCEL && eventType != REST_EVENT_TYPE_PROGRESS && eventType != REST_EVENT_TYPE_COMPLETE) {
        debugPrint("HOOK_RESTTIMER: invalid eventType %d, returning false\n", eventType);
        return false;
    }
    if (hours < 0) {
        debugPrint("HOOK_RESTTIMER: invalid hours %d, returning false\n", hours);
        return false;
    }
    if (minutes < 0 || minutes >= 60) {
        debugPrint("HOOK_RESTTIMER: invalid minutes %d, returning false\n", minutes);
        return false;
    }

    if (scriptHooks[HOOK_RESTTIMER].empty()) {
        return eventType == REST_EVENT_TYPE_CANCEL;
    }

    ScriptHookCall hook(HOOK_RESTTIMER, 1, { normalizeGameTimeForScript(gameTime), eventType, hours, minutes });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return eventType == REST_EVENT_TYPE_CANCEL;
    }

    const int overrideResult = hook.getReturnValueAt(0).asInt();
    if (overrideResult == 1) {
        return true;
    }
    if (overrideResult == 0) {
        if (eventType == REST_EVENT_TYPE_CANCEL) {
            return false;
        }
        debugPrint("HOOK_RESTTIMER: ignoring return value 0 for non-ESC event");
        return false;
    }

    debugPrint("HOOK_RESTTIMER: ignoring invalid return value %d", overrideResult);
    return false;
}

/*
Runs after Fallout has decided how long an explosive timer should run and whether setting it was a success or failure.
The hook can override both the queued delay and the coarse outcome. Critical success/failure collapse to success/failure,
matching sfall's queue event rewrite semantics.

int     arg0 - The explosive delay in ticks before any failure penalty is applied
Obj     arg1 - The explosive object
int     arg2 - The result of engine calculation: 1 - failure, 2 - success

int     ret0 - The new delay in ticks (maximum 18000 == 30min). Negative values use engine behavior.
int     ret1 - The new result: 0/1 - failure, 2/3 - success. Other values use engine behavior.
*/
int scriptHooks_ExplosiveTimer(Object* explosive, int delay, int eventType)
{
    if (explosive == nullptr) {
        debugPrint("HOOK_EXPLOSIVETIMER: explosive is null, returning -1\n");
        return -1;
    }
    if (delay < 0) {
        debugPrint("HOOK_EXPLOSIVETIMER: invalid delay %d, returning -1\n", delay);
        return -1;
    }
    if (eventType != EVENT_TYPE_EXPLOSION && eventType != EVENT_TYPE_EXPLOSION_FAILURE) {
        debugPrint("HOOK_EXPLOSIVETIMER: invalid eventType %d, returning -1\n", eventType);
        return -1;
    }

    if (scriptHooks[HOOK_EXPLOSIVETIMER].empty()) {
        return -1;
    }

    int hookResult = eventType == EVENT_TYPE_EXPLOSION_FAILURE ? ROLL_FAILURE : ROLL_SUCCESS;

    ScriptHookCall hook(HOOK_EXPLOSIVETIMER, 2, { delay, explosive, hookResult });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return -1;
    }

    int overrideDelay = hook.getReturnValueAt(0).asInt();
    if (overrideDelay < 0) {
        return -1;
    }

    delay = std::min(overrideDelay, 18000);
    if (hook.numReturnValues() > 1) {
        int overrideResult = hook.getReturnValueAt(1).asInt();
        if (overrideResult >= ROLL_CRITICAL_FAILURE && overrideResult <= ROLL_FAILURE) {
            eventType = EVENT_TYPE_EXPLOSION_FAILURE;
        } else if (overrideResult >= ROLL_SUCCESS && overrideResult <= ROLL_CRITICAL_SUCCESS) {
            eventType = EVENT_TYPE_EXPLOSION;
        }
    }

    return queueAddEvent(delay, explosive, nullptr, eventType);
}

/*
Runs when retrieving the damage rating of the used weapon. (Which may be fists.)

int     arg0 - The default minimum damage after engine bonuses are applied
int     arg1 - The default maximum damage after engine bonuses are applied
Item    arg2 - The weapon used (0 if unarmed)
Critter arg3 - The critter doing the attacking
int     arg4 - The type of attack (see ATKTYPE_* constants)
int     arg5 - 1 if this is an attack using a melee weapon, 0 otherwise

int     ret0 - Either the damage to be used, if ret1 isn't given, or the new minimum damage if it is
int     ret1 - The new maximum damage
*/
void scriptHooks_ItemDamage(Object* weapon, Object* critter, int hitMode, bool isMeleeWeaponAttack, int* minDamagePtr, int* maxDamagePtr)
{
    if (critter == nullptr || minDamagePtr == nullptr || maxDamagePtr == nullptr) return;

    if (scriptHooks[HOOK_ITEMDAMAGE].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_ITEMDAMAGE, 2, { *minDamagePtr, *maxDamagePtr, weapon, critter, hitMode, isMeleeWeaponAttack ? 1 : 0 });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return;
    }

    // F-M02: Clamp script return values to valid damage range [0, 500].
    // Negative damage is nonsensical; damage above 500 exceeds any weapon in the game.
    constexpr int DAMAGE_MIN = 0;
    constexpr int DAMAGE_MAX = 500;
    int minDmg = std::clamp(hook.getReturnValueAt(0).asInt(), DAMAGE_MIN, DAMAGE_MAX);
    int maxDmg = minDmg;
    if (hook.numReturnValues() > 1) {
        maxDmg = std::clamp(hook.getReturnValueAt(1).asInt(), DAMAGE_MIN, DAMAGE_MAX);
    }
    // Enforce min <= max invariant that callers (randomBetween) assume.
    if (minDmg > maxDmg) {
        std::swap(minDmg, maxDmg);
    }
    *minDamagePtr = minDmg;
    *maxDamagePtr = maxDmg;
}

/*
Runs when calculating ammo cost for a weapon.

Item    arg0 - The weapon
int     arg1 - Number of bullets in burst or 1 for single shots
int     arg2 - The amount of ammo to be consumed, or ammo cost per round for hook type 2
int     arg3 - Type of hook:
               AMMO_COST_HOOK_SINGLE_SHOT - when subtracting ammo after single shot attack
               AMMO_COST_HOOK_CHECK_OUT_OF_AMMO - when checking for "out of ammo" before attack
               AMMO_COST_HOOK_BURST_ROUNDS - when calculating number of burst rounds
               AMMO_COST_HOOK_BURST_SHOT - when subtracting ammo after burst attack

int     ret0 - The new ammo amount/cost. Values below 0 are ignored.
*/
int scriptHooks_AmmoCost(Object* weapon, int rounds, int ammoCost, AmmoCostHookType hookType)
{
    if (scriptHooks[HOOK_AMMOCOST].empty()) {
        return ammoCost;
    }

    ScriptHookCall hook(HOOK_AMMOCOST, 1, { weapon, rounds, ammoCost, hookType });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return ammoCost;
    }

    int overrideAmmoCost = hook.getReturnValueAt(0).asInt();
    return overrideAmmoCost >= 0 ? overrideAmmoCost : ammoCost;
}

/*
Runs when checking an attempt to steal or plant an item.

Critter arg0 - The thief
Obj     arg1 - The target
Item    arg2 - The item being stolen/planted
int     arg3 - 0 when stealing, 1 when planting
int     arg4 - Quantity being stolen/planted

int     ret0 - Override the handler:
               2 - fail without being caught
               1 - success
               0 - fail and get caught
              -1 - use engine handler
int     ret1 - Override XP gained for this action. Values below 0 are ignored.
*/
int scriptHooks_Steal(Object* thief, Object* target, Object* item, bool isPlanting, int quantity, int* xpOverride)
{
    if (thief == nullptr || target == nullptr || item == nullptr || xpOverride == nullptr) {
        return -1;
    }
    // Clamp quantity to non-negative; negative quantities are a programming error
    // but should not propagate as invalid state.
    if (quantity < 0) quantity = 0;

    if (scriptHooks[HOOK_STEAL].empty()) {
        *xpOverride = -1;
        return -1;
    }

    *xpOverride = -1;

    ScriptHookCall hook(HOOK_STEAL, 2, { thief, target, item, isPlanting ? 1 : 0, quantity });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return -1;
    }

    if (hook.numReturnValues() > 1) {
        int overrideXp = hook.getReturnValueAt(1).asInt();
        if (overrideXp >= 0) {
            *xpOverride = overrideXp;
        }
    }

    int overrideResult = hook.getReturnValueAt(0).asInt();
    if (overrideResult >= 0 && overrideResult <= 2) {
        return overrideResult;
    }

    return -1;
}

/*
Runs immediately after a critter dies for any reason.

Critter arg0 - The critter that just died
*/
void scriptHooks_OnDeath(Object* critter)
{
    if (scriptHooks[HOOK_ONDEATH].empty()) {
        return;
    }

    ScriptHookCall(HOOK_ONDEATH, 0, { critter }).call();
}

/*
Runs whenever a random encounter occurs (except the Horrigan meeting and scripted encounters), or when the player enters a local map from the world map.
You can override the map for loading or the encounter.

CE ARGUMENT LAYOUT (5 args):
int     arg0 - event type: 0 - when a random encounter occurs, 2 - when the player enters from the world map
int     arg1 - the map ID that the encounter will load
int     arg2 - 1 when the encounter is a special encounter, 0 otherwise
int     arg3 - encounter table number, or -1 if not an encounter
int     arg4 - encounter index in the table, or -1 if not an encounter

CE's first 3 arguments (eventType, mapId, isSpecial) match sfall's 3-argument
HOOK_ENCOUNTER contract.  Per sfall documentation, the standard layout is:
  arg0 - event type (0=random encounter, 1=special encounter, 0x100=forced)
  arg1 - the map ID being entered
  arg2 - 1 when special/forced, 0 otherwise
CE preserves this compatibility and extends the hook with 2 additional
arguments (tableId, entryId) to provide enhanced encounter context that
is not available in sfall's original 3-argument interface.  Scripts written
for sfall's 3-argument layout receive the expected values in arg0..arg2;
scripts can optionally read arg3..arg4 for the extended information.

int     ret0 - overrides the map ID, or pass -1 for event type 0 to cancel the encounter and continue traveling
int     ret1 - pass 1 to cancel the encounter and load the specified map from the ret0 (only for event type 0)
*/
EncounterHookResult scriptHooks_Encounter(EncounterHookEventType eventType, int* mapIdPtr, bool isSpecial, int tableId, int entryId)
{
    if (mapIdPtr == nullptr) return EncounterHookResult::ContinueEncounter;

    if (scriptHooks[HOOK_ENCOUNTER].empty()) {
        return EncounterHookResult::ContinueEncounter;
    }

    // Random and forced encounters support 2 return values (ret0=map override,
    // ret1=LoadMapDirectly flag). LocalMapEnter only supports 1 (map override).
    const int maxReturnValues = (eventType == EncounterHookEventType::LocalMapEnter) ? 1 : 2;

    // F-10 + F-20 (FIXED): Compute sfall-compatible arg0 encoding.
    // Sfall convention: 0 = normal random encounter, 1 = special encounter,
    // 0x100 (256) = forced encounter. CE-specific: LocalMapEnter uses value 2
    // to avoid collision with sfall's arg0=1 for special encounters.
    // For random encounters, special encounters are flagged with arg0=1.
    // For ForcedEncounter, the enum value (256) is passed directly to match
    // sfall's 0x100 convention.
    // For LocalMapEnter, the enum value (2) is passed directly.
    int arg0;
    switch (eventType) {
    case EncounterHookEventType::RandomEncounter:
        arg0 = isSpecial ? 1 : 0;
        break;
    case EncounterHookEventType::ForcedEncounter:
        arg0 = static_cast<int>(EncounterHookEventType::ForcedEncounter); // 256 = 0x100
        break;
    default:
        // LocalMapEnter — enum value (2) is already non-conflicting.
        arg0 = static_cast<int>(eventType);
        break;
    }

    ScriptHookCall hook(HOOK_ENCOUNTER, maxReturnValues,
        { arg0,
            *mapIdPtr,
            isSpecial ? 1 : 0,
            tableId,
            entryId });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return EncounterHookResult::ContinueEncounter;
    }

    int overrideMapId = hook.getReturnValueAt(0).asInt();
    if (eventType == EncounterHookEventType::LocalMapEnter) {
        if (overrideMapId >= 0) {
            *mapIdPtr = overrideMapId;
        }
        return EncounterHookResult::ContinueEncounter;
    }

    if (overrideMapId < -1) {
        overrideMapId = -1;
    }
    *mapIdPtr = overrideMapId;

    if (overrideMapId < 0) {
        return EncounterHookResult::ContinueTravel;
    }

    if (hook.numReturnValues() > 1 && hook.getReturnValueAt(1).asInt() == 1) {
        return EncounterHookResult::LoadMapDirectly;
    }

    return EncounterHookResult::ContinueEncounter;
}

/*
Runs before and after each turn in combat (for both PC and NPC).

int     arg0 - event type:
               1 - start of turn
               0 - normal end of turn
              -1 - combat ends abruptly (by script or by pressing Enter during PC turn)
              -2 - combat ends normally (hook always runs at the end of combat)
Critter arg1 - critter doing the turn
int     arg2 - 1 at the start/end of the player's turn after loading a game saved in combat mode, 0 otherwise

int     ret0 - pass 1 at the start of turn to skip the turn, pass -1 at the end of turn to force end of combat
*/
// returns true if turn should be skipped
bool scriptHooks_CombatTurnStart(Object* critter, bool reloadedDuringCombat)
{
    if (scriptHooks[HOOK_COMBATTURN].empty()) {
        return false;
    }

    ScriptHookCall hook(HOOK_COMBATTURN, 1, { 1, critter, reloadedDuringCombat ? 1 : 0 });
    hook.call();

    return hook.numReturnValues() > 0 && hook.getReturnValueAt(0).asInt() == 1;
}

// returns true if combat should end immediately
bool scriptHooks_CombatTurnEnd(Object* critter, int turnResult, bool reloadedDuringCombat)
{
    if (scriptHooks[HOOK_COMBATTURN].empty()) {
        return false;
    }

    ScriptHookCall hook(HOOK_COMBATTURN, 1, { turnResult, critter, reloadedDuringCombat ? 1 : 0 });
    hook.call();

    return hook.numReturnValues() > 0 && hook.getReturnValueAt(0).asInt() == -1;
}

void scriptHooks_CombatTurnCombatEnd(Object* critter)
{
    if (scriptHooks[HOOK_COMBATTURN].empty()) {
        return;
    }

    ScriptHookCall(HOOK_COMBATTURN, 0, { -2, critter, 0 }).call();
}

/*
Runs when a critter checks if another object is within perception range.

Critter arg0 - The watcher
Obj     arg1 - The target object
int     arg2 - The original engine result
int     arg3 - Call type:
               1 - obj_can_see_obj
               2 - obj_can_hear_obj
               3 - AI target selection
               0 - other engine checks

int     ret0 - Override the engine result:
               0 - not within perception
               1 - within perception
               2 - force detection for obj_can_see_obj
*/
PerceptionResult scriptHooks_WithinPerception(Object* watcher, Object* target, PerceptionType type, PerceptionResult result)
{
    if (scriptHooks[HOOK_WITHINPERCEPTION].empty()) {
        return result;
    }

    ScriptHookCall hook(HOOK_WITHINPERCEPTION, 1, { watcher, target, result, type });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return result;
    }

    int overrideResult = hook.getReturnValueAt(0).asInt();
    switch (overrideResult) {
    case PERCEPTION_OUT_OF_RANGE:
    case PERCEPTION_IN_RANGE:
    case PERCEPTION_FORCE:
        return static_cast<PerceptionResult>(overrideResult);
    default:
        return result;
    }
}

/*
Runs whenever Fallout calculates the AP cost of using an active item in hand
(or unarmed attack). Doesn't run for moving.

Critter arg0 - The critter performing the action
int     arg1 - Attack Type / hitmode (see ATKTYPE_* constants)
int     arg2 - Is aimed attack (1 or 0)
int     arg3 - The default AP cost
Item    arg4 - The weapon for which the cost is calculated. If it is 0, the
               pointer to the weapon can still be obtained by checking item
               slot based on attack type and then calling critter_inven_obj

int     ret0 - The new AP cost
*/
int scriptHooks_CalcApCost(Object* critter, int hitMode, bool aiming, int actionPoints, Object* weapon)
{
    if (scriptHooks[HOOK_CALCAPCOST].empty()) {
        return actionPoints;
    }

    ScriptHookCall hook(HOOK_CALCAPCOST, 1, { critter, hitMode, aiming ? 1 : 0, actionPoints, weapon });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return actionPoints;
    }

    int cost = hook.getReturnValueAt(0).asInt();
    // Clamp to minimum 0 — negative AP costs are nonsensical and can
    // cause AP gain exploits if a hook script returns a negative value.
    if (cost < 0) {
        cost = 0;
    }
    return cost;
}

/*
Runs when calculating the AP cost of movement.

The engine calls this both for full-path AP previews and for per-hex AP
deduction during movement animation. In practice, arg1 may therefore be the
full path length or 1, depending on the caller. Non-linear overrides can make
the UI preview diverge from the AP actually spent.

Critter arg0 - The critter doing the moving
int     arg1 - The number of hexes being moved
int     arg2 - The original AP cost

int     ret0 - The new AP cost
*/
int scriptHooks_MoveCost(Object* critter, int distance, int actionPoints)
{
    if (scriptHooks[HOOK_MOVECOST].empty()) {
        return actionPoints;
    }

    ScriptHookCall hook(HOOK_MOVECOST, 1, { critter, distance, actionPoints });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return actionPoints;
    }
    int cost = hook.getReturnValueAt(0).asInt();
    // Clamp to minimum 0 — negative AP costs would allow free movement
    // exploits if a hook script returns a negative value.
    if (cost < 0) {
        cost = 0;
    }
    return cost;
}

/*
Runs before moving items between inventory slots in dude interface. You can override the action.

int     arg0 - Target slot:
               0 - main backpack
               1 - left hand
               2 - right hand
               3 - armor slot
               4 - weapon, when reloading it by dropping ammo
               5 - container, like bag/backpack
               6 - dropping on the ground
               7 - picking up item
               8 - dropping item on the character portrait
Item    arg1 - Item being moved
Item    arg2 - Item being replaced, weapon being reloaded, or container being filled (can be 0)

int     ret0 - Override setting (-1 - use engine handler, any other value - prevent relocation)
*/
bool scriptHooks_InventoryMove(HookInventoryMoveType actionType, Object* item, Object* targetItem)
{
    if (scriptHooks[HOOK_INVENTORYMOVE].empty()) {
        return true;
    }

    ScriptHookCall hook(HOOK_INVENTORYMOVE, 1, { actionType, item, targetItem });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return true;
    }

    return hook.getReturnValueAt(0).asInt() == -1;
}

/*
Runs when Fallout is calculating the chances of an attack striking a target.
Runs after the hit chance is fully calculated normally, including applying the 95% cap.

int     arg0 - The hit chance (capped)
Critter arg1 - The attacker
Critter arg2 - The target of the attack
int     arg3 - The targeted bodypart
int     arg4 - Source tile (may differ from attacker's tile, when AI is considering potential fire position)
int     arg5 - Attack Type (see ATKTYPE_* constants)
int     arg6 - Ranged flag. 1 if the hit chance calculation takes into account the distance to the target. This does not mean the attack is a ranged attack
int     arg7 - The raw hit chance before applying the cap

int     ret0 - The new hit chance. The value is limited to the range of -99 to 999
*/
int scriptHooks_ToHit(Object* attacker, Object* defender, int tile, int hitMode, int hitLocation, int hitChance, int hitChanceUncapped, bool useDistance)
{
    if (scriptHooks[HOOK_TOHIT].empty()) {
        return hitChance;
    }

    ScriptHookCall hook(HOOK_TOHIT, 1,
        { hitChance,
            attacker,
            defender,
            hitLocation,
            tile,
            hitMode,
            useDistance,
            hitChanceUncapped });

    hook.call();

    if (hook.numReturnValues() <= 0) return hitChance;

    hitChance = hook.getReturnValueAt(0).asInt();
    return std::clamp(hitChance, -99, 999);
}

/*
Runs after Fallout has decided if an attack will hit or miss.

int     arg0 - If the attack will hit: 0 - critical miss, 1 - miss, 2 - hit, 3 - critical hit
Critter arg1 - The attacker
Critter arg2 - The target of the attack
int     arg3 - The bodypart
int     arg4 - The hit chance

int     ret0 - Override the hit/miss
int     ret1 - Override the targeted bodypart
Critter ret2 - Override the target of the attack
*/
int scriptHooks_AfterHitRoll(Object* attacker, Object** defenderPtr, int* hitLocationPtr, int hitChance, int roll)
{
    if (defenderPtr == nullptr || hitLocationPtr == nullptr) return roll;

    if (scriptHooks[HOOK_AFTERHITROLL].empty()) {
        return roll;
    }

    ScriptHookCall hook(HOOK_AFTERHITROLL, 3, { roll, attacker, *defenderPtr, *hitLocationPtr, hitChance });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return roll;
    }

    int rollOverride = hook.getReturnValueAt(0).asInt();
    if (rollOverride >= ROLL_CRITICAL_FAILURE && rollOverride <= ROLL_CRITICAL_SUCCESS) {
        roll = rollOverride;
    } else {
        debugPrint("HOOK_AFTERHITROLL: ignoring invalid roll override %d", rollOverride);
    }

    if (hook.numReturnValues() > 1) {
        int hitLocationOverride = hook.getReturnValueAt(1).asInt();
        if (hitLocationOverride >= 0 && hitLocationOverride < HIT_LOCATION_COUNT) {
            *hitLocationPtr = hitLocationOverride;
        } else {
            debugPrint("HOOK_AFTERHITROLL: ignoring invalid hit location override %d", hitLocationOverride);
        }
    }
    if (hook.numReturnValues() > 2) {
        Object* overrideDefender = hook.getReturnValueAt(2).asObject();
        // I2-04: The hook may return nullptr to cancel the attack.
        // However, the caller (combat.cc:4085-4154) has multiple
        // downstream paths that dereference attack->defender without
        // any null guard: distance recomputation, Silent Death check,
        // enhanced knockout, damage computation, and ranged-miss
        // handling.  Reject the null override to prevent crashes in
        // those paths; a hook that wants to cancel the attack should
        // set ret0=ROLL_FAILURE instead (F-39 fix covers this gap).
        // Sibling hooks (HOOK_COMBATTURN, HOOK_CANUSEWEAPON) follow
        // the same pattern — null Object* returns from asObject()
        // are treated as "no override."
        // F-31: Validate that the returned object is a critter before
        // accepting it as a defender override.  Non-critter objects
        // (items, scenery, walls, etc.) do not have valid critter
        // combat data, and dereferencing data.critter.combat on them
        // is undefined behavior.  Additionally, reject dead critters
        // (F-42): a dead defender causes incorrect Silent Death
        // multiplier computation in combat.cc (whoHitMe deref at
        // lines 4133 and 4170).  Reject the override and log a
        // diagnostic, consistent with how other invalid return values
        // are handled in this function.
        if (overrideDefender != nullptr && PID_TYPE(overrideDefender->pid) == OBJ_TYPE_CRITTER && !critterIsDead(overrideDefender)) {
            *defenderPtr = overrideDefender;
        } else if (overrideDefender != nullptr) {
            debugPrint("HOOK_AFTERHITROLL: ignoring non-critter or dead defender override (type=%d, dead=%d)", PID_TYPE(overrideDefender->pid), critterIsDead(overrideDefender) ? 1 : 0);
        }
    }

    return roll;
}

/*
Runs after Fallout has calculated the death animation. Lets you set your own death animation id. Performs no validation, so `art_exists` checks are advised.
When using `critter_dmg` function, this script will also run. In that case weapon pid will be -1 and attacker will point to an object with `obj_art_fid == 0x20001F5`.

Does not run for critters in the knockdown/out state.

int     arg0 - The pid of the weapon performing the attack. (-1 if the attack is unarmed)
Critter arg1 - The attacker
Critter arg2 - The target
int     arg3 - The amount of damage
int     arg4 - The death anim id calculated by Fallout

int     ret0 - The death anim id to override with
*/
void scriptHooks_DeathAnim(Object* attacker, Object* defender, Object* weapon, int damage, int* anim)
{
    if (anim == nullptr) {
        debugPrint("HOOK_DEATHANIM2: anim pointer is null, cannot override\n");
        return;
    }

    if (scriptHooks[HOOK_DEATHANIM2].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_DEATHANIM2, 1,
        { weapon != nullptr ? weapon->pid : -1,
            attacker,
            defender,
            damage,
            *anim });

    hook.call();

    if (hook.numReturnValues() > 0) {
        int animFid = hook.getReturnValueAt(0).asInt();
        // F-M03: Validate the animation FID is a known death animation.
        // Only death/knockdown animations (ANIM_FALL_BACK through ANIM_FALL_FRONT_BLOOD)
        // are valid for this hook; non-death-anim FIDs could crash the rendering pipeline.
        int animType = FID_ANIM_TYPE(animFid);
        if (animType >= FIRST_KNOCKDOWN_AND_DEATH_ANIM && animType <= LAST_KNOCKDOWN_AND_DEATH_ANIM) {
            *anim = animFid;
        } else {
            debugPrint("HOOK_DEATHANIM2: ignoring invalid anim FID 0x%x (type=%d outside death range [%d,%d])\n",
                       animFid, animType, FIRST_KNOCKDOWN_AND_DEATH_ANIM, LAST_KNOCKDOWN_AND_DEATH_ANIM);
        }
    }
}

/*
Runs before using any skill on any object. Lets you override the critter that uses the skill.

NOTE: The hook runs for Steal, but return values are ignored.

Critter arg0 - the user critter (usually dude_obj)
Obj     arg1 - the target object/critter
int     arg2 - skill being used

int     ret0 - a new critter to override the user critter. Pass -1 to cancel the skill use, pass 0 to skip this return value
int     ret1 - pass 1 to allow the skill to be used in combat
*/
UseSkillOnHookResult scriptHooks_UseSkillOn(Object** userPtr, Object* target, int skill)
{
    if (userPtr == nullptr || *userPtr == nullptr || target == nullptr) {
        return { true, false, false };
    }

    if (scriptHooks[HOOK_USESKILLON].empty()) {
        return { true, false, false };
    }

    UseSkillOnHookResult result = { true, false, false };

    ScriptHookCall hook(HOOK_USESKILLON, 2, { *userPtr, target, skill });
    hook.call();

    // sfall still runs the hook for Steal, but ignores return values.
    if (skill == SKILL_STEAL || hook.numReturnValues() <= 0) {
        return result;
    }

    ProgramValue userOverride = hook.getReturnValueAt(0);
    if (userOverride.isInt()) {
        int value = userOverride.asInt();
        if (value == -1) {
            result.shouldContinue = false;
            return result;
        }

        if (value != 0) {
            debugPrint("HOOK_USESKILLON: ignoring invalid user override %d", value);
        }
    } else {
        Object* overrideUser = userOverride.asObject();
        // F2-04: Validate that the returned object is a critter before
        // accepting it as a user override.  Non-critter objects do not
        // have valid critter data, and downstream dereferences of
        // data.critter.combat are undefined behavior.  This matches the
        // HOOK_AFTERHITROLL pattern at line ~1064.
        if (overrideUser != nullptr && PID_TYPE(overrideUser->pid) == OBJ_TYPE_CRITTER) {
            *userPtr = overrideUser;
            result.userOverridden = true;
        } else if (overrideUser != nullptr) {
            debugPrint("HOOK_USESKILLON: ignoring non-critter user override (type=%d)", PID_TYPE(overrideUser->pid));
        }
    }

    if (hook.numReturnValues() > 1 && hook.getReturnValueAt(1).asInt() == 1) {
        result.allowInCombat = true;
    }

    return result;
}

/*
Runs when using any skill on any object.

Does not run if the script of the object calls `script_overrides` for using the skill.

Critter arg0 - The user critter
Obj     arg1 - The target object
int     arg2 - skill being used
int     arg3 - skill bonus from items such as first aid kits

int     ret0 - overrides hard-coded handler (-1 - use engine handler, any other value - override)
*/
int scriptHooks_UseSkill(Object* user, Object* target, int skill, int skillBonus)
{
    if (scriptHooks[HOOK_USESKILL].empty()) {
        return -1;
    }

    ScriptHookCall hook(HOOK_USESKILL, 1, { user, target, skill, skillBonus });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return -1;
    }

    int overrideResult = hook.getReturnValueAt(0).asInt();
    // F-07: Validate return value — must be -1 (no override) or a valid skill ID
    if (overrideResult == -1) return -1;
    if (overrideResult >= 0 && overrideResult < SKILL_COUNT) return overrideResult;
    debugPrint("HOOK_USESKILL: ignoring invalid return value %d (expected -1 or 0..%d)\n", overrideResult, SKILL_COUNT - 1);
    return -1;
}

/*
Runs when:
- a critter uses an object from inventory which have “Use” action flag set or it’s an active flare or dynamite.
- player uses an object from main interface

This is fired before the object is used, and the relevant use_obj script procedures are run. You can disable default item behavior.

NOTE: You can’t remove and/or destroy this object during the hookscript (game will crash otherwise). To remove it, return 1.

Critter arg0 - The user
Obj     arg1 - The object used

int     ret0 - overrides hard-coded handler and selects what should happen with the item (0 - place it back, 1 - remove it, -1 - use engine handler)
*/
// TODO: there's an inconsistency with the use of rc = 2. It drops items when used from the main interface, but not from inventory context menu. This matches sfall, but should probably be improved.
int scriptHooks_UseItem(Object* user, Object* objUsed)
{
    if (scriptHooks[HOOK_USEOBJ].empty()) {
        return -1;
    }

    ScriptHookCall hook(HOOK_USEOBJ, 1, { user, objUsed });
    hook.call();

    if (hook.numReturnValues() <= 0)
        return -1;

    int overrideResult = hook.getReturnValueAt(0).asInt();
    // F-07: Validate return value — valid codes are -1 (engine handler),
    // 0 (place back), 1 (remove), or 2 (drop).
    if (overrideResult >= -1 && overrideResult <= 2) {
        return overrideResult;
    }
    debugPrint("HOOK_USEOBJ: ignoring invalid return value %d (expected -1..2)\n", overrideResult);
    return -1;
}

/*
Runs when:
- a critter uses an object on another critter. (Or themselves)
- a critter uses an object from inventory screen AND this object does not have “Use” action flag set and it’s not active flare or explosive.
- player or AI uses any drug

This is fired before the object is used, and the relevant use_obj_on script procedures are run. You can disable default item behavior.

NOTE: You can’t remove and/or destroy this object during the hookscript (game will crash otherwise). To remove it, return 1.

Critter arg0 - The target
Critter arg1 - The user
int     arg2 - The object used

int     ret0 - overrides hard-coded handler and selects what should happen with the item (0 - place it back, 1 - remove it, -1 - use engine handler)
*/
int scriptHooks_UseItemOn(Object* user, Object* target, Object* objUsed)
{
    if (scriptHooks[HOOK_USEOBJON].empty()) {
        return -1;
    }

    ScriptHookCall hook(HOOK_USEOBJON, 1, { target, user, objUsed });
    hook.call();

    if (hook.numReturnValues() <= 0)
        return -1;

    int overrideResult = hook.getReturnValueAt(0).asInt();
    // F-07: Validate return value — valid codes are -1 (engine handler),
    // 0 (place back), 1 (remove), or 2 (drop).
    if (overrideResult >= -1 && overrideResult <= 2) {
        return overrideResult;
    }
    debugPrint("HOOK_USEOBJON: ignoring invalid return value %d (expected -1..2)\n", overrideResult);
    return -1;
}

/*
Runs when:

- Game calculates how much damage each target will get. This includes primary target as well as all extras (explosions and bursts). This happens BEFORE the actual attack animation.
- AI decides whether it is safe to use area attack (burst, grenades), if he might hit friendlies.

Does not run for misses, or non-combat damage like dynamite explosions.

Critter arg0  - The target
Critter arg1  - The attacker
int     arg2  - The amount of damage to the target
int     arg3  - The amount of damage to the attacker
int     arg4  - The special effect flags for the target (use bwand DAM_* to check specific flags)
int     arg5  - The special effect flags for the attacker (use bwand DAM_* to check specific flags)
Item    arg6  - The weapon used in the attack
int     arg7  - The bodypart that was struck
int     arg8  - Damage Multiplier (this is divided by 2, so a value of 3 does 1.5x damage, and 8 does 4x damage. Usually it's 2; for critical hits, the value is taken from the critical table; with Silent Death perk and the corresponding attack conditions, the value will be doubled)
int     arg9 - Number of bullets actually hit the target (1 for melee attacks)
int     arg10 - The amount of knockback to the target
int     arg11 - Attack Type (see ATKTYPE_* constants)
mixed   arg12 - computed attack data (see C_ATTACK_* for offsets and use get/set_object_data functions to get/set the data)

int     ret0 - The damage to the target
int     ret1 - The damage to the attacker
int     ret2 - The special effect flags for the target
int     ret3 - The special effect flags for the attacker
int     ret4 - The amount of knockback to the target
*/
void scriptHooks_ComputeDamage(Attack* attack, int numRounds, int baseDmgMult)
{
    if (scriptHooks[HOOK_COMBATDAMAGE].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_COMBATDAMAGE, 5,
        {
            attack->defender,
            attack->attacker,
            attack->defenderDamage,
            attack->attackerDamage,
            attack->defenderFlags,
            attack->attackerFlags,
            attack->weapon,
            attack->defenderHitLocation,
            baseDmgMult,
            numRounds,
            attack->defenderKnockback,
            attack->hitMode,
            attack // this is how sfall did it — get_object_data has bounds protection (sizeof(Object)), set_object_data is not implemented
        });

    hook.call();

    // F-01: Validate and clamp hook return values. Sibling hooks
    // (HOOK_ITEMDAMAGE, HOOK_TOHIT, HOOK_AFTERHITROLL) all clamp/validate their
    // return values; HOOK_COMBATDAMAGE was the only hook writing raw script return
    // values without any bounds checking.
    constexpr int DAMAGE_FIELDS[] = {0, 1};          // defenderDamage, attackerDamage
    constexpr int FLAGS_FIELDS[] = {2, 3};           // defenderFlags, attackerFlags
    constexpr int KNOCKBACK_FIELD = 4;               // defenderKnockback
    constexpr int COMBAT_DAMAGE_MIN = 0;
    constexpr int COMBAT_DAMAGE_MAX = 9999;
    // Mask matching _set_new_results (combat.cc:5192-5199): only these
    // flags persist into critter->data.critter.combat.results.
    constexpr int COMBAT_FLAGS_MASK = DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_CRIP | DAM_DEAD | DAM_LOSE_TURN;
    constexpr int COMBAT_KNOCKBACK_MIN = 0;
    // COMBAT_KNOCKBACK_MAX is aligned with MAX_KNOCKDOWN_DISTANCE (actions.h:20).
    // All knockback consumers flow through actionKnockdown which caps at
    // MAX_KNOCKDOWN_DISTANCE; advertising a wider range in the hook API would
    // silently clamp higher values to 20, misleading mod authors.
    constexpr int COMBAT_KNOCKBACK_MAX = MAX_KNOCKDOWN_DISTANCE;

    int* fields[] = {
        &attack->defenderDamage,
        &attack->attackerDamage,
        &attack->defenderFlags,
        &attack->attackerFlags,
        &attack->defenderKnockback
    };

    int numRets = hook.numReturnValues();
    for (int i = 0; i < numRets; i++) {
        int value = hook.getReturnValueAt(i).asInt();
        // Clamp damage values.
        if (i == DAMAGE_FIELDS[0] || i == DAMAGE_FIELDS[1]) {
            value = std::clamp(value, COMBAT_DAMAGE_MIN, COMBAT_DAMAGE_MAX);
        }
        // Mask flag values to valid DAM_* bits.
        if (i == FLAGS_FIELDS[0] || i == FLAGS_FIELDS[1]) {
            value &= COMBAT_FLAGS_MASK;
        }
        // Clamp knockback value.
        if (i == KNOCKBACK_FIELD) {
            value = std::clamp(value, COMBAT_KNOCKBACK_MIN, COMBAT_KNOCKBACK_MAX);
        }
        *fields[i] = value;
    }
}

/**
Runs whenever the value of goods being purchased is calculated.

NOTE: the hook is executed twice when entering the barter screen or after transaction: the first time is for the player and the second time is for NPC.

    Obj     arg0 - the critter doing the bartering (either dude_obj or inven_dude)
    Critter arg1 - the critter being bartered with
    int     arg2 - the default value of the goods
    Obj     arg3 - table of requested goods (being bought from NPC)
    int     arg4 - the number of actual caps in the barter stack (as opposed to goods)
    int     arg5 - the value of all goods being traded before skill modifications
    Obj     arg6 - table of offered goods (being sold to NPC)
    int     arg7 - the total cost of the goods offered by the player
    int     arg8 - 1 if the "offers" button was pressed (not for a party member), 0 otherwise
    int     arg9 - 1 if trading with a party member, 0 otherwise

    int     ret0 - the modified value of all of the goods (pass -1 if you just want to modify offered goods)
    int     ret1 - the modified value of all offered goods
*/
void scriptHooks_BarterPrice(BarterPriceContext* ctx)
{
    if (ctx == nullptr) return;

    if (scriptHooks[HOOK_BARTERPRICE].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_BARTERPRICE, 2,
        { ctx->dude,
            ctx->npc,
            ctx->value,
            ctx->requestTable,
            ctx->caps,
            ctx->rawValue,
            ctx->offerTable,
            ctx->offerValue,
            ctx->offerButton,
            ctx->partyMember });

    hook.call();

    // F-27: Add upper-bound clamping to prevent arbitrarily inflated barter
    // values. The previous check only rejected negative values, allowing
    // any non-negative int (including INT_MAX). Matches the DAMAGE_MIN/DAMAGE_MAX
    // precedent from HOOK_ITEMDAMAGE in the same file.
    constexpr int BARTER_PRICE_MIN = 0;
    constexpr int BARTER_PRICE_MAX = 9999999;

    int* fields[] = {
        &ctx->value,
        &ctx->offerValue
    };

    const int numRets = hook.numReturnValues();
    for (int i = 0; i < numRets; i++) {
        const int valueFromScript = hook.getReturnValueAt(i).asInt();
        if (valueFromScript < 0) continue;
        *fields[i] = std::clamp(valueFromScript, BARTER_PRICE_MIN, BARTER_PRICE_MAX);
    }
}

/*
    HOOK_ADJUSTFID

    Runs when the game calculates what FID to display for a critter in UI
    (inventory, barter).

    int     arg0 - the vanilla FID calculated by the engine
    int     arg1 - the modified FID after internal CE adjustments (currently always the same as arg0)

    int     ret0 - override FID
*/
int scriptHooks_AdjustFid(int vanillaFid, int modifiedFid)
{
    if (scriptHooks[HOOK_ADJUSTFID].empty()) {
        return modifiedFid;
    }

    ScriptHookCall hook(HOOK_ADJUSTFID, 1, { vanillaFid, modifiedFid });
    hook.call();

    if (hook.numReturnValues() > 0) {
        int overrideFid = hook.getReturnValueAt(0).asInt();
        // F-07: Validate the return FID — must be a critter FID
        // (OBJ_TYPE_CRITTER type bits) since this hook is for character
        // FID display in UI (inventory, barter).
        if (FID_TYPE(overrideFid) == OBJ_TYPE_CRITTER) {
            return overrideFid;
        }
        debugPrint("HOOK_ADJUSTFID: ignoring invalid FID 0x%x (type=%d, expected OBJ_TYPE_CRITTER=%d)\n",
                   overrideFid, FID_TYPE(overrideFid), OBJ_TYPE_CRITTER);
        return modifiedFid;
    }

    return modifiedFid;
}

/*
    HOOK_INVENWIELD

    Runs before causing a critter or the player to wield/unwield an armor or a weapon (except when using the inventory by PC). An example usage would be to change critter art depending on armor being used or to dynamically customize weapon animations.

    Critter arg0 - the critter wielding/unwielding
    Item    arg1 - the item being moved
    int     arg2 - INVEN_TYPE_WORN (0), INVEN_TYPE_RIGHT_HAND (1), or
                   INVEN_TYPE_LEFT_HAND (2)
    int     arg3 - 1 on wield, 0 on unwield
    int     arg4 - 1 when removing an equipped item from inventory, 0 otherwise

    int     ret0 - overrides hard-coded handler (-1 = use engine handler) (NOT RECOMMENDED)
*/
bool scriptHooks_InvenWield(Object* critter, Object* item, InvenSlot slot, int isWield, int isRemove, bool filterInactiveHand)
{
    if (filterInactiveHand && !isWield) {
        // Sfall: NPCs only ever expose the active (right) hand here.
        if (slot == InvenSlot::LeftHand && critter != gDude) {
            return true;
        }

        // Sfall: ignore player non-active hand slot.
        if (slot != InvenSlot::Armor && critter == gDude) {
            int activeSlot = slot == InvenSlot::RightHand ? HAND_RIGHT : HAND_LEFT;
            if (activeSlot != interfaceGetCurrentHand()) {
                return true;
            }
        }
    }

    if (scriptHooks[HOOK_INVENWIELD].empty()) {
        return true;
    }

    ScriptHookCall hook(HOOK_INVENWIELD, 1, { critter, item, static_cast<int>(slot), isWield, isRemove });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return true;
    }

    int result = hook.getReturnValueAt(0).asInt();
    return result == -1;
}

/*
    HOOK_CANUSEWEAPON

    Runs while the AI or interface bar checks whether a weapon can be used.

    Critter arg0 - the critter being evaluated
    Item    arg1 - the candidate weapon
    int     arg2 - attack type / hit mode, or -1 for dude_obj interface checks
    int     arg3 - original engine result: 1 if weapon can be used, 0 otherwise

    int     ret0 - overrides the result of engine function
*/
bool scriptHooks_CanUseWeapon(bool result, Object* critter, Object* weapon, int hitMode)
{
    if (scriptHooks[HOOK_CANUSEWEAPON].empty()) {
        return result;
    }

    ScriptHookCall hook(HOOK_CANUSEWEAPON, 1, { critter, weapon, hitMode, result ? 1 : 0 });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return result;
    }
    return hook.getReturnValueAt(0).asInt() != 0;
}

/*
Runs when a script triggers an object animation via animate_stand_obj or
animate_stand_reverse_obj. Allows scripts to intercept or modify the animation
before it is registered.

Obj     arg0 - the object being animated
int     arg1 - the animation ID (ANIM_STAND or other)
int     arg2 - the animation delay (0 for immediate)
*/
void scriptHooks_UseAnimObj(Object* object, int animId, int delay)
{
    if (scriptHooks[HOOK_USEANIMOBJ].empty()) {
        return;
    }

    ScriptHookCall(HOOK_USEANIMOBJ, 0, { object, animId, delay }).call();
}

/*
Runs when the player examines an object (right-click to view description).
Per sfall 4.4.0+: the hook can return a plain string directly to override
the description text displayed to the player.

Critter arg0 - the critter performing the examination (may be null)
Obj     arg1 - the object being examined
string  arg2 - the default description text (from proto or script override)

string  ret0 - the new description text to display (empty string = no override)
*/
void scriptHooks_DescriptionObj(Object* examiner, Object* target, std::string& description)
{
    if (scriptHooks[HOOK_DESCRIPTIONOBJ].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_DESCRIPTIONOBJ, 1, { examiner, target, description.c_str() });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return;
    }

    ProgramValue retVal = hook.getReturnValueAt(0);
    const char* overrideDesc = retVal.isString()
        ? retVal.asString(hook.programForReturnValueAt(0))
        : "";
    if (overrideDesc != nullptr && overrideDesc[0] != '\0') {
        description = overrideDesc;
    }
}

/*
Runs when a per-object or ambient lighting level changes.
Allows scripts to override the light intensity or distance.

Obj     arg0 - the object whose light changed (nullptr for ambient light changes)
int     arg1 - the new light intensity
int     arg2 - the new light distance

int     ret0 - overridden light intensity (pass -1 to keep engine value)
int     ret1 - overridden light distance (pass -1 to keep engine value)
*/
void scriptHooks_SetLighting(Object* object, int* lightIntensityPtr, int* lightDistancePtr)
{
    if (scriptHooks[HOOK_SETLIGHTING].empty()) {
        return;
    }

    const int lightIntensity = lightIntensityPtr ? *lightIntensityPtr : 0;
    const int lightDistance = lightDistancePtr ? *lightDistancePtr : 0;

    ScriptHookCall hook(HOOK_SETLIGHTING, 2, { object, lightIntensity, lightDistance });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return;
    }

    if (lightIntensityPtr != nullptr && hook.numReturnValues() >= 1) {
        const int overrideIntensity = hook.getReturnValueAt(0).asInt();
        if (overrideIntensity != -1) {
            *lightIntensityPtr = overrideIntensity;
        }
    }

    if (lightDistancePtr != nullptr && hook.numReturnValues() >= 2) {
        const int overrideDistance = hook.getReturnValueAt(1).asInt();
        if (overrideDistance != -1) {
            *lightDistancePtr = std::min(overrideDistance, 8);
        }
    }
}

/*
Runs continuously during world map travel by car. Allows overriding car speed
(number of steps per worldmap tick) and fuel consumption per step.

int     arg0 - vanilla car speed (number of wmPartyWalkingStep calls per tick)
int     arg1 - vanilla fuel consumption per tick

int     ret0 - car speed override (pass -1 to keep engine value)
int     ret1 - fuel consumption override (pass -1 to keep engine value)
*/
void scriptHooks_CarTravel(int* speedPtr, int* fuelConsumptionPtr)
{
    if (speedPtr == nullptr || fuelConsumptionPtr == nullptr) return;

    if (scriptHooks[HOOK_CARTRAVEL].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_CARTRAVEL, 2, { *speedPtr, *fuelConsumptionPtr });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return;
    }

    int speedOverride = hook.getReturnValueAt(0).asInt();
    if (speedOverride >= 0) {
        *speedPtr = speedOverride;
    }

    if (hook.numReturnValues() > 1) {
        int fuelOverride = hook.getReturnValueAt(1).asInt();
        if (fuelOverride >= 0) {
            *fuelConsumptionPtr = fuelOverride;
        }
    }
}

/*
Runs when setting the value of a global variable via op_set_global_var.
Allows scripts to override the value that gets stored.

int     arg0 - the index number of the global variable being set
int     arg1 - the value being set

int     ret0 - overrides the value of the global variable
*/
int scriptHooks_SetGlobalVar(int varIndex, int value)
{
    if (scriptHooks[HOOK_SETGLOBALVAR].empty()) {
        return value;
    }

    ScriptHookCall hook(HOOK_SETGLOBALVAR, 1, { varIndex, value });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return value;
    }

    int overrideValue = hook.getReturnValueAt(0).asInt();
    // F-07: Log when a script overrides a global var value, for
    // diagnostics.  Global vars are unbounded in practice, so no
    // strict clamping is applied, but extreme values are noted.
    if (overrideValue != value) {
        debugPrint("HOOK_SETGLOBALVAR: script overriding global[%d] from %d to %d\n",
                   varIndex, value, overrideValue);
    }
    return overrideValue;
}

/*
Runs when the Sneak skill is activated or when the game rolls another sneak
check after the current duration expires. The hook fires after skillRoll
determines success/failure and the duration is computed.

int     arg0 - Sneak check result: 1 - success (sneak working), 0 - failure
int     arg1 - the duration in ticks for the current sneak check
Critter arg2 - the critter (usually dude_obj)

int     ret0 - overrides the result of the sneak check
int     ret1 - overrides the duration time for the current result
*/
void scriptHooks_Sneak(int* resultPtr, int* durationPtr, Object* critter)
{
    if (resultPtr == nullptr || durationPtr == nullptr) {
        debugPrint("HOOK_SNEAK: resultPtr or durationPtr is null, cannot override\n");
        return;
    }

    if (scriptHooks[HOOK_SNEAK].empty()) {
        return;
    }

    ScriptHookCall hook(HOOK_SNEAK, 2, { *resultPtr, *durationPtr, critter });
    hook.call();

    if (hook.numReturnValues() <= 0) {
        return;
    }

    const int overrideResult = hook.getReturnValueAt(0).asInt();
    if (overrideResult != -1) {
        *resultPtr = overrideResult;
    }

    if (hook.numReturnValues() > 1) {
        const int overrideDuration = hook.getReturnValueAt(1).asInt();
        if (overrideDuration >= 0) {
            *durationPtr = overrideDuration;
        }
    }
}

/*
Runs when an explosion occurs (timer expires or premature detonation).

Obj     arg0 - The explosive object
int     arg1 - The tile where the explosion occurs
int     arg2 - The elevation
int     arg3 - The minimum damage
int     arg4 - The maximum damage
Obj     arg5 - The source critter (who set/owns the explosive, may be nullptr)
*/
void scriptHooks_OnExplosion(Object* explosive, int tile, int elevation, int minDamage, int maxDamage, Object* sourceObj)
{
    if (scriptHooks[HOOK_ONEXPLOSION].empty()) {
        return;
    }

    ScriptHookCall(HOOK_ONEXPLOSION, 0, { explosive, tile, elevation, minDamage, maxDamage, sourceObj }).call();
}

/*
Runs when a critter (player or AI) selects a target to attack.

Critter arg0 - The attacker
Obj     arg1 - The target object (defender)
int     arg2 - The hit mode / attack type (see ATKTYPE_* constants)
int     arg3 - The hit location (see HIT_LOCATION_* constants)
*/
void scriptHooks_TargetObject(Object* attacker, Object* defender, int hitMode, int hitLocation)
{
    if (scriptHooks[HOOK_TARGETOBJECT].empty()) {
        return;
    }

    ScriptHookCall(HOOK_TARGETOBJECT, 0, { attacker, defender, hitMode, hitLocation }).call();
}

/*
HOOK_GAMEMODECHANGE slideshow scaffolding — fires during endgame slideshow
transitions.

Wire-in points (in endgame.cc or game.cc):
  scriptHooks_SlideshowStart() → call at the start of endgamePlaySlideshow()
    (endgame.cc:214) after the slideshow window is created but before slides
    start rendering. This notifies scripts that the game is entering the
    slideshow mode. Passes exit=0, previous=GameMode::getCurrentGameMode().

  scriptHooks_SlideshowEnd() → call after endgamePlaySlideshow() returns
    (endgame.cc:233) and before endgameEndingSlideshowWindowFree(). This
    notifies scripts that the game is leaving slideshow mode. Passes exit=0,
    previous=GameMode::getCurrentGameMode().

These are wired in at endgamePlaySlideshow() and endgamePlayMovie()
in endgame.cc. The endgame caller has its own reentrancy guard
(gEndgameInProgress), and MaxHookCallDepth (8) provides a secondary
safety net. The _gameModeChangeInProgress guard in both functions
below adds defense-in-depth protection against recursive mode-change
hook dispatches during the slideshow.

When the hook fires:
  arg0 - 0 (not exiting the game; normal mode transition)
  arg1 - the game mode before entering/leaving the slideshow
         (typically 0 or kWorldmap at start; 0 at end)
*/
void scriptHooks_SlideshowStart()
{
    // I2-M07: Drain stale entries from _callStack BEFORE the reentrancy check,
    // mirroring scriptHooks_GameModeChange.  A longjmp from programFatalError
    // during a previous slideshow hook dispatch leaves stale entries that make
    // current() return a non-null stale pointer, permanently blocking slideshow
    // transitions.
    {
        int stackAnchor = 0;
        ScriptHookCall::drainStaleEntries(reinterpret_cast<uintptr_t>(&stackAnchor));
    }

    if (ScriptHookCall::_gameModeChangeInProgress) {
        if (ScriptHookCall::current() != nullptr) {
            // Legitimate reentrancy: a slideshow hook dispatch is actively
            // in progress.  Block recursive mode change.
            return;
        }
        // Flag is stuck from a previous longjmp.
        // No hook is currently dispatching — reset and proceed.
        ScriptHookCall::_gameModeChangeInProgress = false;
    }

    if (scriptHooks[HOOK_GAMEMODECHANGE].empty()) {
        return;
    }

    GameModeChangeGuard guard;
    ScriptHookCall(HOOK_GAMEMODECHANGE, 0, { 0, GameMode::getCurrentGameMode() }).call();
}

void scriptHooks_SlideshowEnd()
{
    // I2-M07: Same drain-and-recovery as SlideshowStart.
    {
        int stackAnchor = 0;
        ScriptHookCall::drainStaleEntries(reinterpret_cast<uintptr_t>(&stackAnchor));
    }

    if (ScriptHookCall::_gameModeChangeInProgress) {
        if (ScriptHookCall::current() != nullptr) {
            return;
        }
        ScriptHookCall::_gameModeChangeInProgress = false;
    }

    if (scriptHooks[HOOK_GAMEMODECHANGE].empty()) {
        return;
    }

    GameModeChangeGuard guard;
    ScriptHookCall(HOOK_GAMEMODECHANGE, 0, { 0, GameMode::getCurrentGameMode() }).call();
}

/*
Runs when the player character levels up after gaining enough experience.
Fires from pcAddExperienceWithOptions() in stat.cc and from
characterEditorUpdateLevel() in character_editor.cc.

Critter arg0 - the critter that leveled up (usually dude_obj)
*/
void scriptHooks_StatLevelUp(Object* critter)
{
    if (scriptHooks[HOOK_STATLEVELUP].empty()) {
        return;
    }

    ScriptHookCall(HOOK_STATLEVELUP, 0, { critter }).call();
}

/*
Runs when barter/trade is initiated between the player and an NPC.
Fires from gameDialogBarter() in game_dialog.cc.

Critter arg0 - the player character (dude)
Critter arg1 - the NPC being traded with
int     arg2 - the barter mode (dialog type)
*/
void scriptHooks_Barter(Object* dude, Object* npc, int mode)
{
    if (scriptHooks[HOOK_BARTER].empty()) {
        return;
    }

    ScriptHookCall(HOOK_BARTER, 0, { dude, npc, mode }).call();
}

/*
Runs when a message is displayed to the player in the message monitor.
Fires from displayMonitorAddMessage() in display_monitor.cc.

string  arg0 - the message text being displayed
*/
void scriptHooks_Message(const char* msg)
{
    if (scriptHooks[HOOK_MESSAGE].empty()) {
        return;
    }

    ScriptHookCall(HOOK_MESSAGE, 0, { msg }).call();
}

} // namespace fallout
