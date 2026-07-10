#include "sfall_arrays.h"

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "db.h"
#include "debug.h"
#include "interpreter.h"
#include "sfall_lists.h"

namespace fallout {

static constexpr ArrayId kInitialArrayId = 1;

#define ARRAY_MAX_STRING (1024) // maximum length of string to be stored as array key or value
#define ARRAY_MAX_SIZE (100000) // maximum number of array elements,

// special actions for arrays using array_resize operator
#define ARRAY_ACTION_SORT (-2)
#define ARRAY_ACTION_RSORT (-3)
#define ARRAY_ACTION_REVERSE (-4)
#define ARRAY_ACTION_SHUFFLE (-5)

template <class T, typename Compare>
static void ListSort(std::vector<T>& arr, int type, Compare cmp)
{
    switch (type) {
    case ARRAY_ACTION_SORT: // sort ascending
        std::sort(arr.begin(), arr.end(), cmp);
        break;
    case ARRAY_ACTION_RSORT: // sort descending
        std::sort(arr.rbegin(), arr.rend(), cmp);
        break;
    case ARRAY_ACTION_REVERSE: // reverse elements
        std::reverse(arr.begin(), arr.end());
        break;
    case ARRAY_ACTION_SHUFFLE: // shuffle elements
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(arr.begin(), arr.end(), g);
        break;
    }
}

enum class ArrayElementType {
    INT,
    FLOAT,
    STRING,
    POINTER
};

// Special key for load_array: returns a temp array of all saved array keys.
static constexpr char kAllArraysSpecialKey[] = "...all_arrays...";

/**
 * This is mostly the same as ProgramValue but it owns strings.
 *
 * This is done because when we pop dynamic string element from
 * the stack we decrease ref count for this string and it memory
 * can be freed.
 *
 * In theory arrays can be shared between programs so we also
 * have to copy static strings.
 *
 */
class ArrayElement {
public:
    ArrayElement()
        : type(ArrayElementType::INT)
        , value({ 0 })
    {
    }

    ArrayElement(const ArrayElement& other)
        : type(ArrayElementType::INT)
        , value({ 0 })
    {
        if (other.type == ArrayElementType::STRING) {
            setString(other.value.stringValue, strlen(other.value.stringValue));
        } else {
            type = other.type;
            value = other.value;
        }
    }

    ArrayElement& operator=(const ArrayElement& other)
    {
        if (this != &other) {
            ArrayElement tmp(other);
            std::swap(type, tmp.type);
            std::swap(value, tmp.value);
        }
        return *this;
    }

    ArrayElement(ArrayElement&& other) noexcept
        : type(ArrayElementType::INT)
        , value({ 0 })
    {
        std::swap(type, other.type);
        std::swap(value, other.value);
    }

    ArrayElement& operator=(ArrayElement&& other) noexcept
    {
        std::swap(type, other.type);
        std::swap(value, other.value);
        return *this;
    }

    ArrayElement(ProgramValue programValue, Program* program)
    {
        switch (programValue.opcode) {
        case VALUE_TYPE_INT:
            type = ArrayElementType::INT;
            value.integerValue = programValue.integerValue;
            break;
        case VALUE_TYPE_FLOAT:
            type = ArrayElementType::FLOAT;
            value.floatValue = programValue.floatValue;
            break;
        case VALUE_TYPE_PTR:
            type = ArrayElementType::POINTER;
            value.pointerValue = programValue.pointerValue;
            break;
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            setString(programGetString(program, programValue.opcode, programValue.integerValue), -1);
            break;
        default:
            type = ArrayElementType::INT;
            value.integerValue = 0;
            break;
        }
    }

    ArrayElement(const char* str)
    {
        setString(str, -1);
    }

    ArrayElement(const char* str, size_t sLen)
    {
        setString(str, sLen);
    }

    ProgramValue toValue(Program* program) const
    {
        ProgramValue out;
        switch (type) {
        case ArrayElementType::INT:
            out.opcode = VALUE_TYPE_INT;
            out.integerValue = value.integerValue;
            break;
        case ArrayElementType::FLOAT:
            out.opcode = VALUE_TYPE_FLOAT;
            out.floatValue = value.floatValue;
            break;
        case ArrayElementType::POINTER:
            out.opcode = VALUE_TYPE_PTR;
            out.pointerValue = value.pointerValue;
            break;
        case ArrayElementType::STRING:
            out.opcode = VALUE_TYPE_DYNAMIC_STRING;
            out.integerValue = programPushString(program, value.stringValue);
            break;
        }
        return out;
    }

    ArrayElementType getType() const { return type; }
    int getRawValue() const { return value.integerValue; }
    const char* getString() const { return value.stringValue; }

    const char* getAsDebugString() const
    {
        static char debugStrBuf[ARRAY_MAX_STRING];
        switch (type) {
        case ArrayElementType::INT:
            snprintf(debugStrBuf, sizeof(debugStrBuf), "%d", value.integerValue);
            break;
        case ArrayElementType::FLOAT:
            snprintf(debugStrBuf, sizeof(debugStrBuf), "%.5f", value.floatValue);
            break;
        case ArrayElementType::POINTER:
            snprintf(debugStrBuf, sizeof(debugStrBuf), "%p", value.pointerValue);
            break;
        case ArrayElementType::STRING:
            snprintf(debugStrBuf, sizeof(debugStrBuf), "%s", value.stringValue);
            break;
        default:
            snprintf(debugStrBuf, sizeof(debugStrBuf), "(unknown)");
        }
        return debugStrBuf;
    }

