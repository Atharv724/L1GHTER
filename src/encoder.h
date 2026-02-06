#pragma once

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>

struct EncodedSample {
    Microsoft::WRL::ComPtr<IMFSample> sample;
    LONGLONG timestampHns = 0;
    LONGLONG durationHns = 0;
};

class VideoEncoder {
public:
    bool Initialize(ID3D11Device* device, UINT width, UINT height, UINT fps);
    bool EncodeFrame(ID3D11Texture2D* texture, LONGLONG timestampHns, EncodedSample& outSample);
    void Shutdown();

    IMFMediaType* OutputType() const { return outputType_.Get(); }

private:
    bool CreateEncoder();

    UINT width_ = 0;
    UINT height_ = 0;
    UINT fps_ = 60;

    Microsoft::WRL::ComPtr<IMFTransform> encoder_;
    Microsoft::WRL::ComPtr<IMFMediaType> inputType_;
    Microsoft::WRL::ComPtr<IMFMediaType> outputType_;
    Microsoft::WRL::ComPtr<IMFMediaBuffer> stagingBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
};

class Mp4Writer {
public:
    bool Initialize(const std::wstring& path,
                    IMFMediaType* videoInputType,
                    IMFMediaType* videoOutputType,
                    IMFMediaType* audioInputType,
                    IMFMediaType* audioOutputType);
    bool WriteVideoSample(IMFSample* sample, LONGLONG timestampHns);
    bool WriteAudioSample(IMFSample* sample, LONGLONG timestampHns);
    void Finalize();

private:
    Microsoft::WRL::ComPtr<IMFSinkWriter> writer_;
    DWORD videoStreamIndex_ = 0;
    DWORD audioStreamIndex_ = 0;
    bool hasAudio_ = false;
    bool started_ = false;
};
