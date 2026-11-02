#include "Presets.h"
using namespace juce;

namespace presets
{
static Preset mk (const char* name, const char* type, StringArray tags, std::initializer_list<std::pair<const char*, double>> over)
{
    Preset p; p.name = name; p.type = type; p.tags = tags; p.factory = true;
    for (auto& o : over)
        for (int i = 0; i < kCount; ++i)
            if (String (o.first) == keys[i]) p.v[i] = o.second;
    return p;
}

const std::vector<Preset>& factory()
{
    // The sixteen factory presets of the approved mockup. Unlisted values stay at the defaults.
    static const std::vector<Preset> list {
        mk ("Start Here", "Start", { "start", "general" }, { { "lowF", 2 }, { "lowB", 3 }, { "lowA", 1.5 }, { "hiF", 4 }, { "hiB", 2 }, { "bw", 5 }, { "attF", 2 }, { "hiA", 0 }, { "tube", 2 } }),
        mk ("Flat With Tubes", "Start", { "clean", "colour" }, { { "tube", 3 } }),
        mk ("Low Trick 60", "Mix", { "trick", "low end" }, { { "lowF", 2 }, { "lowB", 6.5 }, { "lowA", 4 }, { "tube", 1 } }),
        mk ("Low Trick 30", "Bass", { "trick", "sub" }, { { "lowF", 1 }, { "lowB", 7 }, { "lowA", 4 }, { "tube", 1 } }),
        mk ("Kick Weight", "Drums", { "kick" }, { { "lowF", 2 }, { "lowB", 5 }, { "lowA", 2 }, { "hiF", 1 }, { "hiB", 3 }, { "bw", 1 }, { "tube", 2 } }),
        mk ("Hard Dance Kick", "Drums", { "kick", "hard dance" }, { { "lowF", 2 }, { "lowB", 6 }, { "lowA", 3 }, { "hiF", 0 }, { "hiB", 4 }, { "bw", 0 }, { "attF", 1 }, { "hiA", 1 }, { "tube", 4 } }),
        mk ("Bass Body", "Bass", { "body" }, { { "lowF", 2 }, { "lowB", 4 }, { "lowA", 1 }, { "hiF", 1 }, { "hiB", 2 }, { "bw", 2 }, { "tube", 2 } }),
        mk ("Sub Clean", "Bass", { "sub", "clean" }, { { "lowF", 0 }, { "lowB", 4.5 }, { "lowA", 3 }, { "attF", 1 }, { "hiA", 2 }, { "tube", 1 } }),
        mk ("Vocal Air", "Vocal", { "air", "bright" }, { { "lowF", 3 }, { "lowB", 3 }, { "lowA", 2 }, { "hiF", 5 }, { "hiB", 3.5 }, { "bw", 8 }, { "attF", 0 }, { "hiA", 1 }, { "tube", 2 } }),
        mk ("Silky Top", "Master", { "smooth", "air" }, { { "hiF", 6 }, { "hiB", 4 }, { "bw", 10 }, { "tube", 1 } }),
        mk ("Mix Bus Glue", "Mix", { "bus", "gentle" }, { { "lowF", 2 }, { "lowB", 2 }, { "lowA", 1.5 }, { "hiF", 1 }, { "hiB", 1.5 }, { "bw", 10 }, { "attF", 2 }, { "hiA", 1 }, { "tube", 2 } }),
        mk ("Overhead Sheen", "Drums", { "cymbals", "air" }, { { "lowF", 3 }, { "lowB", 0 }, { "lowA", 3 }, { "hiF", 4 }, { "hiB", 4.5 }, { "bw", 7 }, { "attF", 0 }, { "hiA", 1 } }),
        mk ("Mud Cleaner", "Mix", { "cut", "clean" }, { { "lowF", 3 }, { "lowA", 5 }, { "tube", 1 } }),
        mk ("Presence Push", "Synth", { "lead", "presence" }, { { "hiF", 2 }, { "hiB", 5 }, { "bw", 3 }, { "tube", 2 } }),
        mk ("Warm Roll Off", "Synth", { "warm", "dark" }, { { "lowF", 3 }, { "lowB", 3 }, { "attF", 0 }, { "hiA", 5 }, { "tube", 3 } }),
        mk ("Smooth Highs", "Master", { "tame", "smooth" }, { { "hiF", 5 }, { "hiB", 1 }, { "bw", 9 }, { "attF", 1 }, { "hiA", 4 }, { "tube", 0 } }),
    };
    return list;
}

File userFolder()
{
   #if JUCE_LINUX || JUCE_BSD
    const String xdg = SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const File base = (xdg.isNotEmpty() && File::isAbsolutePath (xdg)) ? File (xdg)
                                                                       : File::getSpecialLocation (File::userHomeDirectory).getChildFile (".local/share");
   #else
    const File base = File::getSpecialLocation (File::userApplicationDataDirectory); // %APPDATA% or ~/Library/Application Support
   #endif
    return base.getChildFile ("Studio 87").getChildFile ("Pawltec").getChildFile ("presets");
}

bool valid (const var& o)
{
    auto* obj = o.getDynamicObject();
    if (obj == nullptr) return false;
    const var name = obj->getProperty ("name"), pv = obj->getProperty ("p");
    if (! name.isString() || name.toString().isEmpty() || name.toString().length() > 60) return false;
    auto* po = pv.getDynamicObject();
    if (po == nullptr) return false;
    for (auto& nv : po->getProperties())
        for (int i = 0; i < kCount; ++i)
            if (nv.name.toString() == keys[i])
            {
                if (! (nv.value.isDouble() || nv.value.isInt() || nv.value.isInt64())) return false;
                const double x = (double) nv.value;
                if (! std::isfinite (x) || x < lo[i] || x > hi[i] || (isIndex[i] && x != std::floor (x))) return false;
            }
    return true;
}

Preset fromVar (const var& o)
{
    Preset p;
    p.name = o["name"].toString().substring (0, 60);
    const String t = o["type"].toString();
    p.type = "Mix";
    for (auto* ty : types) if (t == ty) p.type = t;
    if (auto* arr = o["tags"].getArray())
        for (auto& tg : *arr)
            if (tg.isString() && p.tags.size() < 8) p.tags.add (tg.toString().substring (0, 20));
    if (auto* po = o["p"].getDynamicObject())
        for (int i = 0; i < kCount; ++i)
            if (po->hasProperty (keys[i])) p.v[i] = (double) po->getProperty (keys[i]);
    return p;
}

var toVar (const Preset& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("type", p.type);
    Array<var> tg; for (auto& t : p.tags) tg.add (t);
    o->setProperty ("tags", tg);
    auto* pv = new DynamicObject();
    for (int i = 0; i < kCount; ++i) pv->setProperty (keys[i], p.v[i]);
    o->setProperty ("p", var (pv));
    return var (o);
}

static String fileNameFor (const String& name)
{
    String s = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-+.");
    s = s.trim().replaceCharacters (" ", "_");
    if (s.isEmpty()) s = "preset";
    return s.substring (0, 60) + "-" + String::toHexString (name.hashCode()) + ".json";
}

Store::Store() { reload(); }

void Store::reload()
{
    mine.clear();
    auto dir = userFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        if (f.getSize() > 20000) continue;
        const var o = JSON::parse (f);
        if (! valid (o)) continue;
        Preset p = fromVar (o);
        bool clash = false;
        for (auto& x : factory()) if (x.name == p.name) clash = true;
        for (auto& x : mine) if (x.name == p.name) clash = true;
        if (! clash) mine.push_back (p);
    }
    std::sort (mine.begin(), mine.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

std::vector<Preset> Store::all() const
{
    std::vector<Preset> r = factory();
    r.insert (r.end(), mine.begin(), mine.end());
    return r;
}

const Preset* Store::find (const String& name) const
{
    for (auto& p : factory()) if (p.name == name) return &p;
    for (auto& p : mine) if (p.name == name) return &p;
    return nullptr;
}

bool Store::save (const Preset& p, const String& replaceName)
{
    auto dir = userFolder();
    if (! dir.createDirectory().wasOk()) return false;
    const String old = replaceName.isEmpty() ? p.name : replaceName;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        const var o = JSON::parse (f);
        if (o["name"].toString() == old || o["name"].toString() == p.name) f.deleteFile();
    }
    Preset q = p; q.factory = false;
    if (! dir.getChildFile (fileNameFor (q.name)).replaceWithText (JSON::toString (toVar (q), false))) return false;
    reload();
    return true;
}

bool Store::remove (const String& name)
{
    bool ok = false;
    for (auto& f : userFolder().findChildFiles (File::findFiles, false, "*.json"))
        if (JSON::parse (f)["name"].toString() == name) ok = f.deleteFile() || ok;
    reload();
    return ok;
}

String Store::exportJson (const std::vector<Preset>& list) const
{
    auto* root = new DynamicObject();
    root->setProperty ("pawltec", 1);
    Array<var> arr; for (auto& p : list) arr.add (toVar (p));
    root->setProperty ("presets", arr);
    return JSON::toString (var (root), false);
}

int Store::importJson (const String& text)
{
    const var o = JSON::parse (text);
    if (! o.isObject() || (int) o["pawltec"] != 1) return -1;
    auto* arr = o["presets"].getArray();
    if (arr == nullptr || arr->isEmpty() || arr->size() > 200) return -1;
    for (auto& x : *arr) if (! valid (x)) return -1;
    for (auto& x : *arr)
    {
        Preset p = fromVar (x);
        for (auto& f : factory()) if (f.name == p.name) p.name += " (imported)";
        save (p);
    }
    return arr->size();
}
} // namespace presets
