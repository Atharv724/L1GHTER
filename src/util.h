#pragma once

#include <Windows.h>
#include <wil/com.h>
#include <string>
#include <chrono>
#include <filesystem>

inline std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), result.data(), size);
    return result;
}

inline std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), result.data(), size, nullptr, nullptr);
    return result;
}

inline std::filesystem::path GetExecutableDir() {
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

inline std::wstring NowTimeString() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04d-%02d-%02d_%02d-%02d-%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

inline void SetThreadName(const wchar_t* name) {
    using SetThreadDescription_t = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    auto setThreadDescription = reinterpret_cast<SetThreadDescription_t>(GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "SetThreadDescription"));
    if (setThreadDescription) {
        setThreadDescription(GetCurrentThread(), name);
    }
}

inline std::chrono::steady_clock::time_point NowSteady() {
    return std::chrono::steady_clock::now();
}

inline LONGLONG ToHns(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 100;
}

inline LONGLONG HnsFromMillis(int64_t ms) {
    return ms * 10000;
}

