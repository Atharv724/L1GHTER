#include "tray_icon.h"

#include <shellapi.h>

bool TrayIcon::Initialize(HWND hwnd, UINT callbackMessage) {
    data_.cbSize = sizeof(data_);
    data_.hWnd = hwnd;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(data_.szTip, L"L1GHTER");
    return true;
}

void TrayIcon::Show() {
    if (!visible_) {
        Shell_NotifyIconW(NIM_ADD, &data_);
        visible_ = true;
    }
}

void TrayIcon::Hide() {
    if (visible_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        visible_ = false;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring& text) {
    wcsncpy_s(data_.szTip, text.c_str(), _TRUNCATE);
    if (visible_) {
        Shell_NotifyIconW(NIM_MODIFY, &data_);
    }
}
