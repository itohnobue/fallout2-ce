#include "sfall_callbacks.h"

#include "content_config.h"
#include "display_monitor.h"
#include "interface.h"
#include "inventory.h"
#include "script_sound.h"
#include "sfall_config.h"
#include "sfall_global_vars.h"
#include "sfall_script_hooks.h"
#include "stat.h"
#include "worldmap.h"

namespace fallout {

void sfallOnBeforeGameInit()
{
    return;
}

void sfallOnGameInit()
{
    return;
}

void sfallOnAfterGameInit()
{
    return;
}

void sfallOnGameExit()
{
    scriptSoundExit();
    return;
}

void sfallOnGameReset()
{
    inventoryResetInvenApCost();
    scriptSoundReset();
    statResetUnspentApBonuses();

    // Re-initialize SpeedMulti from ddraw.ini after game reset.
    // sfall_gl_vars_reset() clears all sfall global vars including SpeedMulti (key 0).
    // Without re-init, SpeedMulti defaults to 100% in subsequent new games.
    {
        int speedMultiValue = 100;
        bool hasSpeedMulti = configGetInt(&gSfallConfig, SFALL_CONFIG_SPEED_KEY, SFALL_CONFIG_SPEED_MULTI_INITIAL_KEY, &speedMultiValue);
        if (!hasSpeedMulti) {
            configGetInt(&gSfallConfig, SFALL_CONFIG_SPEED_KEY, SFALL_CONFIG_SPEED_MULTI_KEY, &speedMultiValue);
        }
        if (speedMultiValue <= 0) {
            speedMultiValue = 100; // 0 would freeze the game
        }
        sfall_gl_vars_store(0, speedMultiValue);
    }

    // Initialize inventory AP cost from sfall config
    {
        int invenApCost = 4;
        int quickPocketsReduction = 2;
        configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "InventoryApCost", &invenApCost);
        configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "QuickPocketsApCostReduction", &quickPocketsReduction);
        inventorySetInvenApCost(invenApCost);
        inventorySetQuickPocketsApCostReduction(quickPocketsReduction);
    }

    return;
}

void sfallOnBeforeGameStart()
{
    return;
}

void sfallOnAfterGameStarted()
{
    // Disable Horrigan Patch
    bool isDisableHorrigan = false;
    configGetBool(&gContentConfig, CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan", &isDisableHorrigan);

    if (isDisableHorrigan) {
        gDidMeetFrankHorrigan = true;
    }

    // Refresh item art after load, which calls the CALCAPCOST hook if present to
    // display the correct AP cost.
    if (gInterfaceBarWindow != -1) {
        int leftItemAction;
        int rightItemAction;
        interfaceGetItemActions(&leftItemAction, &rightItemAction);
        interfaceUpdateItems(false, leftItemAction, rightItemAction);
    }
}

void sfallOnAfterNewGame()
{
    return;
}

void sfallOnGameModeChange(int exit, int previousGameMode)
{
    scriptHooks_GameModeChange(exit, previousGameMode);
}

void sfallOnBeforeGameClose()
{
    return;
}

void sfallOnCombatStart()
{
    return;
}

void sfallOnCombatEnd()
{
    return;
}

void sfallOnBeforeMapLoad()
{
    return;
}

} // namespace fallout
