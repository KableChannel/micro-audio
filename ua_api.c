// Copyright (c) Caleb Klomparens
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#ifndef UA_EXPORT_MICRO_AUDIO_LIBRARY
#define EXPORT_MICRO_AUDIO_LIBRARY
#endif
#include "ua_api.h"
#include <stddef.h>
#ifdef _DEBUG
#include <stdio.h>
#endif
#undef UA_EXPORT_MICRO_AUDIO_LIBRARY

#if _WIN32
void ua_init_windows(ua_Settings* ua_InitParams);
void ua_term_windows(void);
#endif

#ifdef _DEBUG
#define UA_LOG_ERROR(x) printf(__FUNCTION__": %s failed!\n", #x);
#else
#define UA_LOG_ERROR(x)
#endif

void ua_init(ua_Settings* ua_InitParams) {
    if (ua_InitParams->allocateFunction == NULL) {
        UA_LOG_ERROR(ua_InitParams->allocateFunction != NULL);
        return;
    }
    if (ua_InitParams->freeFunction == NULL) {
        UA_LOG_ERROR(ua_InitParams->freeFunction != NULL);
        return;
    }
#if _WIN32
    ua_init_windows(ua_InitParams);
#endif
}

void ua_term(void) {
#if _WIN32
    ua_term_windows();
#endif
}

#if _WIN32
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

typedef struct {
    float* buffer;
    unsigned readWriteIndex;
    unsigned sampleCount;
} ua_DelayLine;

typedef struct {
    ua_Settings settings;
    float* workBuffer;
    ua_DelayLine delayLine;
    void (*renderToBuffer)(float*);
} ua_Context;
ua_Context ua_gContext;

// TODO: this should not be a define
#define UA_RENDER_CHANNEL_COUNT 2

void RenderToBuffer(float* targetBuffer) {
    ua_gContext.settings.renderCallback(targetBuffer, ua_gContext.settings.maxFramesPerRenderBuffer, UA_RENDER_CHANNEL_COUNT);
}

void ApplyDelayLine(float* targetBuffer)
{
    const ua_Settings* settings = &ua_gContext.settings;
    ua_DelayLine* delay = &ua_gContext.delayLine;
    const float* inBuffer = ua_gContext.workBuffer;
    const unsigned SampleCount = settings->maxFramesPerRenderBuffer * settings->maxChannelCount;
    for (unsigned i = 0; i < SampleCount; ++i)
    {
        targetBuffer[i] = delay->buffer[delay->readWriteIndex];
        delay->buffer[delay->readWriteIndex] = inBuffer[i];
        delay->readWriteIndex = (delay->readWriteIndex + 1) % delay->sampleCount;
    }
}

void RenderToBufferWithDelayLine(float* targetBuffer) {
    ua_gContext.settings.renderCallback(ua_gContext.workBuffer, ua_gContext.settings.maxFramesPerRenderBuffer, UA_RENDER_CHANNEL_COUNT);
    ApplyDelayLine(targetBuffer);
}


#define UA_CHECK(x) do { r = (x); if (!SUCCEEDED(r)) { UA_LOG_ERROR((x)) return; } } while(0)

IXAudio2* ua_xAudio2;
IXAudio2MasteringVoice* ua_xAudio2MasterVoice;
IXAudio2SourceVoice* ua_xAudio2SourceVoice;
typedef struct {
    BYTE* rawData;
    XAUDIO2_BUFFER xAudioBuffer;
    char _RESERVED[4];
} ua_AudioBuffer;

#define UA_RENDER_BUFFER_COUNT 2
ua_AudioBuffer ua_renderBuffers[UA_RENDER_BUFFER_COUNT] = { NULL };
unsigned ua_renderBufferIndex = 0;

