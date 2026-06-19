#ifndef YVEX_VERSION_H
#define YVEX_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_VERSION_MAJOR 0
#define YVEX_VERSION_MINOR 1
#define YVEX_VERSION_PATCH 0

const char *yvex_version_string(void);
int yvex_version_major(void);
int yvex_version_minor(void);
int yvex_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif
