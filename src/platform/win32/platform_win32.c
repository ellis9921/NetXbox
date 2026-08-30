#include "platform_win32.h"
#include "font.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <xinput.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "xinput9_1_0.lib")

static HINSTANCE g_hInstance = NULL;
static HWND g_hWnd = NULL;
static HDC g_hDC = NULL;
static HGLRC g_hRC = NULL;
static bool g_initialized = false;
static HCURSOR g_cursor_arrow = NULL;
static HCURSOR g_cursor_ibeam = NULL;
static HCURSOR g_cursor_hand = NULL;

typedef struct {
    void (*callback)(void*);
    void* arg;
} Win32TimerEntry;

#define MAX_TIMERS 32
static Win32TimerEntry g_timers[MAX_TIMERS];
static int g_timer_count = 0;

static const PlatformAPI g_win32_api;

static HDC g_back_dc = NULL;
static HBITMAP g_back_bmp = NULL;
static HBITMAP g_back_old_bmp = NULL;
static int g_back_w = 0;
static int g_back_h = 0;

static void win32_ensure_back_buffer(HWND hWnd, int w, int h) {
    if (g_back_dc && g_back_w == w && g_back_h == h) return;
    if (g_back_dc) {
        SelectObject(g_back_dc, g_back_old_bmp);
        DeleteObject(g_back_bmp);
        DeleteDC(g_back_dc);
    }
    HDC screen_dc = GetDC(hWnd);
    g_back_dc = CreateCompatibleDC(screen_dc);
    g_back_bmp = CreateCompatibleBitmap(screen_dc, w, h);
    g_back_old_bmp = (HBITMAP)SelectObject(g_back_dc, g_back_bmp);
    ReleaseDC(hWnd, screen_dc);
    g_back_w = w;
    g_back_h = h;
}

static PlatformState* g_platform_state = NULL;

static LRESULT CALLBACK win32_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PlatformState* state = g_platform_state;
    
    switch (msg) {
    case WM_CLOSE:
        if (state) state->should_close = true;
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (state) {
            state->window_width = LOWORD(lParam);
            state->window_height = HIWORD(lParam);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state) {
            state->mouse.x = (short)LOWORD(lParam);
            state->mouse.y = (short)HIWORD(lParam);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state) { state->mouse.left = true; SetCapture(hWnd); }
        return 0;
    case WM_LBUTTONUP:
        if (state) { state->mouse.left = false; ReleaseCapture(); }
        return 0;
    case WM_RBUTTONDOWN:
        if (state) state->mouse.right = true;
        return 0;
    case WM_RBUTTONUP:
        if (state) state->mouse.right = false;
        return 0;
    case WM_MBUTTONDOWN:
        if (state) state->mouse.middle = true;
        return 0;
    case WM_MBUTTONUP:
        if (state) state->mouse.middle = false;
        return 0;
    case WM_MOUSEWHEEL:
        if (state) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            state->mouse.scroll_delta += delta;
        }
        return 0;
    case WM_MOUSEHWHEEL:
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (state && wParam < 256) state->keyboard.keys[(int)wParam] = 1;
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (state && wParam < 256) state->keyboard.keys[(int)wParam] = 0;
        return 0;
    case WM_CHAR:
        if (state && wParam >= 32 && wParam < 127) {
            if (state->char_input.char_count < 255) {
                state->char_input.chars[state->char_input.char_count++] = (uint8_t)wParam;
            }
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && state) {
            switch (state->cursor_type) {
            case CURSOR_IBEAM: SetCursor(g_cursor_ibeam); break;
            case CURSOR_HAND:  SetCursor(g_cursor_hand); break;
            case CURSOR_BUSY:  SetCursor(LoadCursor(NULL, IDC_WAIT)); break;
            default:          SetCursor(g_cursor_arrow); break;
            }
            return TRUE;
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static bool win32_init(void) {
    if (g_initialized) return true;
    
    g_hInstance = GetModuleHandle(NULL);
    g_cursor_arrow = LoadCursor(NULL, IDC_ARROW);
    g_cursor_ibeam = LoadCursor(NULL, IDC_IBEAM);
    g_cursor_hand = LoadCursor(NULL, IDC_HAND);
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = win32_wnd_proc;
    wc.hInstance = g_hInstance;
    wc.hCursor = g_cursor_arrow;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "NetXboxClass";
    
    if (!RegisterClassEx(&wc)) return false;
    
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    
    g_initialized = true;

    font_init_default();

    return true;
}

static void win32_shutdown(void) {
    if (g_hRC) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_hRC); g_hRC = NULL; }
    if (g_hDC) { ReleaseDC(g_hWnd, g_hDC); g_hDC = NULL; }
    WSACleanup();
    g_initialized = false;
}

