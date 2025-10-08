# micro-audio
An ultra-simple audio library focused on the KISS principles.

## What's the point of this?
I'm a game audio programmer by profession, and needed a super basic audio library with which to build more complex sound tools, something closer to portAudio than, say, Wwise or FMOD.

The closest library I could find to my use-case was one called miniaudio, but I wanted to really dig down to the absolute bare minimum requirements. Hence, the name of this library!

## Features
* Open an audio endpoint using the current 'default output device'.
* Render whatever the heck you want into the output buffer via a callback.
* Render at your own sample rate, channel count, and even frame count.
* Provide one (and only one) implementation of this for each major platform.

The goal is to provide 'sane' behaviors for these (this?) feature(s). For example:
* The implementation per platform should have reasonable latency, and recover from all render hiccups / starvations.
* If the default output device changes mid-playback, we should detect this and switch to the new default.
* If we can't find an output device at all, we should treat the target output as a void sink, so program logic based on the audio playhead can still function.
* If you render at a sample rate different from the target output device, we should automatically upsample / downsample the buffer for you.

## Constraints
* Only float32 samples are supported.
* No exclusive mode (i.e. block all other apps from playing sound).
* No audio input. Not yet at least.

To be continued . . .