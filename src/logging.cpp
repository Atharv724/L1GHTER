#include "logging.h"

#include <Windows.h>
#include <filesystem>
#include <iomanip>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::wstring& logPath) {
    std::scoped_lock lock(mutex_);
    std::filesystem::create_directories(std::filesystem::path(logPath).parent_path());
    stream_.open(logPath, std::ios::out | std::ios::app);
}

void Logger::Info(const std::wstring& message) {
    Write(L"INFO", message);
}

void Logger::Error(const std::wstring& message) {
    Write(L"ERROR", message);
}

void Logger::Write(const std::wstring& level, const std::wstring& message) {
    std::scoped_lock lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }
    SYSTEMTIME st{};
    GetLocalTime(&st);
    stream_ << L"[" << std::setfill(L'0') << std::setw(2) << st.wHour << L":"
            << std::setw(2) << st.wMinute << L":" << std::setw(2) << st.wSecond << L"] "
            << level << L" " << message << L"\n";
    stream_.flush();
}
