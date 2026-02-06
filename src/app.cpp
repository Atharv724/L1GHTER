#include "app.h"

#include <Shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <filesystem>
#include <sstream>

#include "logging.h"
#include "util.h"

namespace {
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr int HOTKEY_SAVE = 1;

constexpr int IDC_CLIP_SECONDS = 101;
constexpr int IDC_AUDIO_SYSTEM = 102;
constexpr int IDC_AUDIO_MIC = 103;
constexpr int IDC_SAVE_NOW = 104;

const wchar_t* kMainClass = L"L1GHTERMain";
}

bool LighterApp::Initialize(HINSTANCE instance) {
    instance_ = instance;
    configPath_ = GetExecutableDir() / L"config.ini";
    config_.Load(configPath_);

    Logger::Instance().Initialize((GetExecutableDir() / L"logs" / L"lighter.log").wstring());

    WNDCLASSW wc{};
    wc.lpfnWndProc = LighterApp::WndProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kMainClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    CreateMainWindow();

    tray_.Initialize(hwnd_, WM_TRAY);
    tray_.Show();

    hotkey_.Register(hwnd_, HOTKEY_SAVE, config_.hotkeyModifiers, config_.hotkeyVirtualKey);

    watcher_.Start(L"RobloxPlayerBeta.exe",
                   [this](DWORD pid, HWND hwnd) { StartCapture(pid, hwnd); },
                   [this]() { StopCapture(); });

    return true;
}

int LighterApp::Run() {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void LighterApp::CreateMainWindow() {
    hwnd_ = CreateWindowExW(0, kMainClass, L"L1GHTER", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 640, 420,
                            nullptr, nullptr, instance_, this);
    ShowWindow(hwnd_, SW_HIDE);
}

void LighterApp::UpdateUiState() {
    std::wstringstream ss;
    ss << L"L1GHTER - " << (capturing_ ? L"Recording" : L"Idle");
    tray_.UpdateTooltip(ss.str());
}

void LighterApp::SetupStartupRegistry(bool enable) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }
    if (enable) {
        std::wstring exePath = (GetExecutableDir() / L"L1GHTER.exe").wstring();
        RegSetValueExW(key, L"L1GHTER", 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath.c_str()),
                       static_cast<DWORD>((exePath.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, L"L1GHTER");
    }
    RegCloseKey(key);
}

void LighterApp::StartCapture(DWORD pid, HWND hwnd) {
    if (capturing_) {
        return;
    }
    targetPid_ = pid;
    targetWindow_ = hwnd;

    if (!duplicator_.Initialize(0)) {
        Logger::Instance().Error(L"Failed to initialize desktop duplication.");
        return;
    }

    DXGI_OUTPUT_DESC desc = duplicator_.OutputDesc();
    UINT width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
    UINT height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

    if (!encoder_.Initialize(duplicator_.Device(), width, height, config_.targetFps)) {
        Logger::Instance().Error(L"Failed to initialize encoder.");
        return;
    }

    if (config_.captureSystemAudio || config_.captureMicrophone) {
        audio_.Initialize(config_.captureSystemAudio, config_.captureMicrophone);
        audio_.Buffer().SetMaxDurationHns(config_.clipSeconds * 10000000LL);
        audio_.Start();
    }

    videoBuffer_.SetMaxDurationHns(config_.clipSeconds * 10000000LL);

    overlay_.Start(hwnd);
    overlay_.ShowMessage(L"L1GHTER started", 2500);

    capturing_ = true;
    captureThread_ = std::thread(&LighterApp::CaptureLoop, this);
    UpdateUiState();
}

void LighterApp::StopCapture() {
    capturing_ = false;
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    audio_.Stop();
    audio_.Buffer().Clear();
    videoBuffer_.Clear();
    encoder_.Shutdown();
    duplicator_.Release();
    overlay_.Stop();
    UpdateUiState();
}

void LighterApp::CaptureLoop() {
    SetThreadName(L"CaptureLoop");
    auto frameDuration = std::chrono::milliseconds(1000 / config_.targetFps);
    while (capturing_) {
        if (targetWindow_ && IsIconic(targetWindow_)) {
            Sleep(100);
            continue;
        }
        auto frame = duplicator_.CaptureFrame(5);
        if (!frame) {
            Sleep(1);
            continue;
        }

        EncodedSample encoded{};
        if (encoder_.EncodeFrame(frame->texture.Get(), frame->timestampHns, encoded)) {
            videoBuffer_.Push(std::move(encoded));
        }
        std::this_thread::sleep_for(frameDuration);
    }
}

