#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void* PlatformWindow;
typedef void* PlatformGLContext;
typedef void* PlatformThread;
typedef void* PlatformMutex;
typedef void* PlatformSocket;

typedef struct {
    int x, y;
    int width, height;
} PlatformRect;

typedef struct {
    int x, y;
} PlatformPoint;

typedef struct {
    bool left, right, middle;
    int x, y;
    int scroll_delta;
} PlatformMouseState;

typedef struct {
    uint8_t keys[256];
} PlatformKeyboardState;

typedef struct {
    uint8_t chars[256];
    int char_count;
} PlatformCharInput;

typedef struct {
    const char* title;
    int width, height;
    bool resizable;
    bool fullscreen;
} PlatformWindowDesc;

typedef struct {
    int window_width, window_height;
    int framebuffer_width, framebuffer_height;
    PlatformMouseState mouse;
    PlatformKeyboardState keyboard;
    PlatformCharInput char_input;
    bool should_close;
    int controller_connected;
    int thumb_lx, thumb_ly;
    int thumb_rx, thumb_ry;
    float left_trigger, right_trigger;
    uint16_t buttons;
    uint16_t buttons_pressed;
    uint16_t buttons_released;
    int cursor_type;
} PlatformState;

typedef enum {
    CURSOR_ARROW = 0,
    CURSOR_IBEAM,
    CURSOR_HAND,
    CURSOR_BUSY
} PlatformCursorType;

typedef enum {
    PLATFORM_KEY_UNKNOWN = 0,
    PLATFORM_KEY_BACKSPACE = 8,
    PLATFORM_KEY_TAB = 9,
    PLATFORM_KEY_ENTER = 13,
    PLATFORM_KEY_SHIFT = 16,
    PLATFORM_KEY_CONTROL = 17,
    PLATFORM_KEY_ALT = 18,
    PLATFORM_KEY_ESCAPE = 27,
    PLATFORM_KEY_SPACE = 32,
    PLATFORM_KEY_PAGE_UP = 33,
    PLATFORM_KEY_PAGE_DOWN = 34,
    PLATFORM_KEY_END = 35,
    PLATFORM_KEY_HOME = 36,
    PLATFORM_KEY_LEFT = 37,
    PLATFORM_KEY_UP = 38,
    PLATFORM_KEY_RIGHT = 39,
    PLATFORM_KEY_DOWN = 40,
    PLATFORM_KEY_DELETE = 46,
    PLATFORM_KEY_0 = 48, PLATFORM_KEY_1, PLATFORM_KEY_2, PLATFORM_KEY_3, PLATFORM_KEY_4,
    PLATFORM_KEY_5, PLATFORM_KEY_6, PLATFORM_KEY_7, PLATFORM_KEY_8, PLATFORM_KEY_9,
    PLATFORM_KEY_A = 65, PLATFORM_KEY_B, PLATFORM_KEY_C, PLATFORM_KEY_D, PLATFORM_KEY_E,
    PLATFORM_KEY_F, PLATFORM_KEY_G, PLATFORM_KEY_H, PLATFORM_KEY_I, PLATFORM_KEY_J,
    PLATFORM_KEY_K, PLATFORM_KEY_L, PLATFORM_KEY_M, PLATFORM_KEY_N, PLATFORM_KEY_O,
    PLATFORM_KEY_P, PLATFORM_KEY_Q, PLATFORM_KEY_R, PLATFORM_KEY_S, PLATFORM_KEY_T,
    PLATFORM_KEY_U, PLATFORM_KEY_V, PLATFORM_KEY_W, PLATFORM_KEY_X, PLATFORM_KEY_Y,
    PLATFORM_KEY_Z,
    PLATFORM_KEY_F5 = 116, PLATFORM_KEY_F6, PLATFORM_KEY_F7, PLATFORM_KEY_F8,
    PLATFORM_KEY_F9, PLATFORM_KEY_F10, PLATFORM_KEY_F11, PLATFORM_KEY_F12,
    PLATFORM_KEY_COUNT = 256
} PlatformKey;

typedef enum {
    PLATFORM_LOG_INFO,
    PLATFORM_LOG_WARN,
    PLATFORM_LOG_ERROR,
    PLATFORM_LOG_DEBUG
} PlatformLogLevel;

