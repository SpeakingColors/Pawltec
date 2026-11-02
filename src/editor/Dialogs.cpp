#include "Dialogs.h"

namespace ui
{
namespace
{
// ---- little building blocks -----------------------------------------------------------------------
struct Seg { String text; bool bold = false, mono = false, dim = false, accent = false; };

class TextBlock : public Component
{
public:
    TextBlock (const Ctx& c, std::vector<Seg> s, float px = 15.0f) : ctx (c), segs (std::move (s)), size (px) { setInterceptsMouseClicks (false, false); }
    int heightFor (int w) const
    {
        TextLayout tl; tl.createLayout (build(), (float) w);
        return (int) std::ceil (tl.getHeight()) + 2;
    }
    void paint (Graphics& g) override { build().draw (g, getLocalBounds().toFloat()); }
private:
    AttributedString build() const
    {
        auto& t = ctx.t();
        AttributedString as;
        as.setWordWrap (AttributedString::byWord);
        as.setLineSpacing (3.0f);
        for (auto& s : segs)
            as.append (s.text, theme::font (s.mono ? theme::FontKind::mono : s.bold ? theme::FontKind::bold : theme::FontKind::regular, s.mono ? size - 3.0f : size),
                       s.accent ? theme::selb : s.dim ? t.dim : t.ink);
        return as;
    }
    const Ctx& ctx;
    std::vector<Seg> segs;
    float size;
};

class Kbd : public Component
{
public:
    Kbd (const Ctx& c, String s) : ctx (c), text (std::move (s)) { setInterceptsMouseClicks (false, false); }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto f = theme::font (theme::FontKind::mono, 12.0f);
        const float w = textWidth (f, text) + 12.0f;
        Rectangle<float> r (0.0f, 2.0f, w, 20.0f);
        g.setColour (t.disp); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (t.line); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        drawText (g, text, r, f, t.ink);
    }
private:
    const Ctx& ctx;
    String text;
};

class PToggle : public Component
{
public:
    PToggle (const Ctx& c, String s, bool v) : ctx (c), text (std::move (s)), value (v) { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void (bool)> onChange;
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        Rectangle<float> box (0.0f, (getHeight() - 16) * 0.5f, 16.0f, 16.0f);
        g.setColour (t.disp); g.fillRoundedRectangle (box, 3.0f);
        g.setColour (value ? theme::selb : t.line); g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        if (value)
        {
            Path p; p.startNewSubPath (box.getX() + 3.5f, box.getCentreY()); p.lineTo (box.getX() + 7.0f, box.getBottom() - 4.5f); p.lineTo (box.getRight() - 3.0f, box.getY() + 4.0f);
            g.setColour (t.accent); g.strokePath (p, PathStrokeType (2.0f));
        }
        drawText (g, text, getLocalBounds().toFloat().withTrimmedLeft (24.0f), theme::font (theme::FontKind::regular, 15.0f), t.ink, Justification::centredLeft);
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        value = ! value; repaint();
        if (onChange) onChange (value);
    }
private:
    const Ctx& ctx;
    String text;
    bool value;
};

void styleEditor (TextEditor& e, const Ctx& c, bool multi)
{
    auto& t = c.t();
    e.setColour (TextEditor::backgroundColourId, t.disp);
    e.setColour (TextEditor::textColourId, t.ink);
    e.setColour (TextEditor::outlineColourId, t.line);
    e.setColour (TextEditor::focusedOutlineColourId, theme::selb);
    e.setColour (TextEditor::highlightColourId, theme::sel);
    e.setColour (TextEditor::highlightedTextColourId, theme::lilac);
    e.setColour (CaretComponent::caretColourId, t.ink);
    e.setFont (theme::font (multi ? theme::FontKind::mono : theme::FontKind::regular, multi ? 12.0f : 15.0f));
    e.setIndents (8, 5);
    e.setBorder ({ 1, 1, 1, 1 });
    e.setMultiLine (multi, multi);
    e.setReturnKeyStartsNewLine (multi);
}
} // namespace

