#pragma once
#include "Widgets.h"
#include "../core/Pawltec.h"
#include "../plugin/PluginProcessor.h"
#include <array>

namespace ui
{
// Everything the views draw, refreshed by the editor timer (message thread only).
constexpr int kCols = 300;     // spectrum columns, 20 Hz to 20 kHz on a log axis

struct Live
{
    double fs = 48000.0;
    pawl::Params p;
    float inPk = 0, outPk = 0;       // linear peaks since the last frame, with decay
    std::array<float, kCols> spIn {}, spOut {};
    Live() { spIn.fill (-120.0f); spOut.fill (-120.0f); }
};

inline float l2d (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-6f)); }

// ---- FFT analyser: 4096 points, Hann window, power averaged into log columns (port of the mockup) ------------
class Analyser
{
public:
    static constexpr int N = 4096;
    Analyser()
    {
        double ws = 0.0;
        for (int i = 0; i < N; ++i) { win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * MathConstants<float>::pi * (float) i / (float) N); ws += win[(size_t) i]; }
        norm = (float) (4.0 / (ws * ws));
        for (int i = 0; i < N / 2; ++i)
        {
            cs[(size_t) i] = std::cos (2.0f * MathConstants<float>::pi * (float) i / (float) N);
            sn[(size_t) i] = -std::sin (2.0f * MathConstants<float>::pi * (float) i / (float) N);
        }
        int bits = 0; while ((1 << bits) < N) ++bits;
        for (int i = 0; i < N; ++i) { int r = 0; for (int j = 0; j < bits; ++j) if (i & (1 << j)) r |= 1 << (bits - 1 - j); rev[(size_t) i] = (uint16_t) r; }
    }

    // ring: N samples, oldest first. prev holds the displayed curve (fast attack, 1.2 dB fall per frame at 60 fps).
    void run (const float* ring, double fs, std::array<float, kCols>& prev, float fallPerFrame)
    {
        for (int i = 0; i < N; ++i) { re[rev[(size_t) i]] = ring[i] * win[(size_t) i]; im[rev[(size_t) i]] = 0.0f; }
        for (int len = 2; len <= N; len <<= 1)
        {
            const int h = len >> 1, step = N / len;
            for (int a = 0; a < N; a += len)
                for (int j = 0; j < h; ++j)
                {
                    const float wr = cs[(size_t) (j * step)], wi = sn[(size_t) (j * step)];
                    const int i1 = a + j, i2 = i1 + h;
                    const float tr = re[(size_t) i2] * wr - im[(size_t) i2] * wi, ti = re[(size_t) i2] * wi + im[(size_t) i2] * wr;
                    re[(size_t) i2] = re[(size_t) i1] - tr; im[(size_t) i2] = im[(size_t) i1] - ti;
                    re[(size_t) i1] += tr; im[(size_t) i1] += ti;
                }
        }
        for (int i = 0; i < N / 2; ++i) pw[(size_t) i] = (re[(size_t) i] * re[(size_t) i] + im[(size_t) i] * im[(size_t) i]) * norm;
        const double lo = std::log10 (20.0), hi = std::log10 (20000.0), bw = fs / (double) N;
        std::array<float, kCols> raw {};
        for (int x = 0; x < kCols; ++x)
        {
            const double f0 = std::pow (10.0, lo + (hi - lo) * x / kCols), f1 = std::pow (10.0, lo + (hi - lo) * (x + 1) / kCols);
            const double b0 = f0 / bw, b1 = f1 / bw;
            float db;
            if (b1 - b0 < 1.0)
            {
                const double b = (b0 + b1) * 0.5; const int i = (int) std::floor (b); const double fr = b - i;
                const float d0 = 10.0f * std::log10 (pw[(size_t) std::min (i, N / 2 - 1)] + 1.0e-14f), d1 = 10.0f * std::log10 (pw[(size_t) std::min (i + 1, N / 2 - 1)] + 1.0e-14f);
                db = d0 + (d1 - d0) * (float) fr;
            }
            else
            {
                float s = 0; int c = 0;
                for (int i = (int) std::ceil (b0); i <= (int) std::floor (b1) && i < N / 2; ++i) { s += pw[(size_t) i]; ++c; }
                db = 10.0f * std::log10 ((c ? s / (float) c : 1.0e-14f) + 1.0e-14f);
            }
            raw[(size_t) x] = db;
        }
        for (int x = 0; x < kCols; ++x)
        {
            const float sm = (raw[(size_t) std::max (0, x - 1)] + raw[(size_t) x] * 2.0f + raw[(size_t) std::min (kCols - 1, x + 1)]) * 0.25f;
            prev[(size_t) x] = sm > prev[(size_t) x] ? sm : prev[(size_t) x] - fallPerFrame;
        }
    }
