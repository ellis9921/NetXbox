// xui_ui.cpp
// Xbox 360 XUI on-screen keyboard integration for NetXbox.
// Based on the Xbox 360 SDK "XuiKeyboard" sample (XUI 1.0 runtime, v0.12).
//
// Flow: the host platform calls xui_ui_keyboard_begin() when the user wants to
// enter text, then loops each frame feeding input (xui_ui_feed_input) and
// rendering the overlay (xui_ui_render) until xui_ui_keyboard_poll() reports
// completion (OK) or cancellation (B).

#include <xtl.h>
#include <xui.h>
#include <d3d9.h>
#include <wchar.h>
#include <wctype.h>

#include "xui_ui.h"

namespace {

// ---------------------------------------------------------------------------
// Keyboard mapping data (identical to the SDK sample)
// ---------------------------------------------------------------------------
struct KEYCAP
{
    LPCWSTR strId;
    UINT dwVKey;
    LPCWSTR strNormalCap;
    LPCWSTR strShiftCap;
};

static KEYCAP g_Keys[] =
{
    { L"Key.A", 'A', L"a", L"A" },
    { L"Key.B", 'B', L"b", L"B" },
    { L"Key.C", 'C', L"c", L"C" },
    { L"Key.D", 'D', L"d", L"D" },
    { L"Key.E", 'E', L"e", L"E" },
    { L"Key.F", 'F', L"f", L"F" },
    { L"Key.G", 'G', L"g", L"G" },
    { L"Key.H", 'H', L"h", L"H" },
    { L"Key.I", 'I', L"i", L"I" },
    { L"Key.J", 'J', L"j", L"J" },
    { L"Key.K", 'K', L"k", L"K" },
    { L"Key.L", 'L', L"l", L"L" },
    { L"Key.M", 'M', L"m", L"M" },
    { L"Key.N", 'N', L"n", L"N" },
    { L"Key.O", 'O', L"o", L"O" },
    { L"Key.P", 'P', L"p", L"P" },
    { L"Key.Q", 'Q', L"q", L"Q" },
    { L"Key.R", 'R', L"r", L"R" },
    { L"Key.S", 'S', L"s", L"S" },
    { L"Key.T", 'T', L"t", L"T" },
    { L"Key.U", 'U', L"u", L"U" },
    { L"Key.V", 'V', L"v", L"V" },
    { L"Key.W", 'W', L"w", L"W" },
    { L"Key.X", 'X', L"x", L"X" },
    { L"Key.Y", 'Y', L"y", L"Y" },
    { L"Key.Z", 'Z', L"z", L"Z" },
    { L"Key._", 0xFFFF, L"_", L"_" },
    { L"Key.-", VK_OEM_MINUS, L"-", L"-" },
    { L"Key.0", '0', L"0", L"0" },
    { L"Key.1", '1', L"1", L"1" },
    { L"Key.2", '2', L"2", L"2" },
    { L"Key.3", '3', L"3", L"3" },
    { L"Key.4", '4', L"4", L"4" },
    { L"Key.5", '5', L"5", L"5" },
    { L"Key.6", '6', L"6", L"6" },
    { L"Key.7", '7', L"7", L"7" },
    { L"Key.8", '8', L"8", L"8" },
    { L"Key.9", '9', L"9", L"9" },
    { L"Key..", VK_OEM_PERIOD, L".", L"." },
    { L"Key.OK", VK_RETURN, L"OK", L"OK" },
    { L"Key.Cancel", VK_CANCEL, L"Cancel", L"Cancel" },
    { L"Key.Clear", VK_CLEAR, L"Clear", L"Clear" },
    { L"Key.Space", VK_SPACE, L"Space", L"Space" },
    { L"Key.Shift", VK_SHIFT, L"Shift", L"Shift" },
    { L"Key.Backspace", VK_BACK, L"Backspace", L"Backspace" },
    { L"Key.Left", VK_LEFT, L"Left", L"Left" },
    { L"Key.Right", VK_RIGHT, L"Right", L"Right" },
};

const DWORD g_dwNumKeys = sizeof(g_Keys) / sizeof(g_Keys[0]);

// ---------------------------------------------------------------------------
// XUI globals
// ---------------------------------------------------------------------------
HXUIOBJ  g_hObjRoot = NULL;
HXUIDC   g_hDC = NULL;
XUIClass g_KeyboardSceneClass;
HXUIOBJ  g_hEditCtl = NULL;

// Chrome overlay scenes
XUIClass g_ChromeSceneClass;
HXUIOBJ  g_hChromeToolbar  = NULL;
HXUIOBJ  g_hChromeSettings = NULL;
HXUIOBJ  g_hChromeHome     = NULL;
HXUIOBJ  g_hChromeUrlBox   = NULL;
HXUIOBJ  g_hChromeSearchBox = NULL;
bool     g_chrome_active   = false;
bool     g_chrome_done     = false;
XuiChromeMode g_chrome_mode  = XUI_CHROME_OFF;
int      g_chrome_action_queue[16];
int      g_chrome_action_count = 0;

// Dialog session state
bool  g_session_active = false;
bool  g_session_done   = false;
int   g_session_result = 0;      // 0 pending, 1 OK, -1 cancel
WCHAR g_result_wide[2048] = L"";

} // namespace