    bool operator<(ArrayElement const& other) const
    {
        if (type != other.type) {
            return type < other.type;
        }

        switch (type) {
        case ArrayElementType::INT:
            return value.integerValue < other.value.integerValue;
        case ArrayElementType::FLOAT:
            return value.floatValue < other.value.floatValue;
        case ArrayElementType::POINTER:
            return value.pointerValue < other.value.pointerValue;
        case ArrayElementType::STRING:
            return strcmp(value.stringValue, other.value.stringValue) < 0;
        default:
            return false;
        }
    }

    bool operator==(ArrayElement const& other) const
    {
        if (type != other.type) {
            return false;
        }

        switch (type) {
        case ArrayElementType::INT:
            return value.integerValue == other.value.integerValue;
        case ArrayElementType::FLOAT:
            return value.floatValue == other.value.floatValue;
        case ArrayElementType::POINTER:
            return value.pointerValue == other.value.pointerValue;
        case ArrayElementType::STRING:
            return strcmp(value.stringValue, other.value.stringValue) == 0;
        default:
            return false;
        }
    }

    ~ArrayElement()
    {
        if (type == ArrayElementType::STRING) {
            free(value.stringValue);
        }
    }

private:
    void setString(const char* str, size_t sLen)
    {
        type = ArrayElementType::STRING;

        if (sLen == -1) {
            sLen = strlen(str);
        }

        if (sLen >= ARRAY_MAX_STRING) {
            sLen = ARRAY_MAX_STRING - 1; // memory safety
        }

        value.stringValue = (char*)malloc(sLen + 1);
        memcpy(value.stringValue, str, sLen);
        value.stringValue[sLen] = '\0';
    }

    ArrayElementType type;
    union {
        int integerValue;
        float floatValue;
        char* stringValue;
        void* pointerValue;
    } value;
};

struct ArrayElementHash {
    size_t operator()(const ArrayElement& el) const
    {
        size_t h = std::hash<int>()(static_cast<int>(el.getType()));
        switch (el.getType()) {
        case ArrayElementType::INT:
            return h ^ std::hash<int>()(el.getRawValue());
        case ArrayElementType::FLOAT: {
            // -0.0 + 0.0 = +0.0 per IEEE 754, canonicalizing the sign bit without a branch.
            int bits = el.getRawValue();
            float f;
            memcpy(&f, &bits, sizeof(f));
            f += 0.0f;
            memcpy(&bits, &f, sizeof(bits));
            return h ^ std::hash<int>()(bits);
        }
        case ArrayElementType::POINTER:
            return h ^ std::hash<void*>()(reinterpret_cast<void*>(el.getRawValue()));
        case ArrayElementType::STRING:
            return h ^ std::hash<std::string_view>()(std::string_view(el.getString()));
        default:
            return h;
        }
    }
};

class SFallArray {
public:
    SFallArray(unsigned int flags)
        : flags(flags)
    {
    }
    virtual ~SFallArray() = default;
    virtual ProgramValue GetArrayKey(int index, Program* program) = 0;
    virtual ProgramValue GetArray(const ProgramValue& key, Program* program) = 0;
    virtual void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program) = 0;
    virtual void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset) = 0;
    virtual void ResizeArray(int newLen) = 0;
    virtual ProgramValue ScanArray(const ProgramValue& value, Program* program) = 0;
    virtual int size() = 0;

    bool isReadOnly() const
    {
        return (flags & SFALL_ARRAYFLAG_CONSTVAL) != 0;
    }

    unsigned int getFlags() const { return flags; }
    virtual int flatElementCount() const = 0;
    virtual void forEachElement(std::function<void(const ArrayElement&)> fn) const = 0;
    virtual void loadFlatElements(std::vector<ArrayElement>&& elements) = 0;

protected:
    unsigned int flags;
};

class SFallArrayList : public SFallArray {
public:
    SFallArrayList() = delete;

    SFallArrayList(unsigned int len, unsigned int flags)
        : SFallArray(flags)
    {
        values.resize(len);
    }

    int size() override
    {
        return static_cast<int>(values.size());
    }

    ProgramValue GetArrayKey(int index, Program* program) override
    {
        if (index < -1 || index >= size()) {
            return ProgramValue(0);
        }

        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(0);
        }

