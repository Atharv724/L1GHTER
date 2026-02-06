#include "overlay_window.h"

#include <dwmapi.h>
#include <chrono>

#include "logging.h"
#include "util.h"

using Microsoft::WRL::ComPtr;

namespace {
const wchar_t* kOverlayClassName = L"L1GHTEROverlay";
}

bool OverlayWindow::Start(HWND targetWindow) {
    if (running_) {
        return false;
    }
    targetWindow_ = targetWindow;
    running_ = true;
    thread_ = std::thread(&OverlayWindow::ThreadMain, this);
    return true;
}

void OverlayWindow::Stop() {
    running_ = false;
    if (hwnd_) {
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void OverlayWindow::UpdateTarget(HWND targetWindow) {
    targetWindow_ = targetWindow;
    UpdateWindowPosition();
}

void OverlayWindow::ShowMessage(const std::wstring& text, int durationMs) {
    std::scoped_lock lock(messageMutex_);
    message_ = text;
    messageDurationMs_ = durationMs;
    messageStart_ = GetTickCount64();
}

void OverlayWindow::ThreadMain() {
    SetThreadName(L"OverlayWindow");

    WNDCLASSW wc{};
    wc.lpfnWndProc = OverlayWindow::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kOverlayClassName;
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                            kOverlayClassName, L"", WS_POPUP, 0, 0, 100, 100,
                            nullptr, nullptr, GetModuleHandle(nullptr), this);

    if (!hwnd_) {
        running_ = false;
        return;
    }

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    if (!InitializeD3D() || !InitializeD2D()) {
        running_ = false;
        return;
    }

    ShowWindow(hwnd_, SW_SHOWNA);

    MSG msg{};
    while (running_) {
        while (PeekMessage(&msg, hwnd_, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Render();
        Sleep(16);
    }
}

bool OverlayWindow::InitializeD3D() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2,
                                 D3D11_SDK_VERSION, &device_, &level, &context_))) {
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    device_.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 0;
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    if (FAILED(factory->CreateSwapChainForHwnd(device_.Get(), hwnd_, &desc, nullptr, nullptr, &swapChain_))) {
        return false;
    }
    return true;
}

bool OverlayWindow::InitializeD2D() {
    D2D1_FACTORY_OPTIONS options{};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2dFactory_))) {
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    device_.As(&dxgiDevice);
    if (FAILED(d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_))) {
        return false;
    }
    if (FAILED(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_))) {
        return false;
    }

    ComPtr<IDXGISurface> surface;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface));

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    if (FAILED(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &props, &d2dTarget_))) {
        return false;
    }
    d2dContext_->SetTarget(d2dTarget_.Get());

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dwriteFactory_))) {
        return false;
    }
    dwriteFactory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMIBOLD,
                                     DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                     20.0f, L"en-us", &textFormat_);
    return true;
}

void OverlayWindow::UpdateWindowPosition() {
    if (!targetWindow_ || !hwnd_) {
        return;
    }
    RECT rect{};
    if (!GetWindowRect(targetWindow_, &rect)) {
        return;
    }
    HWND zorder = (GetForegroundWindow() == targetWindow_) ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(hwnd_, zorder, rect.left, rect.top,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void OverlayWindow::Render() {
    UpdateWindowPosition();
    if (!swapChain_) {
        return;
    }

    d2dContext_->BeginDraw();
    d2dContext_->Clear(D2D1::ColorF(0, 0.0f));

    std::wstring message;
    int duration = 0;
    ULONGLONG start = 0;
    {
        std::scoped_lock lock(messageMutex_);
        message = message_;
        duration = messageDurationMs_;
        start = messageStart_;
    }

    if (!message.empty() && duration > 0) {
        ULONGLONG now = GetTickCount64();
        float t = static_cast<float>(now - start) / duration;
        if (t <= 1.0f) {
            float alpha = 1.0f;
            if (t < 0.1f) {
                alpha = t / 0.1f;
            } else if (t > 0.8f) {
                alpha = (1.0f - t) / 0.2f;
            }
            D2D1_COLOR_F color = D2D1::ColorF(0.9f, 0.9f, 0.95f, alpha);
            ComPtr<ID2D1SolidColorBrush> brush;
            d2dContext_->CreateSolidColorBrush(color, &brush);
            D2D1_RECT_F rect = D2D1::RectF(24.0f, 24.0f, 600.0f, 80.0f);
            d2dContext_->DrawTextW(message.c_str(), static_cast<UINT32>(message.size()), textFormat_.Get(), rect, brush.Get());
        }
    }

    if (FAILED(d2dContext_->EndDraw())) {
        swapChain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    }
    swapChain_->Present(1, 0);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto create = reinterpret_cast<CREATESTRUCT*>(lparam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