// ---- Panel: title bar, rows, footer buttons -------------------------------------------------------
class Panel : public Component
{
public:
    Panel (Modal& m, DialogHost& h, String t, int w) : modal (m), host (h), title (std::move (t)), width (w), closeBtn (h.context(), "Close")
    {
        closeBtn.onClick = [this] (const ModifierKeys&) { modal.close(); };
        addAndMakeVisible (closeBtn);
        setSize (w, 200);
    }
    struct Row
    {
        std::vector<Component*> items;
        std::vector<int> widths;                 // 0 = share of the remaining width
        int height = 24;
        std::function<int (int)> heightFor;
        int gap = 6;
    };
    template <class T, class... A> T* make (A&&... a)
    {
        auto p = std::make_unique<T> (std::forward<A> (a)...);
        T* raw = p.get();
        addAndMakeVisible (*raw);
        owned.push_back (std::move (p));
        return raw;
    }
    void row (std::vector<Component*> items, std::vector<int> widths, int height, int gap = 6)
    {
        Row r; r.items = std::move (items); r.widths = std::move (widths); r.height = height; r.gap = gap;
        rows.push_back (std::move (r));
    }
    void textRow (std::vector<Seg> segs, int gap = 8)
    {
        auto* tb = make<TextBlock> (host.context(), std::move (segs));
        Row r; r.items = { tb }; r.widths = { 0 }; r.gap = gap;
        r.heightFor = [tb] (int w) { return tb->heightFor (w); };
        rows.push_back (std::move (r));
    }
    PButton* footer (const String& text, std::function<void()> fn, bool primary = false)
    {
        auto* b = make<PButton> (host.context(), text);
        b->on = primary;
        b->onClick = [fn] (const ModifierKeys&) { fn(); };
        footers.push_back (b);
        return b;
    }
    void fit (int maxH = 524)
    {
        const int h = std::min (maxH, layoutRows (false) + 12 + (footers.empty() ? 0 : kFooter));
        setSize (width, h);
        resized();
    }
    void paint (Graphics& g) override
    {
        auto& t = host.context().t();
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 6).expanded (6), 12.0f);
        g.setColour (t.plate); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (t.line);
        g.fillRect (1, kTitle, getWidth() - 2, 1);
        if (! footers.empty()) g.fillRect (1, getHeight() - kFooter, getWidth() - 2, 1);
        drawText (g, title, { 16.0f, 0.0f, (float) getWidth() - 100.0f, (float) kTitle }, theme::font (theme::FontKind::regular, 20.0f), t.title, Justification::centredLeft, 1.0f);
    }
    void resized() override
    {
        closeBtn.setBounds (getWidth() - 12 - 60, (kTitle - 26) / 2, 60, 26);
        layoutRows (true);
        int x = getWidth() - 16;
        for (int i = (int) footers.size(); --i >= 0;)
        {
            const int w = std::max(70, (int) textWidth (theme::font (theme::FontKind::regular, 14.0f), footers[(size_t) i]->text) + 36);
            footers[(size_t) i]->setBounds (x - w, getHeight() - kFooter + 11, w, 28);
            x -= w + 8;
        }
    }
    Modal& modal;
    DialogHost& host;
    String title;
    int width;
    PButton closeBtn;
    static constexpr int kTitle = 44, kFooter = 50;
private:
    int layoutRows (bool place)
    {
        const int bw = width - 32;
        int y = kTitle + 12;
        for (auto& r : rows)
        {
            const int h = r.heightFor ? r.heightFor (bw) : r.height;
            if (place)
            {
                int fixed = 0, flex = 0;
                for (int w : r.widths) { if (w > 0) fixed += w; else ++flex; }
                const int gaps = (int) r.items.size() - 1;
                const int share = flex > 0 ? std::max (20, (bw - fixed - gaps * 8) / flex) : 0;
                int x = 16;
                for (size_t i = 0; i < r.items.size(); ++i)
                {
                    const int w = r.widths[i] > 0 ? r.widths[i] : share;
                    r.items[i]->setBounds (x, y, w, h);
                    x += w + 8;
                }
            }
            y += h + r.gap;
        }
        return y;
    }
    std::vector<std::unique_ptr<Component>> owned;
    std::vector<Row> rows;
    std::vector<PButton*> footers;
};