        return ProgramValue(index);
    }

    ProgramValue GetArray(const ProgramValue& key, Program* program) override
    {
        if (!key.isInt()) {
            return ProgramValue(0);
        }
        auto index = key.asInt();
        if (index < 0 || index >= size()) {
            return ProgramValue(0);
        }

        return values[index].toValue(program);
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program) override
    {
        SetArray(key, ArrayElement { val, program }, allowUnset);
    }

    void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset) override
    {
        if (key.isInt()) {
            auto index = key.asInt();
            if (index >= 0 && index < size()) {
                std::swap(values[index], val);
            }
        }
    }

    void ResizeArray(int newLen) override
    {
        if (newLen == -1 || size() == newLen) {
            return;
        }

        if (newLen == 0) {
            values.clear();
        } else if (newLen > 0) {
            if (newLen > ARRAY_MAX_SIZE) {
                newLen = ARRAY_MAX_SIZE; // safety
            }

            values.resize(newLen);
        } else if (newLen >= ARRAY_ACTION_SHUFFLE) {
            ListSort(values, newLen, std::less<ArrayElement>());
        } else {
            // F-065: Reject out-of-range action values (≤ -6).
            // Valid negative values: -1 (no-op, handled above), -2..-5
            // (sort/reverse/shuffle, handled by ListSort above).
            // Values ≤ -6 were silently ignored with no error — script
            // authors calling resize_array(arr, -10) got no effect.
            debugPrint("ResizeArray(standard): invalid action value %d — must be in {-1, 0, positive, -2..-5}", newLen);
        }
    }

    ProgramValue ScanArray(const ProgramValue& value, Program* program) override
    {
        auto element = ArrayElement { value, program };
        for (int i = 0; i < size(); i++) {
            if (element == values[i]) {
                return ProgramValue(i);
            }
        }
        return ProgramValue(-1);
    }

    int flatElementCount() const override { return static_cast<int>(values.size()); }

    void forEachElement(std::function<void(const ArrayElement&)> fn) const override
    {
        for (const auto& v : values)
            fn(v);
    }

    void loadFlatElements(std::vector<ArrayElement>&& elements) override
    {
        values = std::move(elements);
    }

private:
    std::vector<ArrayElement> values;
};

class SFallArrayAssoc : public SFallArray {
public:
    SFallArrayAssoc() = delete;

    SFallArrayAssoc(unsigned int flags)
        : SFallArray(flags)
    {
    }

    int size() override
    {
        return static_cast<int>(pairs.size());
    }

    ProgramValue GetArrayKey(int index, Program* program) override
    {
        if (index < -1 || index >= size()) {
            return ProgramValue(0);
        }

        if (index == -1) { // special index to indicate if array is associative
            return ProgramValue(1);
        }

        return pairs[index].key.toValue(program);
    }

    ProgramValue GetArray(const ProgramValue& key, Program* program) override
    {
        auto keyEl = ArrayElement { key, program };
        auto it = keyIndex.find(keyEl);
        if (it == keyIndex.end()) {
            return ProgramValue(0);
        }
        return pairs[it->second].value.toValue(program);
    }

    void SetArray(const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program) override
    {
        auto keyEl = ArrayElement { key, program };
        auto idxIt = keyIndex.find(keyEl);

        if (idxIt != keyIndex.end() && isReadOnly()) {
            return;
        }

        if (allowUnset && !isReadOnly() && val.isInt() && val.asInt() == 0) {
            // after assigning zero to a key, no need to store it, because "get_array" returns 0 for non-existent keys: try unset
            if (idxIt != keyIndex.end()) {
                pairs.erase(pairs.begin() + idxIt->second);
                rebuildKeyIndex();
            }
        } else {
            if (idxIt == keyIndex.end()) {
                if (size() >= ARRAY_MAX_SIZE) {
                    return;
                }
                int newIndex = static_cast<int>(pairs.size());
                pairs.push_back(KeyValuePair { std::move(keyEl), ArrayElement { val, program } });
                keyIndex.emplace(pairs.back().key, newIndex);
            } else {
                pairs[idxIt->second].value = ArrayElement { val, program };
            }
        }
    }

    void SetArray(const ProgramValue& key, ArrayElement&& val, bool allowUnset) override
    {
        assert(false && "This method is not used for associative arrays thus it is not implemented");
    }

    void ResizeArray(int newLen) override
    {
        if (newLen == -1 || size() == newLen) {
            return;
        }

        // only allow to reduce number of elements (adding range of elements is meaningless for maps)
        if (newLen >= 0 && newLen < size()) {
            for (int i = newLen; i < static_cast<int>(pairs.size()); ++i) {
                keyIndex.erase(pairs[i].key);
            }
            pairs.resize(newLen);
        } else if (newLen < 0) {
            // F-065: Validate negative action values for associative arrays.
            // Valid sort-by-key values: -2 (SORT), -3 (RSORT), -4 (REVERSE),
            // -5 (SHUFFLE). Sort-by-value is indicated by offset -4, giving
            // -6..-9. MapSort() handles the offset internally.
            // Values ≤ -10 were silently ignored — script authors calling
            // resize_array(arr, -10) got nothing with no error.
            if (newLen < (ARRAY_ACTION_SHUFFLE - 4)) {
                debugPrint("ResizeArray(assoc): invalid action value %d — must be in {-1, 0..size-1, -2..-9}", newLen);
                return;
            }
            MapSort(newLen);
        } else {
            // newLen >= size(): resize-to-expand is meaningless for associative arrays.
            // Not an error — this is the expected no-op case for expansion.
        }
    }

    ProgramValue ScanArray(const ProgramValue& value, Program* program) override
    {
        auto valueEl = ArrayElement { value, program };
        auto it = std::find_if(pairs.begin(), pairs.end(), [&valueEl](const KeyValuePair& pair) {
            return pair.value == valueEl;
        });

        if (it == pairs.end()) {
            return ProgramValue(-1);
        }

        return it->key.toValue(program);
    }

    int flatElementCount() const override { return static_cast<int>(pairs.size()) * 2; }

    void forEachElement(std::function<void(const ArrayElement&)> fn) const override
    {
        for (const auto& p : pairs) {
            fn(p.key);
            fn(p.value);
        }
    }

