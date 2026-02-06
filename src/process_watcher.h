#pragma once

#include <Windows.h>
#include <functional>
#include <thread>
#include <atomic>

class ProcessWatcher {
public:
    using Callback = std::function<void(DWORD pid, HWND hwnd)>;

    void Start(const wchar_t* processName, Callback onStart, std::function<void()> onExit);
    void Stop();

private:
    void Run();
    bool FindProcess(DWORD& pid, HWND& hwnd);

    std::wstring processName_;
    Callback onStart_;
    std::function<void()> onExit_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    DWORD activePid_ = 0;
};
