#include "encoder.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "util.h"

using Microsoft::WRL::ComPtr;

namespace {
ComPtr<IMFTransform> CreateH264Encoder(bool hardware) {
    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    MFT_REGISTER_TYPE_INFO outputInfo{MFMediaType_Video, MFVideoFormat_H264};
    UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT;
    if (hardware) {
        flags |= MFT_ENUM_FLAG_HARDWARE;
    }
    HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &outputInfo, &activates, &count);
    if (FAILED(hr) || count == 0) {
        return nullptr;
    }
    ComPtr<IMFTransform> encoder;
    activates[0]->ActivateObject(IID_PPV_ARGS(&encoder));
    for (UINT32 i = 0; i < count; ++i) {
        activates[i]->Release();
    }
    CoTaskMemFree(activates);
    return encoder;
}
}

bool VideoEncoder::Initialize(ID3D11Device* device, UINT width, UINT height, UINT fps) {
    width_ = width;
    height_ = height;
    fps_ = fps;
    device_ = device;
    if (device_) {
        device_->GetImmediateContext(&context_);
    }
    return CreateEncoder();
}

bool VideoEncoder::CreateEncoder() {
    encoder_ = CreateH264Encoder(true);
    if (!encoder_) {
        encoder_ = CreateH264Encoder(false);
    }
    if (!encoder_) {
        return false;
    }

    if (!device_) {
        ComPtr<ID3D11Device> device;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                     D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                     D3D11_SDK_VERSION, &device, nullptr, nullptr))) {
            return false;
        }
        device_ = device;
        device_->GetImmediateContext(&context_);
    }

    ComPtr<IMFDXGIDeviceManager> dxgiManager;
    UINT resetToken = 0;
    if (FAILED(MFCreateDXGIDeviceManager(&resetToken, &dxgiManager))) {
        return false;
    }
    dxgiManager->ResetDevice(device_.Get(), resetToken);
    encoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(dxgiManager.Get()));

    ComPtr<IMFMediaType> inputType;
    MFCreateMediaType(&inputType);
    inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(inputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    inputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

    if (FAILED(encoder_->SetInputType(0, inputType.Get(), 0))) {
        return false;
    }

    ComPtr<IMFMediaType> outputType;
    MFCreateMediaType(&outputType);
    outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width_, height_);
    MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, fps_, 1);
    MFSetAttributeRatio(outputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    outputType->SetUINT32(MF_MT_AVG_BITRATE, width_ * height_ * fps_ / 2);
    outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    outputType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);

    if (FAILED(encoder_->SetOutputType(0, outputType.Get(), 0))) {
        return false;
    }

    inputType_ = inputType;
    outputType_ = outputType;
    return true;
}

bool VideoEncoder::EncodeFrame(ID3D11Texture2D* texture, LONGLONG timestampHns, EncodedSample& outSample) {
    if (!encoder_ || !texture) {
        return false;
    }

    ComPtr<IMFMediaBuffer> dxgiBuffer;
    if (FAILED(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), texture, 0, FALSE, &dxgiBuffer))) {
        return false;
    }

    ComPtr<IMFSample> sample;
    MFCreateSample(&sample);
    sample->AddBuffer(dxgiBuffer.Get());
    sample->SetSampleTime(timestampHns);
    sample->SetSampleDuration(HnsFromMillis(1000 / fps_));

    if (FAILED(encoder_->ProcessInput(0, sample.Get(), 0))) {
        return false;
    }

    MFT_OUTPUT_STREAM_INFO streamInfo{};
    encoder_->GetOutputStreamInfo(0, &streamInfo);

    ComPtr<IMFSample> outputSample;
    MFCreateSample(&outputSample);

    ComPtr<IMFMediaBuffer> outputBuffer;
    MFCreateMemoryBuffer(streamInfo.cbSize, &outputBuffer);
    outputSample->AddBuffer(outputBuffer.Get());

    MFT_OUTPUT_DATA_BUFFER outputData{};
    outputData.dwStreamID = 0;
    outputData.pSample = outputSample.Get();

    DWORD status = 0;
    HRESULT hr = encoder_->ProcessOutput(0, 1, &outputData, &status);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        return false;
    }
    if (FAILED(hr)) {
        return false;
    }

    LONGLONG sampleTime = 0;
    LONGLONG sampleDuration = 0;
    outputSample->GetSampleTime(&sampleTime);
    outputSample->GetSampleDuration(&sampleDuration);

    outSample.sample = outputSample;
    outSample.timestampHns = sampleTime;
    outSample.durationHns = sampleDuration;
    return true;
}

void VideoEncoder::Shutdown() {
    encoder_.Reset();
    inputType_.Reset();
    outputType_.Reset();
    context_.Reset();
    device_.Reset();
}

bool Mp4Writer::Initialize(const std::wstring& path,
                           IMFMediaType* videoInputType,
                           IMFMediaType* videoOutputType,
                           IMFMediaType* audioInputType,
                           IMFMediaType* audioOutputType) {
    if (FAILED(MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, &writer_))) {
        return false;
    }

    if (FAILED(writer_->AddStream(videoOutputType, &videoStreamIndex_))) {
        return false;
    }

    if (audioOutputType) {
        if (FAILED(writer_->AddStream(audioOutputType, &audioStreamIndex_))) {
            return false;
        }
        hasAudio_ = true;
    }

    if (FAILED(writer_->SetInputMediaType(videoStreamIndex_, videoInputType, nullptr))) {
        return false;
    }

    if (hasAudio_ && audioInputType) {
        if (FAILED(writer_->SetInputMediaType(audioStreamIndex_, audioInputType, nullptr))) {
            return false;
        }
    }

    if (FAILED(writer_->BeginWriting())) {
        return false;
    }
    started_ = true;
    return true;
}

bool Mp4Writer::WriteVideoSample(IMFSample* sample, LONGLONG timestampHns) {
    if (!started_ || !writer_) {
        return false;
    }
    sample->SetSampleTime(timestampHns);
    return SUCCEEDED(writer_->WriteSample(videoStreamIndex_, sample));
}

bool Mp4Writer::WriteAudioSample(IMFSample* sample, LONGLONG timestampHns) {
    if (!started_ || !writer_ || !hasAudio_) {
        return false;
    }
    sample->SetSampleTime(timestampHns);
    return SUCCEEDED(writer_->WriteSample(audioStreamIndex_, sample));
}

void Mp4Writer::Finalize() {
    if (writer_) {
        writer_->Finalize();
    }
    writer_.Reset();
}