    void loadFlatElements(std::vector<ArrayElement>&& elements) override
    {
        pairs.clear();
        keyIndex.clear();
        for (size_t i = 0; i + 1 < elements.size(); i += 2) {
            int newIndex = static_cast<int>(pairs.size());
            pairs.push_back(KeyValuePair { std::move(elements[i]), std::move(elements[i + 1]) });
            keyIndex.emplace(pairs.back().key, newIndex);
        }
    }

private:
    struct KeyValuePair {
        ArrayElement key;
        ArrayElement value;
    };

    void MapSort(int type)
    {
        bool sortByValue = false;
        if (type < ARRAY_ACTION_SHUFFLE) {
            type += 4;
            sortByValue = true;
        }

        if (sortByValue) {
            ListSort(pairs, type, [](const KeyValuePair& a, const KeyValuePair& b) -> bool {
                return a.value < b.value;
            });
        } else {
            ListSort(pairs, type, [](const KeyValuePair& a, const KeyValuePair& b) -> bool {
                return a.key < b.key;
            });
        }
        rebuildKeyIndex();
    }

    void rebuildKeyIndex()
    {
        keyIndex.clear();
        keyIndex.reserve(pairs.size());
        for (int i = 0; i < static_cast<int>(pairs.size()); ++i) {
            keyIndex.emplace(pairs[i].key, i);
        }
    }

    std::vector<KeyValuePair> pairs;
    std::unordered_map<ArrayElement, int, ArrayElementHash> keyIndex;
};

struct SfallArraysState {
    std::unordered_map<ArrayId, std::unique_ptr<SFallArray>> arrays;
    std::unordered_set<ArrayId> temporaryArrayIds;
    std::map<ArrayElement, ArrayId> savedArrays;

    // auto-incremented ID
    int nextArrayId = kInitialArrayId;

    // special array ID for array expressions, contains the ID number of the currently created array
    ArrayId expressionArrayId = 0;
    // special stack for array expressions, contains ID numbers of the currently created arrays
    std::vector<ArrayId> arrayExpressionStack;
};

static SfallArraysState* _state = nullptr;

bool sfallArraysInit()
{
    _state = new (std::nothrow) SfallArraysState();
    if (_state == nullptr) {
        return false;
    }

    return true;
}

void sfallArraysReset()
{
    if (_state != nullptr) {
        _state->arrays.clear();
        _state->temporaryArrayIds.clear();
        _state->savedArrays.clear();
        _state->nextArrayId = kInitialArrayId;
        _state->arrayExpressionStack.clear();
        _state->expressionArrayId = 0;
    }
}

void sfallArraysExit()
{
    if (_state != nullptr) {
        delete _state;
        _state = nullptr;
    }
}

ArrayId CreateArray(int len, unsigned int flags)
{
    flags = (flags & ~1); // reset 1 bit

    // F-12: len <= 0 creates an associative array (map), matching sfall 4.x
    // convention. create_array(0, 2) now creates a map, not a list.
    if (len <= 0) {
        flags |= SFALL_ARRAYFLAG_ASSOC;
    } else if (len > ARRAY_MAX_SIZE) {
        len = ARRAY_MAX_SIZE; // safecheck
    }

    ArrayId arrayId = _state->nextArrayId++;

    if (flags & SFALL_ARRAYFLAG_ASSOC) {
        _state->arrays.emplace(std::make_pair(arrayId, std::make_unique<SFallArrayAssoc>(flags)));
    } else {
        _state->arrays.emplace(std::make_pair(arrayId, std::make_unique<SFallArrayList>(len, flags)));
    }

    if ((flags & SFALL_ARRAYFLAG_EXPR_PUSH) != 0) {
        // When creating array for sub-expression, make sure to add array for base expression to stack
        // This is messy, but required to support older scripts:
        // - We must always assign expressionArrayId for one-layer expressions from older scripts to work like they did before
        // - We can't directly push first arrayID into stack b/c no way to distinguish between start of an expression and normal temp_array call
        // - Compiler will only add this flag for temp_array call generated from a sub-expression
        // - So only on this second call we know we are in expression and expressionArrayId definitely contains arrayId of the first layer
        auto& expressionStack = _state->arrayExpressionStack;
        if (expressionStack.empty() && _state->expressionArrayId != 0) {
            expressionStack.push_back(_state->expressionArrayId);
        }
        expressionStack.push_back(arrayId);
    }
    _state->expressionArrayId = arrayId;

    return arrayId;
}

ArrayId CreateTempArray(int len, unsigned int flags)
{
    ArrayId arrayId = CreateArray(len, flags);
    _state->temporaryArrayIds.insert(arrayId);
    return arrayId;
}

static SFallArray* get_array_by_id(ArrayId arrayId)
{
    auto it = _state->arrays.find(arrayId);
    if (it == _state->arrays.end()) {
        return nullptr;
    }

    return it->second.get();
}

ProgramValue GetArrayKey(ArrayId arrayId, int index, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return ProgramValue(0);
    }

    return arr->GetArrayKey(index, program);
}

int LenArray(ArrayId arrayId)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return -1;
    }

    return arr->size();
}

bool ArrayExists(ArrayId arrayId)
{
    return get_array_by_id(arrayId) != nullptr;
}

ProgramValue GetArray(ArrayId arrayId, const ProgramValue& key, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return ProgramValue(0);
    }

    return arr->GetArray(key, program);
}

void SetArray(ArrayId arrayId, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return;
    }

    arr->SetArray(key, val, allowUnset, program);
}

