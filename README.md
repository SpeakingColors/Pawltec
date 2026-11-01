# Pawltec

A free, open source, cat-themed passive-style tube program equalizer (VST3, Linux first): low shelf boost and cut, high boost with bandwidth, high cut.

<p align="center"><img src="docs/screenshot.png" width="900" alt="Pawltec design mockup"></p>
<p align="center"><sub>The interactive design mockup (screenshot). The plugin window will follow it exactly.</sub></p>

## Status
Design mockup only. The interactive mockup in `docs/mockup/` runs a WebAudio preview of the planned behaviour. The plugin itself is not built yet; the approved mockup will be ported natively to JUCE (VST3, Linux first). No manual yet: it ships with the plugin.

## Try the mockup
Open `docs/mockup/Pawltec.html` in a browser. Nothing is sent anywhere; it works offline (fonts load from Google Fonts when online, with a fallback).

## Controls in the mockup
Low section Freq, Low section Boost, Low section Atten, High boost Freq, High boost Boost, High boost Width, High atten Freq, High atten Atten, Tube stage Tube, Tube stage Output

Every control has a hover info bubble (32 in total).

## Privacy
Collects nothing. No network access.

## Support
<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).