private:
    std::array<float, N> win {}, re {}, im {}, pw {};
    std::array<float, N / 2> cs {}, sn {};
    std::array<uint16_t, N> rev {};
    float norm = 1.0f;
};

inline String noteName (double f)
{
    static const char* nn[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int r = roundToInt (69.0 + 12.0 * std::log2 (f / 440.0));
    return String (nn[((r % 12) + 12) % 12]) + String ((int) std::floor (r / 12.0) - 1);
}
inline String noteOf (double f)
{
    const double n = 69.0 + 12.0 * std::log2 (f / 440.0);
    const int r = roundToInt (n), c = roundToInt ((n - r) * 100.0);
    return noteName (f) + " " + (c >= 0 ? "+" : "") + String (c) + "c";
}
inline String fmtHz (double f) { return f >= 1000 ? String (f / 1000.0, 2) + " kHz" : (f < 100 ? String (f, 1) : String (roundToInt (f))) + " Hz"; }
inline String fmtHzS (double f) { return f >= 1000 ? String (f / 1000.0, 1) + "k" : String (roundToInt (f)); }

// Base of the display canvases: drawn in the mockup's canvas units, scaled into a bordered box.
class CanvasView : public Component, public TipClient
{
public:
    CanvasView (const Ctx& c, const Live& l, float cw, float ch) : ctx (c), live (l), cvW (cw), cvH (ch) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        g.setColour (t.disp);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.addTransform (AffineTransform::scale (inner.getWidth() / cvW, inner.getHeight() / cvH).translated (inner.getX(), inner.getY()));
            draw (g, t, cvW, cvH);
        }
        g.setColour (t.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
    // Convert a component position to canvas units.
    Point<float> toCanvas (Point<float> p) const
    {
        const float sx = (float) (getWidth() - 2) / cvW, sy = (float) (getHeight() - 2) / cvH;
        return { (p.x - 1.0f) / sx, (p.y - 1.0f) / sy };
    }
protected:
    virtual void draw (Graphics&, const theme::Look&, float w, float h) = 0;
    static void txt (Graphics& g, const String& s, float x, float y, Justification j, Colour c, float px = 11.0f)
    {
        g.setFont (theme::font (theme::FontKind::mono, px));
        g.setColour (c);
        const float w = 200.0f;
        float bx = x;
        if (j.testFlags (Justification::right)) bx = x - w; else if (j.testFlags (Justification::horizontallyCentred)) bx = x - w * 0.5f;
        g.drawText (s, Rectangle<float> (bx, y - 8.0f, w, 16.0f), j.testFlags (Justification::right) ? Justification::centredRight : j.testFlags (Justification::horizontallyCentred) ? Justification::centred : Justification::centredLeft, false);
    }
    static void line (Graphics& g, float x0, float y0, float x1, float y1, float w) { g.drawLine (x0, y0, x1, y1, w); }

    // Hover read-out box shared by both displays.
    static void readout (Graphics& g, const theme::Look& t, float x, float xr, float top, float bot, const String& l1, const String& l2)
    {
        g.setColour (t.ink.withAlpha (0.55f)); line (g, x, top, x, bot, 1.0f);
        const auto f = theme::font (theme::FontKind::mono, 11.0f);
        const float tw = std::max (textWidth (f, l1), textWidth (f, l2)) + 12.0f;
        const float tx = x + 8 + tw > xr ? x - 8 - tw : x + 8;
        g.setColour (Colours::black); g.fillRect (tx, top + 4, tw, 32.0f);
        g.setColour (t.accent); g.drawRect (tx + 0.5f, top + 4.5f, tw - 1.0f, 31.0f, 1.0f);
        txt (g, l1, tx + 6, top + 13, Justification::left, t.ink);
        txt (g, l2, tx + 6, top + 28, Justification::left, t.ink);
    }

    const Ctx& ctx;
    const Live& live;
    float cvW, cvH;
};

// ---- Response curve: the four sections and their total, with draggable handles ---------------------------------
class ResponseView : public CanvasView
{
public:
    static constexpr float W = 630, H = 290, ML = 40, MR = 12, MT = 10, MB = 22, DB = 20;
    ResponseView (const Ctx& c, const Live& l, AudioProcessorValueTreeState& a) : CanvasView (c, l, W, H), apvts (a)
    {
        setMouseCursor (MouseCursor::CrosshairCursor);
    }

    static float rx (double f) { return ML + (float) ((std::log10 (f) - std::log10 (20.0)) / 3.0) * (W - ML - MR); }
    static float ry (double d) { return MT + (float) ((DB - d) / (2 * DB)) * (H - MT - MB); }
    static double rf (float x) { return std::pow (10.0, std::log10 (20.0) + (x - ML) / (W - ML - MR) * 3.0); }

    struct Handle { int sec; double f; float x, y; };
    std::array<Handle, 4> handles() const
    {
        std::array<Handle, 4> hs {};
        for (int s = 0; s < 4; ++s)
        {
            const double f = pawl::sectionMarkerHz ((pawl::Section) s, live.p);
            const double d = pawl::magDb (pawl::sectionBiquad ((pawl::Section) s, live.p, live.fs), f, live.fs);
            hs[(size_t) s] = { s, f, rx (f), ry (std::max (-(double) DB, std::min ((double) DB, d))) };
        }
        return hs;
    }

    bool animated() const { return hasHover || drag.active; }
    void mouseMove (const MouseEvent& e) override
    {
        hover = toCanvas (e.position); hasHover = true;
        const auto hs = handles(); bool onHandle = false;
        for (auto& h : hs) if (std::hypot (h.x - hover.x, h.y - hover.y) < 16.0f) onHandle = true;
        setMouseCursor (onHandle ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor);
        repaint();
    }
    void mouseExit (const MouseEvent&) override { hasHover = false; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const auto p = toCanvas (e.position);
        auto hs = handles();
        struct D { int sec; float d; };
        std::vector<D> cands;
        for (auto& h : hs) { const float d = std::hypot (h.x - p.x, h.y - p.y); if (d < 16.0f) cands.push_back ({ h.sec, d }); }
        if (cands.empty()) return;
        std::sort (cands.begin(), cands.end(), [] (const D& a, const D& b) { return a.d < b.d; });
        drag.active = true;
        drag.sec = cands[0].sec;
        drag.amb = cands.size() > 1 && std::abs (cands[0].d - cands[1].d) < 3.0f && cands[0].sec < 2 && cands[1].sec < 2;
        drag.y0 = p.y; drag.moved = false;
        beginSec (drag.sec);
        drag.v0 = dialOf (drag.sec);
        if (drag.amb) beginSec (cands[1].sec);
        hover = p; hasHover = true;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        const auto p = toCanvas (e.position);
        hover = p; hasHover = true;
        if (! drag.active) return;
        const float dy = p.y - drag.y0;
        if (drag.amb && ! drag.moved && std::abs (dy) > 3.0f)
        {
            drag.sec = dy < 0 ? pawl::LowBoost : pawl::LowAtten;
            drag.v0 = dialOf (drag.sec); drag.moved = true;
        }
        const float pxDb = (H - MT - MB) / (2 * DB);
        double per = dbPerDial (drag.sec);
        if (std::abs (per) < 1.0e-9) per = (drag.sec == pawl::LowBoost || drag.sec == pawl::HiBoost) ? 1.0 : -1.0;
        const double ddb = -dy / pxDb;
        setDial (drag.sec, (float) jlimit (0.0, 10.0, std::round ((drag.v0 + ddb / per) * 10.0) / 10.0));
        // sideways: choose the nearest frequency, once the pointer has left the handle
        const int fIdx = freqParam (drag.sec);
        const int n = fIdx == 0 ? 4 : fIdx == 1 ? 7 : 3;
        int bi = 0; float bd = 1.0e9f;
        for (int i = 0; i < n; ++i)
        {
            const double fq = fIdx == 0 ? pawl::kLowF[i] : fIdx == 1 ? pawl::kHiF[i] : pawl::kAttF[i];
            const float d = std::abs (rx (fq) - p.x);
            if (d < bd) { bd = d; bi = i; }
        }
        if (std::abs (p.x - rx (pawl::sectionMarkerHz ((pawl::Section) drag.sec, live.p))) > 14.0f) setFreq (fIdx, bi);
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (drag.active) { for (auto* q : gestures) q->endChangeGesture(); gestures.clear(); }
        drag.active = false; repaint();
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        const auto p = toCanvas (e.position);
        for (auto& h : handles())
            if (std::hypot (h.x - p.x, h.y - p.y) < 16.0f)
            {
                auto* q = apvts.getParameter (dialId (h.sec));
                q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (0.0f)); q->endChangeGesture();
                break;
            }
    }

protected:
    void draw (Graphics& g, const theme::Look& t, float, float) override
    {
        const float N = W - ML - MR;
        for (int d = -(int) DB; d <= (int) DB; d += 5)
        {
            const float y = ry (d);
            g.setColour ((d == 0 ? t.dim : t.line).withAlpha (d == 0 ? 0.9f : 0.7f));
            line (g, ML, y, W - MR, y, d == 0 ? 1.0f : 0.6f);
            if (d % 10 == 0) txt (g, String (d > 0 ? "+" : "") + String (d), ML - 6, y, Justification::right, t.title);
        }
        static const double fl[10] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        static const char* lt[10] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        for (int i = 0; i < 10; ++i)
        {
            const float x = rx (fl[i]);
            g.setColour (t.line); line (g, x, MT, x, H - MB, 0.6f);
            txt (g, lt[i], i == 0 ? x - 2 : i == 9 ? x + 2 : x, H - MB + 14, i == 0 ? Justification::left : i == 9 ? Justification::right : Justification::horizontallyCentred, t.title);
        }
        const Colour cl[4] = { theme::green, theme::pink, t.yellow, theme::blue };
        std::array<pawl::Biquad, 4> cs;
        for (int s = 0; s < 4; ++s) cs[(size_t) s] = pawl::sectionBiquad ((pawl::Section) s, live.p, live.fs);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (Rectangle<float> (ML, MT, N, H - MT - MB).toNearestInt());
            for (int s = 0; s < 4; ++s)
            {
                Path path;
                for (float x = 0; x <= N; x += 2.0f)
                {
                    const float y = ry (pawl::magDb (cs[(size_t) s], rf (ML + x), live.fs));
                    if (x == 0) path.startNewSubPath (ML + x, y); else path.lineTo (ML + x, y);
                }
                g.setColour (cl[s].withAlpha (0.75f));
                g.strokePath (path, PathStrokeType (1.2f));
            }
            Path total, fill;
            bool first = true;
            for (float x = 0; x <= N; x += 2.0f)
            {
                double tt = 0; for (auto& c : cs) tt += pawl::magDb (c, rf (ML + x), live.fs);
                const float y = ry (tt);
                if (first) { total.startNewSubPath (ML + x, y); fill.startNewSubPath (ML + x, y); first = false; }
                else { total.lineTo (ML + x, y); fill.lineTo (ML + x, y); }
            }
            fill.lineTo (ML + N, ry (0)); fill.lineTo (ML, ry (0)); fill.closeSubPath();
            g.setColour (t.accent.withAlpha (0.16f)); g.fillPath (fill);
            g.setColour (t.ink);
            g.strokePath (total, PathStrokeType (2.6f, PathStrokeType::curved));
        }
        if (live.p.bypass)
        {
            g.setColour (t.ink.withAlpha (0.8f));
            g.setFont (theme::font (theme::FontKind::semi, 14.0f));
            g.drawText ("BYPASSED, the sound is dry", Rectangle<float> (ML, MT + 4, N, 24), Justification::centred, false);
        }
        for (auto& h : handles())
        {
            const bool act = drag.active && drag.sec == h.sec;
            const float r = act ? 7.0f : 5.5f;
            g.setColour (cl[h.sec]); g.fillEllipse (h.x - r, h.y - r, 2 * r, 2 * r);
            g.setColour (Colours::black); g.drawEllipse (h.x - r, h.y - r, 2 * r, 2 * r, 1.5f);
        }
        if (hasHover && hover.x >= ML && hover.x <= W - MR)
        {
            const double f = rf (hover.x);
            const double e = pawl::totalDb (live.p, f, live.fs);
            const int ci = jlimit (0, kCols - 1, roundToInt ((std::log10 (f) - std::log10 (20.0)) / 3.0 * (kCols - 1)));
            const float sp = live.spOut[(size_t) ci];
            readout (g, t, hover.x, W - MR, MT, H - MB, fmtHz (f) + "  " + noteOf (f),
                     "EQ " + String (e >= 0 ? "+" : "") + String (e, 1) + " dB  spec " + (sp < -89 ? String ("-inf") : String (roundToInt (sp))) + " dB");
        }
    }