static void eraseSavedByArrayId(ArrayId arrayId)
{
    auto& saved = _state->savedArrays;
    for (auto it = saved.begin(); it != saved.end(); ++it) {
        if (it->second == arrayId) {
            saved.erase(it);
            return;
        }
    }
}

void FreeArray(ArrayId arrayId)
{
    eraseSavedByArrayId(arrayId);
    _state->arrays.erase(arrayId);
}

void FixArray(ArrayId arrayId)
{
    _state->temporaryArrayIds.erase(arrayId);
}

void DeleteAllTempArrays()
{
    for (ArrayId id : _state->temporaryArrayIds) {
        FreeArray(id);
    }
    _state->temporaryArrayIds.clear();
}

void ResizeArray(ArrayId arrayId, int newLen)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return;
    }

    arr->ResizeArray(newLen);
}

void SetArrayFromExpression(const ProgramValue& key, const ProgramValue& val, Program* program)
{
    ArrayId arrayId = !_state->arrayExpressionStack.empty()
        ? _state->arrayExpressionStack.back()
        : _state->expressionArrayId;

    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return;
    }

    // Expression arrays are always lists (never associative).  Non-int keys
    // cannot index a list — validate BEFORE calling ResizeArray to prevent
    // double-stage corruption: key.asInt() on a non-int key returns 0, which
    // triggers an erroneous resize before SetArray silently rejects the key.
    if (!key.isInt()) {
        return;
    }

    auto size = arr->size();
    if (size >= ARRAY_MAX_SIZE) {
        return;
    }

    int targetIndex = key.asInt();
    if (targetIndex >= size) {
        // Resize to key+1 so the element at key can be stored.
        // Previous code grew by only 1 (size+1), which silently
        // dropped non-sequential key assignments where key > size.
        int newSize = targetIndex + 1;
        if (newSize > ARRAY_MAX_SIZE) {
            newSize = ARRAY_MAX_SIZE;
        }
        arr->ResizeArray(newSize);
    }

    SetArray(arrayId, key, val, false, program);
}

void PopExpressionArray()
{
    auto& expressionStack = _state->arrayExpressionStack;
    if (expressionStack.empty()) return;

    expressionStack.pop_back();

    // Reversing the hack from CreateArray
    if (expressionStack.size() == 1) {
        _state->expressionArrayId = expressionStack.back();
        expressionStack.pop_back();
    }
}

ProgramValue ScanArray(ArrayId arrayId, const ProgramValue& val, Program* program)
{
    auto arr = get_array_by_id(arrayId);
    if (arr == nullptr) {
        return ProgramValue(-1);
    }

    return arr->ScanArray(val, program);
}

ArrayId ListAsArray(int type)
{
    std::vector<Object*> objects;
    sfall_lists_fill(type, objects);

    int count = static_cast<int>(objects.size());
    // CreateTempArray(0, 0) creates an associative array per sfall convention
    // (len <= 0 forces ASSOC flag).  For an empty list, pass count=1 to force
    // list type, then immediately resize to 0 to produce a valid empty list.
    // This ensures scripts iterating with get_array_key(-1) always see a list
    // (returns 0) rather than an associative array (returns 1), regardless of
    // whether the list is empty or populated.
    ArrayId arrayId = CreateTempArray(count > 0 ? count : 1, 0);
    auto arr = get_array_by_id(arrayId);

    if (count == 0) {
        arr->ResizeArray(0);
    }

    // A little bit ugly and likely inefficient.
    for (int index = 0; index < count; index++) {
        arr->SetArray(ProgramValue { index },
            ArrayElement { ProgramValue { objects[index] }, nullptr },
            false);
    }

    return arrayId;
}

ArrayId StringSplit(const char* str, const char* split)
{
    size_t splitLen = strlen(split);
    int count;

    if (splitLen == 0) {
        count = static_cast<int>(strlen(str));
    } else {
        count = 1;
        const char* ptr = str;
        while (true) {
            const char* newptr = strstr(ptr, split);
            if (newptr == nullptr) {
                break;
            }
            count++;
            ptr = newptr + splitLen;
        }
    }

    // Use count > 0 to ensure we create a list (non-associative) array.
    // CreateTempArray(0, 0) creates an associative array per sfall convention,
    // but StringSplit produces ordered lists indexed by position.
    ArrayId arrayId = CreateTempArray(count > 0 ? count : 1, 0);
    auto arr = get_array_by_id(arrayId);

    if (splitLen == 0) {
        if (count == 0) {
            arr->ResizeArray(0);
        }
        for (int i = 0; i < count; i++) {
            arr->SetArray(ProgramValue { i }, ArrayElement { &str[i], 1 }, false);
        }
    } else {
        if (count == 0) {
            arr->ResizeArray(0);
        }
        int idx = 0;
        const char* ptr = str;
        while (true) {
            const char* newptr = strstr(ptr, split);
            size_t len = (newptr != nullptr) ? newptr - ptr : strlen(ptr);

            arr->SetArray(ProgramValue { idx }, ArrayElement { ptr, len }, false);

            if (newptr == nullptr) {
                break;
            }

            idx++;
            ptr = newptr + splitLen;
        }
    }
    return arrayId;
}

// sfall's DataType enum values used in the binary file format.
enum class SfallArrayElementType : int {
    INT = 1,
    FLOAT = 2,
    STR = 3,
};

