#include "Window.h"

#include <system_error>

namespace
{
[[noreturn]] void ThrowLastError(const char* operation)
{
    throw std::system_error(
        static_cast<int>(GetLastError()),
        std::system_category(),
        operation);
}
}

Window::~Window()
{
    if (_handle != nullptr && IsWindow(_handle))
    {
        DestroyWindow(_handle);
    }

    if (_classRegistered)
    {
        UnregisterClassW(ClassName, _instance);
    }
}

void Window::Initialize(
    HINSTANCE instance,
    int showCommand,
    std::uint32_t clientWidth,
    std::uint32_t clientHeight,
    const wchar_t* title)
{
    _instance = instance;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = _instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = ClassName;

    if (RegisterClassExW(&windowClass) == 0)
    {
        ThrowLastError("RegisterClassExW");
    }
    _classRegistered = true;

    constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    RECT windowRectangle{
        0,
        0,
        static_cast<LONG>(clientWidth),
        static_cast<LONG>(clientHeight),
    };

    if (!AdjustWindowRectEx(&windowRectangle, windowStyle, FALSE, 0))
    {
        ThrowLastError("AdjustWindowRectEx");
    }

    _handle = CreateWindowExW(
        0,
        ClassName,
        title,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRectangle.right - windowRectangle.left,
        windowRectangle.bottom - windowRectangle.top,
        nullptr,
        nullptr,
        _instance,
        this);

    if (_handle == nullptr)
    {
        ThrowLastError("CreateWindowExW");
    }

    RECT clientRectangle{};
    if (!GetClientRect(_handle, &clientRectangle))
    {
        ThrowLastError("GetClientRect");
    }

    _clientWidth = static_cast<std::uint32_t>(clientRectangle.right - clientRectangle.left);
    _clientHeight = static_cast<std::uint32_t>(clientRectangle.bottom - clientRectangle.top);
    _resizePending = false;

    ShowWindow(_handle, showCommand);
    UpdateWindow(_handle);
}

bool Window::ProcessMessages() const
{
    MSG message{};

    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return true;
}

bool Window::ConsumeResize(std::uint32_t& width, std::uint32_t& height) noexcept
{
    if (!_resizePending)
    {
        return false;
    }

    _resizePending = false;
    width = _clientWidth;
    height = _clientHeight;
    return true;
}

LRESULT CALLBACK Window::WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    Window* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(create->lpCreateParams);
        self->_handle = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    const LRESULT result = self->HandleMessage(message, wParam, lParam);

    if (message == WM_NCDESTROY)
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        self->_handle = nullptr;
    }

    return result;
}

LRESULT Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
        DestroyWindow(_handle);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        _minimized = wParam == SIZE_MINIMIZED;

        if (!_minimized)
        {
            const std::uint32_t width = LOWORD(lParam);
            const std::uint32_t height = HIWORD(lParam);

            if (width > 0 && height > 0)
            {
                _clientWidth = width;
                _clientHeight = height;
                _resizePending = true;
            }
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(_handle, message, wParam, lParam);
    }
}
