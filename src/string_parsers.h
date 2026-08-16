#ifndef STRING_PARSERS_H
#define STRING_PARSERS_H

#include <stddef.h>

namespace fallout {

typedef int(StringParserCallback)(char* string, int* valuePtr);

int strParseInt(char** stringPtr, int* valuePtr);

template <typename T>
int strParseEnum(char** stringPtr, T* valuePtr)
{
    if (stringPtr == nullptr || *stringPtr == nullptr) {
        return 0;
    }

    int temp;
    int result = strParseInt(stringPtr, &temp);
    *valuePtr = static_cast<T>(temp);
    return result;
}

int strParseStrFromList(char** stringPtr, int* valuePtr, const char** list, int count);

template <typename T>
int strParseStrFromListEnum(char** stringPtr, T* valuePtr, const char** list, int count)
{
    if (stringPtr == nullptr || *stringPtr == nullptr) {
        return 0;
    }

    int temp;
    int result = strParseStrFromList(stringPtr, &temp, list, count);
    *valuePtr = static_cast<T>(temp);
    return result;
}

int strParseStrFromFunc(char** stringPtr, int* valuePtr, StringParserCallback* callback);

template <typename T>
int strParseStrFromFuncEnum(char** stringPtr, T* valuePtr, StringParserCallback* callback)
{
    if (stringPtr == nullptr || *stringPtr == nullptr) {
        return 0;
    }

    int temp;
    int result = strParseStrFromFunc(stringPtr, &temp, callback);
    *valuePtr = static_cast<T>(temp);
    return result;
}

int strParseIntWithKey(char** stringPtr, const char* key, int* valuePtr, const char* delimeter);

// keySize bounds the key buffer (M-191). Callers should pass the size of their
// key buffer; when 0 (legacy) the copy is unbounded — keep callers bounded.
int strParseKeyValue(char** stringPtr, char* key, int* valuePtr, const char* delimeter, size_t keySize = 0);

} // namespace fallout

#endif /* STRING_PARSERS_H */