static SfallArrayElementType toSfallType(ArrayElementType t)
{
    switch (t) {
    case ArrayElementType::FLOAT:
        return SfallArrayElementType::FLOAT;
    case ArrayElementType::STRING:
        return SfallArrayElementType::STR;
    // Pointers should be saved as int, see comment in writeElement below.
    default:
        return SfallArrayElementType::INT;
    }
}

static ArrayElementType fromSfallType(int t)
{
    switch (static_cast<SfallArrayElementType>(t & 0xFFFF)) {
    case SfallArrayElementType::FLOAT:
        return ArrayElementType::FLOAT;
    case SfallArrayElementType::STR:
        return ArrayElementType::STRING;
    default:
        return ArrayElementType::INT;
    }
}

static bool writeElement(const ArrayElement& el, File* stream)
{
    int savedType = static_cast<int>(toSfallType(el.getType()));
    if (fileWrite(&savedType, sizeof(savedType), 1, stream) != 1) return false;
    if (el.getType() == ArrayElementType::STRING) {
        int len = static_cast<int>(strlen(el.getString()) + 1); // +1 for null terminator
        if (fileWrite(&len, sizeof(len), 1, stream) != 1) return false;
        if (fileWrite(el.getString(), len, 1, stream) != 1) return false;
    } else {
        // Saving pointers is meaningless, so just save zero instead.
        int bits = el.getType() == ArrayElementType::POINTER ? 0 : el.getRawValue();
        if (fileWrite(&bits, sizeof(bits), 1, stream) != 1) return false;
    }
    return true;
}

static bool readElement(ArrayElement& out, File* stream)
{
    int savedType;
    if (fileRead(&savedType, sizeof(savedType), 1, stream) != 1) return false;

    ArrayElementType type = fromSfallType(savedType);
    if (type == ArrayElementType::STRING) {
        int len;
        if (fileRead(&len, sizeof(len), 1, stream) != 1) return false;
        // Reject invalid lengths to prevent stream desync on corrupt data
        if (len < 0 || len > ARRAY_MAX_STRING + 1) return false;
        if (len > 0) {
            std::vector<char> buf(len);
            if (fileRead(buf.data(), len, 1, stream) != 1) return false;
            out = ArrayElement { buf.data(), static_cast<size_t>(len - 1) }; // len includes null
        } else {
            out = ArrayElement { "", 0 };
        }
    } else {
        int bits;
        if (fileRead(&bits, sizeof(bits), 1, stream) != 1) return false;
        ProgramValue pv;
        pv.opcode = (type == ArrayElementType::FLOAT) ? VALUE_TYPE_FLOAT : VALUE_TYPE_INT;
        pv.integerValue = bits;
        out = ArrayElement { pv, nullptr };
    }
    return true;
}

SaveArrayResult SaveArray(const ProgramValue& key, ArrayId arrayId, Program* program)
{
    auto& saved = _state->savedArrays;

    // int(0) key means "unsave" the array without destroying it
    if (key.isInt() && key.asInt() == 0) {
        eraseSavedByArrayId(arrayId);
        return SaveArrayResult::OK;
    }
    if (key.isPointer()) {
        return SaveArrayResult::InvalidKeyType;
    }

    ArrayElement keyEl { key, program };
    // Prevent saving array using reserved key for returning all arrays list, since it can never be loaded.
    if (keyEl.getType() == ArrayElementType::STRING
        && strcmp(keyEl.getString(), kAllArraysSpecialKey) == 0) {
        return SaveArrayResult::ReservedKey;
    }
    if (get_array_by_id(arrayId) == nullptr) {
        return SaveArrayResult::InvalidId;
    }

    FixArray(arrayId);

    // Already saved under this exact key - nothing to do
    auto it = saved.find(keyEl);
    if (it != saved.end() && it->second == arrayId) return SaveArrayResult::OK;

    // Remove any existing entry using this key (different array)
    if (it != saved.end()) saved.erase(it);

    // Remove any existing entry for this array id (being re-keyed)
    eraseSavedByArrayId(arrayId);

    saved.emplace(std::move(keyEl), arrayId);
    return SaveArrayResult::OK;
}

ArrayId LoadArray(const ProgramValue& key, Program* program)
{
    if (key.isInt() && key.asInt() == 0) return 0;

    ArrayElement keyEl { key, program };
    if (keyEl.getType() == ArrayElementType::STRING
        && strcmp(keyEl.getString(), kAllArraysSpecialKey) == 0) {
        int count = static_cast<int>(_state->savedArrays.size());
        // CreateTempArray(0, 0) forces ASSOC flag per CreateArray len<=0
        // convention. When count==0, create with len=1 to get a list,
        // then resize to 0 to produce an empty list. Same workaround
        // established in ListAsArray() and StringSplit().
        ArrayId tmpId = CreateTempArray(count > 0 ? count : 1, 0);
        auto* tmpArr = get_array_by_id(tmpId);
        if (tmpArr != nullptr) {
            if (count == 0) {
                tmpArr->ResizeArray(0);
            }
            int index = 0;
            // std::map iterates in sorted order, matching sfall's explicit sort
            for (const auto& [savedKey, arrayId] : _state->savedArrays) {
                tmpArr->SetArray(ProgramValue { index }, ArrayElement(savedKey), false);
                index++;
            }
        }
        return tmpId;
    }

    auto it = _state->savedArrays.find(keyEl);
    return it != _state->savedArrays.end() ? it->second : 0;
}

