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
void ua_init_macos(ua_Settings* ua_InitParams);
void ua_term_macos(void);
#include <string.h>
#elif _WIN32
void ua_init_windows(ua_Settings* ua_InitParams);
void ua_term_windows(void);
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#undef WIN32_LEAN_AND_MEAN
#define UA_CHECK_WITH_RETURN(x, ret) do { r = (x); if (!SUCCEEDED(r)) { \
    UA_LOG_ERROR(x); return (ret); } } while(0)
#define UA_CHECK(x) do { r = (x); if (!SUCCEEDED(r)) { \
    UA_LOG_ERROR(x); } } while(0)
#endif

// TODO: this should not be a define
#define UA_CHANNEL_COUNT 2

typedef struct ua_DelayLine {
    float* buffer;
    unsigned readWriteIndex;
    unsigned sampleCount;
} ua_DelayLine;

typedef struct ua_WorkBuffer {
    unsigned frameIndex;
    unsigned frameCount;
    float* sampleData;
} ua_WorkBuffer;

typedef struct ua_Context {
    ua_Settings settings;
    ua_WorkBuffer workBuffer;
    ua_DelayLine delayLine;
    ua_SampleRate deviceSampleRate;
    void (*renderToBufferFunction)(float*);
} ua_Context;
ua_Context ua_gContext;

void ApplyDelayLine(float* targetBuffer) {
    const ua_Settings* settings = &ua_gContext.settings;
    ua_DelayLine* delay = &ua_gContext.delayLine;
    const float* inBuffer = ua_gContext.workBuffer.sampleData;
    const unsigned SampleCount = settings->maxFramesPerBuffer * settings->maxChannelCount;
    float tmp;
    for (unsigned i = 0; i < SampleCount; ++i) {
        tmp = inBuffer[i];
        targetBuffer[i] = delay->buffer[delay->readWriteIndex];
        delay->buffer[delay->readWriteIndex] = tmp;
        delay->readWriteIndex = (delay->readWriteIndex + 1) % delay->sampleCount;
    }
}

void RenderToBuffer(float* targetBuffer) {
    const unsigned short FramesPerBuffer = ua_gContext.settings.maxFramesPerBuffer;
    ua_gContext.settings.process(targetBuffer, FramesPerBuffer, UA_CHANNEL_COUNT);
}

void RenderToBufferWithDelayLine(float* targetBuffer) {
    const unsigned short FramesPerBuffer = ua_gContext.settings.maxFramesPerBuffer;
    float* sampleData = ua_gContext.workBuffer.sampleData;
    ua_gContext.settings.process(sampleData, FramesPerBuffer, UA_CHANNEL_COUNT);
    ApplyDelayLine(targetBuffer);
}

ua_SampleRate GetDefaultDeviceSampleRate(void) {
#ifdef __APPLE__

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
    return deviceFormatProperties->nSamplesPerSec;
#endif
}

ua_SampleRate ua_init(ua_Settings* ua_InitParams) {
    
    if (ua_InitParams->allocateFunction == NULL) {
        UA_LOG_ERROR(ua_InitParams->allocateFunction != NULL);
        return UA_INVALID_SAMPLE_RATE;
    }
    if (ua_InitParams->freeFunction == NULL) {
        UA_LOG_ERROR(ua_InitParams->freeFunction != NULL);
        return UA_INVALID_SAMPLE_RATE;
    }

    ua_gContext.deviceSampleRate = GetDefaultDeviceSampleRate();
    if (ua_gContext.deviceSampleRate == UA_INVALID_SAMPLE_RATE) {
        UA_LOG_ERROR(ua_gContext.deviceSampleRate != UA_INVALID_SAMPLE_RATE);
        return UA_INVALID_SAMPLE_RATE;
    }
    ua_gContext.settings = *ua_InitParams;
    const ua_Settings* settings = &ua_gContext.settings;

    ua_gContext.delayLine.readWriteIndex = 0;
    const unsigned SampleMilliseconds = 
        settings->maxLatencyMs * ua_gContext.deviceSampleRate * settings->maxChannelCount;
    ua_gContext.delayLine.sampleCount = SampleMilliseconds / 1000;
    
    const unsigned MaxSamplesPerBuffer = settings->maxFramesPerBuffer * settings->maxChannelCount;
    const unsigned MaxWorkBufferByteCount = sizeof(float) * MaxSamplesPerBuffer;
    ua_WorkBuffer* workBuffer = &ua_gContext.workBuffer;
    workBuffer->sampleData = settings->allocateFunction(MaxWorkBufferByteCount);
    workBuffer->frameCount = settings->maxFramesPerBuffer;
    workBuffer->frameIndex = workBuffer->frameCount;
    memset(workBuffer->sampleData, 0, MaxWorkBufferByteCount);
    if (settings->maxLatencyMs != 0) {
        const unsigned DelayByteCount = ua_gContext.delayLine.sampleCount * sizeof(float);
        ua_gContext.delayLine.buffer = settings->allocateFunction(DelayByteCount);
        memset(ua_gContext.delayLine.buffer, 0, DelayByteCount);

        ua_gContext.renderToBufferFunction = RenderToBufferWithDelayLine;
    }
    else {
        ua_gContext.delayLine.buffer = NULL;

        ua_gContext.renderToBufferFunction = RenderToBuffer;
    }
    
#ifdef __APPLE__
    ua_init_macos(ua_InitParams);
#elif _WIN32
    ua_init_windows(ua_InitParams);
#endif

    return ua_gContext.deviceSampleRate;
}