static void win32_poll_events(PlatformState* state) {
    g_platform_state = state;
    state->char_input.char_count = 0;
    state->mouse.scroll_delta = 0;
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            if (state) state->should_close = true;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (state) {
        XINPUT_STATE xstate;
        memset(&xstate, 0, sizeof(xstate));
        state->controller_connected = 0;
        if (XInputGetState(0, &xstate) == ERROR_SUCCESS) {
            state->controller_connected = 1;
            SHORT lx = xstate.Gamepad.sThumbLX;
            SHORT ly = xstate.Gamepad.sThumbLY;
            SHORT rx = xstate.Gamepad.sThumbRX;
            SHORT ry = xstate.Gamepad.sThumbRY;
            float deadzone = 7849.0f;
            float mag_l = sqrtf((float)lx * lx + (float)ly * ly);
            float mag_r = sqrtf((float)rx * rx + (float)ry * ry);
            if (mag_l < deadzone) { lx = 0; ly = 0; }
            else { lx = (SHORT)(lx * (32767.0f - deadzone) / (32767.0f - deadzone)); ly = (SHORT)(ly * (32767.0f - deadzone) / (32767.0f - deadzone)); }
            if (mag_r < deadzone) { rx = 0; ry = 0; }
            state->thumb_lx = lx;
            state->thumb_ly = ly;
            state->thumb_rx = rx;
            state->thumb_ry = ry;
            state->left_trigger = xstate.Gamepad.bLeftTrigger / 255.0f;
            state->right_trigger = xstate.Gamepad.bRightTrigger / 255.0f;

            uint16_t old_buttons = state->buttons;
            state->buttons = xstate.Gamepad.wButtons;
            state->buttons_pressed = state->buttons & ~old_buttons;
            state->buttons_released = ~state->buttons & old_buttons;

            state->mouse.left = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
            state->mouse.right = false;
            state->mouse.middle = (xstate.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
        }
    }
}

static PlatformWindow win32_window_create(const PlatformWindowDesc* desc) {
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_APPWINDOW;
    
    if (desc->fullscreen) {
        style = WS_POPUP;
        exStyle = WS_EX_TOPMOST;
    } else if (!desc->resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    
    RECT rc = {0, 0, desc->width, desc->height};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    
    HWND hWnd = CreateWindowEx(
        exStyle, "NetXboxClass", desc->title,
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, g_hInstance, NULL);
    
    if (!hWnd) return NULL;
    
    HICON hIcon = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (hIcon) {
        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
    
    SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    
    return (PlatformWindow)hWnd;
}

static void win32_window_destroy(PlatformWindow window) {
    if (window) DestroyWindow((HWND)window);
}

static void win32_window_set_title(PlatformWindow window, const char* title) {
    if (window && title) SetWindowText((HWND)window, title);
}

static void win32_window_set_size(PlatformWindow window, int width, int height) {
    if (window) {
        RECT rc = {0, 0, width, height};
        DWORD style = (DWORD)GetWindowLong((HWND)window, GWL_STYLE);
        DWORD exStyle = (DWORD)GetWindowLong((HWND)window, GWL_EXSTYLE);
        AdjustWindowRectEx(&rc, style, FALSE, exStyle);
        SetWindowPos((HWND)window, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }
}

static void win32_window_get_size(PlatformWindow window, int* width, int* height) {
    if (window) {
        RECT rc;
        GetClientRect((HWND)window, &rc);
        if (width) *width = rc.right - rc.left;
        if (height) *height = rc.bottom - rc.top;
    }
}

static void win32_window_show(PlatformWindow window) {
    if (window) ShowWindow((HWND)window, SW_SHOW);
}

static void win32_window_hide(PlatformWindow window) {
    if (window) ShowWindow((HWND)window, SW_HIDE);
}

static void win32_window_focus(PlatformWindow window) {
    if (window) SetForegroundWindow((HWND)window);
}

static PlatformGLContext win32_gl_create_context(PlatformWindow window) {
    g_hWnd = (HWND)window;
    g_hDC = GetDC(g_hWnd);
    
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
    SetPixelFormat(g_hDC, pixelFormat, &pfd);
    
    g_hRC = wglCreateContext(g_hDC);
    wglMakeCurrent(g_hDC, g_hRC);
    
    return (PlatformGLContext)g_hRC;
}

static void win32_gl_destroy_context(PlatformGLContext ctx) {
    if (ctx) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext((HGLRC)ctx);
    }
}

static void win32_gl_make_current(PlatformGLContext ctx) {
    if (ctx) wglMakeCurrent(g_hDC, (HGLRC)ctx);
}

static void win32_gl_swap_buffers(PlatformWindow window) {
    if (window && g_hDC) SwapBuffers(g_hDC);
}

static void* win32_file_open(const char* path, PlatformFileMode mode) {
    DWORD access = 0;
    DWORD creation = 0;
    switch (mode) {
    case PLATFORM_FILE_READ: access = GENERIC_READ; creation = OPEN_EXISTING; break;
    case PLATFORM_FILE_WRITE: access = GENERIC_WRITE; creation = CREATE_ALWAYS; break;
    case PLATFORM_FILE_APPEND: access = FILE_APPEND_DATA; creation = OPEN_ALWAYS; break;
    }
    return (void*)CreateFile(path, access, 0, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void win32_file_close(void* handle) {
    if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle((HANDLE)handle);
}

static int64_t win32_file_read(void* handle, void* buffer, int64_t size) {
    DWORD bytesRead = 0;
    if (ReadFile((HANDLE)handle, buffer, (DWORD)size, &bytesRead, NULL))
        return (int64_t)bytesRead;
    return -1;
}

static int64_t win32_file_write(void* handle, const void* buffer, int64_t size) {
    DWORD bytesWritten = 0;
    if (WriteFile((HANDLE)handle, buffer, (DWORD)size, &bytesWritten, NULL))
        return (int64_t)bytesWritten;
    return -1;
}

static int64_t win32_file_size(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &fad)) return -1;
    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}

static bool win32_file_exists(const char* path) {
    DWORD attr = GetFileAttributes(path);
    return attr != INVALID_FILE_ATTRIBUTES;
}

static bool win32_file_delete(const char* path) {
    return DeleteFile(path) != 0;
}

static char* win32_file_read_all(const char* path, int64_t* out_size) {
    int64_t size = win32_file_size(path);
    if (size <= 0) return NULL;
    
    void* handle = win32_file_open(path, PLATFORM_FILE_READ);
    if (!handle) return NULL;
    
    char* buffer = (char*)malloc((size_t)size + 1);
    int64_t read = win32_file_read(handle, buffer, size);
    win32_file_close(handle);
    
    if (read != size) { free(buffer); return NULL; }
    buffer[size] = '\0';
    if (out_size) *out_size = size;
    return buffer;
}

typedef struct { void (*func)(void*); void* arg; } Win32ThreadData;

static DWORD WINAPI win32_thread_proc(LPVOID param) {
    Win32ThreadData* data = (Win32ThreadData*)param;
    data->func(data->arg);
    free(data);
    return 0;
}

static PlatformThread win32_thread_create(void (*func)(void*), void* arg) {
    Win32ThreadData* data = (Win32ThreadData*)malloc(sizeof(Win32ThreadData));
    data->func = func;
    data->arg = arg;
    return (PlatformThread)CreateThread(NULL, 0, win32_thread_proc, data, 0, NULL);
}

static void win32_thread_join(PlatformThread thread) {
    if (thread) WaitForSingleObject((HANDLE)thread, INFINITE);
}

static PlatformMutex win32_mutex_create(void) {
    return (PlatformMutex)CreateMutex(NULL, FALSE, NULL);
}

static void win32_mutex_lock(PlatformMutex mutex) {
    if (mutex) WaitForSingleObject((HANDLE)mutex, INFINITE);
}

static void win32_mutex_unlock(PlatformMutex mutex) {
    if (mutex) ReleaseMutex((HANDLE)mutex);
}

static void win32_mutex_destroy(PlatformMutex mutex) {
    if (mutex) CloseHandle((HANDLE)mutex);
}

static void win32_sleep_ms(uint32_t ms) {
    Sleep(ms);
}

static PlatformSocket win32_socket_create(PlatformSocketType type) {
    int sockType = (type == PLATFORM_SOCKET_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int proto = (type == PLATFORM_SOCKET_TCP) ? IPPROTO_TCP : IPPROTO_UDP;
    SOCKET s = socket(AF_INET, sockType, proto);
    return (PlatformSocket)(s == INVALID_SOCKET ? NULL : (void*)s);
}

static void win32_socket_destroy(PlatformSocket sock) {
    if (sock) closesocket((SOCKET)sock);
}

static bool win32_socket_connect(PlatformSocket sock, const char* host, uint16_t port) {
    struct addrinfo hints = {0}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char portStr[8];
    _snprintf(portStr, sizeof(portStr), "%u", port);
    
    if (getaddrinfo(host, portStr, &hints, &result) != 0) return false;
    
    bool connected = (connect((SOCKET)sock, result->ai_addr, (int)result->ai_addrlen) == 0);
    freeaddrinfo(result);
    return connected;
}

static int win32_socket_send(PlatformSocket sock, const void* data, int len) {
    return send((SOCKET)sock, (const char*)data, len, 0);
}

static int win32_socket_recv(PlatformSocket sock, void* buffer, int len) {
    return recv((SOCKET)sock, (char*)buffer, len, 0);
}

static bool win32_socket_set_nonblocking(PlatformSocket sock, bool nonblocking) {
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket((SOCKET)sock, FIONBIO, &mode) == 0;
}

static uint64_t win32_get_ticks(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static uint64_t win32_get_freq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

static void win32_log(PlatformLogLevel level, const char* fmt, ...) {
    const char* levelStr[] = {"[INFO]", "[WARN]", "[ERROR]", "[DEBUG]"};
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    OutputDebugString(levelStr[level]);
    OutputDebugString(" ");
    OutputDebugString(buffer);
    OutputDebugString("\n");
}

static char* win32_clipboard_get(void) {
    if (!OpenClipboard(NULL)) return NULL;
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) { CloseClipboard(); return NULL; }
    char* text = (char*)GlobalLock(hData);
    char* result = _strdup(text);
    GlobalUnlock(hData);
    CloseClipboard();
    return result;
}

static void win32_clipboard_set(const char* text) {
    if (!text || !OpenClipboard(NULL)) return;
    EmptyClipboard();
    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    memcpy(GlobalLock(hMem), text, len);
    GlobalUnlock(hMem);
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}

static void win32_surface_blit(PlatformWindow window, const uint32_t* pixels, int width, int height) {
    HWND hWnd = (HWND)window;
    if (!hWnd || !pixels) return;
    
    win32_ensure_back_buffer(hWnd, width, height);
    
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    StretchDIBits(g_back_dc, 0, 0, width, height, 0, 0, width, height, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

static void win32_surface_present(PlatformWindow window) {
    HWND hWnd = (HWND)window;
    if (!hWnd || !g_back_dc) return;
    HDC hdc = GetDC(hWnd);
    BitBlt(hdc, 0, 0, g_back_w, g_back_h, g_back_dc, 0, 0, SRCCOPY);
    ReleaseDC(hWnd, hdc);
}

static void* win32_get_hwnd(PlatformWindow window) {
    return (void*)window;
}

static void win32_text_draw_internal(HDC hdc, int x, int y, const char* text, int font_size, uint32_t color, bool bold, int max_w) {
    if (!hdc || !text || !text[0]) return;
    
    int fw = bold ? FW_BOLD : FW_NORMAL;
    HFONT hFont = CreateFont(font_size, 0, 0, 0, fw, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    
    COLORREF cr = RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    SetTextColor(hdc, cr);
    SetBkMode(hdc, TRANSPARENT);
    
    RECT rc = {x, y, x + (max_w > 0 ? max_w : 8000), y + font_size * 2};
    DrawText(hdc, text, -1, &rc, DT_LEFT | DT_TOP | DT_NOCLIP | DT_SINGLELINE);
    
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

static void win32_text_draw(PlatformWindow window, int x, int y, const char* text, int font_size, uint32_t color, bool bold) {
    HWND hWnd = (HWND)window;
    if (!hWnd || !text || !text[0]) return;
    if (!g_back_dc) {
        RECT rc;
        GetClientRect(hWnd, &rc);
        win32_ensure_back_buffer(hWnd, rc.right, rc.bottom);
    }
    if (!g_back_dc) return;
    win32_text_draw_internal(g_back_dc, x, y, text, font_size, color, bold, 0);
}

static void win32_text_draw_clipped(PlatformWindow window, int x, int y, int max_w, const char* text, int font_size, uint32_t color, bool bold) {
    HWND hWnd = (HWND)window;
    if (!hWnd || !text || !text[0]) return;
    if (!g_back_dc) {
        RECT rc;
        GetClientRect(hWnd, &rc);
        win32_ensure_back_buffer(hWnd, rc.right, rc.bottom);
    }
    if (!g_back_dc) return;
    win32_text_draw_internal(g_back_dc, x, y, text, font_size, color, bold, max_w);
}

static int win32_text_measure(const char* text, int font_size, bool bold) {
    if (!text || !text[0]) return 0;
    
    HDC hdc = GetDC(NULL);
    int fw = bold ? FW_BOLD : FW_NORMAL;
    HFONT hFont = CreateFont(font_size, 0, 0, 0, fw, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    
    SIZE sz;
    GetTextExtentPoint32(hdc, text, (int)strlen(text), &sz);
    
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
    return sz.cx;
}

static int win32_text_measure_clipped(int max_w, const char* text, int font_size, bool bold) {
    if (!text || !text[0] || max_w <= 0) return 0;
    int full_w = win32_text_measure(text, font_size, bold);
    if (full_w <= max_w) return full_w;
    
    char buf[2048];
    strncpy(buf, text, sizeof(buf) - 4);
    int len = (int)strlen(buf);
    while (len > 0 && full_w > max_w) {
        buf[len - 1] = '\0';
        len--;
        if (len >= 3) { buf[len - 1] = '.'; buf[len] = '.'; buf[len + 1] = '.'; buf[len + 2] = '\0'; }
        full_w = win32_text_measure(buf, font_size, bold);
    }
    return full_w;
}

static int win32_text_height(int font_size) {
    return font_size;
}

#include <commdlg.h>

static char* win32_file_save_dialog(const char* default_name, const char* filter) {
    char filename[MAX_PATH] = {0};
    if (default_name) strncpy(filename, default_name, MAX_PATH - 1);

    char filter_buf[512] = {0};
    if (filter) {
        strncpy(filter_buf, filter, sizeof(filter_buf) - 2);
    } else {
        strcpy(filter_buf, "All Files (*.*)\0*.*\0");
    }

    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = filter_buf;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        char* result = (char*)malloc(MAX_PATH);
        if (result) {
            strncpy(result, filename, MAX_PATH - 1);
            result[MAX_PATH - 1] = '\0';
            return result;
        }
    }
    return NULL;
}

static bool win32_file_write_bytes(const char* path, const void* data, int len) {
    if (!path || !data || len <= 0) return false;
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, (DWORD)len, &written, NULL);
    CloseHandle(hFile);
    return ok && (int)written == len;
}

static bool win32_show_keyboard(const char* title, const char* default_text, char* out, int out_size) {
    if (!out || out_size <= 0) return false;
    out[0] = '\0';
    if (default_text) {
        strncpy(out, default_text, out_size - 1);
        out[out_size - 1] = '\0';
    }
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) hwnd = GetDesktopWindow();
    HWND dlg = CreateWindowExA(0, "#32770", title ? title : "Input",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_MODALFRAME | DS_SETFOREGROUND,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 130, hwnd, NULL, GetModuleHandle(NULL), NULL);
    if (!dlg) return false;
    HFONT hfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(dlg, WM_SETFONT, (WPARAM)hfont, TRUE);
    HWND edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", out,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        12, 10, 360, 24, dlg, (HMENU)1, GetModuleHandle(NULL), NULL);
    SendMessage(edit, WM_SETFONT, (WPARAM)hfont, TRUE);
    if (default_text) SetWindowTextA(edit, default_text);
    CreateWindowExA(0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        140, 44, 60, 24, dlg, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
    CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        210, 44, 60, 24, dlg, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);
    SendMessage(edit, EM_SETSEL, 0, -1);
    SetFocus(edit);
    HWND active = dlg;
    MSG msg;
    bool result = false;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (active && msg.hwnd == dlg && msg.message == WM_COMMAND && msg.wParam == IDOK) {
            GetWindowTextA(edit, out, out_size);
            result = (out[0] != '\0');
            break;
        }
        if (active && msg.hwnd == dlg && msg.message == WM_COMMAND && msg.wParam == IDCANCEL) {
            result = false;
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            GetWindowTextA(edit, out, out_size);
            result = (out[0] != '\0');
            break;
        }
        if (active && IsDialogMessageA(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (IsWindow(dlg)) DestroyWindow(dlg);
    return result;
}

static int win32_run_chrome(int mode) {
    (void)mode;
    return -1; // No XUI chrome on Win32.
}

static const PlatformAPI g_win32_api = {
    .init = win32_init,
    .shutdown = win32_shutdown,
    .poll_events = win32_poll_events,
    
    .window_create = win32_window_create,
    .window_destroy = win32_window_destroy,
    .window_set_title = win32_window_set_title,
    .window_set_size = win32_window_set_size,
    .window_get_size = win32_window_get_size,
    .window_show = win32_window_show,
    .window_hide = win32_window_hide,
    .window_focus = win32_window_focus,
    
    .gl_create_context = win32_gl_create_context,
    .gl_destroy_context = win32_gl_destroy_context,
    .gl_make_current = win32_gl_make_current,
    .gl_swap_buffers = win32_gl_swap_buffers,
    
    .file_open = win32_file_open,
    .file_close = win32_file_close,
    .file_read = win32_file_read,
    .file_write = win32_file_write,
    .file_size = win32_file_size,
    .file_exists = win32_file_exists,
    .file_delete = win32_file_delete,
    .file_read_all = win32_file_read_all,
    
    .thread_create = win32_thread_create,
    .thread_join = win32_thread_join,
    .mutex_create = win32_mutex_create,
    .mutex_lock = win32_mutex_lock,
    .mutex_unlock = win32_mutex_unlock,
    .mutex_destroy = win32_mutex_destroy,
    .sleep_ms = win32_sleep_ms,
    
    .socket_create = win32_socket_create,
    .socket_destroy = win32_socket_destroy,
    .socket_connect = win32_socket_connect,
    .socket_send = win32_socket_send,
    .socket_recv = win32_socket_recv,
    .socket_set_nonblocking = win32_socket_set_nonblocking,
    
    .get_ticks = win32_get_ticks,
    .get_freq = win32_get_freq,
    
    .log = win32_log,
    
    .clipboard_set = win32_clipboard_set,
    .clipboard_get = win32_clipboard_get,
    
    .surface_blit = win32_surface_blit,
    .surface_present = win32_surface_present,
    .get_hwnd = win32_get_hwnd,
    
    .text_draw = win32_text_draw,
    .text_draw_clipped = win32_text_draw_clipped,
    .text_measure = win32_text_measure,
    .text_measure_clipped = win32_text_measure_clipped,
    .text_height = win32_text_height,
    .file_save_dialog = win32_file_save_dialog,
    .file_write_bytes = win32_file_write_bytes,
    .show_keyboard = win32_show_keyboard,
    .run_chrome = win32_run_chrome,
};

const PlatformAPI* win32_get_api(void) {
    return &g_win32_api;
}

const char* win32_get_name(void) {
    return "Win32";
}
