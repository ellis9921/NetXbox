#ifndef _XBOX_COMPAT_XINPUT_H
#define _XBOX_COMPAT_XINPUT_H

#include <xinputdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE *pState);
DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);
DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities);
void WINAPI XInputEnable(BOOL enable);

#ifdef __cplusplus
}
#endif

#endif
