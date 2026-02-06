#pragma once

#include <Windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <filesystem>

#include "config.h"
#include "process_watcher.h"
#include "desktop_duplicator.h"
#include "encoder.h"
#include "audio_capture.h"
#include "overlay_window.h"
#include "tray_icon.h"
#include "hotkey.h"
#include "ring_buffer.h"

class LighterApp {
public:
    bool Initialize(HINSTANCE instance);
    int Run();

private:
    void StartCapture(DWORD pid, HWND hwnd);
    void StopCapture();
    void CaptureLoop();
    void SaveClip();

    void CreateMainWindow();
    void UpdateUiState();
    void SetupStartupRegistry(bool enable);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    TrayIcon tray_;
    HotkeyManager hotkey_;
    ProcessWatcher watcher_;
    OverlayWindow overlay_;

    AppConfig config_;
    std::filesystem::path configPath_;

    DesktopDuplicator duplicator_;
    VideoEncoder encoder_;
    AudioCapture audio_;
    TimedRingBuffer<EncodedSample> videoBuffer_;

    std::thread captureThread_;
    std::atomic<bool> capturing_{false};
    HWND targetWindow_ = nullptr;
    DWORD targetPid_ = 0;
};
