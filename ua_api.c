// Copyright (c) Caleb Klomparens
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef EXPORT_MICRO_AUDIO_LIBRARY
#define EXPORT_MICRO_AUDIO_LIBRARY
#endif
#include "ua_api.h"
#ifdef _DEBUG
#include <stdio.h>
#endif
#undef EXPORT_MICRO_AUDIO_LIBRARY

#ifdef _DEBUG
#define UA_LOG_ERROR(x) printf(__FUNCTION__": %s failed!\n", #x);
#else
#define UA_LOG_ERROR(x)
#endif

#ifdef __APPLE__
ua_SampleRate ua_init_macos(ua_Settings* ua_InitParams);
void ua_term_macos(void);
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <math.h>
#include <string.h>
#define UA_CHECK(x, ret) do { s = (x); if (s != noErr) { \
    UA_LOG_ERROR(x); return (ret); } } while(0)
#elif _WIN32
ua_SampleRate ua_init_windows(ua_Settings* ua_InitParams);
void ua_term_windows(void);
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#undef WIN32_LEAN_AND_MEAN
#define UA_CHECK(x, ret) do { r = (x); if (!SUCCEEDED(r)) { \
    UA_LOG_ERROR(x); return (ret); } } while(0)
#endif

#define UA_MIN(a, b) ((a) < (b) ? (a) : (b))


typedef struct ua_AudioBuffer
{
    float* data;
    unsigned frameIndex;
    unsigned numFrames;
    unsigned char numChannels;
} ua_AudioBuffer;

typedef struct ua_AudioFormat
{
    ua_SampleRate sampleRate;
    unsigned char numChannels;
} ua_AudioFormat;

#define UA_MAX_CHANNEL_CONNECTIONS_PER_MAP 256
typedef struct ua_ChannelConnection
{
    unsigned char sourceChannel;
    unsigned char sinkChannel;
    float scaleFactor;
} ua_ChannelConnection;

typedef struct ua_ChannelMap
{
    unsigned char numSourceChannels;
    unsigned char numSinkChannels;

    unsigned char numConnections;
    ua_ChannelConnection connections[UA_MAX_CHANNEL_CONNECTIONS_PER_MAP];
} ua_ChannelMap;

#define UA_TOTAL_PREDEFINED_CHANNEL_MAPS 1
ua_ChannelMap ua_gChannelMaps[UA_TOTAL_PREDEFINED_CHANNEL_MAPS];

typedef struct ua_Context
{
    ua_ChannelMap channelMap;
    ua_Settings settings;
    ua_AudioBuffer workBuffer;
    ua_AudioBuffer delayLine;
    ua_AudioFormat deviceFormat;
    void (*renderToBufferFunction)(ua_AudioBuffer*);
} ua_Context;
ua_Context ua_gContext;

void ApplyDelayLine(ua_AudioBuffer* targetBuffer, ua_AudioBuffer* inBuffer)
{
    const ua_Settings* settings = &ua_gContext.settings;
    ua_AudioBuffer* delay = &ua_gContext.delayLine;
    const float* inData = inBuffer->data;
    const unsigned NumSamples = settings->framesPerBuffer * settings->numChannels;
    const unsigned NumDelaySamples = delay->numFrames * delay->numChannels;
    float tmp;
    float* outData = delay->data;
    for (unsigned i = 0; i < NumSamples; ++i)
    {
        tmp = inData[i];
        targetBuffer->data[i] = outData[delay->frameIndex];
        outData[delay->frameIndex] = tmp;
        delay->frameIndex = (delay->frameIndex + 1) % NumDelaySamples;
    }
}

void RenderToBuffer(ua_AudioBuffer* targetBuffer)
{
    const unsigned short NumChannels = ua_gContext.settings.numChannels;
    const unsigned short FramesPerBuffer = ua_gContext.settings.framesPerBuffer;
    ua_gContext.settings.audioCallback(targetBuffer->data, FramesPerBuffer, NumChannels);
}

void RenderToBufferWithDelayLine(ua_AudioBuffer* targetBuffer)
{
    const unsigned short NumChannels = ua_gContext.settings.numChannels;
    const unsigned short FramesPerBuffer = ua_gContext.settings.framesPerBuffer;
    float* data = ua_gContext.workBuffer.data;
    ua_gContext.settings.audioCallback(data, FramesPerBuffer, NumChannels);
    ApplyDelayLine(targetBuffer, &ua_gContext.workBuffer);
}

