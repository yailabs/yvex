#include <yvex/version.h>

const char *yvex_version_string(void)
{
    return "0.1.0";
}

int yvex_version_major(void)
{
    return YVEX_VERSION_MAJOR;
}

int yvex_version_minor(void)
{
    return YVEX_VERSION_MINOR;
}

int yvex_version_patch(void)
{
    return YVEX_VERSION_PATCH;
}
