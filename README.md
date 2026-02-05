# A Pico-TTS Speech Provider

This is a Spiel speech provider that features the Pico TTS voice sinthesizer from SVox, included in Android AOSP.

## Build instructions

```sh
meson setup build
meson compile -C build
```

To run the provider without installing:
```sh
meson devenv -C build
./src/speech-provider-picotts
```

