#include "PluginEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "Dialogs.h"
#include "Presets.h"

using namespace juce;
using namespace ui;

namespace
{
// ---- the wood cheeks: same three colours and vertical grain as PurrOne ---------------------------
struct Mul32
{
    uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a ^ (a >> 15);
        t *= (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

Image makeWood (uint32_t seed, bool edgeRight)
{
    const int w = 104, h = 1280;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    Mul32 r { seed };
    g.fillAll (Colour (0xff3d2210));
    for (int x = 0; x < w; ++x)
    {
        const double a = r.next();
        g.setColour (Colour::fromFloatRGBA (123 / 255.0f, 74 / 255.0f, 38 / 255.0f, (float) (0.40 * (0.4 + 0.6 * a))));
        g.fillRect (x, 0, 1, h);
        if (r.next() < 0.35)
        {
            const float al = (float) (0.75 * r.next());
            const double y0 = r.next() * h;
            const double len = h * (0.2 + 0.6 * r.next());
            g.setColour (Colour::fromFloatRGBA (90 / 255.0f, 52 / 255.0f, 25 / 255.0f, al));
            g.fillRect ((float) x, (float) y0, 1.0f, (float) len);
        }
        if (r.next() < 0.12)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRect (x, 0, 1, h);
        }
    }
    g.setColour (Colours::black);
    g.fillRect (edgeRight ? 0 : w - 3, 0, 3, h);
    return img;
}

std::unique_ptr<Drawable> makeLogo()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>)";
    if (auto xml = XmlDocument::parse (svg)) return Drawable::createFromSVG (*xml);
    return {};
}

class Cheek : public Component, public TipClient
{
public:
    Cheek (Image i, std::function<void()> reset) : img (std::move (i)), onReset (std::move (reset)) {}
    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onReset) onReset(); }
private:
    Image img;
    std::function<void()> onReset;
};

struct Delay : Timer
{
    std::function<void()> fn;
    void start (int ms) { startTimer (ms); }
    void timerCallback() override { stopTimer(); if (fn) fn(); }
};

class Bubble : public Component
{
public:
    Bubble (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (false, false); setVisible (false); }
    void showAt (const String& s, Rectangle<int> target, int parentW, int parentH)
    {
        text = s;
        AttributedString as = build();
        TextLayout tl; tl.createLayout (as, 282.0f);
        const int tw = (int) std::ceil (std::min (282.0f, tl.getWidth())) + 20, th = (int) std::ceil (tl.getHeight()) + 14;
        int x = jlimit (60, std::max (60, parentW - tw - 60), target.getX());
        int y = target.getBottom() + 6;
        if (y + th > parentH - 4) y = target.getY() - th - 6;
        setBounds (x, y, tw, th);
        setVisible (true);
        toFront (false);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        build().draw (g, r.reduced (10.0f, 7.0f));
    }
private:
    AttributedString build() const
    {
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
        as.append (text, theme::font (theme::FontKind::regular, 14.0f), ctx.t().ink);
        return as;
    }
    const Ctx& ctx;
    String text;
};

class Toast : public Component
{
public:
    Toast (const Ctx&) { setInterceptsMouseClicks (false, false); setVisible (false); done.fn = [this] { setVisible (false); }; }
    void say (const String& s, int parentW, int parentH)
    {
        text = s;
        const int w = (int) textWidth (theme::font (theme::FontKind::regular, 15.0f), s) + 30;
        setBounds ((parentW - w) / 2, parentH - 34 - 28, w, 28);
        setVisible (true); toFront (false); repaint();
        done.start (1600);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::regular, 15.0f), Colours::white);
    }
private:
    String text;
    Delay done;
};