ua_AudioFormat GetDefaultDeviceFormat(void)
{
#ifdef __APPLE__
    ua_AudioFormat format = { .sampleRate = UA_INVALID_SAMPLE_RATE, .numChannels = 0 };

    AudioDeviceID deviceId = kAudioDeviceUnknown;
    UInt32 propertySize = sizeof(AudioDeviceID);
    AudioObjectPropertyAddress addr =
    {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    OSStatus s;
    UA_CHECK(AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL,
                                        &propertySize, &deviceId), format);
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
    propertyAddress.mScope = kAudioObjectPropertyScopeInput;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;
    Float64 sampleRateAsFloat;

    propertySize = sizeof(Float64);
    UA_CHECK(AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL,
                                        &propertySize, &sampleRateAsFloat), format);
    format.sampleRate = (ua_SampleRate)sampleRateAsFloat;
    propertyAddress.mSelector = kAudioDevicePropertyPreferredChannelLayout;
    propertyAddress.mScope = kAudioObjectPropertyScopeOutput; // WHY IS THIS OUTPUT???
    propertyAddress.mElement = kAudioObjectPropertyElementMain;
    propertySize = sizeof(AudioChannelLayout);
    AudioChannelLayout channelLayout;
    UA_CHECK(AudioObjectGetPropertyDataSize(deviceId, &propertyAddress, 0, NULL,
                                            &propertySize), format);
    UA_CHECK(AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL,
                                        &propertySize, &channelLayout), format);
    // # channel descriptions = # channels
    format.numChannels = (unsigned char)channelLayout.mNumberChannelDescriptions;
    
    return format;
