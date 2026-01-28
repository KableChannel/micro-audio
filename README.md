# micro-audio
An ultra-simple audio library focused on the KISS principles, written in C99.

Compiled with all warnings (minus the stupid ones) enabled on Windows / macOS.

# How to use:
### ua_SampleRate ua_init(ua_Settings* ua_InitParams)
* Opens an audio stream using the default audio output device.
* Provide your audio rendering callback to fill the buffers with data.
* Returns the output device's chosen sample rate.

### void ua_term(void)
* Gracefully closes the default audio stream.

## Features
* Open an audio endpoint using the current 'default output device'.
* Default output device determines the channel count and sample rate.
* Decoupled buffering lets you choose a fixed number of frames per buffer, for consistent processing.

## Constraints
* Only IEEE float32 samples / interleaved channels are currently supported.
* No exclusive mode (i.e. block all other apps from playing sound).
* Currently used just by one person, so use at your own risk.

## Wishlist:
* More decent channel maps (e.g. 5.1 -> stereo).
* Gracefully handle default output device changing at runtime.
* Treat the target output device as a void sink if no device exists, so program logic based on the audio playhead can still function.
* Default audio input, maybe some day...
