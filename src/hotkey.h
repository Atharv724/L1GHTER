#pragma once

#include <Windows.h>

class HotkeyManager {
public:
    bool Register(HWND hwnd, int id, UINT modifiers, UINT vk);
    void Unregister(HWND hwnd, int id);
};
