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
#undef WIN32_LEAN_AND_MEAN

#if _DEBUG
#define LOG_ERROR(x) printf(__FUNCTION__": %s failed!\n", #x);
#else
#define LOG_ERROR(x)
#endif
#define UA_CHECK(x) do { r = (x); if (!SUCCEEDED(r)) { LOG_ERROR((x)) return; } } while(0)

void ua_init_windows(ua_InitParams* ua_InitParams)
{
    HRESULT r;
    IMMDeviceEnumerator* deviceEnumerator;
    // I don't understand why Windows
    // has to be like this, but the
    // CLSID_MMDeviceEnumerator and 
    // IID_IMMDeviceEnumerator GUID
    // do not get defined anywhere
    // when compiling for C language.
    // Therefore, I define them here.
    GUID mmClassGuid = (GUID){ 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
    GUID mmInterfaceGuid = (GUID){ 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
    UA_CHECK(CoInitializeEx(NULL, COINIT_MULTITHREADED));
    UA_CHECK(CoCreateInstance((CLSID*)&mmClassGuid, NULL, CLSCTX_ALL, (IID*)&mmInterfaceGuid, (void**)&deviceEnumerator));
    IMMDevice* device;
    deviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(deviceEnumerator, eRender, eMultimedia, &device);
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