#elif _WIN32
    // I don't understand why Windows is like this, but the CLSID_MMDeviceEnumerator and
    // IID_IMMDeviceEnumerator GUID do not get defined anywhere when compiling for C language.
    // Therefore, I define them here. May they never change!
    const GUID MM_Class = (GUID){ 0xBCDE0395, 0xE52F, 0x467C,
        { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
    const GUID MM_Interface = (GUID){ 0xA95664D2, 0x9614, 0x4F35,
        { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };

    HRESULT r;
    r = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* deviceEnumerator;
    r = CoCreateInstance(&MM_Class, NULL, CLSCTX_ALL, &MM_Interface, (void**)&deviceEnumerator);
    IMMDevice* pDefaultDevice;
    r = deviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(
        deviceEnumerator, eRender, eConsole, &pDefaultDevice
    );

    IPropertyStore* pStore = NULL;
    r = pDefaultDevice->lpVtbl->OpenPropertyStore(pDefaultDevice, STGM_READ, &pStore);
    PROPVARIANT prop = { 0 };
    r = pStore->lpVtbl->GetValue(pStore, &PKEY_AudioEngine_DeviceFormat, &prop);

    PWAVEFORMATEX deviceFormatProperties = (PWAVEFORMATEX)prop.blob.pBlobData;

    ua_AudioFormat format;
    format.numChannels = (unsigned char)deviceFormatProperties->nChannels;
    format.sampleRate = deviceFormatProperties->nSamplesPerSec; // should be called nFramesPerSec
    return format;
#endif
}

void* AllocateHelper(unsigned numBytes)
{
    return malloc((size_t)numBytes);
}

void InitChannelMaps(void)
{
    const float MINUS_THREE_DB_LINEAR = 0.7079f;

    ua_ChannelMap* maps = ua_gChannelMaps;
    maps[0].numSourceChannels = 1;
    maps[0].numSinkChannels = 2;

    maps[0].numConnections = 2;
    maps[0].connections[0].sourceChannel = 0;
    maps[0].connections[0].sinkChannel = 0;
    maps[0].connections[0].scaleFactor = MINUS_THREE_DB_LINEAR;

    maps[0].connections[1].sourceChannel = 0;
    maps[0].connections[1].sinkChannel = 1;
    maps[0].connections[1].scaleFactor = MINUS_THREE_DB_LINEAR;
}

ua_SampleRate ua_init(ua_Settings* ua_InitParams)
{
    ua_AudioFormat* deviceFormat = &ua_gContext.deviceFormat;
    *deviceFormat = GetDefaultDeviceFormat();
    if (deviceFormat->sampleRate == UA_INVALID_SAMPLE_RATE)
    {
        UA_LOG_ERROR(ua_gContext.deviceSampleRate != UA_INVALID_SAMPLE_RATE);
        return UA_INVALID_SAMPLE_RATE;
    }

    InitChannelMaps();
    const unsigned char MinConnections =
        UA_MIN(ua_InitParams->numChannels, deviceFormat->numChannels);
    ua_gContext.channelMap.numConnections = MinConnections;
    ua_gContext.channelMap.numSourceChannels = ua_InitParams->numChannels;
    ua_gContext.channelMap.numSinkChannels = (unsigned char)deviceFormat->numChannels;
    for (unsigned char i = 0; i < ua_gContext.channelMap.numConnections; ++i)
    {
        ua_gContext.channelMap.connections[i].scaleFactor = 1.f;
        ua_gContext.channelMap.connections[i].sinkChannel = i;
        ua_gContext.channelMap.connections[i].sourceChannel = i;
    }

    for (unsigned i = 0; i < UA_TOTAL_PREDEFINED_CHANNEL_MAPS; ++i)
    {
        if (ua_gChannelMaps[i].numSourceChannels == ua_InitParams->numChannels &&
            ua_gChannelMaps[i].numSinkChannels == deviceFormat->numChannels)
        {
            ua_gContext.channelMap = ua_gChannelMaps[i];
            break;
        }
    }

    ua_gContext.settings = *ua_InitParams;

    if (ua_InitParams->memAllocate == NULL)
        ua_gContext.settings.memAllocate = AllocateHelper;
    if (ua_InitParams->memFree == NULL)
        ua_gContext.settings.memFree = free;

    const ua_Settings* settings = &ua_gContext.settings;

    const unsigned FrameMilliseconds = settings->maxLatencyMs * ua_gContext.deviceFormat.sampleRate;
    ua_AudioBuffer* delayLine = &ua_gContext.delayLine;
    delayLine->numFrames = FrameMilliseconds / 1000;
    delayLine->numChannels = settings->numChannels;
    ua_gContext.delayLine.frameIndex = 0;

    const unsigned MaxSamplesPerBuffer = settings->framesPerBuffer * settings->numChannels;
    const unsigned MaxWorkBufferByteCount = sizeof(float) * MaxSamplesPerBuffer;
    ua_AudioBuffer* workBuffer = &ua_gContext.workBuffer;
    workBuffer->data = settings->memAllocate(MaxWorkBufferByteCount);
    if (workBuffer->data == NULL)
    {
        UA_LOG_ERROR(workBuffer->data != NULL);
        return UA_INVALID_SAMPLE_RATE;
    }
    workBuffer->numFrames = settings->framesPerBuffer;
    workBuffer->frameIndex = workBuffer->numFrames;
    workBuffer->numChannels = settings->numChannels;
    memset(workBuffer->data, 0, MaxWorkBufferByteCount);

    if (settings->maxLatencyMs != 0) {
        const unsigned NumDelaySamples = delayLine->numFrames * ua_gContext.delayLine.numChannels;
        const unsigned NumDelayBytes = NumDelaySamples * sizeof(float);
        ua_gContext.delayLine.data = settings->memAllocate(NumDelayBytes);
        if (ua_gContext.delayLine.data == NULL)
        {
            UA_LOG_ERROR(ua_gContext.delayLine.data != NULL);
            return UA_INVALID_SAMPLE_RATE;
        }
        memset(ua_gContext.delayLine.data, 0, NumDelayBytes);

        ua_gContext.renderToBufferFunction = RenderToBufferWithDelayLine;
    }
    else
    {
        ua_gContext.delayLine.data = NULL;

        ua_gContext.renderToBufferFunction = RenderToBuffer;
    }

#ifdef __APPLE__
    return ua_init_macos(ua_InitParams);
#elif _WIN32
    return ua_init_windows(ua_InitParams);
#endif
}

void ua_term(void)
{
#ifdef __APPLE__
    ua_term_macos();
#elif _WIN32
    ua_term_windows();
#endif
}

#ifdef __APPLE__
static OSStatus RenderCallback(void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    ua_Context* context = (ua_Context*)inRefCon;
    ua_AudioBuffer* workBuffer = &context->workBuffer;
    ua_ChannelMap* map = &context->channelMap;

    for (unsigned channel = 0; channel < ioData->mNumberBuffers; ++channel)
    {
        memset(ioData->mBuffers[channel].mData, 0, ioData->mBuffers[channel].mDataByteSize);
    }

    unsigned frame = 0;
    unsigned framesLeft = inNumberFrames;
    while (framesLeft)
    {
        if (workBuffer->frameIndex >= workBuffer->numFrames)
        {
            workBuffer->frameIndex = 0;
            context->renderToBufferFunction(workBuffer);
        }

        const unsigned WorkFrames = workBuffer->numFrames - workBuffer->frameIndex;
        const unsigned FramesToProcess = WorkFrames < framesLeft ? WorkFrames : framesLeft;

        for (unsigned char mapIndex = 0; mapIndex < map->numConnections; ++mapIndex)
        {
            const unsigned char SourceChannel = map->connections[mapIndex].sourceChannel;
            const unsigned char SinkChannel = map->connections[mapIndex].sinkChannel;
            const float ScaleFactor = map->connections[mapIndex].scaleFactor;
            for (UInt32 i = 0; i < FramesToProcess; ++i)
            {
                float* buffer = (float*)ioData->mBuffers[SinkChannel].mData;
                const float Sample = workBuffer->data[(workBuffer->frameIndex + i)
                    * workBuffer->numChannels + SourceChannel];
                buffer[frame + i] += Sample * ScaleFactor;
            }
        }

        framesLeft -= FramesToProcess;
        frame += FramesToProcess;
        workBuffer->frameIndex += FramesToProcess;
    }

    return noErr;
}

AudioComponentInstance auHAL;

AudioDeviceID ua_get_default_output_device()
{
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    UInt32 propertySize = sizeof(AudioDeviceID);
    AudioObjectPropertyAddress addr =
    {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &propertySize, &deviceID);

    return deviceID;
}

ua_SampleRate ua_init_macos(ua_Settings* ua_InitParams)
{
    AudioComponentDescription desc =
    {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_HALOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    OSStatus s;
    UA_CHECK(AudioComponentInstanceNew(comp, &auHAL), UA_INVALID_SAMPLE_RATE);
    UA_CHECK(AudioUnitInitialize(auHAL), UA_INVALID_SAMPLE_RATE);
    AudioDeviceID outputDeviceId = ua_get_default_output_device();
    const AudioUnitPropertyID kCurrentDevice = kAudioOutputUnitProperty_CurrentDevice;
    UA_CHECK(AudioUnitSetProperty(auHAL, kCurrentDevice, kAudioUnitScope_Global, 0,
             &outputDeviceId, sizeof(outputDeviceId)), UA_INVALID_SAMPLE_RATE);

    Float64 targetSampleRate = ua_gContext.deviceFormat.sampleRate;
    UA_CHECK(AudioUnitSetProperty(auHAL, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0,
             &targetSampleRate, sizeof(targetSampleRate)), UA_INVALID_SAMPLE_RATE);
    
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = RenderCallback;
    callbackStruct.inputProcRefCon = &ua_gContext;
    const AudioUnitPropertyID kRenderCallback = kAudioUnitProperty_SetRenderCallback;
    UA_CHECK(AudioUnitSetProperty(auHAL, kRenderCallback, kAudioUnitScope_Input, 0,
             &callbackStruct, sizeof(callbackStruct)), UA_INVALID_SAMPLE_RATE);
    UInt32 F64Size = sizeof(Float64);
    UA_CHECK(AudioUnitGetProperty(auHAL, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input, 0,
             &targetSampleRate, &F64Size), UA_INVALID_SAMPLE_RATE);

    AudioOutputUnitStart(auHAL);

    return ua_gContext.deviceFormat.sampleRate;
}

void ua_term_macos(void)
{
    AudioOutputUnitStop(auHAL);
    AudioUnitUninitialize(auHAL);
    AudioComponentInstanceDispose(auHAL);
}

#elif _WIN32
#define WIN32_LEAN_AND_MEAN

// At /Wall levels of warnings, these must be defined for MS.
#ifndef WINAPI_PARTITION_TV_APP 
#define WINAPI_PARTITION_TV_APP 0
#endif
#ifndef WINAPI_PARTITION_TV_TITLE
#define WINAPI_PARTITION_TV_TITLE 0
#endif

#include <xaudio2.h>
#undef WIN32_LEAN_AND_MEAN

IXAudio2* ua_xAudio2;
IXAudio2MasteringVoice* ua_xAudio2MasterVoice;
IXAudio2SourceVoice* ua_xAudio2SourceVoice;
typedef struct ua_XAudio2Buffer
{
    BYTE* rawData;
    XAUDIO2_BUFFER xAudioBuffer;
    ua_AudioBuffer buffer;
} ua_XAudio2Buffer;

#define UA_RENDER_BUFFER_COUNT 2
ua_XAudio2Buffer ua_buffers[UA_RENDER_BUFFER_COUNT] = { NULL };

void XAudio2OnBufferEnd(IXAudio2VoiceCallback* This, void* pCtx)
{
    (void)This;
    ua_XAudio2Buffer* self = (ua_XAudio2Buffer*)pCtx;
    ua_AudioBuffer* sink = &self->buffer;
    memset(sink->data, 0, sizeof(float) * sink->numChannels * sink->numFrames);
    ua_AudioBuffer* source = &ua_gContext.workBuffer;
    if (source->numFrames != sink->numFrames)
    {
        exit(0); // TODO: not this
    }

    // TODO: this is repeated logic from macOS. Should consolidate.
    ua_gContext.renderToBufferFunction(source);
    const ua_ChannelMap* Map = &ua_gContext.channelMap;
    for (unsigned char mapIndex = 0; mapIndex < Map->numConnections; ++mapIndex)
    {
        const unsigned char SourceChannel = Map->connections[mapIndex].sourceChannel;
        const unsigned char SinkChannel = Map->connections[mapIndex].sinkChannel;
        const float ScaleFactor = Map->connections[mapIndex].scaleFactor;
        for (unsigned frame = 0; frame < sink->numFrames; ++frame)
        {
            const float Sample = source->data[frame * source->numChannels + SourceChannel];
            sink->data[frame * sink->numChannels + SinkChannel] += Sample * ScaleFactor;
        }
    }

    IXAudio2SourceVoice_SubmitSourceBuffer(ua_xAudio2SourceVoice, &self->xAudioBuffer, NULL);
}

void XA2OSE(IXAudio2VoiceCallback* pXa2) { (void)pXa2; } // unused stubs
void XA2OVPPE(IXAudio2VoiceCallback* pXa2) { (void)pXa2; }
void XA2OVPPS(IXAudio2VoiceCallback* pXa2, UINT32 s) { (void)pXa2; (void)s; }
void XA2OBS(IXAudio2VoiceCallback* pXa2, void* pCtx) { (void)pXa2; (void)pCtx; }
void XA2OLE(IXAudio2VoiceCallback* pXa2, void* pCtx) { (void)pXa2; (void)pCtx; }
void XA2OVE(IXAudio2VoiceCallback* pXa2, void* pCtx, HRESULT result)
{
    (void)pXa2; (void)pCtx; (void)result;
}

IXAudio2VoiceCallback xAudio2Callbacks =
{
    .lpVtbl = &(IXAudio2VoiceCallbackVtbl)
    {
        .OnStreamEnd = XA2OSE,
        .OnVoiceProcessingPassEnd = XA2OVPPE,
        .OnVoiceProcessingPassStart = XA2OVPPS,
        .OnBufferEnd = XAudio2OnBufferEnd,
        .OnBufferStart = XA2OBS,
        .OnLoopEnd = XA2OLE,
        .OnVoiceError = XA2OVE
    }
};

ua_SampleRate ua_init_windows(ua_Settings* settings)
{
    HRESULT r;
    // per Microsoft, first param must be NULL
    UA_CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED), UA_INVALID_SAMPLE_RATE);
    // per Microsoft, param 2 must be 0
    UA_CHECK(XAudio2Create(&ua_xAudio2, 0, XAUDIO2_USE_DEFAULT_PROCESSOR), UA_INVALID_SAMPLE_RATE);

    UA_CHECK(IXAudio2_CreateMasteringVoice(ua_xAudio2, &ua_xAudio2MasterVoice,
        XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
        0, // no flags
        NULL, // use default device
        NULL, // no effects
        AudioCategory_GameMedia
    ), UA_INVALID_SAMPLE_RATE);
    const WORD BytesPerSample = sizeof(float);
    const ua_SampleRate SampleRate = ua_gContext.deviceFormat.sampleRate;
    const unsigned char NumChannels = ua_gContext.deviceFormat.numChannels;
    WAVEFORMATEX waveFormat =
    {
        .wFormatTag = WAVE_FORMAT_IEEE_FLOAT,
        .nChannels = NumChannels,
        .nSamplesPerSec = SampleRate,
        .nAvgBytesPerSec = SampleRate * NumChannels * BytesPerSample,
        .nBlockAlign = NumChannels * BytesPerSample,
        .wBitsPerSample = BytesPerSample * 8,
        .cbSize = 0 // set to zero for PCM or IEEE float
    };
    const float DefaultPitchRatio = 1.f;
    UA_CHECK(IXAudio2_CreateSourceVoice(ua_xAudio2, &ua_xAudio2SourceVoice, &waveFormat,
        XAUDIO2_VOICE_NOPITCH, DefaultPitchRatio, &xAudio2Callbacks, NULL, NULL // no sends/effects
    ), UA_INVALID_SAMPLE_RATE);
    IXAudio2SourceVoice_Start(ua_xAudio2SourceVoice, 0, XAUDIO2_COMMIT_NOW);

    const unsigned short FramesPerBuffer = settings->framesPerBuffer;
    const unsigned BufferByteCount = FramesPerBuffer * NumChannels * sizeof(float);
    for (int i = 0; i < UA_RENDER_BUFFER_COUNT; ++i)
    {
        ua_XAudio2Buffer* ab = &ua_buffers[i];
        *ab = (const ua_XAudio2Buffer){ 0 };
        ab->xAudioBuffer.pContext = ab;
        ab->rawData = settings->memAllocate(BufferByteCount);
        if (ab->rawData == NULL)
        {
            UA_LOG_ERROR(ab->rawData != NULL);
            return UA_INVALID_SAMPLE_RATE;
        }

        ab->buffer.data = (float*)ab->rawData;
        ab->buffer.numChannels = NumChannels;
        ab->buffer.numFrames = FramesPerBuffer;

        memset(ab->rawData, 0, BufferByteCount);
        ab->xAudioBuffer.AudioBytes = BufferByteCount;
        ab->xAudioBuffer.pAudioData = (const BYTE*)ua_buffers[i].rawData;
        IXAudio2SourceVoice_SubmitSourceBuffer(ua_xAudio2SourceVoice, &ab->xAudioBuffer, NULL);
    }

    return ua_gContext.deviceFormat.sampleRate;
}

void ua_term_windows(void)
{
    IXAudio2SourceVoice_DestroyVoice(ua_xAudio2SourceVoice);
    ua_xAudio2SourceVoice = NULL;
    IXAudio2MasteringVoice_DestroyVoice(ua_xAudio2MasterVoice);
    ua_xAudio2MasterVoice = NULL;
    IXAudio2_StopEngine(ua_xAudio2);
    IXAudio2_Release(ua_xAudio2);
    ua_xAudio2 = NULL;

    CoUninitialize();

    for (int i = 0; i < UA_RENDER_BUFFER_COUNT; ++i)
    {
        ua_gContext.settings.memFree(ua_buffers[i].rawData);
    }

    if (ua_gContext.workBuffer.data != NULL)
    {
        ua_gContext.settings.memFree(ua_gContext.workBuffer.data);
        ua_gContext.workBuffer.data = NULL;
    }

    if (ua_gContext.delayLine.data != NULL)
    {
        ua_gContext.settings.memFree(ua_gContext.delayLine.data);
        ua_gContext.delayLine.data = NULL;
    }
}

#endif

#ifdef __cplusplus
}
#endif
