#include "InputSystem.h"

#pragma comment(lib, "gameinput.lib")

bool InputSystem::Initialize()
{
    if (m_gameInput)
        return true;

    const HRESULT hr = GI::GameInputCreate(
        m_gameInput.GetAddressOf());

    return SUCCEEDED(hr);
}

InputSystem::~InputSystem()
{
    Shutdown();
}

void InputSystem::Shutdown()
{
    m_gamepadDevice.Reset();
    m_mouseDevice.Reset();
    m_keyboardDevice.Reset();

    m_gameInput.Reset();

    m_currentKeys.reset();
    m_previousKeys.reset();

    m_mouse = {};
    m_previousMouse = {};
    m_hasMouseState = false;

    m_gamepad = {};
    m_previousGamepad = {};
    m_gamepadConnected = false;
}

void InputSystem::Update()
{
    if (!m_gameInput)
        return;

    UpdateKeyboard();
    UpdateMouse();
    UpdateGamepad();
}

void InputSystem::UpdateKeyboard()
{
    Microsoft::WRL::ComPtr<GI::IGameInputReading> reading;

    const HRESULT hr = m_gameInput->GetCurrentReading(
        GI::GameInputKindKeyboard,
        nullptr,
        reading.GetAddressOf());

    if (FAILED(hr))
    {
        char buffer[128];
        sprintf_s(
            buffer,
            "Keyboard GetCurrentReading failed: 0x%08X\n",
            static_cast<unsigned int>(hr));

        OutputDebugStringA(buffer);

        return;
    }

    // 次の状態を一時変数で構築
    std::bitset<256> nextKeys;

    const uint32_t keyCount = reading->GetKeyCount();

    m_keyBuffer.resize(keyCount);

    if (keyCount > 0)
    {
        const uint32_t validCount =
            reading->GetKeyState(
                keyCount,
                m_keyBuffer.data());

        for (uint32_t i = 0; i < validCount; ++i)
        {
            nextKeys.set(
                m_keyBuffer[i].virtualKey);
        }
    }

    // 正常にReadingを取得できたときだけcommit
    m_previousKeys = m_currentKeys;
    m_currentKeys = nextKeys;
}

bool InputSystem::IsKeyDown(uint8_t virtualKey) const
{
    return m_currentKeys.test(virtualKey);
}

bool InputSystem::IsKeyPressed(uint8_t virtualKey) const
{
    return
        m_currentKeys.test(virtualKey) &&
        !m_previousKeys.test(virtualKey);
}

bool InputSystem::IsKeyReleased(uint8_t virtualKey) const
{
    return
        !m_currentKeys.test(virtualKey) &&
        m_previousKeys.test(virtualKey);
}

void InputSystem::UpdateMouse()
{
    if (m_hasMouseState)
        m_previousMouse = m_mouse;

    Microsoft::WRL::ComPtr<GI::IGameInputReading> reading;

    const HRESULT hr = m_gameInput->GetCurrentReading(
        GI::GameInputKindMouse,
        m_mouseDevice.Get(),
        reading.GetAddressOf());

    if (FAILED(hr))
    {
        if (m_mouseDevice)
            m_mouseDevice.Reset();

        m_mouse = {};
        m_previousMouse = {};
        m_hasMouseState = false;

        return;
    }

    if (!m_mouseDevice)
    {
        reading->GetDevice(
            m_mouseDevice.GetAddressOf());
    }

    GI::GameInputMouseState state{};

    if (!reading->GetMouseState(&state))
        return;

    if (!m_hasMouseState)
    {
        // 最初のフレームで巨大な delta が出ないようにする
        m_mouse = state;
        m_previousMouse = state;
        m_hasMouseState = true;
        return;
    }

    m_mouse = state;
}

bool InputSystem::IsMouseDown(
    GI::GameInputMouseButtons button) const
{
    return
        (static_cast<uint32_t>(m_mouse.buttons) &
            static_cast<uint32_t>(button)) != 0;
}

bool InputSystem::IsMousePressed(
    GI::GameInputMouseButtons button) const
{
    const uint32_t current =
        static_cast<uint32_t>(m_mouse.buttons);

    const uint32_t previous =
        static_cast<uint32_t>(m_previousMouse.buttons);

    const uint32_t mask =
        static_cast<uint32_t>(button);

    return
        (current & mask) != 0 &&
        (previous & mask) == 0;
}

bool InputSystem::IsMouseReleased(
    GI::GameInputMouseButtons button) const
{
    const uint32_t current =
        static_cast<uint32_t>(m_mouse.buttons);

    const uint32_t previous =
        static_cast<uint32_t>(m_previousMouse.buttons);

    const uint32_t mask =
        static_cast<uint32_t>(button);

    return
        (current & mask) == 0 &&
        (previous & mask) != 0;
}

int64_t InputSystem::GetMouseDeltaX() const
{
    return m_mouse.positionX -
        m_previousMouse.positionX;
}

int64_t InputSystem::GetMouseDeltaY() const
{
    return m_mouse.positionY -
        m_previousMouse.positionY;
}

int64_t InputSystem::GetMouseWheelDeltaX() const
{
    return m_mouse.wheelX -
        m_previousMouse.wheelX;
}

int64_t InputSystem::GetMouseWheelDeltaY() const
{
    return m_mouse.wheelY -
        m_previousMouse.wheelY;
}

void InputSystem::UpdateGamepad()
{
    m_previousGamepad = m_gamepad;

    Microsoft::WRL::ComPtr<GI::IGameInputReading> reading;

    const HRESULT hr = m_gameInput->GetCurrentReading(
        GI::GameInputKindGamepad,
        m_gamepadDevice.Get(),
        reading.GetAddressOf());

    if (FAILED(hr))
    {
        m_gamepadDevice.Reset();

        m_gamepad = {};
        m_previousGamepad = {};
        m_gamepadConnected = false;

        return;
    }

    if (!m_gamepadDevice)
    {
        reading->GetDevice(
            m_gamepadDevice.GetAddressOf());
    }

    GI::GameInputGamepadState state{};

    if (!reading->GetGamepadState(&state))
        return;

    m_gamepad = state;
    m_gamepadConnected = true;
}

bool InputSystem::IsGamepadConnected() const
{
    return m_gamepadConnected;
}

bool InputSystem::IsGamepadDown(
    GI::GameInputGamepadButtons button) const
{
    return
        (static_cast<uint32_t>(m_gamepad.buttons) &
            static_cast<uint32_t>(button)) != 0;
}

bool InputSystem::IsGamepadPressed(
    GI::GameInputGamepadButtons button) const
{
    const uint32_t current =
        static_cast<uint32_t>(m_gamepad.buttons);

    const uint32_t previous =
        static_cast<uint32_t>(m_previousGamepad.buttons);

    const uint32_t mask =
        static_cast<uint32_t>(button);

    return
        (current & mask) != 0 &&
        (previous & mask) == 0;
}

bool InputSystem::IsGamepadReleased(
    GI::GameInputGamepadButtons button) const
{
    const uint32_t current =
        static_cast<uint32_t>(m_gamepad.buttons);

    const uint32_t previous =
        static_cast<uint32_t>(m_previousGamepad.buttons);

    const uint32_t mask =
        static_cast<uint32_t>(button);

    return
        (current & mask) == 0 &&
        (previous & mask) != 0;
}

const GI::GameInputGamepadState&
InputSystem::GetGamepadState() const
{
    return m_gamepad;
}
