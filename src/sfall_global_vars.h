#ifndef FALLOUT_SFALL_GLOBAL_VARS_H_
#define FALLOUT_SFALL_GLOBAL_VARS_H_

#include "db.h"

namespace fallout {

bool sfall_gl_vars_init();
void sfall_gl_vars_reset();
void sfall_gl_vars_exit();
bool sfall_gl_vars_save(File* stream);
bool sfall_gl_vars_load(File* stream);
bool sfall_gl_vars_store(const char* key, int value);
bool sfall_gl_vars_store(int key, int value);
bool sfall_gl_vars_fetch(const char* key, int& value);
bool sfall_gl_vars_fetch(int key, int& value);
bool sfall_gl_vars_store_float(const char* key, float value);
bool sfall_gl_vars_store_float(int key, float value);
bool sfall_gl_vars_fetch_float(const char* key, float& value);
bool sfall_gl_vars_fetch_float(int key, float& value);
bool sfall_gl_vars_remove(const char* key);
bool sfall_gl_vars_remove(int key);

// F-003/F-004: String global var storage for persisting dynamically-allocated
// string state (perk name/desc overrides, fake perk/trait name/desc, perkbox
// title, movie path overrides). Strings are stored in a parallel map and
// serialized as length-prefixed records after the float section in sfallgv.sav
// (version 2+). Caller owns the returned char* from fetch_string and must
// delete[] it. Returns nullptr if the key is not found or the value is empty.
bool sfall_gl_vars_store_string(const char* key, const char* value);
char* sfall_gl_vars_fetch_string(const char* key);
bool sfall_gl_vars_remove_string(const char* key);

} // namespace fallout

#endif /* FALLOUT_SFALL_GLOBAL_VARS_H_ */
