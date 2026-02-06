#include "desktop_duplicator.h"

#include <chrono>

bool DesktopDuplicator::Initialize(UINT outputIndex) {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapters1(0, &adapter))) {
        return false;
    }

    if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, levels, 2,
                                 D3D11_SDK_VERSION, &device_, &level, &context_))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(outputIndex, &output))) {
        return false;
    }
    output->GetDesc(&outputDesc_);

    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    if (FAILED(output.As(&output1))) {
        return false;
    }

    if (FAILED(output1->DuplicateOutput(device_.Get(), &duplication_))) {
        return false;
    }
    return true;
}

std::optional<CapturedFrame> DesktopDuplicator::CaptureFrame(int timeoutMs) {
    if (!duplication_) {
        return std::nullopt;
    }
    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    Microsoft::WRL::ComPtr<IDXGIResource> resource;
    HRESULT hr = duplication_->AcquireNextFrame(timeoutMs, &frameInfo, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return std::nullopt;
    }
    if (FAILED(hr)) {
        duplication_->ReleaseFrame();
        return std::nullopt;
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    resource.As(&tex);
    duplication_->ReleaseFrame();

    CapturedFrame frame;
    frame.texture = tex;
    frame.timestampHns = frameInfo.LastPresentTime.QuadPart;
    if (frame.timestampHns == 0) {
        frame.timestampHns = static_cast<LONGLONG>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now().time_since_epoch()).count() / 100);
    }
    return frame;
}

void DesktopDuplicator::Release() {
    duplication_.Reset();
    context_.Reset();
    device_.Reset();
}
