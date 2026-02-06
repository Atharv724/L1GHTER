#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <thread>
#include <atomic>
#include <mutex>

class OverlayWindow {
public:
    bool Start(HWND targetWindow);
    void Stop();
    void ShowMessage(const std::wstring& text, int durationMs);
    void UpdateTarget(HWND targetWindow);

private:
    void ThreadMain();
    void Render();
    void UpdateWindowPosition();
    bool InitializeD3D();
    bool InitializeD2D();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    std::thread thread_;
    std::atomic<bool> running_{false};
    HWND targetWindow_ = nullptr;
    HWND hwnd_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2dTarget_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;

    std::mutex messageMutex_;
    std::wstring message_;
    int messageDurationMs_ = 0;
    ULONGLONG messageStart_ = 0;
};
