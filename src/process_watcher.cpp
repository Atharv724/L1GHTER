#include "process_watcher.h"

#include <TlHelp32.h>
#include <string>
#include <chrono>

namespace {
BOOL CALLBACK EnumWindowProc(HWND hwnd, LPARAM lParam) {
    auto target = reinterpret_cast<std::pair<DWORD, HWND*>*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == target->first) {
        if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
            *target->second = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}
}

void ProcessWatcher::Start(const wchar_t* processName, Callback onStart, std::function<void()> onExit) {
    processName_ = processName;
    onStart_ = std::move(onStart);
    onExit_ = std::move(onExit);
    running_ = true;
    thread_ = std::thread(&ProcessWatcher::Run, this);
}

void ProcessWatcher::Stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ProcessWatcher::Run() {
    while (running_) {
        DWORD pid = 0;
        HWND hwnd = nullptr;
        bool found = FindProcess(pid, hwnd);
        if (found && activePid_ == 0) {
            activePid_ = pid;
            if (onStart_) {
                onStart_(pid, hwnd);
            }
        } else if (!found && activePid_ != 0) {
            activePid_ = 0;
            if (onExit_) {
                onExit_();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool ProcessWatcher::FindProcess(DWORD& pid, HWND& hwnd) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName_.c_str()) == 0) {
                pid = entry.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (found) {
        HWND window = nullptr;
        std::pair<DWORD, HWND*> data{pid, &window};
        EnumWindows(EnumWindowProc, reinterpret_cast<LPARAM>(&data));
        hwnd = window;
    }

    return found;
}