void ua_term(void) {
#ifdef __APPLE__
    ua_term_macos();
#elif _WIN32
    ua_term_windows();
#endif
}

#ifdef __APPLE__

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <math.h>

// Callback function that fills audio buffers with data
static OSStatus RenderCallback(void *inRefCon,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList *ioData) {
    ua_Context* context = (ua_Context*)inRefCon;
    ua_WorkBuffer* workBuffer = &context->workBuffer;

    unsigned frame = 0;
    unsigned framesLeft = inNumberFrames;
    while (framesLeft) {
        if (workBuffer->frameIndex >= workBuffer->frameCount) {
            workBuffer->frameIndex = 0;
            context->renderToBufferFunction(workBuffer->sampleData);
        }
        
        const unsigned WorkFrames = workBuffer->frameCount - workBuffer->frameIndex;
        const unsigned FramesToProcess = WorkFrames < framesLeft ? WorkFrames : framesLeft;
        for (UInt32 i = 0; i < FramesToProcess; ++i) {
            for (UInt32 channel = 0; channel < ioData->mNumberBuffers; ++channel) {
                float* buffer = (float*)ioData->mBuffers[channel].mData;
                buffer[frame + i] = context->workBuffer[(workBuffer->frameIndex + i) 
                                  * context->settings.maxChannelCount + channel];
            }
        }
        
        framesLeft -= FramesToProcess;
        frame += FramesToProcess;
        context->workBufferFrameIndex += FramesToProcess;
    }
    
    return noErr;
}

AudioComponentInstance auHAL;

AudioDeviceID ua_get_default_output_device() {
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    UInt32 propertySize = sizeof(AudioDeviceID);
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                              0, NULL, &propertySize, &deviceID);
    if (err != noErr) {
        // Handle error
        return kAudioDeviceUnknown;
    }
    return deviceID;
}

// Helper to check errors (simplified)
void CheckError(OSStatus err, const char* message) {
    if (err != noErr) {
        printf("%s : %d\n", message, err);
    }
}

void ua_test_ranges(AudioDeviceID deviceId)
{
    // TODO: THIS NEEDS CLEANUP. A LOT OF CLEANUP.
    // NEED TO PROVIDE VALID SAMPLE RATES BACK
    // TO THE USER TO SELECT.
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
    propertyAddress.mScope = kAudioObjectPropertyScopeInput;
    propertyAddress.mElement = kAudioObjectPropertyElementMain;
    UInt32 propertySize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(deviceId, &propertyAddress,
                                                     0, NULL, &propertySize);
    UInt32 numRanges = propertySize / sizeof(AudioValueRange);
    
    AudioValueRange* ranges = (AudioValueRange*)malloc(propertySize);
    status = AudioObjectGetPropertyData(deviceId, &propertyAddress,
                                        0, NULL, &propertySize, ranges);

    printf("macOS permits these sample rates:\n");
    for (UInt32 i = 0; i < numRanges; ++i)
    {
        printf("Min: %f Max: %f\n", ranges[i].mMinimum, ranges[i].mMaximum);
    }
    free(ranges);
}