private:
    static const char* dialId (int sec) { return sec == pawl::LowBoost ? ids::lowB : sec == pawl::LowAtten ? ids::lowA : sec == pawl::HiBoost ? ids::hiB : ids::hiA; }
    static int freqParam (int sec) { return sec < 2 ? 0 : sec == pawl::HiBoost ? 1 : 2; }
    static const char* freqId (int f) { return f == 0 ? ids::lowF : f == 1 ? ids::hiF : ids::attF; }
    float dialOf (int sec) const { return apvts.getRawParameterValue (dialId (sec))->load(); }
    void setDial (int sec, float v) { auto* q = apvts.getParameter (dialId (sec)); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
    void setFreq (int f, int idx) { auto* q = apvts.getParameter (freqId (f)); q->setValueNotifyingHost (q->convertTo0to1 ((float) idx)); }
    void beginSec (int sec)
    {
        for (const char* id : { dialId (sec), freqId (freqParam (sec)) })
        {
            auto* q = apvts.getParameter (id);
            if (std::find (gestures.begin(), gestures.end(), q) == gestures.end()) { q->beginChangeGesture(); gestures.push_back (q); }
        }
    }
    double dbPerDial (int sec) const
    {
        pawl::Params q = live.p;
        if (sec == pawl::LowBoost) q.lowB = 10; else if (sec == pawl::LowAtten) q.lowA = 10; else if (sec == pawl::HiBoost) q.hiB = 10; else q.hiA = 10;
        return pawl::magDb (pawl::sectionBiquad ((pawl::Section) sec, q, live.fs), pawl::sectionMarkerHz ((pawl::Section) sec, live.p), live.fs) / 10.0;
    }

    AudioProcessorValueTreeState& apvts;
    Point<float> hover; bool hasHover = false;
    struct Drag { bool active = false, amb = false, moved = false; int sec = 0; float y0 = 0; float v0 = 0; } drag;
    std::vector<AudioProcessorParameter*> gestures;
};

