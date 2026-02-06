#include "hotkey.h"

bool HotkeyManager::Register(HWND hwnd, int id, UINT modifiers, UINT vk) {
    return RegisterHotKey(hwnd, id, modifiers, vk) != FALSE;
}

void HotkeyManager::Unregister(HWND hwnd, int id) {
    UnregisterHotKey(hwnd, id);
}