// ---- Modal ------------------------------------------------------------------------------------------
Modal::Modal (DialogHost& h) : host (h) { setVisible (false); setWantsKeyboardFocus (true); }
Modal::~Modal() = default;

void Modal::paint (Graphics& g) { g.fillAll (Colours::black.withAlpha (0.6f)); }
void Modal::resized() { if (panel) panel->setCentrePosition (getWidth() / 2, getHeight() / 2); }
void Modal::mouseDown (const MouseEvent&) { close(); }
bool Modal::keyPressed (const KeyPress& k)
{
    if (k.isKeyCode (KeyPress::escapeKey)) { close(); return true; }
    return true; // swallow keys while a dialog is open
}
void Modal::close()
{
    setVisible (false);
    panel.reset();
    if (auto* p = getParentComponent()) p->grabKeyboardFocus();
}
void Modal::show (std::unique_ptr<Panel> p)
{
    panel = std::move (p);
    addAndMakeVisible (*panel);
    setBounds (getParentComponent()->getLocalBounds());
    setVisible (true);
    toFront (true);
    resized();
    grabKeyboardFocus();
}

// ---- Settings, Guide, About -------------------------------------------------------------------------
void Modal::openSettings()
{
    auto p = std::make_unique<Panel> (*this, host, "Settings", 760);
    auto& c = host.context();
    auto& proc = host.processor();
    auto* tips = p->make<PToggle> (c, "Info bubbles after hovering for about one second", c.tips);
    tips->onChange = [this, &proc] (bool v) { host.context().tips = v; proc.setSetting (setting::tips, v); host.settingsChanged(); };
    p->row ({ tips }, { 0 }, 24);
    auto* pw = p->make<PToggle> (c, "Power saving (redraw at 16 frames per second)", c.power);
    pw->onChange = [this, &proc] (bool v) { host.context().power = v; proc.setSetting (setting::power, v); host.settingsChanged(); };
    p->row ({ pw }, { 0 }, 24, 10);

    auto* lookLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Colour look" } });
    std::vector<PButton*> looks;
    const char* names[3] = { "Look C, charcoal and white (default)", "Look A, neon navy", "Look B, purple soda pop" };
    const char ids[3] = { 'C', 'A', 'B' };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = p->make<PButton> (c, names[i]);
        b->on = c.look == ids[i];
        looks.push_back (b);
    }
    for (int i = 0; i < 3; ++i)
        looks[(size_t) i]->onClick = [this, looks, ids, i, &proc] (const ModifierKeys&)
        {
            host.context().look = ids[i];
            proc.setSetting (setting::look, String::charToString ((juce_wchar) ids[i]));
            for (int k = 0; k < 3; ++k) looks[(size_t) k]->setOn (k == i);
            host.settingsChanged();
        };
    p->row ({ lookLabel, looks[0], looks[1], looks[2] }, { 90, 0, 0, 0 }, 26);
    p->textRow ({ { "The look is saved with the plugin settings, not with each preset. Wood, cat logo, meter colours and dB scales stay the same in all three.", false, false, true } }, 12);

    auto* reset = p->make<PButton> (c, "Reset window to 100%");
    reset->onClick = [this] (const ModifierKeys&) { host.resetWindow(); host.toast ("Window reset to 100%"); };
    p->row ({ reset }, { 170 }, 28);
    p->fit();
    show (std::move (p));
}