// ---------------------------------------------------------------------------
// Keyboard scene private object state
// ---------------------------------------------------------------------------
struct KEYBOARDSCENE
{
    HXUIOBJ m_btnMap[64];
    HXUIOBJ m_btnCancel;
    HXUIOBJ m_hEditCtl;
    DWORD m_dwNumKeys;
    BOOL m_bShift;
    HXUIOBJ m_hOk;
};

// ---------------------------------------------------------------------------
// Keyboard scene helpers
// ---------------------------------------------------------------------------
static BOOL KeyboardScene_IsGamepadInput(UINT dwVKey)
{
    return (dwVKey >= VK_PAD_A && dwVKey <= VK_PAD_RTHUMB_DOWNLEFT);
}

static void KeyboardScene_RefreshKeycaps(KEYBOARDSCENE* pObject)
{
    for (DWORD i = 0; i < pObject->m_dwNumKeys; i++)
    {
        DWORD dwId = 0;
        XUIElementPropVal val;
        if (pObject->m_bShift)
            XUIElementPropVal_SetString(&val, g_Keys[i].strShiftCap);
        else
            XUIElementPropVal_SetString(&val, g_Keys[i].strNormalCap);
        XuiObjectGetPropertyId(pObject->m_btnMap[i], L"Text", &dwId);
        XuiObjectSetProperty(pObject->m_btnMap[i], dwId, 0, &val);
    }
}

static void KeyboardScene_ToggleShift(KEYBOARDSCENE* pObject)
{
    pObject->m_bShift = !pObject->m_bShift;
    KeyboardScene_RefreshKeycaps(pObject);
}

static void KeyboardScene_SendKeydown(KEYBOARDSCENE* pObject, UINT dwVKey, DWORD dwFlags)
{
    XUIMessage msg;
    XUIMessageInput msgExt;
    XuiMessageInput(&msg, &msgExt, XUI_KEYDOWN, dwVKey, 0, dwFlags, 0);
    XuiSendMessage(pObject->m_hEditCtl, &msg);
}

static void KeyboardScene_SendChar(KEYBOARDSCENE* pObject, WCHAR ch)
{
    XUIMessage msg;
    XUIMessageChar msgExt;
    XuiMessageChar(&msg, &msgExt, ch, 0, 0);
    XuiSendMessage(pObject->m_hEditCtl, &msg);
}

static void KeyboardScene_SendKeyup(KEYBOARDSCENE* pObject, UINT dwVKey, DWORD dwFlags)
{
    XUIMessage msg;
    XUIMessageInput msgExt;
    XuiMessageInput(&msg, &msgExt, XUI_KEYUP, dwVKey, 0, dwFlags, 0);
    XuiSendMessage(pObject->m_hEditCtl, &msg);
}

// Commit the typed text (OK) or cancel (B). Drives the host modal session.
static void KeyboardScene_Commit()
{
    LPCWSTR str = XuiControlGetText(g_hEditCtl);
    if (str)
    {
        wcscpy_s(g_result_wide, ARRAYSIZE(g_result_wide), str);
    }
    else
    {
        g_result_wide[0] = L'\0';
    }
    g_session_result = 1;
    g_session_done = true;
}

