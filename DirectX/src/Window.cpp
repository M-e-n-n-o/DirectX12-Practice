#include "dxpch.h"
#include "Window.h"

Window::Window(const WindowSettings& settings, WNDPROC windowCallback, bool tearingSupported)
{
    m_tearingSupported = tearingSupported;

    const wchar_t* windowClassName = L"DX12WindowClass";

    registerWindowClass(settings.hInstance, windowClassName, windowCallback);
    createWindow(windowClassName, settings.hInstance, settings.title, settings.width, settings.height);

    // Initialize the global window rect variable.
    ::GetWindowRect(m_windowHandle, &m_windowRect);
    m_width = m_windowRect.right - m_windowRect.left;
    m_height = m_windowRect.bottom - m_windowRect.top;
}

void Window::show()
{
    ::ShowWindow(m_windowHandle, SW_SHOW);
}

void Window::onResize()
{
    RECT clientRect = {};
    ::GetClientRect(m_windowHandle, &m_windowRect);

    uint32_t width = clientRect.right - clientRect.left;
    uint32_t height = clientRect.bottom - clientRect.top;

    if (m_width != width || m_height != height)
    {
        // Don't allow 0 size swap chain back buffers.
        m_width = std::max(1u, width);
        m_height = std::max(1u, height);

        // Flush the GPU queue to make sure the swap chain's back buffers
        // are not being referenced by an in-flight command list.
        //Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

        m_swapChain->resize(m_width, m_height);
    }
}

void Window::setFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen)
    {
        return;
    }

    m_fullscreen = fullscreen;

    if (m_fullscreen)
    {
        // Store the current window dimensions so they can be restored 
        // when switching out of fullscreen state.
        ::GetWindowRect(m_windowHandle, &m_windowRect);

        // Set the window style to a borderless window so the client area fills the entire screen.
        UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

        ::SetWindowLongW(m_windowHandle, GWL_STYLE, windowStyle);

        // Query the name of the nearest display device for the window.
        // This is required to set the fullscreen dimensions of the window
        // when using a multi-monitor setup.
        HMONITOR hMonitor = ::MonitorFromWindow(m_windowHandle, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        ::GetMonitorInfo(hMonitor, &monitorInfo);

        ::SetWindowPos(m_windowHandle, HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(m_windowHandle, SW_MAXIMIZE);
    }
    else
    {
        // Restore all the window decorators.
        ::SetWindowLong(m_windowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

        ::SetWindowPos(m_windowHandle, HWND_NOTOPMOST,
            m_windowRect.left,
            m_windowRect.top,
            m_windowRect.right - m_windowRect.left,
            m_windowRect.bottom - m_windowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(m_windowHandle, SW_NORMAL);
    }
}

void Window::registerWindowClass(HINSTANCE hInstance, const wchar_t* windowClassName, WNDPROC windowCallback)
{
    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowCallback;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = ::LoadIcon(hInstance, NULL);
    windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = windowClassName;
    windowClass.hIconSm = ::LoadIcon(hInstance, NULL);

    static ATOM atom = ::RegisterClassExW(&windowClass);
    assert(atom > 0);
}

void Window::createWindow(const wchar_t* windowClassName, HINSTANCE hInstance, const wchar_t* appName, uint32_t width, uint32_t height)
{
    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    m_windowHandle = ::CreateWindowExW(
        NULL,
        windowClassName,
        appName,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        NULL,
        NULL,
        hInstance,
        nullptr
    );

    assert(m_windowHandle && "Failed to create window");
}