void Modal::openGuide()
{
    auto p = std::make_unique<Panel> (*this, host, "Guide", 760);
    auto& c = host.context();
    p->textRow ({ { "Signal flow:", true }, { " Input, then the passive equalizer network (low boost, low atten, high boost with bandwidth, high atten), then the tube stage that keeps the level steady, then Output." } }, 8);
    p->textRow ({ { "The low trick:", true }, { " set Boost and Atten on the same low frequency. You get a fat bump at the bottom and a clean dip just above it, so the bass is bigger without getting muddy. Try the preset Low Trick 60." } }, 8);
    p->textRow ({ { "Drag the curve:", true }, { " the round handles on the response display are the knobs. Drag up or down to change the amount, sideways to pick another frequency, double-click a handle to zero it." } }, 10);
    const std::pair<const char*, const char*> keys[] = {
        { "B", "Bypass on or off, to compare with the dry sound" },
        { "[  /  ]", "Previous or next preset" },
        { "Knob: drag, wheel, arrows", "Hold Shift for fine steps, double-click to reset" },
        { "Wood cheeks", "Double-click to reset the window to 100%" } };
    for (auto& k : keys)
    {
        auto* kb = p->make<Kbd> (c, k.first);
        auto* tb = p->make<TextBlock> (c, std::vector<Seg> { { k.second } });
        p->row ({ kb, tb }, { 200, 0 }, 24, 2);
    }
    p->textRow ({ { "Keys work while the plugin window has keyboard focus.", false, false, true } }, 10);
    p->fit();
    show (std::move (p));
}

void Modal::openAbout()
{
    auto p = std::make_unique<Panel> (*this, host, "About & Support", 760);
    auto& c = host.context();
    p->textRow ({ { "Pawltec", true }, { String (" by Studio 87, version ") + JucePlugin_VersionString + ". Free and open source, licence AGPL-3.0-or-later. Collects nothing and never uses the network." } });
    p->textRow ({ { "A free open source equalizer inspired by classic hardware and written from public manuals." } });
    p->textRow ({ { "Your presets are saved here:", true }, { "\nLinux  " }, { "~/.local/share/Studio 87/Pawltec/presets", false, true },
                  { "\nWindows  " }, { "%APPDATA%\\Studio 87\\Pawltec\\presets", false, true },
                  { "\nmacOS  " }, { "~/Library/Application Support/Studio 87/Pawltec/presets", false, true } });
    p->textRow ({ { "Install and uninstall:", true }, { " the installer puts the VST3 in the standard folder for your system. The uninstaller removes everything it installed and asks before touching your presets (it keeps them by default)." } });
    auto* gh = p->make<PButton> (c, "GitHub");
    gh->onClick = [] (const ModifierKeys&) { URL ("https://github.com/SpeakingColors/pawltec").launchInDefaultBrowser(); };
    auto* rel = p->make<PButton> (c, "Releases");
    rel->onClick = [] (const ModifierKeys&) { URL ("https://github.com/SpeakingColors/pawltec/releases").launchInDefaultBrowser(); };
    auto* kofi = p->make<PButton> (c, "Ko-fi studio87");
    kofi->onClick = [] (const ModifierKeys&) { URL ("https://ko-fi.com/studio87").launchInDefaultBrowser(); };
    p->row ({ gh, rel, kofi }, { 90, 90, 130 }, 28);
    p->textRow ({ { "Support is entirely optional. Nothing is locked.", false, false, true } });
    p->fit();
    show (std::move (p));
}