void LighterApp::SaveClip() {
    auto videoSamples = videoBuffer_.Snapshot();
    if (videoSamples.empty()) {
        return;
    }
    std::filesystem::create_directories(config_.outputDir);
    auto filePath = config_.outputDir / (L"L1GHTER_" + NowTimeString() + L".mp4");

    Mp4Writer writer;
    IMFMediaType* audioInput = nullptr;
    IMFMediaType* audioOutput = nullptr;

    auto audioSamples = audio_.Buffer().Snapshot();
    if (!audioSamples.empty() && audio_.InputType() && audio_.OutputType()) {
        audioInput = audio_.InputType();
        audioOutput = audio_.OutputType();
    }

    if (!writer.Initialize(filePath.wstring(), encoder_.OutputType(), encoder_.OutputType(), audioInput, audioOutput)) {
        Logger::Instance().Error(L"Failed to initialize MP4 writer.");
        return;
    }

    LONGLONG baseTime = videoSamples.front().timestampHns;
    for (auto& sample : videoSamples) {
        writer.WriteVideoSample(sample.sample.Get(), sample.timestampHns - baseTime);
    }

    if (!audioSamples.empty() && audioInput && audioOutput) {
        LONGLONG audioBase = audioSamples.front().timestampHns;
        for (auto& sample : audioSamples) {
            writer.WriteAudioSample(sample.sample.Get(), sample.timestampHns - audioBase);
        }
    }

    writer.Finalize();
    overlay_.ShowMessage(L"Clip saved", 2000);
}

LRESULT CALLBACK LighterApp::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LighterApp* app = reinterpret_cast<LighterApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto create = reinterpret_cast<CREATESTRUCT*>(lparam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"L1GHTER", WS_CHILD | WS_VISIBLE,
                          24, 20, 200, 30, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"STATIC", L"Clip length (seconds)", WS_CHILD | WS_VISIBLE,
                          24, 70, 200, 20, hwnd, nullptr, nullptr, nullptr);
            std::wstring secondsText = app ? std::to_wstring(app->config_.clipSeconds) : L"30";
            HWND edit = CreateWindowW(L"EDIT", secondsText.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                      24, 92, 80, 24, hwnd, reinterpret_cast<HMENU>(IDC_CLIP_SECONDS), nullptr, nullptr);
            SendMessageW(edit, EM_SETLIMITTEXT, 3, 0);

            HWND chkSystem = CreateWindowW(L"BUTTON", L"Record system audio", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                           24, 130, 200, 24, hwnd, reinterpret_cast<HMENU>(IDC_AUDIO_SYSTEM), nullptr, nullptr);
            SendMessageW(chkSystem, BM_SETCHECK, app && app->config_.captureSystemAudio ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND chkMic = CreateWindowW(L"BUTTON", L"Record microphone", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                        24, 160, 200, 24, hwnd, reinterpret_cast<HMENU>(IDC_AUDIO_MIC), nullptr, nullptr);
            SendMessageW(chkMic, BM_SETCHECK, app && app->config_.captureMicrophone ? BST_CHECKED : BST_UNCHECKED, 0);

            CreateWindowW(L"BUTTON", L"Save clip now", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          24, 210, 160, 32, hwnd, reinterpret_cast<HMENU>(IDC_SAVE_NOW), nullptr, nullptr);
            return 0;
        }
        case WM_TRAY:
            if (lparam == WM_LBUTTONUP) {
                ShowWindow(hwnd, SW_SHOW);
            } else if (lparam == WM_RBUTTONUP) {
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, 1, L"Open L1GHTER");
                AppendMenuW(menu, MF_STRING, 2, L"Save Clip");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, 3, L"Exit");
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                if (cmd == 1) {
                    ShowWindow(hwnd, SW_SHOW);
                } else if (cmd == 2) {
                    if (app) {
                        app->SaveClip();
                    }
                } else if (cmd == 3) {
                    PostQuitMessage(0);
                }
            }
            break;
        case WM_HOTKEY:
            if (wparam == HOTKEY_SAVE && app) {
                app->SaveClip();
            }
            break;
        case WM_COMMAND: {
            if (!app) {
                break;
            }
            const int id = LOWORD(wparam);
            if (id == IDC_SAVE_NOW) {
                app->SaveClip();
            } else if (id == IDC_AUDIO_SYSTEM) {
                app->config_.captureSystemAudio = (SendMessageW(reinterpret_cast<HWND>(lparam), BM_GETCHECK, 0, 0) == BST_CHECKED);
                app->config_.Save(app->configPath_);
            } else if (id == IDC_AUDIO_MIC) {
                app->config_.captureMicrophone = (SendMessageW(reinterpret_cast<HWND>(lparam), BM_GETCHECK, 0, 0) == BST_CHECKED);
                app->config_.Save(app->configPath_);
            } else if (id == IDC_CLIP_SECONDS && HIWORD(wparam) == EN_CHANGE) {
                wchar_t buffer[8] = {};
                GetWindowTextW(reinterpret_cast<HWND>(lparam), buffer, 8);
                int seconds = _wtoi(buffer);
                if (seconds >= 5 && seconds <= 300) {
                    app->config_.clipSeconds = seconds;
                    app->config_.Save(app->configPath_);
                    app->videoBuffer_.SetMaxDurationHns(app->config_.clipSeconds * 10000000LL);
                    app->audio_.Buffer().SetMaxDurationHns(app->config_.clipSeconds * 10000000LL);
                }
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN: {
            HDC hdc = reinterpret_cast<HDC>(wparam);
            SetTextColor(hdc, RGB(220, 220, 230));
            SetBkColor(hdc, RGB(20, 20, 24));
            static HBRUSH brush = CreateSolidBrush(RGB(20, 20, 24));
            return reinterpret_cast<INT_PTR>(brush);
        }
        case WM_ERASEBKGND: {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            HDC hdc = reinterpret_cast<HDC>(wparam);
            HBRUSH brush = CreateSolidBrush(RGB(20, 20, 24));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
            return 1;
        }
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
