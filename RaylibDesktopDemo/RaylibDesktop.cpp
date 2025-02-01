#include "RaylibDesktop.h"

#include <Windows.h>
#include <shellscalingapi.h>  // For DPI awareness functions
#pragma comment(lib, "Shcore.lib") // Required for SetProcessDpiAwareness and GetDpiForMonitor

// Global variable to hold the WorkerW handle (the window behind desktop icons)
HWND g_workerWindowHandle = NULL;

int g_desktopWidth = 0;
int g_desktopHeight = 0;

// Callback function for EnumWindows to locate the proper WorkerW window
BOOL CALLBACK EnumWindowsProc(HWND windowHandle, LPARAM lParam)
{
    // Look for a child window named "SHELLDLL_DefView" in each top-level window.
    HWND shellViewWindow = FindWindowEx(windowHandle, NULL, L"SHELLDLL_DefView", NULL);
    if (shellViewWindow != NULL)
    {
        // If found, get the WorkerW window that is a sibling of the found window.
        g_workerWindowHandle = FindWindowEx(NULL, windowHandle, L"WorkerW", NULL);
        return FALSE;  // Stop enumeration since we have found the desired window.
    }
    return TRUE;
}

int InitRaylibDesktop()
{
    // Set the process DPI awareness to get physical pixel coordinates.
    // This must be done before any windows are created.
    HRESULT dpiAwarenessResult = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (FAILED(dpiAwarenessResult))
    {
        MessageBox(NULL, L"Failed to set DPI awareness.", L"Error", MB_OK);
        // Continue if needed, but coordinate values may be scaled.
    }

    int primaryMonitorX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int primaryMonitorY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_desktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_desktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Locate the Progman window (the desktop window)
    HWND progmanWindowHandle = FindWindow(L"Progman", NULL);

    // Send message 0x052C to Progman to force creation of a WorkerW window
    LRESULT result = 0;
    SendMessageTimeout(
        progmanWindowHandle,
        0x052C,          // Undocumented message to trigger WorkerW creation
        0,
        0,
        SMTO_NORMAL,
        1000,
        reinterpret_cast<PDWORD_PTR>(&result)
    );

    // Enumerate through top-level windows to find the WorkerW window behind desktop icons
    EnumWindows(EnumWindowsProc, 0);

    // Ensure the WorkerW handle was found
    if (g_workerWindowHandle == NULL)
    {
        MessageBox(NULL, L"Failed to find WorkerW window.", L"Error", MB_OK);
        return -1;
    }

    return 0;
}

void RaylibDesktopReparentWindow(void* raylibWindowHandle)
{
    // Reparent the raylib window to your custom WorkerW window.
    // This attaches the raylib rendering window as a child of your WorkerW,
    // which should place it behind desktop icons if your WorkerW is set up that way.
    SetParent((HWND)raylibWindowHandle, g_workerWindowHandle);

    // Optionally, adjust window styles so that it behaves like a wallpaper.
    // For example, you may remove the title bar or border:
    LONG_PTR style = GetWindowLongPtr((HWND)raylibWindowHandle, GWL_STYLE);
    style &= ~(WS_OVERLAPPEDWINDOW); // Remove common overlapped window styles.
    style |= WS_CHILD;                // Make it a child window.
    SetWindowLongPtr((HWND)raylibWindowHandle, GWL_STYLE, style);

    // (Optional) Resize/reposition the raylib window to match its new parent.
    // For example, if g_workerWindowHandle covers the full primary monitor:
    SetWindowPos((HWND)raylibWindowHandle, NULL, 0, 0, g_desktopWidth, g_desktopHeight, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CleanupRaylibDesktop()
{
    wchar_t wallpaperPath[MAX_PATH] = { 0 };
    // Retrieve the current wallpaper path
    if (SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, wallpaperPath, 0))
    {
        // Reapply the wallpaper to force a refresh.
        SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, wallpaperPath, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }
}

// Mouse replacement

// Avoid including raylib.h due to windows.h conflicts
typedef struct Vector2 {
    float x;
    float y;
} Vector2;

// We support 5 mouse buttons.
#define MOUSE_BUTTON_COUNT 5

// Arrays to hold the state of each mouse button.
// prevMouseState[] holds the state from the previous frame,
// currMouseState[] holds the state for the current frame.
static bool prevMouseState[MOUSE_BUTTON_COUNT] = { false, false, false, false, false };
static bool currMouseState[MOUSE_BUTTON_COUNT] = { false, false, false, false, false };

// Helper function: maps a button index to the corresponding virtual key.
static int GetVirtualKeyForMouseButton(int button)
{
    switch (button)
    {
    case 0: return VK_LBUTTON;   // Left button
    case 1: return VK_RBUTTON;   // Right button
    case 2: return VK_MBUTTON;   // Middle button
    case 3: return VK_XBUTTON1;  // XButton1
    case 4: return VK_XBUTTON2;  // XButton2
    default: return 0;           // Invalid button index
    }
}

// UpdateMouseState() should be called once per frame.
// It copies the current state into previous state, then queries the current state.
void RaylibDesktopUpdateMouseState(void)
{
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        prevMouseState[i] = currMouseState[i];
        int vk = GetVirtualKeyForMouseButton(i);
        if (vk != 0)
        {
            // GetAsyncKeyState returns a SHORT. The high-order bit is set if the key is currently down.
            currMouseState[i] = ((GetAsyncKeyState(vk) & 0x8000) != 0);
        }
        else
        {
            currMouseState[i] = false;
        }
    }
}

// Returns true if the mouse button was pressed this frame (down now but was up previously)
bool RaylibDesktopIsMouseButtonPressed(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return (currMouseState[button] && !prevMouseState[button]);
}

// Returns true if the mouse button is currently held down
bool RaylibDesktopIsMouseButtonDown(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return currMouseState[button];
}

// Returns true if the mouse button was released this frame (up now but was down previously)
bool RaylibDesktopIsMouseButtonReleased(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return (!currMouseState[button] && prevMouseState[button]);
}

// Returns true if the mouse button is currently up
bool RaylibDesktopIsMouseButtonUp(int button)
{
    if (button < 0 || button >= MOUSE_BUTTON_COUNT)
        return false;
    return !currMouseState[button];
}

bool GetRelativeCursorPos(POINT *p)
{
    int virtualScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int virtualScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);

	if (GetCursorPos(p))
	{
		p->x -= virtualScreenX;
		p->y -= virtualScreenY;
		return true;
	}

	return false;
}

// GetMouseX() and GetMouseY() return the global cursor position in physical pixels.
int RaylibDesktopGetMouseX(void)
{
    POINT p;
    if (GetRelativeCursorPos(&p))
        return p.x;
    return 0;
}

int RaylibDesktopGetMouseY(void)
{
    POINT p;
    if (GetRelativeCursorPos(&p))
        return p.y;
    return 0;
}

// GetMousePosition() returns a Vector2 with the cursor's x and y coordinates.
Vector2 RaylibDesktopGetMousePosition(void)
{
    POINT p;
    Vector2 pos = { 0.0f, 0.0f };
    if (GetRelativeCursorPos(&p))
    {
        pos.x = (float)p.x;
        pos.y = (float)p.y;
    }
    return pos;
}