constexpr unsigned int kSavedFlagsMask = SFALL_ARRAYFLAG_ASSOC | SFALL_ARRAYFLAG_CONSTVAL;

bool sfallArraysSave(File* stream)
{
    auto& saved = _state->savedArrays;

    // Remove stale entries for arrays that no longer exist
    for (auto it = saved.begin(); it != saved.end();) {
        it = (get_array_by_id(it->second) == nullptr) ? saved.erase(it) : std::next(it);
    }

    // First DWORD is always 0: backward-compat sentinel for sfall v3.3
    int oldCount = 0;
    if (fileWrite(&oldCount, sizeof(oldCount), 1, stream) != 1) return false;

    int count = static_cast<int>(saved.size());
    if (fileWrite(&count, sizeof(count), 1, stream) != 1) return false;

    for (const auto& [key, arrayId] : saved) {
        auto* arr = get_array_by_id(arrayId);

        if (!writeElement(key, stream)) return false;

        unsigned int flags = arr->getFlags() & kSavedFlagsMask;
        if (fileWrite(&flags, sizeof(flags), 1, stream) != 1) return false;

        int elCount = arr->flatElementCount();
        if (fileWrite(&elCount, sizeof(elCount), 1, stream) != 1) return false;

        bool ok = true;
        bool hasPointers = key.getType() == ArrayElementType::POINTER;
        arr->forEachElement([&](const ArrayElement& el) {
            if (ok) ok = writeElement(el, stream);
            hasPointers = hasPointers || el.getType() == ArrayElementType::POINTER;
        });
        if (!ok) return false;

        if (hasPointers) {
            debugPrint("LOADSAVE (SFALL): Array %s had some values of Pointer types. They will be saved as 0.\n", key.getAsDebugString());
        }
    }
    return true;
}

bool sfallArraysLoad(File* stream)
{
    // First DWORD is 0 for new format (or a nonzero count for sfall v3.3 old format)
    int oldCount;
    if (fileRead(&oldCount, sizeof(oldCount), 1, stream) != 1) return false;
    if (oldCount != 0) {
        debugPrint("LOADSAVE (SFALL): arrays in old format, skipping\n");
        return true;
    }

    int count;
    if (fileRead(&count, sizeof(count), 1, stream) != 1) return false;
    if (count <= 0) return true;
    if (count > ARRAY_MAX_SIZE) return false; // sanity bound against corrupt data

    for (int i = 0; i < count; i++) {
        ArrayElement key;
        if (!readElement(key, stream)) return false;

        unsigned int flags;
        if (fileRead(&flags, sizeof(flags), 1, stream) != 1) return false;

        int elCount;
        if (fileRead(&elCount, sizeof(elCount), 1, stream) != 1) return false;
        if (elCount < 0 || elCount > ARRAY_MAX_SIZE * 2) return false; // sanity bound

        std::vector<ArrayElement> elements;
        elements.reserve(elCount);
        for (int j = 0; j < elCount; j++) {
            ArrayElement el;
            if (!readElement(el, stream)) return false;
            elements.push_back(std::move(el));
        }

        // Strip transient expression flags before creating the array
        unsigned int safeFlags = flags & kSavedFlagsMask;
        bool isAssoc = (safeFlags & SFALL_ARRAYFLAG_ASSOC) != 0;

        // Associative arrays store key-value pairs; element count must be even.
        if (isAssoc && (elCount & 1) != 0) {
            return false;
        }

        // For non-associative arrays, use the actual element count as the
        // initial size so CreateArray creates a list (not a map). CreateArray
        // forces ASSOC for len<=0 per sfall convention.  When the list is
        // empty, pass count=1 to force list type, then immediately resize to 0
        // — the same workaround used by ListAsArray (line 881).
        int initLen;
        if (isAssoc) {
            initLen = -1;
        } else {
            initLen = static_cast<int>(elements.size());
            if (initLen == 0) {
                initLen = 1; // CreateArray forces ASSOC for len<=0; use 1 then resize
            }
        }
        ArrayId id = CreateArray(initLen, safeFlags);
        SFallArray* arr = get_array_by_id(id);
        if (arr == nullptr) return false;
        if (!isAssoc && elements.empty()) {
            arr->ResizeArray(0);
        }
        arr->loadFlatElements(std::move(elements));

        _state->savedArrays.emplace(std::move(key), id);
    }
    return true;
}

// arrays_equal(array1, array2) -> int
// Returns 1 if both arrays have the same keys and values in the same order,
// 0 otherwise. Compares element-by-element: sizes must match, and each
// (key, value) pair must be equal at every index.
// Uses GetArrayKey() and GetArray() so the comparison works for both list
// and associative arrays. Order-sensitive — arrays with the same elements
// in different insertion order are NOT equal. This matches sfall behavior
// where array iteration is always in insertion order.
// F-003 (M-8): Implements the missing arrays_equal function called by
// gl_test_arrays.ssl in Et Tu tests.
int ArraysEqual(ArrayId array1, ArrayId array2, Program* program)
{
    SFallArray* a1 = get_array_by_id(array1);
    SFallArray* a2 = get_array_by_id(array2);

    if (a1 == nullptr) return (a2 == nullptr) ? 1 : 0;
    if (a2 == nullptr) return 0;

    int size1 = a1->size();
    int size2 = a2->size();
    if (size1 != size2) return 0;

    for (int i = 0; i < size1; i++) {
        ProgramValue key1 = a1->GetArrayKey(i, program);
        ProgramValue key2 = a2->GetArrayKey(i, program);

        // Keys must be equal. For list arrays, keys are sequential integers.
        ArrayElement k1 { key1, program };
        ArrayElement k2 { key2, program };
        if (!(k1 == k2)) return 0;

        // Values must be equal at each key.
        ProgramValue val1 = a1->GetArray(key1, program);
        ProgramValue val2 = a2->GetArray(key2, program);
        ArrayElement v1 { val1, program };
        ArrayElement v2 { val2, program };
        if (!(v1 == v2)) return 0;
    }

    return 1;
}