// ---- Spectrum: before (grey line) and after (coloured fill), with peak labels --------------------------------
class SpectrumView : public CanvasView
{
public:
    static constexpr float W = 346, H = 290, ML = 38, MR = 10, MT = 10, MB = 22, LO = -90;
    SpectrumView (const Ctx& c, const Live& l) : CanvasView (c, l, W, H) { setMouseCursor (MouseCursor::CrosshairCursor); }
    static float sy (float d) { return MT + (0 - d) / (0 - LO) * (H - MT - MB); }

    void mouseMove (const MouseEvent& e) override { hover = toCanvas (e.position); hasHover = true; repaint(); }
    void mouseExit (const MouseEvent&) override { hasHover = false; repaint(); }

    // Called by the editor about 10 times a second.
    void trackPeaks()
    {
        std::vector<std::pair<int, float>> c;
        float mx = -999;
        for (int x = 3; x < kCols - 3; ++x)
        {
            const float v = live.spOut[(size_t) x];
            if (v > mx) mx = v;
            bool ok = v > -62.0f;
            for (int k = -3; k <= 3 && ok; ++k) if (k && live.spOut[(size_t) (x + k)] > v) ok = false;
            if (ok) c.push_back ({ x, v });
        }
        for (auto& p : pk) p.seen = false;
        std::sort (c.begin(), c.end(), [] (auto& a, auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < c.size() && i < 4; ++i)
        {
            const auto cd = c[i];
            bool found = false;
            for (auto& p : pk) if (std::abs (p.x - cd.first) <= 4) { p.x = cd.first; p.v = cd.second; p.seen = true; p.miss = 0; found = true; break; }
            if (found) continue;
            if (pk.size() < 4) pk.push_back ({ cd.first, cd.second, true, 0 });
            else
            {
                auto w = std::min_element (pk.begin(), pk.end(), [] (auto& a, auto& b) { return a.v < b.v; });
                if (cd.second > w->v + 6.0f) *w = { cd.first, cd.second, true, 0 };
            }
        }
        for (auto& p : pk) if (! p.seen) ++p.miss;
        pk.erase (std::remove_if (pk.begin(), pk.end(), [] (auto& p) { return p.miss >= 12; }), pk.end());
        if (mx < -58.0f) pk.clear();
    }

protected:
    void draw (Graphics& g, const theme::Look& t, float, float) override
    {
        const float N = W - ML - MR;
        for (int d = 0; d >= (int) LO; d -= 10)
        {
            const float y = sy ((float) d);
            g.setColour (t.line); line (g, ML, y, W - MR, y, 0.6f);
            if (d % 20 == 0) txt (g, String (d), ML - 6, y, Justification::right, t.title);
        }
        static const double fl[4] = { 20, 100, 1000, 10000 };
        static const char* lt[4] = { "20", "100", "1k", "10k" };
        for (int i = 0; i < 4; ++i)
        {
            const float x = ML + (float) ((std::log10 (fl[i]) - std::log10 (20.0)) / 3.0) * N;
            g.setColour (t.line); line (g, x, MT, x, H - MB, 0.6f);
            txt (g, lt[i], i == 0 ? x - 2 : x, H - MB + 14, i == 0 ? Justification::left : Justification::horizontallyCentred, t.title);
        }
        auto path = [&] (const std::array<float, kCols>& a, bool close)
        {
            Path p;
            for (int x = 0; x < kCols; ++x)
            {
                const float px = ML + x * N / (kCols - 1), py = sy (jlimit (LO, 0.0f, a[(size_t) x]));
                if (x == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            if (close) { p.lineTo (ML + N, H - MB); p.lineTo (ML, H - MB); p.closeSubPath(); }
            return p;
        };
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (Rectangle<float> (ML, MT, N, H - MT - MB).toNearestInt());
            g.setColour (t.accent.withAlpha (0.28f)); g.fillPath (path (live.spOut, true));
            g.setColour (t.accent); g.strokePath (path (live.spOut, false), PathStrokeType (1.4f));
            g.setColour (t.dim); g.strokePath (path (live.spIn, false), PathStrokeType (1.2f));
        }
        // peak labels
        std::vector<Rectangle<float>> boxes;
        auto sorted = pk; std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.x < b.x; });
        const auto pf = theme::font (theme::FontKind::mono, 10.0f);
        for (auto& p : sorted)
        {
            const double f = std::pow (10.0, std::log10 (20.0) + p.x / (double) (kCols - 1) * 3.0);
            const String s = noteName (f) + " " + fmtHzS (f);
            const float w = textWidth (pf, s) + 6.0f, px = ML + p.x * N / (kCols - 1), py = sy (jlimit (LO, 0.0f, p.v));
            const float bx = jlimit (ML + 2, std::max (ML + 2, W - MR - w - 1), px - w / 2), by = std::max (MT + 2, py - 18);
            Rectangle<float> b (bx, by, w, 12);
            bool hit = false;
            for (auto& o : boxes) if (b.expanded (2.0f, 0.0f).intersects (o)) hit = true;
            if (hit) continue;
            boxes.push_back (b);
            g.setColour (Colours::black.withAlpha (0.72f)); g.fillRect (b);
            txt (g, s, bx + 3, by + 6.5f, Justification::left, t.ink, 10.0f);
            g.setColour (t.accent); line (g, px, by + 12, px, py - 2, 1.0f);
        }
        if (hasHover && hover.x >= ML && hover.x <= W - MR)
        {
            const double f = std::pow (10.0, std::log10 (20.0) + (hover.x - ML) / N * 3.0);
            const float xx = (float) ((std::log10 (f) - std::log10 (20.0)) / 3.0 * (kCols - 1));
            const int i = jlimit (0, kCols - 2, (int) std::floor (xx)); const float fr = jlimit (0.0f, 1.0f, xx - i);
            const float sp = live.spOut[(size_t) i] + (live.spOut[(size_t) i + 1] - live.spOut[(size_t) i]) * fr;
            const float si = live.spIn[(size_t) i] + (live.spIn[(size_t) i + 1] - live.spIn[(size_t) i]) * fr;
            readout (g, t, hover.x, W - MR, MT, H - MB, fmtHz (f) + "  " + noteOf (f),
                     "out " + (sp < -89 ? String ("-inf") : String (roundToInt (sp))) + " dB  in " + (si < -89 ? String ("-inf") : String (roundToInt (si))) + " dB");
        }
    }

private:
    struct Pk { int x; float v; bool seen; int miss; };
    std::vector<Pk> pk;
    Point<float> hover; bool hasHover = false;
};

