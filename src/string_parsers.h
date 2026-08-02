#ifndef STRING_PARSERS_H
#define STRING_PARSERS_H

#include <stddef.h>

namespace fallout {

typedef int(StringParserCallback)(char* string, int* valuePtr);

int strParseInt(char** stringPtr, int* valuePtr);
int strParseStrFromList(char** stringPtr, int* valuePtr, const char** list, int count);
int strParseStrFromFunc(char** stringPtr, int* valuePtr, StringParserCallback* callback);
int strParseIntWithKey(char** stringPtr, const char* key, int* valuePtr, const char* delimeter);

// keySize bounds the key buffer (M-191). Callers should pass the size of their
// key buffer; when 0 (legacy) the copy is unbounded — keep callers bounded.
int strParseKeyValue(char** stringPtr, char* key, int* valuePtr, const char* delimeter, size_t keySize = 0);

} // namespace fallout

#endif /* STRING_PARSERS_H */