// The static faceplate: wood-free parts (header strip, captions, group frames, labels, meter column, footer).
// Drawn once into an image; it repaints only when the look changes.
class Backdrop : public Component
{
public:
    explicit Backdrop (const Ctx& c) : ctx (c), logo (makeLogo())
    {
        setInterceptsMouseClicks (false, false);
        setBufferedToImage (true);
    }
    void paint (Graphics& g) override
    {
        using namespace layout;
        auto& t = ctx.t();
        g.fillAll (Colours::black);
        g.setColour (t.plate); g.fillRect (rc (hdr).withHeight ((float) H));
        // header
        g.setColour (t.plate2); g.fillRect (rc (hdr));
        g.setColour (t.line); g.fillRect (hdr.x, hdr.h - 1.0f, hdr.w, 1.0f);
        if (logo) logo->drawWithin (g, rc (layout::logo), RectanglePlacement::centred, 1.0f);
        {   // name: Paw + pink l + tec, italic extra bold
            const auto f = theme::font (theme::FontKind::brand, 26.0f);
            const float k = 0.5f;
            const String parts[3] = { "Paw", "l", "tec" };
            float x = rc (name).getX();
            for (int i = 0; i < 3; ++i)
            {
                const float w = textWidth (f, parts[i]) + k * (float) parts[i].length();
                drawText (g, parts[i], { x, name.y, w, name.h }, f, i == 1 ? theme::pink : Colours::white, Justification::centredLeft, k);
                x += w;
            }
        }
        drawText (g, "STUDIO 87", rc (s87), theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centred, 2.0f);
        // displays band
        g.setColour (t.plate2); g.fillRect (rc (disp));
        g.setColour (t.line); g.fillRect (disp.x, disp.y + disp.h - 1.0f, disp.w, 1.0f);
        for (int i = 0; i < 2; ++i)
        {
            const auto& c = caps[i];
            drawText (g, String (c.title).toUpperCase(), rc (c.titleR), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centredLeft, 1.5f);
            for (int k = 0; k < c.nLeg; ++k)
            {
                const auto& l = c.leg[k];
                if (String (l.text).isEmpty()) continue;
                const Colour col = String (l.color) == "ink" ? t.ink : String (l.color) == "g" ? theme::green : String (l.color) == "pink" ? theme::pink
                                 : String (l.color) == "y" ? t.yellow : String (l.color) == "b" ? theme::blue : String (l.color) == "accent" ? t.accent : t.dim;
                g.setColour (col);
                g.fillRect (l.r.x, l.r.y + 6.5f - 1.5f, 9.0f, 3.0f);
                drawText (g, l.text, { l.r.x + 13.0f, l.r.y, l.r.w - 13.0f, l.r.h }, theme::font (theme::FontKind::regular, 11.0f), col, Justification::centredLeft, 1.0f);
            }
        }
        // knob panel
        g.setColour (t.plate); g.fillRect (rc (knobs));
        for (float x = knobs.x; x < knobs.x + knobs.w; x += 4.0f)
        {
            g.setColour (Colours::white.withAlpha (0.012f)); g.fillRect (x, knobs.y, 2.0f, knobs.h);
            g.setColour (Colours::black.withAlpha (0.02f));  g.fillRect (x + 2.0f, knobs.y, 2.0f, knobs.h);
        }
        for (float sx : { knobs.x + 3.0f, knobs.x + knobs.w - 3.0f - 7.0f })
        {
            ColourGradient gr (Colour (0xff8c8f99), sx + 2.45f, knobs.y + 4.0f + 2.45f, Colour (0xff2a2b30), sx + 2.45f + 4.9f, knobs.y + 4.0f + 2.45f, true);
            g.setGradientFill (gr);
            g.fillEllipse (sx, knobs.y + 4.0f, 7.0f, 7.0f);
        }
        for (int gi = 0; gi < 4; ++gi)
        {
            const auto& gp = groups[gi];
            g.setColour (Colours::black.withAlpha (0.14f)); g.fillRoundedRectangle (rc (gp.r), 10.0f);
            g.setColour (t.line); g.drawRoundedRectangle (rc (gp.r).reduced (0.5f), 10.0f, 1.0f);
            // title sits on the frame line with a plate coloured background
            g.setColour (t.plate); g.fillRect (gp.titleR.x, gp.titleR.y, gp.titleR.w, gp.titleR.h);
            const float gx = gp.titleR.x + 6.0f, gy = gp.titleR.y + 1.5f;
            Path tube;
            tube.startNewSubPath (gx + 2, gy + 12); tube.lineTo (gx + 2, gy + 6); tube.quadraticTo (gx + 2, gy + 1, gx + 5.5f, gy + 1);
            tube.quadraticTo (gx + 9, gy + 1, gx + 9, gy + 6); tube.lineTo (gx + 9, gy + 12); tube.closeSubPath();
            tube.startNewSubPath (gx + 4, gy + 12); tube.lineTo (gx + 4, gy + 8);
            tube.startNewSubPath (gx + 7, gy + 12); tube.lineTo (gx + 7, gy + 8);
            g.setColour (t.title); g.strokePath (tube, PathStrokeType (1.1f));
            drawText (g, String (gp.title).toUpperCase(), { gp.titleR.x + 21.0f, gp.titleR.y, gp.titleR.w - 21.0f, gp.titleR.h }, theme::font (theme::FontKind::bold, 13.0f), t.title, Justification::centredLeft, 2.0f);
        }
        for (auto& c : cells)
            drawText (g, String (c.label).toUpperCase(), rc (c.labelR), theme::font (theme::FontKind::bold, 14.0f), t.title, Justification::centred, 2.0f);
        // meter column
        g.setColour (t.plate2); g.fillRect (rc (meters));
        g.setColour (t.line); g.fillRect (meters.x, meters.y, 1.0f, meters.h);
        for (auto& m : meterCaps)
            drawText (g, String (m.text).toUpperCase(), rc (m.r), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centred, 1.5f);
        // footer
        g.setColour (t.plate2); g.fillRect (rc (foot));
        g.setColour (t.line); g.fillRect (foot.x, foot.y, foot.w, 1.0f);
        drawText (g, String ("Studio 87 ") + String::fromUTF8 ("\xc2\xb7") + " Pawltec v" + JucePlugin_VersionString,
                  { footText.x, footText.y, 400.0f, footText.h }, theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredLeft, 1.0f);
    }
private:
    const Ctx& ctx;
    std::unique_ptr<Drawable> logo;
};

