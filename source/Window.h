#pragma once

#include <Windows.h>

#include <cstdint>

class Window final
{
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void Initialize(
        HINSTANCE instance,
        int showCommand,
        std::uint32_t clientWidth,
        std::uint32_t clientHeight,
        const wchar_t* title);

    bool ProcessMessages() const;
    bool ConsumeResize(std::uint32_t& width, std::uint32_t& height) noexcept;

    [[nodiscard]] HWND GetHandle() const noexcept { return _handle; }
    [[nodiscard]] std::uint32_t GetClientWidth() const noexcept { return _clientWidth; }
    [[nodiscard]] std::uint32_t GetClientHeight() const noexcept { return _clientHeight; }
    [[nodiscard]] bool IsMinimized() const noexcept { return _minimized; }

private:
    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    static constexpr wchar_t ClassName[] = L"D3D12ApplicationWindowClass";

    HINSTANCE _instance = nullptr;
    HWND _handle = nullptr;
    std::uint32_t _clientWidth = 0;
    std::uint32_t _clientHeight = 0;
    bool _resizePending = false;
    bool _minimized = false;
    bool _classRegistered = false;
};
