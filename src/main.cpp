#include <Windows.h>
#include <mfapi.h>

#include "app.h"
#include "logging.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);

    LighterApp app;
    if (!app.Initialize(hInstance)) {
        Logger::Instance().Error(L"Failed to initialize L1GHTER.");
        return -1;
    }

    int result = app.Run();

    MFShutdown();
    CoUninitialize();
    return result;
}
