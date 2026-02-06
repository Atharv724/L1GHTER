#pragma once

#include <Windows.h>
#include <string>

class TrayIcon {
public:
    bool Initialize(HWND hwnd, UINT callbackMessage);
    void Show();
    void Hide();
    void UpdateTooltip(const std::wstring& text);

private:
    NOTIFYICONDATAW data_{};
    bool visible_ = false;
};
