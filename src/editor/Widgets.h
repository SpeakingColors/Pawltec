#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Theme.h"
#include "Layout.h"

namespace ui
{
using namespace juce;

// Settings every widget needs to paint (look and info bubbles).
struct Ctx
{
    char look = 'C';
    bool tips = true;
    bool power = false;
    const theme::Look& t() const { return theme::look (look); }
};

// Anything that can show an info bubble.
struct TipClient
{
    String tip;
    virtual ~TipClient() = default;
};

inline Rectangle<float> rc (const layout::Rc& r) { return { r.x, r.y, r.w, r.h }; }

inline float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Text centred in a box, never shortened with dots. Letter spacing is in px.
inline void drawText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c,
                      Justification j = Justification::centred, float spacingPx = 0.0f)
{
    if (std::abs (spacingPx) > 1.0e-6f) f = f.withExtraKerningFactor (spacingPx / jmax (1.0f, f.getHeight())); // JUCE kerning is a fraction of the font height
    g.setFont (f);
    g.setColour (c);
    Rectangle<float> box = r.expanded (30.0f, 0.0f); // room for font differences, text is never shortened
    if (j.testFlags (Justification::left))       box = r.withWidth (r.getWidth() + 60.0f);
    else if (j.testFlags (Justification::right)) box = r.withLeft (r.getX() - 60.0f);
    g.drawText (s, box, j, false);
}

// ---- Button in the mockup's .btn style -----------------------------------------------------------
class PButton : public Component, public TipClient
{
public:
    PButton (const Ctx& c, String t, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
        : text (std::move (t)), ctx (c), size (px), kind (k) { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<void (const ModifierKeys&)> onClick;
    std::function<void()> onDouble;
    bool on = false;
    bool hot = false;                       // red border and pink text (Auto Trim has trimmed)
    float spacing = 0.5f;
    float radius = 4.0f;
    String text;

    void setText (const String& t) { if (t != text) { text = t; repaint(); } }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::sel : t.plate);
        g.fillRoundedRectangle (r, radius);
        Colour border = hot ? theme::red : on ? theme::selb : (over ? t.dim : t.line);
        g.setColour (border);
        g.drawRoundedRectangle (r, radius, 1.0f);
        const Colour tc = hot ? theme::pink : on ? theme::lilac : t.ink;
        auto f = theme::font (kind, size);
        if (text.contains ("\n"))
        {
            auto lines = StringArray::fromLines (text);
            const float lh = size * 1.15f;
            float y = r.getCentreY() - lh * (float) lines.size() * 0.5f;
            for (auto& l : lines) { drawText (g, l, { r.getX(), y, r.getWidth(), lh }, f, tc, Justification::centred, spacing); y += lh; }
        }
        else drawText (g, text, r, f, tc, Justification::centred, spacing);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods);
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDouble) onDouble(); }

private:
    const Ctx& ctx;
    float size;
    theme::FontKind kind;
    bool over = false;
};