typedef enum {
    PLATFORM_FILE_READ,
    PLATFORM_FILE_WRITE,
    PLATFORM_FILE_APPEND
} PlatformFileMode;

typedef enum {
    PLATFORM_SOCKET_TCP,
    PLATFORM_SOCKET_UDP
} PlatformSocketType;

typedef struct {
    bool (*init)(void);
    void (*shutdown)(void);
    void (*poll_events)(PlatformState* state);
    
    PlatformWindow (*window_create)(const PlatformWindowDesc* desc);
    void (*window_destroy)(PlatformWindow window);
    void (*window_set_title)(PlatformWindow window, const char* title);
    void (*window_set_size)(PlatformWindow window, int width, int height);
    void (*window_get_size)(PlatformWindow window, int* width, int* height);
    void (*window_show)(PlatformWindow window);
    void (*window_hide)(PlatformWindow window);
    void (*window_focus)(PlatformWindow window);
    
    PlatformGLContext (*gl_create_context)(PlatformWindow window);
    void (*gl_destroy_context)(PlatformGLContext ctx);
    void (*gl_make_current)(PlatformGLContext ctx);
    void (*gl_swap_buffers)(PlatformWindow window);
    
    void* (*file_open)(const char* path, PlatformFileMode mode);
    void (*file_close)(void* handle);
    int64_t (*file_read)(void* handle, void* buffer, int64_t size);
    int64_t (*file_write)(void* handle, const void* buffer, int64_t size);
    int64_t (*file_size)(const char* path);
    bool (*file_exists)(const char* path);
    bool (*file_delete)(const char* path);
    char* (*file_read_all)(const char* path, int64_t* out_size);
    
    PlatformThread (*thread_create)(void (*func)(void*), void* arg);
    void (*thread_join)(PlatformThread thread);
    PlatformMutex (*mutex_create)(void);
    void (*mutex_lock)(PlatformMutex mutex);
    void (*mutex_unlock)(PlatformMutex mutex);
    void (*mutex_destroy)(PlatformMutex mutex);
    void (*sleep_ms)(uint32_t ms);
    
    PlatformSocket (*socket_create)(PlatformSocketType type);
    void (*socket_destroy)(PlatformSocket sock);
    bool (*socket_connect)(PlatformSocket sock, const char* host, uint16_t port);
    int (*socket_send)(PlatformSocket sock, const void* data, int len);
    int (*socket_recv)(PlatformSocket sock, void* buffer, int len);
    bool (*socket_set_nonblocking)(PlatformSocket sock, bool nonblocking);
    
    uint64_t (*get_ticks)(void);
    uint64_t (*get_freq)(void);
    
    void (*log)(PlatformLogLevel level, const char* fmt, ...);
    
    void (*clipboard_set)(const char* text);
    char* (*clipboard_get)(void);
    
    void (*surface_blit)(PlatformWindow window, const uint32_t* pixels, int width, int height);
    void (*surface_present)(PlatformWindow window);
    void* (*get_hwnd)(PlatformWindow window);
    
    void (*text_draw)(PlatformWindow window, int x, int y, const char* text, int font_size, uint32_t color, bool bold);
    void (*text_draw_clipped)(PlatformWindow window, int x, int y, int max_w, const char* text, int font_size, uint32_t color, bool bold);
    int (*text_measure)(const char* text, int font_size, bool bold);
    int (*text_measure_clipped)(int max_w, const char* text, int font_size, bool bold);
    int (*text_height)(int font_size);
    char* (*file_save_dialog)(const char* default_name, const char* filter);
    bool (*file_write_bytes)(const char* path, const void* data, int len);
    bool (*show_keyboard)(const char* title, const char* default_text, char* out, int out_size);
    // Run a modal XUI chrome session (mode 0=off,1=toolbar,2=settings,3=home).
    // Returns the selected action id (0 if none / dismissed), or -1 if the
    // platform has no XUI chrome. Defined only where XUI is available.
    int (*run_chrome)(int mode);
} PlatformAPI;

const PlatformAPI* platform_get_api(void);
const char* platform_get_name(void);