// ---- Preset browser ---------------------------------------------------------------------------------
namespace
{
String pbSel, pbType = "All", pbQuery;
int pbPage = 0;

// Paged list (the mockup has no scrollbars): 11 presets per page.
class BrowserBody : public Component
{
public:
    static constexpr int kPerPage = 11, kRow = 30;
    BrowserBody (Modal& m, DialogHost& h) : modal (m), host (h)
    {
        auto& c = h.context();
        styleEditor (search, c, false);
        search.setTextToShowWhenEmpty ("Search name, type or tag", c.t().dim);
        search.setText (pbQuery, false);
        search.onTextChange = [this] { pbQuery = search.getText(); pbPage = 0; refresh(); };
        search.onEscapeKey = [this] { modal.close(); };
        addAndMakeVisible (search);
        StringArray all ("All");
        for (auto* t : presets::types) all.add (t);
        for (auto& n : all)
        {
            auto* b = chips.add (new PButton (c, n, 13.0f));
            b->radius = 12.0f; b->spacing = 0.0f;
            const String nn = n;
            b->onClick = [this, nn] (const ModifierKeys&) { pbType = nn; pbPage = 0; refresh(); };
            addAndMakeVisible (b);
        }
        for (auto* n : { "Load", "Export", "Rename", "Delete" })
            addAndMakeVisible (sideBtns.add (new PButton (c, n)));
        pagePrev = new PButton (c, String::fromUTF8 ("\xe2\x97\x80"), 12.0f); pageNext = new PButton (c, String::fromUTF8 ("\xe2\x96\xb6"), 12.0f);
        addAndMakeVisible (pagePrev); addAndMakeVisible (pageNext);
        owned.add (pagePrev); owned.add (pageNext);
        pagePrev->onClick = [this] (const ModifierKeys&) { pbPage = (pbPage + pages() - 1) % pages(); refresh(); };
        pageNext->onClick = [this] (const ModifierKeys&) { pbPage = (pbPage + 1) % pages(); refresh(); };
        sideBtns[0]->onClick = [this] (const ModifierKeys&) { if (auto* p = selected()) { Preset q = *p; modal.close(); host.loadPreset (q); } };
        sideBtns[1]->onClick = [this] (const ModifierKeys&) { if (auto* p = selected()) { Preset q = *p; modal.openExport (&q); } };
        sideBtns[2]->onClick = [this] (const ModifierKeys&) { if (auto* p = selected()) { Preset q = *p; modal.openSave (&q); } };
        sideBtns[3]->onClick = [this] (const ModifierKeys&)
        {
            if (auto* p = selected())
            {
                const String n = p->name;
                host.store().remove (n);
                if (host.presetName() == n) host.setPresetName (presets::factory()[0].name);
                pbSel = {};
                host.toast ("Deleted");
                refresh();
            }
        };
        refresh();
    }

    const Preset* selected() const { return host.store().find (pbSel); }
    int pages() const { return std::max (1, ((int) shown.size() + kPerPage - 1) / kPerPage); }

    void refresh()
    {
        for (auto* b : chips) b->setOn (b->text == pbType);
        shown.clear();
        const String q = pbQuery.toLowerCase();
        for (auto& p : host.store().all())
        {
            if (pbType != "All" && p.type != pbType) continue;
            if (q.isNotEmpty() && ! (p.name + " " + p.type + " " + p.tags.joinIntoString (" ")).toLowerCase().contains (q)) continue;
            shown.push_back (p);
        }
        if (pbPage >= pages()) pbPage = pages() - 1;
        const Preset* s = selected();
        sideBtns[0]->setVisible (s != nullptr);
        sideBtns[1]->setVisible (s != nullptr);
        sideBtns[2]->setVisible (s != nullptr && ! s->factory);
        sideBtns[3]->setVisible (s != nullptr && ! s->factory);
        pagePrev->setVisible (pages() > 1); pageNext->setVisible (pages() > 1);
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (300);
        search.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        int x = 0, y = left.getY();
        for (auto* b : chips)
        {
            const int w = (int) textWidth (theme::font (theme::FontKind::regular, 13.0f), b->text) + 20;
            if (x + w > left.getWidth()) { x = 0; y += 26; }
            b->setBounds (left.getX() + x, y, w, 22);
            x += w + 4;
        }
        left.setTop (y + 32);
        listArea = left.withHeight (kPerPage * kRow + 2);
        const int py = listArea.getBottom() + 6;
        pagePrev->setBounds (left.getX(), py, 28, 22);
        pageNext->setBounds (left.getRight() - 28, py, 28, 22);
        pageText = Rectangle<int> (left.getX() + 34, py, left.getWidth() - 68, 22);
        r.removeFromLeft (14);
        sideArea = r;
        int bx = r.getX(), by = r.getY() + 100;
        for (auto* b : sideBtns)
        {
            if (! b->isVisible()) continue;
            const int w = (int) textWidth (theme::font (theme::FontKind::regular, 14.0f), b->text) + 28;
            if (bx + w > r.getRight()) { bx = r.getX(); by += 34; }
            b->setBounds (bx, by, w, 28);
            bx += w + 6;
        }
    }

