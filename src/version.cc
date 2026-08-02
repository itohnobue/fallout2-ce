#include "version.h"
#include "content_config.h"

#include <stdio.h>

namespace fallout {

// 0x4B4580 getverstr
void versionGetVersion(char* dest, size_t size)
{
    // SFALL: custom version string.
    char* versionString = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string", &versionString, "");
    if (!*versionString) {
        versionString = nullptr;
    }

    // M-188: Never use the user-writable version_string as the format
    // string. The old code passed it directly to snprintf, so a config
    // value containing format specifiers (e.g. "%s", "%n") made snprintf
    // read VERSION_MAJOR as a char* or write through a format-derived
    // address — arbitrary read/write from a config file. Use it only as
    // an argument.
    if (versionString != nullptr) {
        snprintf(dest, size, "%s", versionString);
    } else {
        snprintf(dest, size, "FALLOUT II %d.%02d", VERSION_MAJOR, VERSION_MINOR);
    }
}

} // namespace fallout
