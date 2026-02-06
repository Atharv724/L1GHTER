#pragma once

#include <string>
#include <filesystem>

struct AppConfig {
    int clipSeconds = 30;
    int targetFps = 60;
    bool captureSystemAudio = true;
    bool captureMicrophone = false;
    std::filesystem::path outputDir;
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT hotkeyVirtualKey = 'S';

    void Load(const std::filesystem::path& path);
    void Save(const std::filesystem::path& path) const;
};
