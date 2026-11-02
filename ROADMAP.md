# Pawltec roadmap

## Done in v0.1.0 (test build)
- DSP core with tests (`sh tests/run.sh`), JUCE VST3, native editor ported from the approved mockup
- Presets, resizing, themes, About & Support, installers, PDF manual with interface tour

## Not verified yet
- Installers on real Windows and macOS machines; macOS uninstaller app never run on a Mac
- Flatpak extension in a real Flatpak DAW
- Plugin in real DAWs (only the pluginval validator on Linux)
- Sound by ear on real material; filter shapes are our own model, not hardware measurements
- Window icon on Windows plugin folders (desktop.ini) untested

## Next
- Listening pass and retune of presets and shelf slopes
- Worst-case tube aliasing is about -53.6 dB at tube 10 near 14 to 15 kHz; ADAA or 8x oversampling could improve it
- Optional output safety limiter (none today; big boosts can exceed 0 dBFS)
- Signing and notarisation if funding allows
