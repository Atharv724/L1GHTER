#include "config.h"

#include <fstream>
#include <sstream>
#include <Windows.h>

void AppConfig::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        outputDir = std::filesystem::path(_wgetenv(L"USERPROFILE")) / L"Videos" / L"L1GHTER";
        return;
    }
    std::wifstream file(path);
    std::wstring line;
    while (std::getline(file, line)) {
        std::wistringstream iss(line);
        std::wstring key;
        if (std::getline(iss, key, L'=')) {
            std::wstring value;
            if (std::getline(iss, value)) {
                if (key == L"clipSeconds") {
                    clipSeconds = std::stoi(value);
                } else if (key == L"targetFps") {
                    targetFps = std::stoi(value);
                } else if (key == L"captureSystemAudio") {
                    captureSystemAudio = value == L"1";
                } else if (key == L"captureMicrophone") {
                    captureMicrophone = value == L"1";
                } else if (key == L"outputDir") {
                    outputDir = value;
                }
            }
        }
    }
    if (outputDir.empty()) {
        outputDir = std::filesystem::path(_wgetenv(L"USERPROFILE")) / L"Videos" / L"L1GHTER";
    }
}

void AppConfig::Save(const std::filesystem::path& path) const {
    std::wofstream file(path, std::ios::trunc);
    file << L"clipSeconds=" << clipSeconds << L"\n";
    file << L"targetFps=" << targetFps << L"\n";
    file << L"captureSystemAudio=" << (captureSystemAudio ? 1 : 0) << L"\n";
    file << L"captureMicrophone=" << (captureMicrophone ? 1 : 0) << L"\n";
    file << L"outputDir=" << outputDir.wstring() << L"\n";
}
