#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
#include <thread>
#include <atomic>

#include "ring_buffer.h"

struct AudioSample {
    Microsoft::WRL::ComPtr<IMFSample> sample;
    LONGLONG timestampHns = 0;
    LONGLONG durationHns = 0;
};

class AudioCapture {
public:
    bool Initialize(bool captureSystem, bool captureMic);
    void Start();
    void Stop();

    IMFMediaType* InputType() const { return inputType_.Get(); }
    IMFMediaType* OutputType() const { return outputType_.Get(); }

    TimedRingBuffer<AudioSample>& Buffer() { return buffer_; }

private:
    bool InitializeClients();
    void CaptureLoop();
    bool SetupDevice(bool loopback, Microsoft::WRL::ComPtr<IAudioClient>& client,
                     Microsoft::WRL::ComPtr<IAudioCaptureClient>& capture,
                     WAVEFORMATEX** mixFormat);
    void MixAndStore(const BYTE* systemData, UINT32 systemFrames,
                     const BYTE* micData, UINT32 micFrames, WAVEFORMATEX* format,
                     LONGLONG timestampHns);

    bool captureSystem_ = true;
    bool captureMic_ = false;
    std::thread thread_;
    std::atomic<bool> running_{false};

    Microsoft::WRL::ComPtr<IAudioClient> systemClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> systemCapture_;
    Microsoft::WRL::ComPtr<IAudioClient> micClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> micCapture_;
    WAVEFORMATEX* systemFormat_ = nullptr;
    WAVEFORMATEX* micFormat_ = nullptr;

    TimedRingBuffer<AudioSample> buffer_;
    Microsoft::WRL::ComPtr<IMFMediaType> inputType_;
    Microsoft::WRL::ComPtr<IMFMediaType> outputType_;
};
