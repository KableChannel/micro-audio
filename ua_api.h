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


#ifndef __MICRO_AUDIO_API
#define __MICRO_AUDIO_API

#if defined(_WIN32) && defined(BUILD_SHARED_LIBS)
    #if defined(EXPORT_MICRO_AUDIO_LIBRARY)
        #define MICRO_AUDIO_API __declspec(dllexport)
    #else // import harmony library
        #define MICRO_AUDIO_API __declspec(dllimport)
    #endif // defined(EXPORT_HARMONY_LIBRARY)
#else
    #define MICRO_AUDIO_API
#endif // defined(_WIN32)

typedef void (*ua_RenderCallback)(float* /* buffer */, unsigned /* frameCount */, unsigned /* channelCount */);

typedef struct ua_Settings {
    void* (*allocateFunction)(unsigned);
    void (*freeFunction)(void*);
	ua_RenderCallback renderCallback;
	unsigned renderSampleRate;
	unsigned short maxFramesPerRenderBuffer;
    unsigned short maxLatencyMs;
    unsigned short maxChannelCount;
    unsigned char _RESERVED[6];
} ua_Settings;

MICRO_AUDIO_API void ua_init(ua_Settings* ua_InitParams);
MICRO_AUDIO_API void ua_term(void);

#endif // __MICRO_AUDIO_API