// Peak read-outs under the strips and the window size in the footer.
class Readouts : public Component
{
public:
    Readouts (const Ctx& c, const MeterView& m) : ctx (c), meter (m) { setInterceptsMouseClicks (false, false); }
    String sizeText;
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const auto f = theme::font (theme::FontKind::mono, 11.0f);
        for (int i = 0; i < 2; ++i)
        {
            const float d = meter.holdDb (i);
            const auto& r = i == 0 ? layout::pkIn : layout::pkOut;
            drawText (g, d < -80.0f ? String ("-inf") : String (d, 1), rc (r), f, MeterView::zoneCol (d));
        }
        drawText (g, sizeText, rc (layout::winsz).expanded (20.0f, 0.0f).withTrimmedLeft (0.0f), theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
    }
private:
    const Ctx& ctx;
    const MeterView& meter;
};
} // namespace

// =====================================================================================================
class Face : public Component, public DialogHost, private Timer
{
public:
    Face (PawltecProcessor& p, std::function<void()> resetSize)
        : proc (p), onResetSize (std::move (resetSize)),
          backdrop (ctx), resp (ctx, live, p.apvts), spec (ctx, live), meter (ctx, live), readouts (ctx, meter),
          bubble (ctx), toastView (ctx), modal (*this)
    {
        const String lk = proc.getSetting (setting::look, "C").toString();
        ctx.look = lk == "A" ? 'A' : lk == "B" ? 'B' : 'C';
        ctx.tips = (bool) proc.getSetting (setting::tips, true);
        ctx.power = (bool) proc.getSetting (setting::power, false);
        setSize (layout::W, layout::H);
        setWantsKeyboardFocus (true);

        addAndMakeVisible (backdrop);
        backdrop.setBounds (0, 0, layout::W, layout::H);
        addAndMakeVisible (cheekL = new Cheek (makeWood (11, true), onResetSize));
        addAndMakeVisible (cheekR = new Cheek (makeWood (29, false), onResetSize));
        cheekL->setBounds (roundToInt (layout::cheekL.x), 0, roundToInt (layout::cheekL.w), layout::H);
        cheekR->setBounds (roundToInt (layout::cheekR.x), 0, roundToInt (layout::cheekR.w), layout::H);
        cheekL->tip = layout::tip_cheekL; cheekR->tip = layout::tip_cheekL;

        auto* brand = new TipArea(); owned.add (brand);
        addAndMakeVisible (brand);
        brand->setBounds (roundToInt (layout::brand.x), roundToInt (layout::brand.y), roundToInt (layout::brand.w), roundToInt (layout::brand.h));
        brand->tip = layout::tip_brand;

        buildHeader();
        buildDisplays();
        buildKnobs();
        buildMeters();
        addAndMakeVisible (readouts);
        readouts.setBounds (0, 0, layout::W, layout::H);

        addAndMakeVisible (bubble);
        addAndMakeVisible (toastView);
        addChildComponent (modal);
        addMouseListener (this, true);
        tipTimer.fn = [this] { showTip(); };
        tick (1.0f / 60.0f);
        syncName();
        startTimerHz (ctx.power ? 16 : 60);
    }
    ~Face() override { stopTimer(); removeMouseListener (this); }

