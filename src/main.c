#include "app.h"
#include "platform.h"
#include <stdlib.h>

#ifdef PLATFORM_XBOX360
#include "platform_xbox360.h"
#else
#include "platform_win32.h"
#endif

const PlatformAPI* platform_get_api(void) {
#ifdef PLATFORM_XBOX360
    return xbox360_get_api();
#else
    return win32_get_api();
#endif
}

const char* platform_get_name(void) {
#ifdef PLATFORM_XBOX360
    return xbox360_get_name();
#else
    return win32_get_name();
#endif
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    AppContext* app = (AppContext*)calloc(1, sizeof(AppContext));
    app_init(app);

    app_run(app);

    app_shutdown(app);
    free(app);
    return 0;
}
