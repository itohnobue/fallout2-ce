#ifndef FALLOUT_SFALL_ARRAYS_H_
#define FALLOUT_SFALL_ARRAYS_H_

#include "interpreter.h"

namespace fallout {

struct XFile;
typedef XFile File;

#define SFALL_ARRAYFLAG_ASSOC (1) // is map
#define SFALL_ARRAYFLAG_CONSTVAL (2) // don't update value of key if the key exists in map
#define SFALL_ARRAYFLAG_RESERVED (4) // has no significance in sfall or CE
#define SFALL_ARRAYFLAG_EXPR_PUSH (32) // is created as part of array sub-expression
#define SFALL_ARRAYFLAG_EXPR_POP (64) // is used to indicate end of array sub-expression, not used in actual array

using ArrayId = unsigned int;

enum class SaveArrayResult {
    OK = 0,
    InvalidId,
    InvalidKeyType,
    ReservedKey,
};

bool sfallArraysInit();
void sfallArraysReset();
void sfallArraysExit();
ArrayId CreateArray(int len, unsigned int flags);
ArrayId CreateTempArray(int len, unsigned int flags);
ProgramValue GetArrayKey(ArrayId arrayId, int index, Program* program);
int LenArray(ArrayId arrayId);
bool ArrayExists(ArrayId arrayId);
ProgramValue GetArray(ArrayId arrayId, const ProgramValue& key, Program* program);
void SetArray(ArrayId arrayId, const ProgramValue& key, const ProgramValue& val, bool allowUnset, Program* program);
void FreeArray(ArrayId arrayId);
void FixArray(ArrayId id);
void ResizeArray(ArrayId arrayId, int newLen);
void DeleteAllTempArrays();
void SetArrayFromExpression(const ProgramValue& key, const ProgramValue& val, Program* program);
void PopExpressionArray();
ProgramValue ScanArray(ArrayId arrayId, const ProgramValue& val, Program* program);
ArrayId ListAsArray(int type);

ArrayId StringSplit(const char* str, const char* split);

SaveArrayResult SaveArray(const ProgramValue& key, ArrayId arrayId, Program* program);
ArrayId LoadArray(const ProgramValue& key, Program* program);
bool sfallArraysSave(File* stream);
bool sfallArraysLoad(File* stream);

// arrays_equal(array1, array2) -> int
// Returns 1 if both arrays have the same keys and values in the same order,
// 0 otherwise. Compares element-by-element: sizes must match, and each
// (key, value) pair must be equal at every index. F-003 (M-8).
int ArraysEqual(ArrayId array1, ArrayId array2, Program* program);

// array_filter(array, filterProcedurePtr) -> newArrayId
// Creates a new array containing only elements for which the callback
// procedure returns non-zero. The callback is invoked as:
//   procedure(elementValue)
// and its return value determines whether the element is kept (non-zero)
// or discarded (0). F-004 (M-9).
ArrayId ArrayFilter(ArrayId arrayId, Program* program, int procedureIndex);

// array_transform(array, transformProcedurePtr) -> newArrayId
// Creates a new array with each element transformed by the callback
// procedure. The callback is invoked as:
//   procedure(elementValue)
// and its return value replaces the element's value in the new array.
// F-005 (M-10).
ArrayId ArrayTransform(ArrayId arrayId, Program* program, int procedureIndex);

} // namespace fallout

#endif /* FALLOUT_SFALL_ARRAYS_H_ */
