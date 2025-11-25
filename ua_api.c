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


#ifdef __cplusplus
extern "C"
{
#endif

#ifndef UA_EXPORT_MICRO_AUDIO_LIBRARY
#define EXPORT_MICRO_AUDIO_LIBRARY
#endif
#include "ua_api.h"
#include <core/memory.h>
#ifdef _DEBUG
#include <stdio.h>
#endif
#undef UA_EXPORT_MICRO_AUDIO_LIBRARY

#if _WIN32
void ua_init_windows(ua_Settings* ua_InitParams);
void ua_term_windows(void);
#endif

void ua_init(ua_Settings* ua_InitParams) {
    if (ua_InitParams->allocateFunction == NULL) {
        ua_InitParams->allocateFunction = malloc;
    }
    if (ua_InitParams->freeFunction == NULL) {
        ua_InitParams->freeFunction = free;
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

// TODO: this should not be a define
#define UA_RENDER_CHANNEL_COUNT 2

#ifdef _DEBUG
#define LOG_ERROR(x) printf(__FUNCTION__": %s failed!\n", #x);
#else
#define LOG_ERROR(x)
#endif
#define UA_CHECK(x) do { r = (x); if (!SUCCEEDED(r)) { LOG_ERROR((x)) return; } } while(0)

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

ua_Settings ua_settings;
void XAudio2OnBufferEnd(IXAudio2VoiceCallback* This, void* pBufferContext) {
    (void)This;
    (void)pBufferContext;
    ua_settings.renderCallback((float*)ua_renderBuffers[ua_renderBufferIndex].rawData, ua_settings.maxFramesPerRenderBuffer, UA_RENDER_CHANNEL_COUNT);
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
    ua_settings = *settings;
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
        .nSamplesPerSec = ua_settings.renderSampleRate,
        .nAvgBytesPerSec = ua_settings.renderSampleRate * UA_RENDER_CHANNEL_COUNT * BytesPerSample,
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
    const unsigned RenderBufferByteCount = ua_settings.maxFramesPerRenderBuffer * UA_RENDER_CHANNEL_COUNT * sizeof(float);
    for (int i = 0; i < UA_RENDER_BUFFER_COUNT; ++i)
    {
        ua_AudioBuffer* ab = &ua_renderBuffers[ua_renderBufferIndex];
        *ab = (const ua_AudioBuffer){ 0 };
        ab->rawData = ua_settings.allocateFunction(RenderBufferByteCount);
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
        ua_settings.freeFunction(ua_renderBuffers[ua_renderBufferIndex].rawData);
        ua_renderBufferIndex = (ua_renderBufferIndex + 1) % UA_RENDER_BUFFER_COUNT;
    }
}

#endif

#ifdef __cplusplus
}
#endif