    void openForTesting (const String& n)
    {
        if (n == "settings") modal.openSettings();
        else if (n == "guide") modal.openGuide();
        else if (n == "about") modal.openAbout();
        else if (n == "presets") modal.openBrowser();
        else if (n == "save") modal.openSave (nullptr);
        else if (n == "export") modal.openExport (nullptr);
        else if (n == "import") modal.openImport();
        else if (n == "tip") { tipHost = knobViews[1]; tipText = layout::cells[1].tip; showTip(); }
    }
    void tickForTesting (int frames) { for (int i = 0; i < frames; ++i) tick (1.0f / 60.0f); }

    // ---- DialogHost --------------------------------------------------------------------------
    PawltecProcessor& processor() override { return proc; }
    presets::Store& store() override { return presetStore; }
    Ctx& context() override { return ctx; }
    String presetName() override { return proc.getSetting (setting::preset, presets::factory()[0].name).toString(); }
    void setPresetName (const String& n) override { proc.setSetting (setting::preset, n); syncName(); }
    void toast (const String& s) override { toastView.say (s, getWidth(), getHeight()); }
    void resetWindow() override { if (onResetSize) onResetSize(); }
    void settingsChanged() override
    {
        startTimerHz (ctx.power ? 16 : 60);
        backdrop.repaint();
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }

    Preset currentValues() override
    {
        Preset p;
        for (int i = 0; i < presets::kCount; ++i) p.v[i] = std::round ((double) real (presets::keys[i]) * 100.0) / 100.0;
        return p;
    }

    void loadPreset (const Preset& p) override
    {
        for (int i = 0; i < presets::kCount; ++i) setReal (presets::keys[i], (float) p.v[i]);
        setPresetName (p.name);
        toast ("Loaded " + p.name);
        tick (1.0f / 60.0f);
    }

    bool dirty() override
    {
        const Preset* base = presetStore.find (presetName());
        if (base == nullptr) base = &presets::factory()[0];
        const Preset now = currentValues();
        for (int i = 0; i < presets::kCount; ++i)
            if (std::abs (now.v[i] - base->v[i]) > 0.011) return true;
        return false;
    }

    // ---- keyboard ----------------------------------------------------------------------------
    bool keyPressed (const KeyPress& k) override
    {
        if (modal.isOpen() || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (c == 'b') toggleBypass();
        else if (c == '[') step (-1);
        else if (c == ']') step (1);
        else return false;
        return true;
    }

    // ---- info bubbles ------------------------------------------------------------------------
    void mouseMove (const MouseEvent& e) override { trackTip (e); }
    void mouseEnter (const MouseEvent& e) override { trackTip (e); }
    void mouseExit (const MouseEvent&) override { hideTip(); }
    void mouseDown (const MouseEvent& e) override
    {
        hideTip();
        if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && ! modal.isOpen()) grabKeyboardFocus();
    }

private:
    // The display refresh stops while the window is not on screen (plugin windows are often hidden or minimised).
    void timerCallback() override
    {
        if (! isShowing()) return;
        tick (1.0f / (float) (ctx.power ? 16 : 60));
    }