static void KeyboardScene_Cancel()
{
    g_session_result = -1;
    g_session_done = true;
}

static void KeyboardScene_DispatchInput(KEYBOARDSCENE* pObject, UINT dwVKey)
{
    switch (dwVKey)
    {
        case VK_CANCEL:
            break; // Cancel handled by host
        case VK_RETURN:
            KeyboardScene_Commit();
            break;

        case 0xFFFF:
        {
            KeyboardScene_SendKeydown(pObject, VK_OEM_MINUS, XUI_INPUT_FLAG_SHIFT);
            KeyboardScene_SendChar(pObject, L'_');
            KeyboardScene_SendKeyup(pObject, VK_OEM_MINUS, XUI_INPUT_FLAG_SHIFT);
            break;
        }

        default:
        {
            KeyboardScene_SendKeydown(pObject, dwVKey, 0);
            if (dwVKey >= 'A' && dwVKey <= 'Z')
            {
                if (pObject->m_bShift)
                    KeyboardScene_SendChar(pObject, (WCHAR)dwVKey);
                else
                    KeyboardScene_SendChar(pObject, towlower((WCHAR)dwVKey));
            }
            else if ((dwVKey >= '0' && dwVKey <= '9') || (dwVKey == VK_SPACE) || (dwVKey == VK_BACK))
            {
                KeyboardScene_SendChar(pObject, (WCHAR)dwVKey);
            }
            else if (dwVKey == VK_OEM_PERIOD)
            {
                KeyboardScene_SendChar(pObject, L'.');
            }
            else if (dwVKey == VK_OEM_MINUS)
            {
                if (pObject->m_bShift)
                    KeyboardScene_SendChar(pObject, L'_');
                else
                    KeyboardScene_SendChar(pObject, L'-');
            }
            KeyboardScene_SendKeyup(pObject, dwVKey, 0);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Keyboard scene ObjectProc etc.
// ---------------------------------------------------------------------------
static HRESULT KeyboardScene_OnInit(HXUIOBJ hSceneObj, XUIMessageInit* pInitData)
{
    (void)pInitData;
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle(hSceneObj, (void**)&pObject);

    pObject->m_dwNumKeys = g_dwNumKeys;
    XuiElementGetChildById(hSceneObj, L"XuiEdit1", &pObject->m_hEditCtl);
    g_hEditCtl = pObject->m_hEditCtl;

    for (DWORD i = 0; i < g_dwNumKeys; i++)
    {
        XuiElementGetChildById(hSceneObj, g_Keys[i].strId, &pObject->m_btnMap[i]);
        if (g_Keys[i].dwVKey == VK_RETURN)
            pObject->m_hOk = pObject->m_btnMap[i];
    }
    XuiElementGetChildById(hSceneObj, L"Key.Cancel", &pObject->m_btnCancel);

    pObject->m_bShift = FALSE;
    KeyboardScene_RefreshKeycaps(pObject);
    return S_OK;
}

static HRESULT KeyboardScene_OnNotifyPressing(HXUIOBJ hSceneObj, HXUIOBJ hObjPressed)
{
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle(hSceneObj, (void**)&pObject);

    for (DWORD i = 0; i < g_dwNumKeys; i++)
    {
        if (hObjPressed == pObject->m_btnMap[i])
        {
            if (g_Keys[i].dwVKey == VK_SHIFT)
                KeyboardScene_ToggleShift(pObject);
            KeyboardScene_DispatchInput(pObject, g_Keys[i].dwVKey);
            break;
        }
    }
    return S_OK;
}

static HRESULT KeyboardScene_OnKeyDown(HXUIOBJ hSceneObj, XUIMessageInput* pInputData)
{
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle(hSceneObj, (void**)&pObject);

    // B button anywhere cancels the dialog.
    if (pInputData->dwKeyCode == VK_PAD_B)
    {
        if (!(pInputData->dwFlags & XUI_INPUT_FLAG_REPEAT))
            KeyboardScene_Cancel();
        return S_OK;
    }

    if (!KeyboardScene_IsGamepadInput(pInputData->dwKeyCode) &&
        (pInputData->dwKeyCode == VK_SHIFT))
    {
        if (!pObject->m_bShift)
            KeyboardScene_ToggleShift(pObject);
    }

    if (!KeyboardScene_IsGamepadInput(pInputData->dwKeyCode))
    {
        for (DWORD i = 0; i < g_dwNumKeys; i++)
        {
            if (g_Keys[i].dwVKey == pInputData->dwKeyCode)
            {
                if (!XuiElementHasFocus(pObject->m_hOk))
                    XuiElementSetFocus(pObject->m_hOk);
                if (pInputData->dwKeyCode != VK_SHIFT)
                    XuiControlPress(pObject->m_btnMap[i], XUSER_INDEX_ANY);
                break;
            }
        }
    }
    return S_OK;
}

static HRESULT KeyboardScene_ObjectProc(HXUIOBJ hObj, XUIMessage* pMessage, void* pvThis)
{
    (void)pvThis;
    switch (pMessage->dwMessage)
    {
        case XM_INIT:
            return KeyboardScene_OnInit(hObj, (XUIMessageInit*)pMessage->pvData);

        case XM_NOTIFY:
        {
            XUINotify* pNotify = (XUINotify*)pMessage->pvData;
            if (pNotify->dwNotify == XN_PRESSING)
                return KeyboardScene_OnNotifyPressing(hObj, pNotify->hObjSource);
            break;
        }

        case XM_KEYDOWN:
            return KeyboardScene_OnKeyDown(hObj, (XUIMessageInput*)pMessage->pvData);
    }
    return S_OK;
}

static HRESULT KeyboardScene_CreateInstance(HXUIOBJ hObj, void** ppObject)
{
    (void)hObj;
    KEYBOARDSCENE* pObject = new KEYBOARDSCENE;
    if (!pObject) return E_OUTOFMEMORY;
    ZeroMemory(pObject, sizeof(KEYBOARDSCENE));
    *ppObject = (void*)pObject;
    return S_OK;
}

static HRESULT KeyboardScene_DestroyInstance(void* pObject)
{
    delete (KEYBOARDSCENE*)pObject;
    return S_OK;
}

// ---------------------------------------------------------------------------
// Chrome overlay scenes (toolbar / settings / home)
// ---------------------------------------------------------------------------
struct ChromeButtonDef { LPCWSTR id; int action; };
struct ChromeButton { HXUIOBJ hObj; int action; };
static ChromeButton g_chrome_buttons[24];
static int g_chrome_button_count = 0;
static int g_chrome_openurl_action = XUI_ACTION_NONE;

static void ChromeScene_ResetButtons(void)
{
    g_chrome_button_count = 0;
    g_chrome_openurl_action = XUI_ACTION_NONE;
}

static void ChromeScene_RegisterButton(HXUIOBJ hCtl, int action)
{
    if (!hCtl) return;
    if (g_chrome_button_count < (int)(sizeof(g_chrome_buttons) / sizeof(g_chrome_buttons[0])))
    {
        g_chrome_buttons[g_chrome_button_count].hObj = hCtl;
        g_chrome_buttons[g_chrome_button_count].action = action;
        g_chrome_button_count++;
    }
}

static void ChromeQueueAction(int action)
{
    if (action == XUI_ACTION_NONE) return;
    if (g_chrome_action_count < (int)(sizeof(g_chrome_action_queue) / sizeof(int)))
    {
        g_chrome_action_queue[g_chrome_action_count++] = action;
    }
    if (action == XUI_ACTION_SETTINGS || action == XUI_ACTION_CLOSE ||
        action == XUI_ACTION_URL || action == XUI_ACTION_SEARCH ||
        action == XUI_ACTION_LINK0 || action == XUI_ACTION_LINK1 ||
        action == XUI_ACTION_LINK2 || action == XUI_ACTION_LINK3 ||
        action == XUI_ACTION_HOMEPAGE_DEFAULT || action == XUI_ACTION_HOMEPAGE_NETXBOX ||
        action == XUI_ACTION_HOMEPAGE_DUCKDUCKGO || action == XUI_ACTION_TOGGLE_TOOLBAR)
    {
        g_chrome_done = true;
    }
}

static HRESULT ChromeScene_OnInit(HXUIOBJ hSceneObj, XUIMessageInit* pInitData)
{
    (void)pInitData;
    ChromeScene_ResetButtons();
    switch (g_chrome_mode)
    {
        case XUI_CHROME_TOOLBAR:
        {
            ChromeButtonDef defs[] = {
                { L"Back",   XUI_ACTION_BACK },
                { L"Forward",XUI_ACTION_FORWARD },
                { L"Refresh",XUI_ACTION_REFRESH },
                { L"Home",   XUI_ACTION_HOME },
                { L"NewTab", XUI_ACTION_NEW_TAB },
                { L"Settings",XUI_ACTION_SETTINGS },
            };
            for (int i = 0; i < (int)(sizeof(defs)/sizeof(defs[0])); i++)
            {
                HXUIOBJ h = NULL;
                if (SUCCEEDED(XuiElementGetChildById(hSceneObj, defs[i].id, &h)))
                    ChromeScene_RegisterButton(h, defs[i].action);
            }
            g_chrome_openurl_action = XUI_ACTION_URL;
            break;
        }
        case XUI_CHROME_SETTINGS:
        {
            ChromeButtonDef defs[] = {
                { L"HomeDefault",  XUI_ACTION_HOMEPAGE_DEFAULT },
                { L"HomeNetXbox",  XUI_ACTION_HOMEPAGE_NETXBOX },
                { L"HomeDuckDuckGo",XUI_ACTION_HOMEPAGE_DUCKDUCKGO },
                { L"ShowToolbar",  XUI_ACTION_TOGGLE_TOOLBAR },
                { L"Close",        XUI_ACTION_CLOSE },
            };
            for (int i = 0; i < (int)(sizeof(defs)/sizeof(defs[0])); i++)
            {
                HXUIOBJ h = NULL;
                if (SUCCEEDED(XuiElementGetChildById(hSceneObj, defs[i].id, &h)))
                    ChromeScene_RegisterButton(h, defs[i].action);
            }
            break;
        }
        case XUI_CHROME_HOME:
        {
            ChromeButtonDef defs[] = {
                { L"Link0", XUI_ACTION_LINK0 },
                { L"Link1", XUI_ACTION_LINK1 },
                { L"Link2", XUI_ACTION_LINK2 },
                { L"Link3", XUI_ACTION_LINK3 },
                { L"BtnA",  XUI_ACTION_SEARCH },
                { L"BtnB",  XUI_ACTION_BACK },
                { L"BtnX",  XUI_ACTION_FORWARD },
                { L"BtnY",  XUI_ACTION_REFRESH },
            };
            for (int i = 0; i < (int)(sizeof(defs)/sizeof(defs[0])); i++)
            {
                HXUIOBJ h = NULL;
                if (SUCCEEDED(XuiElementGetChildById(hSceneObj, defs[i].id, &h)))
                    ChromeScene_RegisterButton(h, defs[i].action);
            }
            g_chrome_openurl_action = XUI_ACTION_SEARCH;
            break;
        }
        default:
            break;
    }
    return S_OK;
}

static HRESULT ChromeScene_OnNotifyPressing(HXUIOBJ hObjPressed)
{
    if (!hObjPressed) return S_OK;
    for (int i = 0; i < g_chrome_button_count; i++)
    {
        if (hObjPressed == g_chrome_buttons[i].hObj)
        {
            ChromeQueueAction(g_chrome_buttons[i].action);
            break;
        }
    }
    return S_OK;
}

static HRESULT ChromeScene_OnKeyDown(XUIMessageInput* pInputData)
{
    if (pInputData->dwKeyCode == VK_PAD_B)
    {
        if (!(pInputData->dwFlags & XUI_INPUT_FLAG_REPEAT))
            g_chrome_done = true;
        return S_OK;
    }
    return S_OK;
}

static HRESULT ChromeScene_ObjectProc(HXUIOBJ hObj, XUIMessage* pMessage, void* pvThis)
{
    (void)pvThis;
    switch (pMessage->dwMessage)
    {
        case XM_INIT:
            return ChromeScene_OnInit(hObj, (XUIMessageInit*)pMessage->pvData);
        case XM_NOTIFY:
        {
            XUINotify* pNotify = (XUINotify*)pMessage->pvData;
            if (pNotify->dwNotify == XN_PRESSING)
                return ChromeScene_OnNotifyPressing(pNotify->hObjSource);
            break;
        }
        case XM_KEYDOWN:
            return ChromeScene_OnKeyDown((XUIMessageInput*)pMessage->pvData);
    }
    return S_OK;
}

static HRESULT ChromeScene_CreateInstance(HXUIOBJ hObj, void** ppObject)
{
    (void)hObj;
    *ppObject = NULL;
    return S_OK;
}

static HRESULT ChromeScene_DestroyInstance(void* pObject)
{
    (void)pObject;
    return S_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
BOOL xui_ui_init(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresent)
{
    HRESULT hr;

    g_hObjRoot = NULL;
    g_hDC = NULL;
    g_hEditCtl = NULL;

    hr = XuiRenderInitShared(pDevice, pPresent, XuiD3DXTextureLoader);
    if (FAILED(hr)) return FALSE;

    hr = XuiRenderCreateDC(&g_hDC);
    if (FAILED(hr)) return FALSE;

    XUIInitParams initparams = {0};
    initparams.cbSize = sizeof(initparams);
    initparams.dwFlags = XUI_INIT_PARAMS_FLAGS_NOKEYBOARD;
    hr = XuiInit(&initparams);
    if (FAILED(hr)) return FALSE;

    hr = XuiSoundXACTRegister();
    if (FAILED(hr)) return FALSE;

    HXUICLASS hClass = NULL;
    ZeroMemory(&g_KeyboardSceneClass, sizeof(g_KeyboardSceneClass));
    g_KeyboardSceneClass.szClassName = L"XuiKeyboardScene";
    g_KeyboardSceneClass.szBaseClassName = XUI_CLASS_SCENE;
    g_KeyboardSceneClass.Methods.CreateInstance = KeyboardScene_CreateInstance;
    g_KeyboardSceneClass.Methods.DestroyInstance = KeyboardScene_DestroyInstance;
    g_KeyboardSceneClass.Methods.ObjectProc = KeyboardScene_ObjectProc;
    hr = XuiRegisterClass(&g_KeyboardSceneClass, &hClass);
    if (FAILED(hr)) return FALSE;

    hr = XuiCreateObject(L"XuiCanvas", &g_hObjRoot);
    if (FAILED(hr)) return FALSE;

    hr = XuiElementSetBounds(g_hObjRoot, 640.0f, 480.0f);
    if (FAILED(hr)) return FALSE;

    TypefaceDescriptor typeface = {
        L"Arial Unicode MS",
        L"file://game:/media/xarialuni.ttf"
    };
    hr = XuiRegisterTypeface(&typeface, TRUE);
    if (FAILED(hr)) return FALSE;

    // Register the shared chrome scene class. The keyboard is the essential
    // resource; the skin and chrome scenes are best-effort so that one bad
    // resource can never disable the on-screen keyboard.
    ZeroMemory(&g_ChromeSceneClass, sizeof(g_ChromeSceneClass));
    g_ChromeSceneClass.szClassName = L"XuiChromeScene";
    g_ChromeSceneClass.szBaseClassName = XUI_CLASS_SCENE;
    g_ChromeSceneClass.Methods.CreateInstance = ChromeScene_CreateInstance;
    g_ChromeSceneClass.Methods.DestroyInstance = ChromeScene_DestroyInstance;
    g_ChromeSceneClass.Methods.ObjectProc = ChromeScene_ObjectProc;
    XuiRegisterClass(&g_ChromeSceneClass, &hClass);

    // The keyboard is the ONLY hard requirement. If it fails, XUI is unusable.
    HXUIOBJ hScene = NULL;
    hr = XuiSceneCreate(L"file://game:/media/netxbox.xzp#xui/", L"us_keyboard.xur", NULL, &hScene);
    if (FAILED(hr)) return FALSE;
    hr = XuiSceneNavigateFirst(g_hObjRoot, hScene, XUSER_INDEX_ANY);
    if (FAILED(hr))
    {
        XuiDestroyObject(hScene);
        return FALSE;
    }

    // Best-effort skin. XUI browser chrome is degraded (unstyled) if this
    // fails, but the keyboard and chrome scenes still function.
    XuiLoadVisualFromBinary(L"file://game:/media/netxbox.xzp#xui/netxbox_skin.xur", NULL);

    return TRUE;
}

BOOL xui_ui_chrome_resources(void)
{
    if (g_hChromeToolbar && g_hChromeSettings && g_hChromeHome)
        return TRUE;

    static const WCHAR cPkg[] = L"file://game:/media/netxbox.xzp#xui/";
    if (!g_hChromeToolbar)
        XuiSceneCreate(cPkg, L"toolbar.xur", NULL, &g_hChromeToolbar);
    if (!g_hChromeSettings)
        XuiSceneCreate(cPkg, L"settings.xur", NULL, &g_hChromeSettings);
    if (!g_hChromeHome)
        XuiSceneCreate(cPkg, L"home.xur", NULL, &g_hChromeHome);

    XuiElementGetChildById(g_hChromeToolbar, L"UrlBox", &g_hChromeUrlBox);
    XuiElementGetChildById(g_hChromeHome, L"SearchBox", &g_hChromeSearchBox);

    g_session_active = false;
    g_session_done = false;
    g_session_result = 0;
    g_chrome_active = false;
    g_chrome_done = false;
    g_chrome_mode = XUI_CHROME_OFF;
    g_chrome_action_count = 0;

    return TRUE;
}

void xui_ui_shutdown(void)
{
    if (g_hObjRoot)
    {
        XuiDestroyObject(g_hObjRoot);
        g_hObjRoot = NULL;
    }

    XuiUnregisterClass(g_KeyboardSceneClass.szClassName);
    XuiUnregisterClass(g_ChromeSceneClass.szClassName);

    if (g_hDC)
    {
        XuiRenderDestroyDC(g_hDC);
        g_hDC = NULL;
    }

    XuiSoundXACTUnregister();
    XuiUninit();
    XuiRenderUninit();

    g_hEditCtl = NULL;
    g_hChromeToolbar = NULL;
    g_hChromeSettings = NULL;
    g_hChromeHome = NULL;
    g_session_active = false;
    g_chrome_active = false;
}

BOOL xui_ui_keyboard_begin(const char* title, const char* default_text)
{
    (void)title;
    if (!g_hEditCtl) return FALSE;
    if (g_session_active) return FALSE;

    WCHAR wdef[512] = L"";
    if (default_text)
        MultiByteToWideChar(CP_UTF8, 0, default_text, -1, wdef, 512);

    XuiControlSetText(g_hEditCtl, wdef);
    XuiEditSetCaretPosition(g_hEditCtl, (UINT)wcslen(wdef));

    g_result_wide[0] = L'\0';
    g_session_result = 0;
    g_session_done = false;
    g_session_active = true;
    return TRUE;
}

int xui_ui_keyboard_poll(char* out_utf8, int out_utf8_size)
{
    if (!g_session_active)
        return g_session_result;

    if (!g_session_done)
        return 0;

    g_session_active = false;

    if (g_session_result == 1 && out_utf8 && out_utf8_size > 0)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, g_result_wide, -1,
                                    out_utf8, out_utf8_size, NULL, NULL);
        if (n <= 0)
            out_utf8[0] = '\0';
    }
    else if (out_utf8 && out_utf8_size > 0)
    {
        out_utf8[0] = '\0';
    }
    return g_session_result;
}

void xui_ui_feed_input(void)
{
    XINPUT_KEYSTROKE keystroke;
    if (XInputGetKeystroke(XUSER_INDEX_ANY, XINPUT_FLAG_ANYDEVICE, &keystroke) == ERROR_SUCCESS)
    {
        XuiProcessInput(&keystroke);
    }
}

void xui_ui_render(DWORD dwBackWidth, DWORD dwBackHeight, float fDeltaTimeSeconds)
{
    if (!g_hDC || !g_hObjRoot) return;

    XuiAnimRun(fDeltaTimeSeconds * 1000.0f);

    XuiRenderBegin(g_hDC, D3DCOLOR_ARGB(255, 0, 0, 0));

    D3DXMATRIX matOrigView;
    XuiRenderGetViewTransform(g_hDC, &matOrigView);

    // Scale the 640x480 scene to the back buffer, centered horizontally.
    D3DXMATRIX matView;
    D3DXVECTOR2 vScaling = D3DXVECTOR2((float)dwBackHeight / 480.0f, (float)dwBackHeight / 480.0f);
    D3DXVECTOR2 vTranslation = D3DXVECTOR2((float)((int)dwBackWidth / 2) -
                                          (640.0f * (float)dwBackHeight / (2.0f * 480.0f)), 0.0f);
    D3DXMatrixTransformation2D(&matView, NULL, 0.0f, &vScaling, NULL, 0.0f, &vTranslation);
    XuiRenderSetViewTransform(g_hDC, &matView);

    XUIMessage msg;
    XUIMessageRender msgRender;
    XuiMessageRender(&msg, &msgRender, g_hDC, 0xffffffff, XUI_BLEND_NORMAL);
    XuiSendMessage(g_hObjRoot, &msg);

    XuiRenderSetViewTransform(g_hDC, &matOrigView);

    XuiRenderEnd(g_hDC);

    XuiTimersRun();
}

// ---------------------------------------------------------------------------
// Chrome overlay session API
// ---------------------------------------------------------------------------
BOOL xui_ui_chrome_begin(XuiChromeMode mode)
{
    if (!g_hObjRoot) return FALSE;
    if (g_chrome_active || g_session_active) return FALSE;

    if (!xui_ui_chrome_resources())
        return FALSE;

    HXUIOBJ hScene = NULL;
    switch (mode)
    {
        case XUI_CHROME_TOOLBAR: hScene = g_hChromeToolbar; break;
        case XUI_CHROME_SETTINGS: hScene = g_hChromeSettings; break;
        case XUI_CHROME_HOME: hScene = g_hChromeHome; break;
        default: return FALSE;
    }
    if (!hScene) return FALSE;

    HRESULT hr = XuiSceneNavigateFirst(g_hObjRoot, hScene, XUSER_INDEX_ANY);
    if (FAILED(hr)) return FALSE;

    g_chrome_mode = mode;
    g_chrome_active = true;
    g_chrome_done = false;
    g_chrome_action_count = 0;
    return TRUE;
}

int xui_ui_chrome_poll(void)
{
    if (!g_chrome_active)
        return 1;
    if (!g_chrome_done)
        return 0;

    // Restore the keyboard scene so it remains available for the next URL entry.
    if (g_hEditCtl)
    {
        HXUIOBJ hKeyscene = NULL;
        if (SUCCEEDED(XuiSceneCreate(L"file://game:/media/netxbox.xzp#xui/",
                                     L"us_keyboard.xur", NULL, &hKeyscene)))
        {
            XuiSceneNavigateFirst(g_hObjRoot, hKeyscene, XUSER_INDEX_ANY);
            g_hEditCtl = NULL;
            XuiElementGetChildById(hKeyscene, L"XuiEdit1", &g_hEditCtl);
        }
    }

    g_chrome_active = false;
    g_chrome_mode = XUI_CHROME_OFF;
    return 1;
}

int xui_ui_chrome_take_action(void)
{
    if (g_chrome_action_count <= 0) return XUI_ACTION_NONE;
    int a = g_chrome_action_queue[0];
    for (int i = 1; i < g_chrome_action_count; i++)
        g_chrome_action_queue[i - 1] = g_chrome_action_queue[i];
    g_chrome_action_count--;
    return a;
}

void xui_ui_chrome_set_url(const char* url)
{
    if (!g_hChromeUrlBox) return;
    WCHAR wbuf[1024] = L"";
    if (url)
        MultiByteToWideChar(CP_UTF8, 0, url, -1, wbuf, 1024);
    XuiControlSetText(g_hChromeUrlBox, wbuf);
}

void xui_ui_chrome_set_toolbar_visible(BOOL visible)
{
    (void)visible;
}