void ua_init_macos(ua_Settings* ua_InitParams) {
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    OSStatus s = AudioComponentInstanceNew(comp, &auHAL);
    s = AudioUnitInitialize(auHAL);
    AudioDeviceID outputDeviceId = ua_get_default_output_device();
    s = AudioUnitSetProperty(auHAL, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
                             0, &outputDeviceId, sizeof(outputDeviceId));
    
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = RenderCallback;
    callbackStruct.inputProcRefCon = &ua_gContext;
    
    // TODO: in the future, the API should probably
    // only get the closest available frames per buffer
    // and sample rate, instead of error on mismatch.
    Float64 targetSampleRate = ua_InitParams->renderSampleRate; // now this is busted
    
    /*
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    Float64 newSampleRate = 0.0;
    s = AudioObjectSetPropertyData(ua_get_default_output_device(), &address, 
                                        0, NULL, sizeof(Float64), &newSampleRate);
    */
    
    ua_test_ranges(outputDeviceId);
    
    /*UInt32 enableOutput = 1;
    const AudioUnitElement AU_OUTPUT_BUS = 0;
    s = AudioUnitSetProperty(auHAL, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
                                           AU_OUTPUT_BUS, &enableOutput, sizeof(enableOutput));*/
    s = AudioUnitSetProperty(auHAL, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input,
                                  0, &targetSampleRate, sizeof(targetSampleRate));
    s = AudioUnitSetProperty(auHAL, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
                                  0, &callbackStruct, sizeof(callbackStruct));
    UInt32 F64Size = sizeof(Float64);
    s = AudioUnitGetProperty(auHAL, kAudioUnitProperty_SampleRate, kAudioUnitScope_Input,
                             0, &targetSampleRate, &F64Size);
    // status = AudioUnitSetProperty(auHAL, kAudioUnitProperty_ElementCount)
    
    assert((unsigned)targetSampleRate == ua_InitParams->renderSampleRate); // busted too
    printf("Output sample rate is now at %f Hz", targetSampleRate);
    
    // 4. Start the Audio Unit
    AudioOutputUnitStart(auHAL);
}

void ua_term_macos(void) {
    AudioOutputUnitStop(auHAL);
    AudioUnitUninitialize(auHAL);
    AudioComponentInstanceDispose(auHAL);
}


#elif _WIN32
#define WIN32_LEAN_AND_MEAN

// At /Wall levels of warnings,
// these must be defined for MS.
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
typedef struct {
    BYTE* rawData;
    XAUDIO2_BUFFER xAudioBuffer;
} ua_AudioBuffer;

#define UA_RENDER_BUFFER_COUNT 2
ua_AudioBuffer ua_buffers[UA_RENDER_BUFFER_COUNT] = { NULL };
unsigned ua_bufferIndex = 0;