    void tick (float dt)
    {
        live.fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        const auto newP = proc.readParams();
        const bool pChanged = ! (newP == live.p) || frame == 0;
        live.p = newP;
        const float dec = std::pow (0.9f, dt * (float) live.fs / 512.0f);
        live.inPk = std::max (proc.inPeak.exchange (0.0f), live.inPk * dec);
        live.outPk = std::max (proc.outPeak.exchange (0.0f), live.outPk * dec);
        proc.copyRings (ringIn.data(), ringOut.data());
        const float fall = 1.2f * dt * 60.0f;
        analyser.run (ringIn.data(), live.fs, live.spIn, fall);
        analyser.run (ringOut.data(), live.fs, live.spOut, fall);
        if (++frame % 6 == 0) spec.trackPeaks();
        if (pChanged || resp.animated() || ++respTick % 6 == 0) resp.repaint();
        spec.repaint();
        meter.update (Time::getMillisecondCounterHiRes());
        if (auto* ed = findParentComponentOfClass<PawltecEditor>())
            readouts.sizeText = String (ed->getWidth()) + String::fromUTF8 (" \xc3\x97 ") + String (ed->getHeight()) + " px";
        readouts.repaint (0, 360, layout::W, 40);
        readouts.repaint (roundToInt (layout::winsz.x) - 30, roundToInt (layout::winsz.y) - 2, 120, 20);
        syncControls();
    }

