#pragma once

#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    static Logger& Instance();

    void Initialize(const std::wstring& logPath);
    void Info(const std::wstring& message);
    void Error(const std::wstring& message);

private:
    void Write(const std::wstring& level, const std::wstring& message);

    std::wofstream stream_;
    std::mutex mutex_;
};