void XAudio2OnBufferEnd(IXAudio2VoiceCallback* This, void* pBufferContext) {
    (void)This;
    (void)pBufferContext;
    ua_gContext.renderToBufferFunction((float*)ua_buffers[ua_bufferIndex].rawData);
    IXAudio2SourceVoice_SubmitSourceBuffer(ua_xAudio2SourceVoice,
        &(ua_buffers[ua_bufferIndex].xAudioBuffer), NULL);
    ua_bufferIndex = (ua_bufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
}

void XA2OSE(IXAudio2VoiceCallback* pXa2) { (void)pXa2; } // unused stubs
void XA2OVPPE(IXAudio2VoiceCallback* pXa2) { (void)pXa2; }
void XA2OVPPS(IXAudio2VoiceCallback* pXa2, UINT32 s) { (void)pXa2; (void)s; }
void XA2OBS(IXAudio2VoiceCallback* pXa2, void* pCtx) { (void)pXa2; (void)pCtx; }
void XA2OLE(IXAudio2VoiceCallback* pXa2, void* pCtx) { (void)pXa2; (void)pCtx; }
void XA2OVE(IXAudio2VoiceCallback* pXa2, void* pCtx, HRESULT result) { 
    (void)pXa2; (void)pCtx; (void)result;
}

IXAudio2VoiceCallback xAudio2Callbacks = {
    .lpVtbl = &(IXAudio2VoiceCallbackVtbl) {
        .OnStreamEnd = XA2OSE,
        .OnVoiceProcessingPassEnd = XA2OVPPE,
        .OnVoiceProcessingPassStart = XA2OVPPS,
        .OnBufferEnd = XAudio2OnBufferEnd,
        .OnBufferStart = XA2OBS,
        .OnLoopEnd = XA2OLE,
        .OnVoiceError = XA2OVE
    }
};

void ua_init_windows(ua_Settings* settings) {
    HRESULT r;
    // per Microsoft, first param must be NULL
    UA_CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED)); 
    // per Microsoft, param 2 must be 0
    UA_CHECK(XAudio2Create(&ua_xAudio2, 0, XAUDIO2_USE_DEFAULT_PROCESSOR));

    UA_CHECK(IXAudio2_CreateMasteringVoice(ua_xAudio2, &ua_xAudio2MasterVoice, 
        XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
        0, // no flags
        NULL, // use default device
        NULL, // no effects
        AudioCategory_GameMedia
    ));
    const WORD BytesPerSample = sizeof(float);
    const ua_SampleRate SampleRate = ua_gContext.deviceSampleRate;
    WAVEFORMATEX waveFormat = {
        .wFormatTag = WAVE_FORMAT_IEEE_FLOAT,
        .nChannels = UA_CHANNEL_COUNT,
        .nSamplesPerSec = SampleRate,
        .nAvgBytesPerSec = SampleRate * UA_CHANNEL_COUNT * BytesPerSample,
        .nBlockAlign = UA_CHANNEL_COUNT * BytesPerSample,
        .wBitsPerSample = BytesPerSample * 8,
        .cbSize = 0 // set to zero for PCM or IEEE float
    };
    const float DefaultPitchRatio = 1.f;
    UA_CHECK(IXAudio2_CreateSourceVoice(ua_xAudio2, &ua_xAudio2SourceVoice, &waveFormat, 
        XAUDIO2_VOICE_NOPITCH, DefaultPitchRatio, &xAudio2Callbacks, NULL, NULL // no sends/effects
    ));
    IXAudio2SourceVoice_Start(ua_xAudio2SourceVoice, 0, XAUDIO2_COMMIT_NOW);

    ua_bufferIndex = 0;
    const unsigned short FramesPerBuffer = settings->maxFramesPerBuffer;
    const unsigned BufferByteCount = FramesPerBuffer * UA_CHANNEL_COUNT * sizeof(float);
    for (int i = 0; i < UA_RENDER_BUFFER_COUNT; ++i) {
        ua_AudioBuffer* ab = &ua_buffers[ua_bufferIndex];
        *ab = (const ua_AudioBuffer){ 0 };
        ab->rawData = settings->allocateFunction(BufferByteCount);
        memset(ab->rawData, 0, BufferByteCount);
        ab->xAudioBuffer.AudioBytes = BufferByteCount;
        ab->xAudioBuffer.pAudioData = (const BYTE*)ua_buffers[i].rawData;
        IXAudio2SourceVoice_SubmitSourceBuffer(ua_xAudio2SourceVoice, &ab->xAudioBuffer, NULL);
        ua_bufferIndex = (ua_bufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
    }
}

void ua_term_windows(void) {
    IXAudio2SourceVoice_DestroyVoice(ua_xAudio2SourceVoice);
    ua_xAudio2SourceVoice = NULL;
    IXAudio2MasteringVoice_DestroyVoice(ua_xAudio2MasterVoice);
    ua_xAudio2MasterVoice = NULL;
    IXAudio2_StopEngine(ua_xAudio2);
    IXAudio2_Release(ua_xAudio2);
    ua_xAudio2 = NULL;

    CoUninitialize();

    for (int i = 0; i < UA_RENDER_BUFFER_COUNT; ++i) {
        ua_gContext.settings.freeFunction(ua_buffers[ua_bufferIndex].rawData);
        ua_bufferIndex = (ua_bufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
    }

    if (ua_gContext.workBuffer.sampleData != NULL) {
        ua_gContext.settings.freeFunction(ua_gContext.workBuffer.sampleData);
        ua_gContext.workBuffer.sampleData = NULL;
    }

    if (ua_gContext.delayLine.buffer != NULL) {
        ua_gContext.settings.freeFunction(ua_gContext.delayLine.buffer); 
        ua_gContext.delayLine.buffer = NULL;
    }
}

#endif

#ifdef __cplusplus
}
#endif
