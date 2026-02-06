#include "audio_capture.h"

#include <functiondiscoverykeys_devpkey.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <avrt.h>
#include <vector>
#include <ksmedia.h>

#include "logging.h"
#include "util.h"

using Microsoft::WRL::ComPtr;

bool AudioCapture::Initialize(bool captureSystem, bool captureMic) {
    captureSystem_ = captureSystem;
    captureMic_ = captureMic;
    if (!captureSystem_ && !captureMic_) {
        return false;
    }
    if (!InitializeClients()) {
        return false;
    }

    WAVEFORMATEX* baseFormat = systemFormat_ ? systemFormat_ : micFormat_;
    if (!baseFormat) {
        return false;
    }

    MFCreateMediaType(&inputType_);
    inputType_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    inputType_->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    inputType_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, baseFormat->wBitsPerSample);
    inputType_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, baseFormat->nSamplesPerSec);
    inputType_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, baseFormat->nChannels);
    inputType_->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, baseFormat->nBlockAlign);
    inputType_->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, baseFormat->nAvgBytesPerSec);

    MFCreateMediaType(&outputType_);
    outputType_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outputType_->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    outputType_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    outputType_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, baseFormat->nSamplesPerSec);
    outputType_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, baseFormat->nChannels);
    outputType_->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, baseFormat->nAvgBytesPerSec / 2);
    outputType_->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
    outputType_->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
    outputType_->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);

    return true;
}

bool AudioCapture::InitializeClients() {
    if (captureSystem_) {
        if (!SetupDevice(true, systemClient_, systemCapture_, &systemFormat_)) {
            return false;
        }
    }
    if (captureMic_) {
        if (!SetupDevice(false, micClient_, micCapture_, &micFormat_)) {
            Logger::Instance().Error(L"Mic capture unavailable, disabling mic.");
            captureMic_ = false;
        }
    }

    if (captureMic_ && micFormat_ && systemFormat_) {
        if (micFormat_->nSamplesPerSec != systemFormat_->nSamplesPerSec ||
            micFormat_->nChannels != systemFormat_->nChannels ||
            micFormat_->wBitsPerSample != systemFormat_->wBitsPerSample) {
            Logger::Instance().Error(L"Mic format mismatch; disabling mic.");
            captureMic_ = false;
        }
    }
    return true;
}

bool AudioCapture::SetupDevice(bool loopback, ComPtr<IAudioClient>& client,
                               ComPtr<IAudioCaptureClient>& capture, WAVEFORMATEX** mixFormat) {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&enumerator)))) {
        return false;
    }

    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(loopback ? eRender : eCapture, eConsole, &device))) {
        return false;
    }

    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client))) {
        return false;
    }

    if (FAILED(client->GetMixFormat(mixFormat))) {
        return false;
    }

    REFERENCE_TIME bufferDuration = 10000000; // 1 second
    DWORD flags = loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, bufferDuration, 0, *mixFormat, nullptr))) {
        return false;
    }

    if (FAILED(client->GetService(IID_PPV_ARGS(&capture)))) {
        return false;
    }
    return true;
}

void AudioCapture::Start() {
    if (running_) {
        return;
    }
    running_ = true;
    thread_ = std::thread(&AudioCapture::CaptureLoop, this);
}

void AudioCapture::Stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (systemClient_) {
        systemClient_->Stop();
    }
    if (micClient_) {
        micClient_->Stop();
    }
}

void AudioCapture::CaptureLoop() {
    SetThreadName(L"AudioCapture");
    HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Audio", nullptr);
    if (systemClient_) {
        systemClient_->Start();
    }
    if (micClient_) {
        micClient_->Start();
    }

    while (running_) {
        UINT32 systemFrames = 0;
        UINT32 micFrames = 0;
        BYTE* systemData = nullptr;
        BYTE* micData = nullptr;
        DWORD flags = 0;
        LONGLONG position = 0;
        LONGLONG timestamp = 0;

        if (systemCapture_) {
            if (SUCCEEDED(systemCapture_->GetNextPacketSize(&systemFrames)) && systemFrames > 0) {
                systemCapture_->GetBuffer(&systemData, &systemFrames, &flags, &position, &timestamp);
            }
        }
        if (captureMic_ && micCapture_) {
            if (SUCCEEDED(micCapture_->GetNextPacketSize(&micFrames)) && micFrames > 0) {
                micCapture_->GetBuffer(&micData, &micFrames, &flags, &position, &timestamp);
            }
        }

        if (systemData && systemFrames > 0) {
            MixAndStore(systemData, systemFrames, micData, micFrames, systemFormat_, timestamp);
        } else if (!systemData && micData && micFrames > 0) {
            MixAndStore(micData, micFrames, nullptr, 0, micFormat_, timestamp);
        }

        if (systemData && systemCapture_) {
            systemCapture_->ReleaseBuffer(systemFrames);
        }
        if (micData && micCapture_) {
            micCapture_->ReleaseBuffer(micFrames);
        }
        Sleep(5);
    }

    if (avrt) {
        AvRevertMmThreadCharacteristics(avrt);
    }
}

void AudioCapture::MixAndStore(const BYTE* systemData, UINT32 systemFrames,
                               const BYTE* micData, UINT32 micFrames, WAVEFORMATEX* format,
                               LONGLONG timestampHns) {
    if (!format) {
        return;
    }
    const UINT32 frameSize = format->nBlockAlign;
    const UINT32 bytes = systemFrames * frameSize;

    std::vector<BYTE> mixed(bytes);
    memcpy(mixed.data(), systemData, bytes);

    const bool isFloat = (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                         (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                          reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    if (captureMic_ && micData && micFrames == systemFrames && isFloat) {
        const float* mic = reinterpret_cast<const float*>(micData);
        float* out = reinterpret_cast<float*>(mixed.data());
        const size_t samples = bytes / sizeof(float);
        for (size_t i = 0; i < samples; ++i) {
            out[i] = (out[i] + mic[i]) * 0.5f;
        }
    }

    ComPtr<IMFMediaBuffer> buffer;
    MFCreateMemoryBuffer(bytes, &buffer);
    BYTE* dst = nullptr;
    DWORD maxLen = 0;
    DWORD curLen = 0;
    buffer->Lock(&dst, &maxLen, &curLen);
    memcpy(dst, mixed.data(), bytes);
    buffer->Unlock();
    buffer->SetCurrentLength(bytes);

    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(timestampHns);
    const LONGLONG duration = (static_cast<LONGLONG>(systemFrames) * 10000000) / format->nSamplesPerSec;
    sample->SetSampleDuration(duration);

    AudioSample audioSample;
    audioSample.sample = sample;
    audioSample.timestampHns = timestampHns;
    audioSample.durationHns = duration;
    buffer_.Push(std::move(audioSample));
}
