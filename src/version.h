#ifndef VERSION_H
#define VERSION_H

#include <stddef.h>

namespace fallout {

// The size of buffer for version string.
#define VERSION_MAX (32)

#define VERSION_MAJOR (1)
// C-03/C-04 (save-format pass): bumped 3 -> 4 (1.3R -> 1.4R). The on-disk
// versionMajor field (which stores VERSION_MINOR) now distinguishes the
// legacy 27-chunk handler layout (1.2R/1.3R, versionMajor 2/3) from the
// 28-chunk layout with lightSave/lightLoad at index 26 (1.4R, versionMajor 4).
#define VERSION_MINOR (4)
#define VERSION_RELEASE ('R')

void versionGetVersion(char* dest, size_t size);

} // namespace fallout

#endif /* VERSION_H */
