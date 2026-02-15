# micro-audio
An ultra-simple audio library focused on the KISS principles, written in C99.

Compiled with all warnings (minus the stupid ones) enabled on Windows / macOS.

# How to use:
### ua_SampleRate ua_init(ua_Settings* ua_InitParams)
* Initialize a dormant audio stream using the default output device.
* Returns the output device's native sample rate, or UA_INVALID_SAMPLE_RATE if initialization fails.
* Audio will not stream until ua_start() is called.

### void ua_start(void)
* Starts the audio stream.
* Fill the buffers with data using your rendering callback.

### void ua_stop(void)
* Stops the audio stream.

### void ua_term(void)
* Releases all resources, also stops audio stream (if playing).

## Features
* Open an audio endpoint using the current 'default output device'.
* Default output device determines the channel count and sample rate.
* Decoupled buffering lets you choose a fixed number of frames per buffer.

## Constraints
* No sample rate conversion. You'll have to render at the sample rate ua_init() returns.
* Only IEEE float32 samples / interleaved channels are currently supported.
* No exclusive mode (i.e. block all other apps from playing sound).
* Built and tested by just by one guy, so try at your own risk.

## Wishlist:
* More decent channel maps (e.g. 5.1 -> stereo).
* Gracefully handle default output device changing at runtime.
* Treat the target output device as a void sink if no device exists, so program logic based on the audio playhead can still function.
* Default audio input, maybe some day...