    void paint (Graphics& g) override
    {
        auto& t = host.context().t();
        g.setColour (t.disp); g.fillRoundedRectangle (listArea.toFloat(), 4.0f);
        const int first = pbPage * kPerPage;
        for (int i = 0; i < kPerPage && first + i < (int) shown.size(); ++i)
        {
            auto& p = shown[(size_t) (first + i)];
            const bool sel = p.name == pbSel;
            auto row = Rectangle<int> (listArea.getX(), listArea.getY() + 1 + i * kRow, listArea.getWidth(), kRow);
            if (sel) { g.setColour (theme::sel); g.fillRect (row); }
            g.setColour (t.line); g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
            drawText (g, p.name, row.toFloat().withTrimmedLeft (10.0f).withTrimmedRight (110.0f), theme::font (theme::FontKind::regular, 15.0f), sel ? theme::lilac : t.ink, Justification::centredLeft);
            drawText (g, (p.factory ? String() : String ("mine ") + String::fromUTF8 ("\xc2\xb7 ")) + p.type, row.toFloat().withTrimmedLeft ((float) row.getWidth() - 150.0f).withTrimmedRight (10.0f),
                      theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
        }
        g.setColour (t.line); g.drawRoundedRectangle (listArea.toFloat().reduced (0.5f), 4.0f, 1.0f);
        if (pages() > 1)
            drawText (g, "Page " + String (pbPage + 1) + " of " + String (pages()) + " (" + String ((int) shown.size()) + " presets)", pageText.toFloat(),
                      theme::font (theme::FontKind::regular, 13.0f), t.dim);
        const Preset* p = selected();
        if (p == nullptr) return;
        auto r = sideArea.toFloat();
        drawText (g, p->name, r.withHeight (24.0f), theme::font (theme::FontKind::bold, 18.0f), t.ink, Justification::centredLeft);
        const String sep = String::fromUTF8 (" \xc2\xb7 ");
        String meta = p->type + (p->tags.isEmpty() ? String() : sep + p->tags.joinIntoString (", ")) + (p->factory ? sep + "factory" : sep + "yours");
        drawText (g, meta, r.withTrimmedTop (26.0f).withHeight (20.0f), theme::font (theme::FontKind::regular, 15.0f), t.dim, Justification::centredLeft);
        auto hz = [] (double f) { return f >= 1000 ? String (f / 1000.0, f == std::floor (f / 1000.0) * 1000.0 ? 0 : 1) + " kHz" : String (roundToInt (f)) + " Hz"; };
        const String sum = "Low " + hz (pawl::kLowF[jlimit (0, 3, (int) p->v[0])]) + " boost " + String (p->v[1], 1) + " atten " + String (p->v[2], 1)
                           + "\nHigh " + hz (pawl::kHiF[jlimit (0, 6, (int) p->v[3])]) + " boost " + String (p->v[4], 1) + " width " + String (p->v[5], 1)
                           + "\nAtten " + hz (pawl::kAttF[jlimit (0, 2, (int) p->v[6])]) + " " + String (p->v[7], 1) + sep + "Tube " + String (p->v[8], 1)
                           + sep + "Out " + String (p->v[9], 1);
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (3.0f);
        as.append (sum, theme::font (theme::FontKind::regular, 15.0f), t.dim);
        as.draw (g, r.withTrimmedTop (54.0f).withHeight (60.0f));
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (! listArea.contains (e.getPosition())) return;
        const int i = (e.y - listArea.getY() - 1) / kRow, idx = pbPage * kPerPage + i;
        if (i < 0 || idx >= (int) shown.size()) return;
        pbSel = shown[(size_t) idx].name;
        refresh();
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (! listArea.contains (e.getPosition())) return;
        const int i = (e.y - listArea.getY() - 1) / kRow, idx = pbPage * kPerPage + i;
        if (i >= 0 && idx < (int) shown.size()) { Preset q = shown[(size_t) idx]; modal.close(); host.loadPreset (q); }
    }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        if (pages() < 2 || std::abs (w.deltaY) < 1.0e-6f) return;
        pbPage = (pbPage + (w.deltaY < 0 ? 1 : pages() - 1)) % pages();
        refresh();
    }