// array_filter(array, filterProcedurePtr) -> newArrayId
// Creates a new array of the same type (list or associative) containing only
// the elements for which the callback procedure returns non-zero. The callback
// is invoked once per element with the element value pushed onto the program
// stack. If the return value is non-zero (truthy), the element is kept.
// The original array is not modified.
// F-004 (M-9): Implements the missing array_filter function.
ArrayId ArrayFilter(ArrayId arrayId, Program* program, int procedureIndex)
{
    SFallArray* src = get_array_by_id(arrayId);
    if (src == nullptr) {
        return 0;
    }

    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("array_filter: procedure index %d is out of range", procedureIndex);
        return 0;
    }

    // GetArrayKey(-1) returns 0 for list arrays, 1 for associative arrays.
    bool isAssoc = (src->GetArrayKey(-1, program).integerValue == 1);

    // CreateTempArray(0, 0) forces ASSOC flag per CreateArray len<=0
    // convention. For list sources, create with len=1 to get a list,
    // then resize to 0 for an empty starting point. Same workaround
    // established in ListAsArray() and StringSplit().
    ArrayId resultId = CreateTempArray(isAssoc ? -1 : 1, 0);
    SFallArray* dst = get_array_by_id(resultId);
    if (dst == nullptr) {
        return 0;
    }
    if (!isAssoc) {
        dst->ResizeArray(0);
    }

    int size = src->size();
    int keepCount = 0;
    for (int i = 0; i < size; i++) {
        ProgramValue key = src->GetArrayKey(i, program);
        ProgramValue val = src->GetArray(key, program);

        // Push the value onto the stack and call the callback procedure.
        programStackPushValue(program, val);
        programExecuteProcedure(program, procedureIndex);
        ProgramValue retVal = programStackPopValue(program);

        // Truthy check: non-zero int, non-zero float, non-null pointer/string.
        bool keep = false;
        if (retVal.isInt()) {
            keep = retVal.asInt() != 0;
        } else if (retVal.isFloat()) {
            keep = retVal.asFloat() != 0.0f;
        } else {
            // Pointer, string, or other — treat non-null as truthy.
            // For strings, the integerValue is a string table index;
            // a valid (non-empty) string always has a non-zero effect.
            keep = (retVal.integerValue != 0);
        }

        if (keep) {
            // For list arrays, resize before setting so the index is in bounds.
            // Use sequential indices for list, original key for assoc.
            if (!isAssoc) {
                ResizeArray(resultId, keepCount + 1);
            }
            ProgramValue targetKey = isAssoc ? key : ProgramValue(keepCount);
            dst->SetArray(targetKey, val, false, program);
            keepCount++;
        }
    }

    return resultId;
}

// array_transform(array, transformProcedurePtr) -> newArrayId
// Creates a new array of the same type where each element's value has been
// replaced by the return value of the callback procedure. The callback is
// invoked once per element with the element value pushed onto the program
// stack. The return value from the callback becomes the new element value.
// The original array is not modified.
// F-005 (M-10): Implements the missing array_transform function.
ArrayId ArrayTransform(ArrayId arrayId, Program* program, int procedureIndex)
{
    SFallArray* src = get_array_by_id(arrayId);
    if (src == nullptr) {
        return 0;
    }

    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("array_transform: procedure index %d is out of range", procedureIndex);
        return 0;
    }

    // GetArrayKey(-1) returns 0 for list arrays, 1 for associative arrays.
    bool isAssoc = (src->GetArrayKey(-1, program).integerValue == 1);

    // CreateTempArray(0, 0) forces ASSOC flag per CreateArray len<=0
    // convention. For list sources, create with len=1 to get a list,
    // then resize to 0 for an empty starting point. Same workaround
    // established in ListAsArray() and StringSplit().
    ArrayId resultId = CreateTempArray(isAssoc ? -1 : 1, 0);
    SFallArray* dst = get_array_by_id(resultId);
    if (dst == nullptr) {
        return 0;
    }
    if (!isAssoc) {
        dst->ResizeArray(0);
    }

    int size = src->size();
    for (int i = 0; i < size; i++) {
        ProgramValue key = src->GetArrayKey(i, program);
        ProgramValue val = src->GetArray(key, program);

        // Push the value onto the stack and call the callback procedure.
        programStackPushValue(program, val);
        programExecuteProcedure(program, procedureIndex);
        ProgramValue retVal = programStackPopValue(program);

        // For list arrays, resize before setting so the index is in bounds.
        if (!isAssoc) {
            ResizeArray(resultId, i + 1);
        }
        // Set the transformed value; for list arrays use sequential indices,
        // for associative arrays preserve the original key.
        ProgramValue targetKey = isAssoc ? key : ProgramValue(i);
        dst->SetArray(targetKey, retVal, false, program);
    }

    return resultId;
}

} // namespace fallout
