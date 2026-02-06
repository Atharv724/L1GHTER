#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <optional>

struct CapturedFrame {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    LONGLONG timestampHns = 0;
};

class DesktopDuplicator {
public:
    bool Initialize(UINT outputIndex = 0);
    std::optional<CapturedFrame> CaptureFrame(int timeoutMs);
    void Release();

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return context_.Get(); }

    DXGI_OUTPUT_DESC OutputDesc() const { return outputDesc_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    DXGI_OUTPUT_DESC outputDesc_{};
};