    Modal& modal;
    DialogHost& host;
    TextEditor search;
    OwnedArray<PButton> chips, sideBtns;
    OwnedArray<PButton> owned;   // pagePrev, pageNext (owned here, added as children below)
    PButton *pagePrev = nullptr, *pageNext = nullptr;
    std::vector<Preset> shown;
    Rectangle<int> sideArea, listArea, pageText;
};
} // namespace

void Modal::openBrowser()
{
    auto p = std::make_unique<Panel> (*this, host, "Presets", 820);
    if (pbSel.isEmpty()) pbSel = host.presetName();
    auto* body = p->make<BrowserBody> (*this, host);
    p->row ({ body }, { 0 }, 372);
    p->footer ("Import", [this] { openImport(); });
    p->footer ("Export all", [this] { openExport (nullptr); });
    p->footer ("Save current", [this] { openSave (nullptr); });
    p->fit();
    show (std::move (p));
}

// ---- Save, Export, Import ---------------------------------------------------------------------------
void Modal::openSave (const Preset* edit)
{
    auto p = std::make_unique<Panel> (*this, host, edit ? "Edit preset" : "Save preset", 520);
    auto& c = host.context();
    const bool hasEdit = edit != nullptr;
    const Preset editCopy = hasEdit ? *edit : Preset();
    const Preset* curP = host.store().find (host.presetName());
    if (curP == nullptr) curP = &presets::factory()[0];

    auto* nameLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Name" } });
    auto* nameBox = p->make<TextEditor>();
    styleEditor (*nameBox, c, false);
    nameBox->setInputRestrictions (60);
    nameBox->setText (hasEdit ? editCopy.name : ((host.dirty() || curP->factory) ? curP->name + " mine" : curP->name), false);
    nameBox->onEscapeKey = [this] { close(); };
    p->row ({ nameLabel, nameBox }, { 50, 0 }, 28);

    auto* typeLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Type" } });
    auto typeSel = std::make_shared<String> (hasEdit ? editCopy.type : curP->type);
    auto chipList = std::make_shared<std::vector<PButton*>>();
    for (auto* ty : presets::types)
    {
        auto* b = p->make<PButton> (c, ty, 13.0f);
        b->radius = 12.0f; b->spacing = 0.0f;
        b->on = *typeSel == ty;
        chipList->push_back (b);
    }
    for (auto* b : *chipList)
        b->onClick = [typeSel, chipList, b] (const ModifierKeys&) { *typeSel = b->text; for (auto* x : *chipList) x->setOn (x == b); };
    {
        std::vector<Component*> items { typeLabel }; std::vector<int> widths { 50 };
        for (auto* b : *chipList) { items.push_back (b); widths.push_back (0); }
        p->row (items, widths, 26);
    }

    auto* tagLabel = p->make<TextBlock> (c, std::vector<Seg> { { "Tags" } });
    auto* tagBox = p->make<TextEditor>();
    styleEditor (*tagBox, c, false);
    tagBox->setTextToShowWhenEmpty ("comma separated", c.t().dim);
    tagBox->setText ((hasEdit ? editCopy.tags : curP->tags).joinIntoString (", "), false);
    tagBox->onEscapeKey = [this] { close(); };
    p->row ({ tagLabel, tagBox }, { 50, 0 }, 28);

    auto* warnText = p->make<Label>();
    warnText->setColour (Label::textColourId, theme::red);
    warnText->setFont (theme::font (theme::FontKind::regular, 15.0f));
    auto* overwrite = p->make<PButton> (c, "Overwrite");
    auto* asNew = p->make<PButton> (c, "Save as new");
    overwrite->setVisible (false); asNew->setVisible (false);
    p->row ({ warnText, overwrite, asNew }, { 0, 90, 100 }, 28);

    p->footer ("Cancel", [this] { close(); });
    auto saveFn = [this, hasEdit, editCopy, nameBox, tagBox, typeSel, warnText, overwrite, asNew]
    {
        const String name = nameBox->getText().trim();
        overwrite->setVisible (false); asNew->setVisible (false);
        auto commit = [this, hasEdit, editCopy, tagBox, typeSel, warnText] (const String& n)
        {
            StringArray tags;
            tags.addTokens (tagBox->getText(), ",", "");
            tags.trim(); tags.removeEmptyStrings();
            while (tags.size() > 8) tags.remove (tags.size() - 1);
            for (auto& t : tags) t = t.substring (0, 20);
            Preset q = hasEdit ? editCopy : host.currentValues();
            q.name = n; q.type = *typeSel; q.tags = tags; q.factory = false;
            if (host.store().save (q, hasEdit ? editCopy.name : String()))
            {
                if (! hasEdit || host.presetName() == editCopy.name) host.setPresetName (n);
                close();
                host.toast ("Saved " + n);
            }
            else warnText->setText ("Could not write the preset file. Check that your presets folder is writable.", dontSendNotification);
        };
        if (name.isEmpty()) { warnText->setText ("Give it a name.", dontSendNotification); return; }
        for (auto& f : presets::factory())
            if (f.name == name) { warnText->setText ("That is a factory name. Pick another, or it would be confusing.", dontSendNotification); return; }
        bool clash = false;
        for (auto& u : host.store().user()) if (u.name == name && ! (hasEdit && u.name == editCopy.name)) clash = true;
        if (clash)
        {
            warnText->setText ("A preset named " + name + " already exists.", dontSendNotification);
            overwrite->setVisible (true); asNew->setVisible (true);
            overwrite->onClick = [commit, name] (const ModifierKeys&) { commit (name); };
            asNew->onClick = [nameBox, name, warnText, overwrite, asNew] (const ModifierKeys&)
            {
                nameBox->setText (name + " 2", false);
                warnText->setText ({}, dontSendNotification);
                overwrite->setVisible (false); asNew->setVisible (false);
            };
            return;
        }
        commit (name);
    };
    p->footer (hasEdit ? "Save changes" : "Save", saveFn, true);
    p->fit();
    show (std::move (p));
    nameBox->grabKeyboardFocus();
}