void XAudio2OnBufferEnd(IXAudio2VoiceCallback* This, void* pBufferContext) {
    (void)This;
    (void)pBufferContext;
    ua_gContext.renderToBuffer((float*)ua_renderBuffers[ua_renderBufferIndex].rawData);
    IXAudio2SourceVoice_SubmitSourceBuffer(ua_xAudio2SourceVoice, &(ua_renderBuffers[ua_renderBufferIndex].xAudioBuffer), NULL);
    ua_renderBufferIndex = (ua_renderBufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
}

void XAudio2OnStreamEnd(IXAudio2VoiceCallback* This) { (void)This; }
void XAudio2OnVoiceProcessingPassEnd(IXAudio2VoiceCallback* This) { (void)This; }
void XAudio2OnVoiceProcessingPassStart(IXAudio2VoiceCallback* This, UINT32 SamplesRequired) { (void)This; (void)SamplesRequired; }
void XAudio2OnBufferStart(IXAudio2VoiceCallback* This, void* pBufferContext) { (void)This; (void)pBufferContext; }
void XAudio2OnLoopEnd(IXAudio2VoiceCallback* This, void* pBufferContext) { (void)This; (void)pBufferContext; }
void XAudio2OnVoiceError(IXAudio2VoiceCallback* This, void* pBufferContext, HRESULT Error) { (void)This; (void)pBufferContext; (void)Error; }

IXAudio2VoiceCallback xAudio2Callbacks = {
    .lpVtbl = &(IXAudio2VoiceCallbackVtbl) {
        .OnStreamEnd = XAudio2OnStreamEnd,
        .OnVoiceProcessingPassEnd = XAudio2OnVoiceProcessingPassEnd,
        .OnVoiceProcessingPassStart = XAudio2OnVoiceProcessingPassStart,
        .OnBufferEnd = XAudio2OnBufferEnd,
        .OnBufferStart = XAudio2OnBufferStart,
        .OnLoopEnd = XAudio2OnLoopEnd,
        .OnVoiceError = XAudio2OnVoiceError
    }
};

void ua_init_windows(ua_Settings* settings) {
    ua_gContext.settings = *settings;

    ua_gContext.delayLine.readWriteIndex = 0;
    ua_gContext.delayLine.sampleCount = (settings->maxLatencyMs * settings->renderSampleRate * settings->maxChannelCount) / 1000;
    if (settings->maxLatencyMs != 0) {
        const unsigned MaxWorkBufferByteCount = sizeof(float) * settings->maxFramesPerRenderBuffer * settings->maxChannelCount;
        ua_gContext.workBuffer = settings->allocateFunction(MaxWorkBufferByteCount);
        memset(ua_gContext.workBuffer, 0, MaxWorkBufferByteCount);

        const unsigned DelayByteCount = ua_gContext.delayLine.sampleCount * sizeof(float);
        ua_gContext.delayLine.buffer = settings->allocateFunction(DelayByteCount);
        memset(ua_gContext.delayLine.buffer, 0, DelayByteCount);

        ua_gContext.renderToBuffer = RenderToBufferWithDelayLine;
    }
    else {
        ua_gContext.workBuffer = NULL;
        ua_gContext.delayLine.buffer = NULL;

        ua_gContext.renderToBuffer = RenderToBuffer;
    }

    HRESULT r;
    UA_CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED)); // per Microsoft, first param must be NULL
    UA_CHECK(XAudio2Create(&ua_xAudio2, 0, XAUDIO2_USE_DEFAULT_PROCESSOR)); // per Microsoft, param 2 must be 0

    UA_CHECK(IXAudio2_CreateMasteringVoice(ua_xAudio2, &ua_xAudio2MasterVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
        0, // no flags
        NULL, // use default device
        NULL, // no effects
        AudioCategory_GameMedia
    ));
    const WORD BytesPerSample = sizeof(float);
    WAVEFORMATEX waveFormat = {
        .wFormatTag = WAVE_FORMAT_IEEE_FLOAT,
        .nChannels = UA_RENDER_CHANNEL_COUNT,
        .nSamplesPerSec = settings->renderSampleRate,
        .nAvgBytesPerSec = settings->renderSampleRate * UA_RENDER_CHANNEL_COUNT * BytesPerSample,
        .nBlockAlign = UA_RENDER_CHANNEL_COUNT * BytesPerSample,
        .wBitsPerSample = BytesPerSample * 8,
        .cbSize = 0 // set to zero for PCM or IEEE float
    };
    UA_CHECK(IXAudio2_CreateSourceVoice(ua_xAudio2, &ua_xAudio2SourceVoice, &waveFormat, XAUDIO2_VOICE_NOPITCH,
        1.f, // default pitch ratio
        &xAudio2Callbacks,
        NULL, // no sends
        NULL // no effects
    ));
    IXAudio2SourceVoice_Start(ua_xAudio2SourceVoice, 0, XAUDIO2_COMMIT_NOW);

    ua_renderBufferIndex = 0;
    const unsigned RenderBufferByteCount = settings->maxFramesPerRenderBuffer * UA_RENDER_CHANNEL_COUNT * sizeof(float);
    for (int i = 0; i < UA_RENDER_BUFFER_COUNT; ++i)
    {
        ua_AudioBuffer* ab = &ua_renderBuffers[ua_renderBufferIndex];
        *ab = (const ua_AudioBuffer){ 0 };
        ab->rawData = settings->allocateFunction(RenderBufferByteCount);
        memset(ab->rawData, 0, RenderBufferByteCount);
        ab->xAudioBuffer.AudioBytes = RenderBufferByteCount;
        ab->xAudioBuffer.pAudioData = (const BYTE*)ua_renderBuffers[i].rawData;
        IXAudio2SourceVoice_SubmitSourceBuffer(ua_xAudio2SourceVoice, &ab->xAudioBuffer, NULL);
        ua_renderBufferIndex = (ua_renderBufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
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
        ua_gContext.settings.freeFunction(ua_renderBuffers[ua_renderBufferIndex].rawData);
        ua_renderBufferIndex = (ua_renderBufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
    }

    if (ua_gContext.workBuffer != NULL) {
        ua_gContext.settings.freeFunction(ua_gContext.workBuffer);
        ua_gContext.workBuffer = NULL;
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