// ---- Faceplate knob, drawn like the mockup's SVG (viewBox 100, shown at 90 x 90) --------------------------
// Continuous knobs run 0..10 with numbers every 2. Stepped knobs (the frequency switches) show their values
// around the dial; click a number, drag, use the wheel or the arrow keys.
class KnobView : public Component, public TipClient
{
public:
    KnobView (RangedAudioParameter& prm, StringArray stepLabels)
        : p (prm), steps (std::move (stepLabels)), att (prm, [this] (float) { repaint(); })
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        att.sendInitialUpdate();
    }

    bool stepped() const { return steps.size() > 0; }
    float real() const { return p.convertFrom0to1 (p.getValue()); }
    void setReal (float v)
    {
        v = stepped() ? (float) jlimit (0, steps.size() - 1, roundToInt (v)) : jlimit (0.0f, 10.0f, std::round (v * 10.0f) / 10.0f);
        att.setValueAsPartOfGesture (v);
    }

    void paint (Graphics& g) override
    {
        g.addTransform (AffineTransform::scale (0.9f));
        const int n = steps.size();
        auto pt = [] (float deg, float r) { const float a = degreesToRadians (deg); return Point<float> (50.0f + std::sin (a) * r, 50.0f - std::cos (a) * r); };
        const auto labelFont = theme::font (theme::FontKind::semi, 10.0f * 0.9f * 1.0f);
        if (! stepped())
        {
            for (int i = 0; i <= 20; ++i)
            {
                const bool major = i % 2 == 0;
                const float deg = -135.0f + (float) i * 13.5f;
                g.setColour (major ? Colour (0xfff4f5f7) : Colour (0xff8c8f99));
                g.drawLine (Line<float> (pt (deg, major ? 31.0f : 33.0f), pt (deg, 36.0f)), major ? 1.5f : 0.9f);
            }
            for (int i = 0; i <= 10; i += 2)
            {
                const auto q = pt (-135.0f + (float) i * 27.0f, 43.0f);
                drawLabel (g, String (i), q, labelFont);
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
            {
                const float deg = -135.0f + (float) i * 270.0f / (float) (n - 1);
                g.setColour (Colour (0xfff4f5f7));
                g.drawLine (Line<float> (pt (deg, 31.0f), pt (deg, 36.0f)), 1.5f);
                drawLabel (g, steps[i], pt (deg, 43.0f), labelFont);
            }
        }
        // body
        g.setColour (Colour (0xff0b0b0d)); g.fillEllipse (23.0f, 23.0f, 54.0f, 54.0f);
        g.setColour (Colours::black);      g.drawEllipse (23.0f, 23.0f, 54.0f, 54.0f, 1.0f);
        {
            Path ring; ring.addEllipse (50.0f - 25.5f, 50.0f - 25.5f, 51.0f, 51.0f);
            Path dashes; const float d[2] = { 1.2f, 1.4f };
            PathStrokeType (2.0f, PathStrokeType::mitered, PathStrokeType::butt).createDashedStroke (dashes, ring, d, 2);
            g.setColour (Colour (0xff3a3c43)); g.fillPath (dashes);
        }
        const float v = real();
        const float ang = stepped() ? -135.0f + v * 270.0f / (float) (n - 1) : -135.0f + v * 27.0f;
        ColourGradient grad (Colour (0xfff1f1f4), 45.6f, 43.4f, Colour (0xff6e6f78), 45.6f + 30.8f, 43.4f, true);
        grad.addColour (0.55, Colour (0xffb9bac2));
        g.setGradientFill (grad);
        g.fillEllipse (28.0f, 28.0f, 44.0f, 44.0f);
        g.setColour (Colour (0xff4a4b53)); g.drawEllipse (28.0f, 28.0f, 44.0f, 44.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawEllipse (31.0f, 31.0f, 38.0f, 38.0f, 1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (degreesToRadians (ang), 50.0f, 50.0f));
            g.setColour (Colour (0xff16171a));
            g.drawLine (50.0f, 50.0f, 50.0f, 33.0f, 3.0f);
            g.fillEllipse (48.5f, 31.5f, 3.0f, 3.0f);
            g.fillEllipse (48.5f, 48.5f, 3.0f, 3.0f);
        }
        if (hasKeyboardFocus (false))
        {
            g.setColour (theme::selb);
            g.drawEllipse (2.0f, 2.0f, 96.0f, 96.0f, 2.2f);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        labelHit = false;
        if (stepped())
        {
            const auto q = e.position / 0.9f;
            for (int i = 0; i < steps.size(); ++i)
            {
                const float a = degreesToRadians (-135.0f + (float) i * 270.0f / (float) (steps.size() - 1));
                if (q.getDistanceFrom (Point<float> (50.0f + std::sin (a) * 43.0f, 50.0f - std::cos (a) * 43.0f)) < 7.5f)
                {
                    att.beginGesture(); setReal ((float) i); att.endGesture();
                    labelHit = true;
                    return;
                }
            }
        }
        start = real();
        att.beginGesture();
        dragging = true;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragging) return;
        const float up = -(float) e.getDistanceFromDragStartY();
        if (stepped()) setReal (start + up / 22.0f);
        else setReal (start + up / (e.mods.isShiftDown() ? 1000.0f : 150.0f) * 10.0f);
    }
    void mouseUp (const MouseEvent&) override { if (dragging) att.endGesture(); dragging = false; }
    void mouseDoubleClick (const MouseEvent&) override
    {
        if (labelHit) return;
        att.beginGesture();
        att.setValueAsPartOfGesture (p.convertFrom0to1 (p.getDefaultValue()));
        att.endGesture();
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const float dy = w.isReversed ? -w.deltaY : w.deltaY;
        if (std::abs (dy) < 1.0e-6f) return;
        const float d = dy > 0 ? 1.0f : -1.0f;
        att.beginGesture();
        setReal (stepped() ? real() + d : real() + d * (e.mods.isShiftDown() ? 0.1f : 0.5f));
        att.endGesture();
    }
    bool keyPressed (const KeyPress& k) override
    {
        float d = 0.0f;
        if (k.isKeyCode (KeyPress::upKey) || k.isKeyCode (KeyPress::rightKey)) d = 1.0f;
        else if (k.isKeyCode (KeyPress::downKey) || k.isKeyCode (KeyPress::leftKey)) d = -1.0f;
        else return false;
        att.beginGesture();
        setReal (stepped() ? real() + d : real() + d * (k.getModifiers().isShiftDown() ? 0.1f : 0.5f));
        att.endGesture();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    static void drawLabel (Graphics& g, const String& s, Point<float> c, const Font& f)
    {
        g.setFont (f);
        g.setColour (Colour (0xffc9cbd3));
        g.drawText (s, Rectangle<float> (c.x - 14.0f, c.y - 6.0f + 3.4f - 3.4f, 28.0f, 12.0f), Justification::centred, false);
    }
    RangedAudioParameter& p;
    StringArray steps;
    ParameterAttachment att;
    float start = 0.0f;
    bool dragging = false, labelHit = false;
};

// Two lines of text under a knob: the value in ink and a note in grey, both centred, mono 11 px, 15 px apart.
class KnobValue : public Component, public TipClient
{
public:
    explicit KnobValue (const Ctx& c) : ctx (c) {}
    String bold, rest;
    void setParts (const String& b, const String& r) { if (b != bold || r != rest) { bold = b; rest = r; repaint(); } }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const auto f = theme::font (theme::FontKind::mono, 11.0f);
        auto r = getLocalBounds().toFloat();
        drawText (g, bold, r.removeFromTop (15.0f), f, t.ink);
        drawText (g, rest, r.removeFromTop (15.0f), f, t.dim);
    }
private:
    const Ctx& ctx;
};

// A transparent area that only carries an info bubble (group boxes, the brand).
class TipArea : public Component, public TipClient {};
} // namespace ui
