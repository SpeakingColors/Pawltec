# Pawltec roadmap

## Done in v0.1.0 (test build)
- DSP core with tests (`sh tests/run.sh`), JUCE VST3, native editor ported from the approved mockup
- Presets, resizing, themes, About & Support, installers, PDF manual with interface tour

## Done in v0.1.1
- Safety limiter at 0 dBFS (32 sample lookahead, 80 ms release, bit exact below the ceiling; host parameter "Safety Limiter", on by default). Latency is now 72 samples.
- First order antiderivative anti-aliasing on the tube stage: worst alias -53.6 dB to -72.5 dB (tube 10, up to 15 kHz sines).

## Not verified yet
- Installers on real Windows and macOS machines; macOS uninstaller app never run on a Mac
- Flatpak extension in a real Flatpak DAW
- Plugin in real DAWs (only the pluginval validator on Linux)
- Sound by ear on real material; filter shapes are our own model, not hardware measurements
- Window icon on Windows plugin folders (desktop.ini) untested

## Next
- Listening pass and retune of presets and shelf slopes
- Signing and notarisation if funding allows
