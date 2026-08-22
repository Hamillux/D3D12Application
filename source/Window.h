#pragma once

#include <Windows.h>
#include <vector>
#include <cstdint>
#include <optional>
#include <stdexcept>

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

	using HandleMessageFunction = std::optional<LRESULT>(*)(HWND, UINT, WPARAM, LPARAM);
	using HandleMessageFunctionWithContext = std::optional<LRESULT>(*)(void*, HWND, UINT, WPARAM, LPARAM);
    using MessageHandlerId = uint64_t;
    static constexpr MessageHandlerId NULL_MESSAGE_HANDLER_ID = 0;

    MessageHandlerId RegisterMessageHandler(void* context, HandleMessageFunctionWithContext msgHandler)
    {
        if (_iteratingMessageHandlers)
        {
            throw std::runtime_error("Register during message handlers iteration is not supported");
        }

        if (!context || !msgHandler)
        {
            throw std::invalid_argument("");
        }
        const MessageHandlerId id = _nextMsgHandlerId++;
        _messageHandlers.emplace_back(id, context, msgHandler);
        return id;
    }

    MessageHandlerId RegisterMessageHandler(HandleMessageFunction msgHandler)
    {
        if (_iteratingMessageHandlers)
        {
            throw std::runtime_error("Register during message handlers iteration is not supported");
        }

        if (!msgHandler)
        {
            throw std::invalid_argument("");
        }

        const MessageHandlerId id = _nextMsgHandlerId++;
        _messageHandlers.emplace_back(id, msgHandler);
        return id;
    }

    void UnregisterMessageHandler(MessageHandlerId id)
    {
        if (_iteratingMessageHandlers)
        {
            throw std::runtime_error("UnRegister during message handlers iteration is not supported");
        }

        for (auto it = _messageHandlers.begin(); it != _messageHandlers.end(); ++it)
        {
            if (it->_id == id)
            {
                _messageHandlers.erase(it);
                return;
            }
        }
    }

    template<
        typename T,
        std::optional<LRESULT>(T::* Method)(HWND, UINT, WPARAM, LPARAM)>
    MessageHandlerId RegisterHandler(T* instance)
    {
        if (_iteratingMessageHandlers)
        {
            throw std::runtime_error("Register during message handlers iteration is not supported");
        }

        return RegisterMessageHandler(
            instance,
            [](void* context, HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
            -> std::optional<LRESULT>
        {
            T* obj = static_cast<T*>(context);
            return (obj->*Method)(hwnd, msg, wparam, lparam);
        }
        );
    }

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

    class MessageHandler
    {
    public:
        MessageHandler(MessageHandlerId id, HandleMessageFunction funcNoContext)
			: _id{ id }
            , _context{ nullptr }
            , _funcWithContext{ nullptr }
			, _funcNoContext{ funcNoContext }
        {}

        MessageHandler(MessageHandlerId id, void* context, HandleMessageFunctionWithContext funcWithContext)
            : _id{ id }
            , _context{ context }
			, _funcWithContext{ funcWithContext }
            , _funcNoContext{ nullptr }
        {}

        std::optional<LRESULT> HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) const
        {
            if (_context)
            {
                return _funcWithContext(_context, hwnd, message, wParam, lParam);
            }
            else
            {
                return _funcNoContext(hwnd, message, wParam, lParam);
            }
        }

        MessageHandlerId _id;
    private:
        void* _context = nullptr;
        HandleMessageFunctionWithContext _funcWithContext;
        HandleMessageFunction _funcNoContext;
    };
    std::vector<MessageHandler> _messageHandlers;
    bool _iteratingMessageHandlers = false;
    MessageHandlerId _nextMsgHandlerId = NULL_MESSAGE_HANDLER_ID + 1;
};