// ---- Meters: in and out, blue below -12 dB, purple to 0 dB, pink above, fixed at their dB positions ----------
class MeterView : public CanvasView
{
public:
    static constexpr float H = 300, T = 8, B = 6, LO = -60, HI = 6;
    MeterView (const Ctx& c, const Live& l) : CanvasView (c, l, 84, H) { setMouseCursor (MouseCursor::PointingHandCursor); }
    static float my (float d) { return T + (HI - d) / (HI - LO) * (H - T - B); }
    static Colour zoneCol (float d) { return d >= 0 ? theme::red : d >= -12 ? theme::purple : theme::blue; }

    // now in ms; levels in dB
    void update (double now)
    {
        const float lv[2] = { l2d (live.inPk), l2d (live.outPk) };
        for (int i = 0; i < 2; ++i)
        {
            if (lv[i] >= -0.05f) clipT[i] = now;
            if (lv[i] > hold[i] || now - holdT[i] > 1200.0) { hold[i] = lv[i]; holdT[i] = now; }
        }
        t = now;
        repaint();
    }
    void clear() { clipT[0] = clipT[1] = 0; hold[0] = hold[1] = -99.0f; }
    float holdDb (int i) const { return hold[i]; }
    void mouseDown (const MouseEvent&) override { clear(); if (onClear) onClear(); }
    std::function<void()> onClear;

protected:
    void draw (Graphics& g, const theme::Look& tk, float, float) override
    {
        const float lv[2] = { l2d (live.inPk), l2d (live.outPk) };
        const float xs[2] = { 13, 55 };
        struct Z { float a, b; Colour c; };
        const Z zones[3] = { { -60, -12, theme::blue }, { -12, 0, theme::purple }, { 0, 6, theme::red } };
        for (int i = 0; i < 2; ++i)
        {
            const float x = xs[i];
            g.setColour (tk.disp); g.fillRect (x, T, 16.0f, H - T - B);
            for (auto& z : zones)
            {
                const float a = std::max (z.a, LO), b = std::min ({ z.b, lv[i], HI });
                if (b > a) { g.setColour (z.c); g.fillRect (x, my (b), 16.0f, my (a) - my (b)); }
            }
            if (hold[i] > LO) { g.setColour (zoneCol (hold[i])); g.fillRect (x, my (std::min (hold[i], HI)) - 1, 16.0f, 2.0f); }
            g.setColour (clipT[i] > 0 && t - clipT[i] < 4000.0 ? theme::red : Colour (0xff2a2c33));
            g.fillRect (x, T - 5, 16.0f, 3.0f);
        }
        for (float d : { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f })
        {
            const float y = my (d);
            g.setColour (tk.dim.withAlpha (0.7f));
            line (g, xs[0] + 16, y, xs[0] + 19, y, 1.0f);
            line (g, xs[1] - 3, y, xs[1], y, 1.0f);
            txt (g, d > 0 ? "+" + String ((int) d) : String ((int) d), 42, y, Justification::horizontallyCentred, tk.title, 10.0f);
        }
    }
private:
    float hold[2] = { -99.0f, -99.0f };
    double holdT[2] = { 0, 0 }, clipT[2] = { 0, 0 }, t = 0;
};
} // namespace ui
