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

#ifndef EXPORT_MICRO_AUDIO_LIBRARY
#define EXPORT_MICRO_AUDIO_LIBRARY
#endif
#include "ua_api.h"
#if _DEBUG
#include <stdio.h>
#endif
#undef EXPORT_MICRO_AUDIO_LIBRARY

#if _WIN32
void ua_init_windows(ua_InitParams* ua_InitParams);
void ua_term_windows(void);
#endif

void ua_init(ua_InitParams* ua_InitParams)
{
#if _WIN32
    ua_init_windows(ua_InitParams);
#endif
}

void ua_term(void)
{
#if _WIN32
    ua_term_windows();
#endif
}





#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <mmdeviceapi.h>
#include <Audioclient.h>
#undef WIN32_LEAN_AND_MEAN

#if _DEBUG
#define LOG_ERROR(x) printf(__FUNCTION__": %s failed!\n", #x);
#else
#define LOG_ERROR(x)
#endif
#define UA_CHECK(x) do { r = (x); if (!SUCCEEDED(r)) { LOG_ERROR((x)) return; } } while(0)

void ua_init_windows(ua_InitParams* ua_InitParams)
{
    // I don't understand why Windows has to be like this, but the CLSID_MMDeviceEnumerator and IID_IMMDeviceEnumerator GUID
    // do not get defined anywhere when compiling for C language. Therefore, I define them here. Hope they don't change!
    const GUID mmClassGuid = (GUID){ 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
    const GUID mmInterfaceGuid = (GUID){ 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
    
    HRESULT r;
    const GUID audioClient3Guid = (GUID){ 0x7ED4EE07, 0x8E67, 0x4CD4, { 0x8C, 0x1A, 0x2B, 0x7A, 0x59, 0x87, 0xAD, 0x42 } };
    UA_CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED));
    IMMDeviceEnumerator* deviceEnumerator;
    UA_CHECK(CoCreateInstance(&mmClassGuid, NULL, CLSCTX_ALL, &mmInterfaceGuid, (void**)&deviceEnumerator));
    IMMDevice* device;
    UA_CHECK(deviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(deviceEnumerator, eRender, eConsole, &device));
    IAudioClient3* audioClient;
    UA_CHECK(device->lpVtbl->Activate(device, &audioClient3Guid, CLSCTX_ALL, NULL, &audioClient));

    // set up audio format.
    WAVEFORMATEXTENSIBLE w = { 0 };
    const unsigned BitsPerFloat = 32;
    const unsigned ChannelCount = 2;
    const unsigned BitsPerByte = 8;
    w.Samples.wValidBitsPerSample = BitsPerFloat;
    w.dwChannelMask = KSAUDIO_SPEAKER_DIRECTOUT;
    w.Format.wBitsPerSample = BitsPerFloat;
    w.Format.cbSize = 22;
    w.Format.nChannels = ChannelCount; // TODO: LISTEN TO OUTPUT DEVICE CHANNEL COUNT
    w.Format.nSamplesPerSec = ua_InitParams->renderSampleRate;
    w.Format.nBlockAlign = ChannelCount * (BitsPerFloat / BitsPerByte);
    w.Format.nAvgBytesPerSec = w.Format.nSamplesPerSec * w.Format.nBlockAlign;
    w.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    const GUID fpGuid = { STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT };
    memcpy(&w.SubFormat, &fpGuid, sizeof(GUID));
    WAVEFORMATEX* targetFormat = &w;

    // REFERENCE_TIME defaultInterval;
    REFERENCE_TIME minimumInterval;
    audioClient->lpVtbl->GetDevicePeriod(audioClient, NULL, &minimumInterval);
    
    WAVEFORMATEX* closest;
    r = audioClient->lpVtbl->IsFormatSupported(audioClient, AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&w, &closest);
    if (r != S_OK && closest != NULL)
    {
        targetFormat = closest;
    }

    UA_CHECK(audioClient->lpVtbl->Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minimumInterval, 0, targetFormat, NULL));
    HANDLE bufferReadyHandle = CreateEvent(NULL, 0, 0, NULL);
    if (!bufferReadyHandle)
    {
        printf("Huh\n");
        return;
    }
    UA_CHECK(audioClient->lpVtbl->SetEventHandle(audioClient, bufferReadyHandle));
    const GUID renderClientGuid = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
    
    IAudioRenderClient* renderClient;
    UA_CHECK(audioClient->lpVtbl->GetService(audioClient, &renderClientGuid, &renderClient));
    unsigned framesPerBuffer;
    UA_CHECK(audioClient->lpVtbl->GetBufferSize(audioClient, &framesPerBuffer));
    float* buffer;
    UA_CHECK(renderClient->lpVtbl->GetBuffer(renderClient, framesPerBuffer, (BYTE**)&buffer));
    UA_CHECK(renderClient->lpVtbl->ReleaseBuffer(renderClient, framesPerBuffer, AUDCLNT_BUFFERFLAGS_SILENT));
    UA_CHECK(audioClient->lpVtbl->Start(audioClient));
    unsigned paddingFrameCount;
    
    // TODO: set this up in a breakout thread.
    for (;;)
    {
        WaitForSingleObject(bufferReadyHandle, INFINITE);
        audioClient->lpVtbl->GetCurrentPadding(audioClient, &paddingFrameCount);
        unsigned targetFramesToRender = framesPerBuffer - paddingFrameCount;
        printf("Padding %d ToRender %d\n", paddingFrameCount, targetFramesToRender);
        if (S_OK == renderClient->lpVtbl->GetBuffer(renderClient, targetFramesToRender, (BYTE**)&buffer))
        {
            for (unsigned channel = 0; channel < ChannelCount; ++channel)
            {
                for (unsigned frame = 0; frame < targetFramesToRender; ++frame)
                {
                    buffer[frame * ChannelCount + channel] = 0.05f * ((float)frame / (float)targetFramesToRender) - 0.025f;
                }
            }
            renderClient->lpVtbl->ReleaseBuffer(renderClient, targetFramesToRender, 0);
        }
    }

    if (closest != NULL)
    {
        CoTaskMemFree(closest);
    }

    CloseHandle(bufferReadyHandle);
}

void ua_term_windows(void)
{
    CoUninitialize();
}

#endif





int main(void)
{
    ua_InitParams params;
    params.maxFramesPerRenderBuffer = 256;
    params.renderSampleRate = 48000;
    params.renderCallback = NULL;
    ua_init_windows(&params);
    printf("huh");
}

#ifdef __cplusplus
}
#endif
