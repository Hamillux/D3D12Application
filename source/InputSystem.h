#pragma once

#include "Singleton.h"

#include <GameInput.h>
#include <wrl/client.h>

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

namespace GI = GameInput::v3;

class InputSystem final : public Singleton<InputSystem>
{
    friend class Singleton<InputSystem>;

public:
    bool Initialize();
    void Shutdown();

    // 1フレームに1回呼ぶ
    void Update();

    // --------------------------------
    // Keyboard
    // --------------------------------

    bool IsKeyDown(uint8_t virtualKey) const;
    bool IsKeyPressed(uint8_t virtualKey) const;
    bool IsKeyReleased(uint8_t virtualKey) const;

    // --------------------------------
    // Mouse
    // --------------------------------

    bool IsMouseDown(GI::GameInputMouseButtons button) const;
    bool IsMousePressed(GI::GameInputMouseButtons button) const;
    bool IsMouseReleased(GI::GameInputMouseButtons button) const;

    int64_t GetMouseDeltaX() const;
    int64_t GetMouseDeltaY() const;

    int64_t GetMouseWheelDeltaX() const;
    int64_t GetMouseWheelDeltaY() const;

    // --------------------------------
    // Gamepad
    // --------------------------------

    bool IsGamepadConnected() const;

    bool IsGamepadDown(GI::GameInputGamepadButtons button) const;
    bool IsGamepadPressed(GI::GameInputGamepadButtons button) const;
    bool IsGamepadReleased(GI::GameInputGamepadButtons button) const;

    const GI::GameInputGamepadState& GetGamepadState() const;

private:
    InputSystem() = default;
    ~InputSystem();

    void UpdateKeyboard();
    void UpdateMouse();
    void UpdateGamepad();

private:
    Microsoft::WRL::ComPtr<GI::IGameInput> m_gameInput;

    // Keyboard
    Microsoft::WRL::ComPtr<GI::IGameInputDevice> m_keyboardDevice;

    std::bitset<256> m_currentKeys;
    std::bitset<256> m_previousKeys;

    std::vector<GI::GameInputKeyState> m_keyBuffer;

    // Mouse
    Microsoft::WRL::ComPtr<GI::IGameInputDevice> m_mouseDevice;

    GI::GameInputMouseState m_mouse{};
    GI::GameInputMouseState m_previousMouse{};

    bool m_hasMouseState = false;

    // Gamepad
    Microsoft::WRL::ComPtr<GI::IGameInputDevice> m_gamepadDevice;

    GI::GameInputGamepadState m_gamepad{};
    GI::GameInputGamepadState m_previousGamepad{};

    bool m_gamepadConnected = false;
};
