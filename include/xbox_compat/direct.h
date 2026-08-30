#ifndef _XBOX_COMPAT_DIRECT_H
#define _XBOX_COMPAT_H

#include <xtl.h>

static __inline int _mkdir(const char* dir) {
    char wide[512];
    int i = 0;
    if (!dir) return -1;
    while (dir[i] && i < 255) { wide[i] = (char)dir[i]; i++; }
    wide[i] = 0;
    return CreateDirectoryA(wide, NULL) ? 0 : -1;
}

#endif