    float real (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setReal (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    void toggleBypass() { setReal (ids::bypass, real (ids::bypass) > 0.5f ? 0.0f : 1.0f); }

    String dot() const { return String::fromUTF8 ("\xc2\xb7"); }
    void syncName() { pName->setText (presetName() + (dirty() ? " *" : "")); }

    static String sg (double mx, double v, const char* sign)
    {
        const double d = mx * v / 10.0;
        return d < 0.05 ? String ("0.0 dB") : String (sign) + String (d, 1) + " dB";
    }
    static String fmtF (double f) { return f >= 1000 ? String (f / 1000.0, f / 1000.0 == std::floor (f / 1000.0) ? 0 : 1) + " kHz" : String (roundToInt (f)) + " Hz"; }

    void syncControls()
    {
        bBypass->setOn (live.p.bypass);
        const auto& p = live.p;
        values[0]->setParts (fmtF (pawl::kLowF[p.lowF]), "shared");
        values[1]->setParts (String (p.lowB, 1), sg (pawl::kMaxLowBoostDb, p.lowB, "+"));
        values[2]->setParts (String (p.lowA, 1), sg (pawl::kMaxLowAttenDb, p.lowA, "-"));
        values[3]->setParts (fmtF (pawl::kHiF[p.hiF]), "centre");
        values[4]->setParts (String (p.hiB, 1), sg (pawl::kMaxHiBoostDb, p.hiB, "+"));
        values[5]->setParts (String (p.bw, 1), "Q " + String (pawl::qOfWidth (p.bw), 2));
        values[6]->setParts (fmtF (pawl::kAttF[p.attF]), "corner");
        values[7]->setParts (String (p.hiA, 1), sg (pawl::kMaxHiAttenDb, p.hiA, "-"));
        values[8]->setParts (String (p.tube, 1), p.tube < 0.5f ? "clean" : p.tube < 3 ? "light" : p.tube < 6 ? "warm" : "hot");
        const double od = (p.out - 5.0) * pawl::kOutDbPerStep;
        values[9]->setParts (String (p.out, 1), (od >= 0 ? "+" : "") + String (od, 1) + " dB");
        syncName();
    }

    void step (int d)
    {
        auto all = presetStore.all();
        int i = 0;
        for (size_t k = 0; k < all.size(); ++k) if (all[k].name == presetName()) i = (int) k;
        i = (i + d + (int) all.size()) % (int) all.size();
        loadPreset (all[(size_t) i]);
    }

    // ---- building ----------------------------------------------------------------------------
    PButton* addButton (Component& parent, const String& text, const layout::Rc& r, const char* tip, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
    {
        auto* b = new PButton (ctx, text, px, k);
        owned.add (b);
        parent.addAndMakeVisible (b);
        b->setBounds (roundToInt (r.x), roundToInt (r.y), roundToInt (r.w), roundToInt (r.h));
        b->tip = tip;
        return b;
    }

    void buildHeader()
    {
        pPrev = addButton (*this, String::fromUTF8 ("\xe2\x97\x80"), layout::pPrev, layout::tip_pPrev);
        pName = addButton (*this, presetName(), layout::pName, layout::tip_pName, 16.0f, theme::FontKind::semi);
        pNext = addButton (*this, String::fromUTF8 ("\xe2\x96\xb6"), layout::pNext, layout::tip_pNext);
        pSave = addButton (*this, "Save", layout::pSave, layout::tip_pSave);
        bBypass = addButton (*this, "Bypass", layout::bBypass, layout::tip_bBypass);
        bGuide = addButton (*this, "Guide", layout::bGuide, layout::tip_bGuide);
        bSettings = addButton (*this, "Settings", layout::bSettings, layout::tip_bSettings);
        bAbout = addButton (*this, "About & Support", layout::bAbout, layout::tip_bAbout);
        pPrev->onClick = [this] (const ModifierKeys&) { step (-1); };
        pNext->onClick = [this] (const ModifierKeys&) { step (1); };
        pSave->onClick = [this] (const ModifierKeys&) { modal.openSave (nullptr); };
        bBypass->onClick = [this] (const ModifierKeys&) { toggleBypass(); };
        bGuide->onClick = [this] (const ModifierKeys&) { modal.openGuide(); };
        bSettings->onClick = [this] (const ModifierKeys&) { modal.openSettings(); };
        bAbout->onClick = [this] (const ModifierKeys&) { modal.openAbout(); };
        pName->onClick = [this] (const ModifierKeys&) { nameDelay.start (260); };
        nameDelay.fn = [this] { modal.openBrowser(); };
        pName->onDouble = [this]
        {
            nameDelay.stopTimer();
            const Preset* p = presetStore.find (presetName());
            if (p == nullptr || p->factory) { toast ("Save a copy first, factory presets cannot be renamed"); return; }
            Preset q = *p; modal.openSave (&q);
        };
    }

    void buildDisplays()
    {
        addAndMakeVisible (resp); addAndMakeVisible (spec);
        resp.setBounds (roundToInt (layout::cvResp.x), roundToInt (layout::cvResp.y), roundToInt (layout::cvResp.w), roundToInt (layout::cvResp.h));
        spec.setBounds (roundToInt (layout::cvSpec.x), roundToInt (layout::cvSpec.y), roundToInt (layout::cvSpec.w), roundToInt (layout::cvSpec.h));
        resp.tip = layout::tip_cvResp;
        spec.tip = layout::tip_cvSpec;
    }

    void buildKnobs()
    {
        static const char* ids_[10] = { ids::lowF, ids::lowB, ids::lowA, ids::hiF, ids::hiB, ids::bw, ids::attF, ids::hiA, ids::tube, ids::out };
        for (int gi = 0; gi < 4; ++gi)
        {
            auto* a = new TipArea(); owned.add (a);
            addAndMakeVisible (a);
            a->setBounds (roundToInt (layout::groups[gi].r.x), roundToInt (layout::groups[gi].r.y), roundToInt (layout::groups[gi].r.w), roundToInt (layout::groups[gi].r.h));
            a->tip = layout::groups[gi].tip;
        }
        for (int i = 0; i < 10; ++i)
        {
            const auto& c = layout::cells[i];
            StringArray steps;
            if (i == 0) for (double f : pawl::kLowF) steps.add (String (roundToInt (f)));
            if (i == 3) for (double f : pawl::kHiF) steps.add (String (f / 1000.0, 0));
            if (i == 6) for (double f : pawl::kAttF) steps.add (String (f / 1000.0, 0));
            auto* k = new KnobView (*proc.apvts.getParameter (ids_[i]), steps);
            knobs.add (k); knobViews[i] = k; addAndMakeVisible (k);
            k->setBounds (roundToInt (c.knob.x), roundToInt (c.knob.y), 90, 90);
            k->tip = c.tip;
            auto* v = new KnobValue (ctx);
            values.add (v); addAndMakeVisible (v);
            v->setBounds (roundToInt (c.value.x), roundToInt (c.value.y), roundToInt (c.value.w), roundToInt (c.value.h));
            v->tip = c.tip;
        }
    }

    void buildMeters()
    {
        addAndMakeVisible (meter);
        meter.setBounds (roundToInt (layout::cvMet.x), roundToInt (layout::cvMet.y), roundToInt (layout::cvMet.w), roundToInt (layout::cvMet.h));
        meter.tip = layout::tip_cvMet;
        meter.onClear = [this] { toast ("Clip lights cleared"); };
    }

    // ---- tips --------------------------------------------------------------------------------
    void trackTip (const MouseEvent& e)
    {
        if (modal.isOpen() || ! ctx.tips) { hideTip(); return; }
        Component* c = e.eventComponent;
        String text; Component* host = nullptr;
        while (c != nullptr && c != this)
        {
            if (auto* tc = dynamic_cast<TipClient*> (c); tc != nullptr && tc->tip.isNotEmpty())
            {
                text = tc->tip; host = c; break;
            }
            c = c->getParentComponent();
        }
        if (host == nullptr || text.isEmpty()) { hideTip(); return; }
        if (host == tipHost && text == tipText) return;
        hideTip();
        tipHost = host; tipText = text;
        tipTimer.start (1000);
    }
    void showTip()
    {
        if (tipHost == nullptr || modal.isOpen()) return;
        auto target = getLocalArea (tipHost, tipHost->getLocalBounds());
        // the big displays: keep the bubble near the top so it does not cover the curve
        if (tipHost == &resp || tipHost == &spec) target = target.withHeight (40);
        bubble.showAt (tipText, target, getWidth(), getHeight());
    }
    void hideTip() { tipTimer.stopTimer(); bubble.setVisible (false); tipHost = nullptr; tipText = {}; }

    // ---- members -----------------------------------------------------------------------------
    PawltecProcessor& proc;
    std::function<void()> onResetSize;
    Ctx ctx;
    Live live;
    Analyser analyser;
    std::array<float, 4096> ringIn {}, ringOut {};
    int frame = 0, respTick = 0;
    presets::Store presetStore;
    Backdrop backdrop;
    Cheek* cheekL = nullptr; Cheek* cheekR = nullptr;
    OwnedArray<Component> owned;
    PButton *pPrev = nullptr, *pName = nullptr, *pNext = nullptr, *pSave = nullptr, *bBypass = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr;
    ResponseView resp;
    SpectrumView spec;
    OwnedArray<KnobView> knobs;
    KnobView* knobViews[10] {};
    OwnedArray<KnobValue> values;
    MeterView meter;
    Readouts readouts;
    Bubble bubble; Toast toastView;
    Modal modal;
    Delay nameDelay, tipTimer;
    Component* tipHost = nullptr;
    String tipText;
};

// =====================================================================================================
PawltecEditor::PawltecEditor (PawltecProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    face = std::make_unique<Face> (p, [this] { resetSize(); });
    addAndMakeVisible (*face);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    constrainer.setMinimumSize (480, 235);
    constrainer.setMaximumSize (1800, 882);
    setConstrainer (&constrainer);
    setResizable (true, false);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer);
    border->setBorderThickness (BorderSize<int> (5));
    addAndMakeVisible (*border);
    const double sc = jlimit (0.4, 1.5, (double) proc.getSetting (setting::scale, 1.0));
    setSize (roundToInt (kW * sc), roundToInt (kH * sc));
}

PawltecEditor::~PawltecEditor() = default;

void PawltecEditor::resized()
{
    const double s = (double) getWidth() / (double) kW;
    face->setTransform (AffineTransform::scale ((float) s));
    face->setBounds (0, 0, kW, kH);
    border->setBounds (getLocalBounds());
    proc.setSetting (setting::scale, s);
}

void PawltecEditor::resetSize() { setSize (kW, kH); }
void PawltecEditor::openDialogForTesting (const juce::String& n) { face->openForTesting (n); }
void PawltecEditor::tickForTesting (int frames) { face->tickForTesting (frames); }