void Modal::openExport (const Preset* one)
{
    auto p = std::make_unique<Panel> (*this, host, "Export", 620);
    auto& c = host.context();
    p->textRow ({ { "Copy this text and keep it as your backup. Import it again later or on another computer." } });
    auto* box = p->make<TextEditor>();
    styleEditor (*box, c, true);
    box->setReadOnly (true);
    std::vector<Preset> list;
    if (one != nullptr) list.push_back (*one); else list = host.store().user();
    box->setText (host.store().exportJson (list), false);
    p->row ({ box }, { 0 }, 230);
    p->footer ("Copy", [this, box] { SystemClipboard::copyTextToClipboard (box->getText()); host.toast ("Copied"); });
    p->footer ("Back", [this] { openBrowser(); });
    p->fit();
    show (std::move (p));
}

void Modal::openImport()
{
    auto p = std::make_unique<Panel> (*this, host, "Import", 620);
    auto& c = host.context();
    p->textRow ({ { "Paste exported preset text here. Unknown settings are ignored and anything invalid is refused." } });
    auto* box = p->make<TextEditor>();
    styleEditor (*box, c, true);
    p->row ({ box }, { 0 }, 200);
    auto* warn = p->make<Label>();
    warn->setColour (Label::textColourId, theme::red);
    warn->setFont (theme::font (theme::FontKind::regular, 15.0f));
    p->row ({ warn }, { 0 }, 24);
    p->footer ("Back", [this] { openBrowser(); });
    p->footer ("Import", [this, box, warn]
    {
        const int n = host.store().importJson (box->getText());
        if (n < 0) { warn->setText ("Nothing imported: this is not valid Pawltec preset text.", dontSendNotification); return; }
        host.toast ("Imported " + String (n) + (n == 1 ? " preset" : " presets"));
        openBrowser();
    }, true);
    p->fit();
    show (std::move (p));
}
} // namespace ui
