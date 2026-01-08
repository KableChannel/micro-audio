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


#ifndef __MICRO_AUDIO_API
#define __MICRO_AUDIO_API

#if defined(_WIN32) && defined(BUILD_SHARED_LIBS)
    #if defined(EXPORT_MICRO_AUDIO_LIBRARY)
        #define MICRO_AUDIO_API_EXPORT __declspec(dllexport)
    #else // import harmony library
        #define MICRO_AUDIO_API_EXPORT __declspec(dllimport)
    #endif // defined(EXPORT_HARMONY_LIBRARY)
#else
    #define MICRO_AUDIO_API_EXPORT
#endif // defined(_WIN32)
//                                 buffer, # frames, # channels
typedef void (*ua_AudioCallbackFn)(float*, unsigned, unsigned);
typedef void* (*ua_AllocateFn)(unsigned);
typedef void (*ua_FreeFn)(void*);


typedef struct ua_Settings {
    ua_AllocateFn memAllocate;
    ua_FreeFn memFree;
	ua_AudioCallbackFn audioCallback;
	unsigned short framesPerBuffer;
    unsigned short numChannels;
    unsigned short maxLatencyMs;
} ua_Settings;

#define UA_INVALID_SAMPLE_RATE 0
typedef unsigned ua_SampleRate;

MICRO_AUDIO_API_EXPORT ua_SampleRate ua_init(ua_Settings* ua_InitParams);
MICRO_AUDIO_API_EXPORT void ua_term(void);

#endif // __MICRO_AUDIO_API
