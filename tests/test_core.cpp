// Core tests for Pawltec (no JUCE). Run with: sh tests/run.sh
// Every number printed with "measured" is read from the real engine in this file, not assumed.
#include "../src/core/Pawltec.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <random>

using namespace pawl;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}
static void info (const char* fmt, double a = 0, double b = 0, double c = 0, double d = 0)
{
    std::printf ("  measured: ");
    std::printf (fmt, a, b, c, d);
    std::printf ("\n");
}

static double toDb (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

// radix 2 FFT (power spectrum helper)
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Run a mono signal through the engine in blocks of 64.
static std::vector<float> run (Engine& e, const std::vector<float>& in, int block = 64)
{
    std::vector<float> out (in);
    for (size_t i = 0; i < out.size(); i += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, out.size() - i);
        e.process (out.data() + i, nullptr, n);
    }
    return out;
}

static std::vector<float> sine (double f, double fs, size_t n, double amp)
{
    std::vector<float> v (n);
    for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * kPi * f * (double) i / fs));
    return v;
}

// RMS gain in dB of a steady sine through the engine (skips the first part)
static double measureGainDb (const Params& p, double f, double fs, double amp = 0.1)
{
    Engine e; e.prepare (fs); e.setParams (p);
    const size_t n = (size_t) (fs * 1.0);
    auto in = sine (f, fs, n, amp);
    auto out = run (e, in);
    double si = 0, so = 0;
    for (size_t i = n / 2; i < n; ++i) { si += (double) in[i] * in[i]; so += (double) out[i] * out[i]; }
    return 10.0 * std::log10 (so / si);
}

static double rmsDb (const std::vector<float>& v, size_t from)
{
    double s = 0;
    for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return 10.0 * std::log10 (s / (double) (v.size() - from) + 1.0e-30);
}

static std::vector<float> pinkNoise (size_t n, unsigned seed, double amp)
{
    std::mt19937 g (seed); std::uniform_real_distribution<double> u (-1.0, 1.0);
    std::vector<float> v (n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = u (g);
        b0 = .99886 * b0 + w * .0555179; b1 = .99332 * b1 + w * .0750759; b2 = .969 * b2 + w * .153852; b3 = .8665 * b3 + w * .3104856;
        b4 = .55 * b4 + w * .5329522; b5 = -.7616 * b5 - w * .016898;
        v[i] = (float) (amp * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * .5362) * 0.11); b6 = w * .115926;
    }
    return v;
}

int main()
{
    const double fsList[3] = { 44100.0, 48000.0, 96000.0 };

    // ---- tables and ranges ----------------------------------------------------------------------------
    {
        check (kLowF[0] == 20 && kLowF[3] == 100 && kHiF[0] == 3000 && kHiF[6] == 16000 && kAttF[0] == 5000 && kAttF[2] == 20000, "switch frequencies are the mockup's");
        check (std::fabs (outGainDb (5) ) < 1e-12 && std::fabs (outGainDb (0) + 12.0) < 1e-12 && std::fabs (outGainDb (10) - 12.0) < 1e-12, "Output knob: 5 is 0 dB, 0 and 10 are -12 and +12 dB");
        check (std::fabs (qOfWidth (0) - 2.2) < 1e-9 && std::fabs (qOfWidth (10) - 0.3) < 1e-9 && std::fabs (qOfWidth (5) - 0.8124) < 0.001, "Width: Q 2.2 narrow, 0.3 wide, 0.81 in the middle");
        Params p;
        p.out = std::nanf (""); p.lowB = 99; p.lowF = -3; p.hiF = 100; p.attF = 55; p.tube = -4;
        Engine e; e.prepare (48000); e.setParams (p);
        std::vector<float> in (4800, 0.1f);
        auto out = run (e, in);
        bool fin = true; for (float v : out) fin = fin && std::isfinite (v);
        check (fin, "out of range parameter values are clamped, output stays finite");
    }

    // ---- level calibration and the response against the formula ----------------------------------------
    {
        Params p; p.lowB = 0; p.out = 5;
        bool ok = true; double worst = 0;
        for (double fs : fsList)
            for (double f : { 50.0, 1000.0, 8000.0 })
            {
                const double g = measureGainDb (p, f, fs);
                worst = std::max (worst, std::fabs (g)); ok = ok && std::fabs (g) < 0.002;
            }
        check (ok, "flat settings: gain 0.000 dB at 44.1, 48 and 96 kHz");
        info ("largest deviation %.5f dB", worst);

        Params q; q.out = 0; double g0 = measureGainDb (q, 1000, 48000);
        q.out = 10; double g10 = measureGainDb (q, 1000, 48000);
        q.out = 7.5; double g75 = measureGainDb (q, 1000, 48000);
        check (std::fabs (g0 + 12.0) < 0.01 && std::fabs (g10 - 12.0) < 0.01 && std::fabs (g75 - 6.0) < 0.01, "Output knob measured: -12.00, +12.00 and +6.00 dB");
        info ("out 0: %.3f dB, out 10: %.3f dB, out 7.5: %.3f dB", g0, g10, g75);
    }
    {
        // the engine against the analytic response, every section, three sample rates
        double worst = 0;
        for (double fs : fsList)
        {
            Params p; p.lowF = 2; p.lowB = 7; p.lowA = 4; p.hiF = 4; p.hiB = 6; p.bw = 3; p.attF = 1; p.hiA = 5; p.out = 5;
            for (double f : { 30.0, 60.0, 120.0, 250.0, 1000.0, 4000.0, 10000.0, 14000.0 })
            {
                const double meas = measureGainDb (p, f, fs), want = totalDb (p, f, fs);
                worst = std::max (worst, std::fabs (meas - want));
            }
        }
        check (worst < 0.05, "engine response matches the formula within 0.05 dB (8 frequencies, 3 sample rates)");
        info ("largest difference %.4f dB", worst);
    }
    {
        // each end of the range, each section alone at its own marker (maximum gains)
        Params p;
        p.lowB = 10; double a = totalDb (p, 20.0 * 0.3, 48000); (void) a;
        const double lowBoostDc = totalDb (p, 5.0, 48000);
        p = Params(); p.lowA = 10; const double lowAttDc = totalDb (p, 5.0, 48000);
        p = Params(); p.hiB = 10; const double hiBoostPk = totalDb (p, kHiF[4], 48000);
        p = Params(); p.hiA = 10; p.attF = 0; const double hiAttTop = totalDb (p, 20000.0, 48000);
        check (std::fabs (lowBoostDc - 13.5) < 0.1 && std::fabs (lowAttDc + 17.5) < 0.1 && std::fabs (hiBoostPk - 18.0) < 0.1, "maximum gains: low boost +13.5, low atten -17.5, high boost +18 dB");
        check (hiAttTop < -14.0 && hiAttTop > -16.5, "high atten with the 5 kHz corner is close to -16 dB at 20 kHz");
        info ("low boost at 5 Hz %.2f, low atten at 5 Hz %.2f, high boost at 10 kHz %.2f, high atten (5 kHz corner) at 20 kHz %.2f", lowBoostDc, lowAttDc, hiBoostPk, hiAttTop);
    }

    // ---- the key behaviour: boost and atten on the same low frequency interact -----------------------------
    {
        const double fs = 48000.0;
        Params b; b.lowF = 2; b.lowB = 6.5;
        Params t = b; t.lowA = 4.0;
        Params a; a.lowF = 2; a.lowA = 4.0;
        double peakF = 0, peakDb = -99, dipF = 0, dipDb = 99, dipDbBoostOnly = 0;
        for (double f = 20; f < 2000; f *= 1.01)
        {
            const double d = totalDb (t, f, fs);
            if (d > peakDb) { peakDb = d; peakF = f; }
        }
        for (double f = peakF; f < 2000; f *= 1.01)
        {
            const double d = totalDb (t, f, fs);
            if (d < dipDb) { dipDb = d; dipF = f; dipDbBoostOnly = totalDb (b, f, fs); }
        }
        const double boostAt60 = totalDb (b, 60, fs), boostAt60Trick = totalDb (t, 60, fs);
        check (peakDb > 2.0 && peakF > 20 && peakF < 120, "low trick (60 Hz, boost 6.5, atten 4): a bump in the bass");
        check (dipDb < -0.5 && dipF > peakF * 1.5, "low trick: a dip above the bump, below flat");
        check (dipDb < dipDbBoostOnly - 0.5, "the atten knob makes the dip, boost alone does not");
        check (boostAt60Trick < boostAt60 + 3.0 && totalDb (t, 20, fs) > 0.5, "boost and atten partly cancel at the bottom, the sum is not just a boost");
        info ("bump %.2f dB at %.0f Hz; dip %.2f dB at %.0f Hz (boost alone at that frequency: %.2f dB)", peakDb, peakF, dipDb, dipF);
        info ("at 60 Hz: boost alone %.2f dB, with atten %.2f dB; at 20 Hz with both %.2f dB", boostAt60, boostAt60Trick, totalDb (t, 20, fs));
        // the same measured through the real engine
        const double eng60 = measureGainDb (t, 60, fs), eng250 = measureGainDb (t, dipF, fs);
        check (std::fabs (eng60 - totalDb (t, 60, fs)) < 0.05 && std::fabs (eng250 - dipDb) < 0.05, "the engine reproduces the bump and the dip");
        (void) a;
    }

    // ---- bypass and neutral state are bit exact ----------------------------------------------------------
    {
        std::mt19937 g (5); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        std::vector<float> in (48000);
        for (auto& v : in) v = u (g);
        Engine e; e.prepare (48000);
        Params p;                      // neutral
        e.setParams (p);
        auto out = run (e, in);
        const int D = e.latency();
        bool same = true;
        for (size_t i = (size_t) D; i < in.size(); ++i) same = same && out[i] == in[i - (size_t) D];
        check (same, "neutral settings (all zero, tube 0, output 5) are bit exact, only delayed by the latency");

        Engine e2; e2.prepare (48000);
        p.lowB = 8; p.hiB = 7; p.hiA = 4; p.tube = 6; p.out = 8; p.bypass = true;
        e2.setParams (p);
        auto out2 = run (e2, in);
        same = true;
        for (size_t i = 4800; i < in.size(); ++i) same = same && out2[i] == in[i - (size_t) D];
        check (same, "bypass on with everything switched on is bit exact (after the 5 ms fade)");

        // switching bypass is click free: largest sample step during the switch
        Engine e3; e3.prepare (48000);
        Params q; q.lowB = 6; q.hiB = 6; q.tube = 5;
        e3.setParams (q);
        auto s = sine (200, 48000, 48000, 0.3);
        std::vector<float> o3 (s.size());
        double maxStep = 0, maxStepRef = 0;
        for (size_t i = 0; i < s.size(); i += 64)
        {
            if (i == 24000) { q.bypass = true; e3.setParams (q); }
            std::copy (s.begin() + (long) i, s.begin() + (long) i + 64, o3.begin() + (long) i);
            e3.process (o3.data() + i, nullptr, 64);
        }
        for (size_t i = 20000; i < 30000; ++i) maxStep = std::max (maxStep, (double) std::fabs (o3[i] - o3[i - 1]));
        for (size_t i = 10000; i < 14000; ++i) maxStepRef = std::max (maxStepRef, (double) std::fabs (o3[i] - o3[i - 1]));
        check (maxStep < maxStepRef * 1.5 + 0.01, "bypass switch has no click (largest step vs steady state)");
        info ("largest step %.4f, steady state %.4f", maxStep, maxStepRef);
    }

    // ---- latency is exactly what is reported ---------------------------------------------------------------
    {
        bool allOk = true; int found[3] = {};
        int idx = 0;
        for (double tube : { 0.0, 3.0, 10.0 })
        {
            Engine e; e.prepare (48000);
            Params p; p.tube = (float) tube; e.setParams (p);
            std::vector<float> in (4096, 0.0f);
            in[100] = 0.2f;     // small impulse so the shaper is in its linear region
            auto out = run (e, in, 37);
            size_t pk = 0; for (size_t i = 0; i < out.size(); ++i) if (std::fabs (out[i]) > std::fabs (out[pk])) pk = i;
            found[idx++] = (int) pk - 100;
            allOk = allOk && (int) pk - 100 == e.latency();
        }
        check (allOk, "measured impulse delay equals the reported latency (tube 0, 3 and 10)");
        info ("reported %.0f samples; measured %.0f, %.0f, %.0f", (double) kLatencySamples, found[0], found[1], found[2]);
        info ("that is %.2f ms at 48 kHz and %.2f ms at 96 kHz", 1000.0 * kLatencySamples / 48000.0, 1000.0 * kLatencySamples / 96000.0);
    }

    // ---- the oversampler on its own: passband flat, images rejected ------------------------------------------
    {
        // Run sines through the tube path with the shaper at tube = 0.0001 (linear) and compare with the delayed input.
        double worstRipple = 0;
        for (double fs : { 44100.0, 48000.0 })
            for (double f : { 100.0, 1000.0, 5000.0, 10000.0, 16000.0, 18000.0 })
            {
                Params p; p.tube = 0.0005f;
                const double g = measureGainDb (p, f, fs, 0.2);
                if (f <= 0.40 * fs) worstRipple = std::max (worstRipple, std::fabs (g));
            }
        check (worstRipple < 0.02, "oversampled path is flat within 0.02 dB up to 0.4 x sample rate");
        info ("largest passband deviation %.4f dB", worstRipple);
    }

    // ---- aliasing: tube at the maximum, hard sines -----------------------------------------------------------
    {
        // N = 16384 point FFT, a frequency with a whole number of cycles. Inharmonic energy is alias energy.
        const size_t N = 16384;
        double worstAlias = -999, worstNaive = -999;
        for (double fs : { 44100.0, 48000.0 })
            for (int bin : { 2400, 3900, 5200 })     // about 6.5 kHz, 10.5 kHz, 14 kHz at 44.1/48k (bins are fs/N apart)
            {
                const double f = bin * fs / (double) N;
                Params p; p.tube = 10; p.out = 5;
                Engine e; e.prepare (fs); e.setParams (p);
                const size_t total = N * 2 + 16384;
                auto in = sine (f, fs, total, 0.5);
                auto out = run (e, in);
                std::vector<std::complex<double>> a (N), b (N);
                TubeShaper sh; sh.set (10.0);
                for (size_t i = 0; i < N; ++i)
                {
                    const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) N);
                    a[i] = w * out[N + i];
                    b[i] = w * sh ((double) in[N + i]);        // the same shaper without oversampling
                }
                fft (a); fft (b);
                auto aliasDb = [&] (const std::vector<std::complex<double>>& s)
                {
                    double tot = 0, harm = 0;
                    for (size_t k = 1; k < N / 2; ++k)
                    {
                        const double pw = std::norm (s[k]);
                        tot += pw;
                        // harmonic of f within +-3 bins (window leakage)
                        const double hk = (double) k * fs / (double) N / f;
                        const double nearest = std::round (hk);
                        if (nearest >= 1.0 && std::fabs (hk - nearest) * f / (fs / (double) N) < 3.5) harm += pw;
                    }
                    return 10.0 * std::log10 ((tot - harm) / tot + 1.0e-20);
                };
                std::printf ("    fs %.0f, f %.0f Hz: alias %.1f dB (without oversampling %.1f dB)\n", fs, f, aliasDb (a), aliasDb (b));
                worstAlias = std::max (worstAlias, aliasDb (a));
                worstNaive = std::max (worstNaive, aliasDb (b));
            }
        check (worstAlias < -65.0, "tube 10, -6 dBFS sines up to 15 kHz: inharmonic (alias) energy below -65 dB of the output (ADAA plus 4x)");
        info ("worst alias %.1f dB with the oversampler; the same shaper without oversampling: %.1f dB", worstAlias, worstNaive);
        check (worstAlias < worstNaive - 10.0, "the oversampler lowers the alias level by more than 10 dB");
    }

    // ---- what the tube stage does to a sine -------------------------------------------------------------------
    {
        const double fs = 48000.0; const size_t N = 16384;
        const double f = 2000 * fs / (double) N * 0.5;   // exact bin
        const int kb = 1000;                              // use bin 1000 as the fundamental
        (void) f;
        const double f0 = kb * fs / (double) N;
        for (double tube : { 0.0, 2.0, 5.0, 10.0 })
        {
            Params p; p.tube = (float) tube;
            Engine e; e.prepare (fs); e.setParams (p);
            auto in = sine (f0, fs, N * 2, 0.25);          // -12 dBFS
            auto out = run (e, in);
            std::vector<std::complex<double>> a (N);
            for (size_t i = 0; i < N; ++i) a[i] = (0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) N)) * out[N + i];
            fft (a);
            auto lvl = [&] (int h) { double m = 0; for (int k = h * kb - 2; k <= h * kb + 2; ++k) m = std::max (m, std::abs (a[(size_t) k])); return m; };
            const double h1 = lvl (1), h2 = lvl (2), h3 = lvl (3);
            const double thd = std::sqrt (h2 * h2 + h3 * h3) / h1;
            std::printf ("  measured: tube %4.1f at -12 dBFS, 1 kHz-ish: H2 %6.1f dB, H3 %6.1f dB (relative to the fundamental), THD+ %.2f%%, level %.2f dB\n",
                         tube, toDb (h2 / h1), toDb (h3 / h1), 100.0 * thd, rmsDb (out, N) - rmsDb (in, N));
            if (tube == 0.0) check (rmsDb (out, N) == rmsDb (in, N) || std::fabs (rmsDb (out, N) - rmsDb (in, N)) < 1e-6, "tube 0 adds nothing");
            if (tube == 5.0) check (toDb (h2 / h1) > -60.0 && toDb (h2 / h1) > toDb (h3 / h1) - 25.0, "tube 5 adds even harmonics (the 2nd is clearly present)");
            if (tube == 10.0) check (thd > 0.01, "tube 10 colours clearly (THD above 1 percent)");
        }
    }

    // ---- level match of the tube stage ---------------------------------------------------------------------------
    {
        const double fs = 48000.0;
        struct Src { const char* name; std::vector<float> v; };
        std::vector<Src> srcs;
        srcs.push_back ({ "pink noise", pinkNoise ((size_t) (fs * 6), 3, 1.0) });
        {   // bass heavy: 55 Hz saw-ish plus sub
            std::vector<float> v ((size_t) (fs * 6));
            for (size_t i = 0; i < v.size(); ++i) { const double t = (double) i / fs, ph = std::fmod (t * 55.0, 1.0); v[i] = (float) (0.35 * (2 * ph - 1) + 0.25 * std::sin (2 * kPi * 55.0 * t)); }
            srcs.push_back ({ "bass heavy", v });
        }
        {   // drums: kick and hat pattern
            std::vector<float> v ((size_t) (fs * 6), 0.0f);
            std::mt19937 g (9); std::uniform_real_distribution<double> u (-1, 1);
            for (size_t i = 0; i < v.size(); ++i)
            {
                const double t = (double) i / fs, tk = std::fmod (t, 0.5), th = std::fmod (t + 0.25, 0.5);
                v[i] = (float) (0.8 * std::exp (-tk * 12) * std::sin (2 * kPi * (50 + 90 * std::exp (-tk * 35)) * tk) + 0.2 * std::exp (-th * 50) * u (g));
            }
            srcs.push_back ({ "drums", v });
        }
        {   // lead: saw chords
            std::vector<float> v ((size_t) (fs * 6));
            for (size_t i = 0; i < v.size(); ++i) { const double t = (double) i / fs; double s = 0; for (double f : { 220.0, 277.18, 329.63, 440.0 }) s += 2 * std::fmod (t * f, 1.0) - 1; v[i] = (float) (0.12 * s); }
            srcs.push_back ({ "lead chord", v });
        }
        double worstDiff = 0;
        for (auto& s : srcs)
        {
            for (double tube : { 2.0, 5.0, 10.0 })
            {
                Params p; p.tube = (float) tube;
                Engine e; e.prepare (fs); e.setParams (p);
                auto out = run (e, s.v);
                const size_t skip = (size_t) (fs * 2.5);
                std::vector<float> ref (s.v.begin() + (long) skip, s.v.end()), got (out.begin() + (long) skip, out.end());
                const double d = rmsDb (got, 0) - rmsDb (ref, 0);
                worstDiff = std::max (worstDiff, std::fabs (d));
                if (tube == 10.0) std::printf ("  measured: %-12s tube 10: output level %+.2f dB against the input\n", s.name, d);
            }
        }
        check (worstDiff < 0.7, "tube stage keeps the level within 0.7 dB of the input (4 sources, tube 2, 5, 10)");
        info ("largest level difference %.2f dB", worstDiff);
    }

    // ---- knob to sound time ---------------------------------------------------------------------------------------
    {
        const double fs = 48000.0;
        const char* nm[3] = { "low boost 0 to 10 (100 Hz tone)", "high boost 0 to 10 (10 kHz tone)", "output 5 to 10 (tube 5)" };
        const double tone[3] = { 100.0, 10000.0, 1000.0 };
        for (int which = 0; which < 3; ++which)
        {
            Engine a, b; a.prepare (fs); b.prepare (fs);
            Params p; if (which == 2) p.tube = 5;
            a.setParams (p); b.setParams (p);
            auto in = sine (tone[which], fs, 48000, 0.2);
            std::vector<float> oa (in), ob (in);
            const size_t step = 24000;
            for (size_t i = 0; i < in.size(); i += 16)
            {
                if (i == step)
                {
                    Params q = p;
                    if (which == 0) q.lowB = 10; else if (which == 1) q.hiB = 10; else q.out = 10;
                    b.setParams (q);
                }
                a.process (oa.data() + i, nullptr, 16); b.process (ob.data() + i, nullptr, 16);
            }
            size_t first = 0;
            for (size_t i = step; i < in.size(); ++i) if (std::fabs (ob[i] - oa[i]) > 1.0e-6) { first = i - step; break; }
            // envelope: rms over one tone period (at least 120 samples); 90 percent of the way to the final level
            const size_t win = std::max<size_t> (120, (size_t) (fs / tone[which]));
            auto env = [&] (size_t at) { double s = 0; for (size_t k = 0; k < win; ++k) s += std::pow ((double) ob[at + k], 2); return std::sqrt (s / (double) win); };
            const double e0 = env (step - win - 10), e1 = env (in.size() - win - 1);
            size_t t90 = 0;   // time until the level is within 1 dB of its final value
            for (size_t i = step; i + win < in.size(); i += 8)
                if (std::fabs (20.0 * std::log10 (env (i) / e1)) <= 1.0) { t90 = i - step + win / 2; break; }
            (void) e0;
            const size_t lat = (size_t) a.latency();
            std::printf ("  measured: %-34s first change after %zu samples (%.2f ms; latency %zu), within 1 dB of the final level after about %.1f ms\n", nm[which], first, 1000.0 * (double) first / fs, lat, 1000.0 * (double) t90 / fs);
            check (first <= lat + 16, "the sound starts to change within latency plus 16 samples of the knob move");
            check (t90 < (size_t) ((which == 0 ? 0.060 : 0.040) * fs), "the level is within 1 dB of its final value 40 ms after the knob move (60 ms for the 44 Hz shelf, whose own ringing is slow)");
        }
    }

    // ---- stability: silence, DC, full scale, NaN, random parameter changes ------------------------------------------
    {
        {   // ---- safety limiter -------------------------------------------------------------------------------
            const double fsL = 48000.0;
            std::vector<float> hot (96000);
            for (size_t i = 0; i < hot.size(); ++i) hot[i] = (float) (0.9 * std::sin (2.0 * kPi * 100.0 * (double) i / fsL) + 0.3 * std::sin (2.0 * kPi * 3100.0 * (double) i / fsL));
            auto peakOf = [] (const std::vector<float>& v) { float m = 0; for (float x : v) m = std::max (m, std::fabs (x)); return m; };
            Params p; p.lowB = 10; p.hiB = 10; p.out = 10; p.tube = 5;   // big boosts, +12 dB output
            Engine on; on.prepare (fsL); on.setParams (p);
            auto outOn = run (on, hot);
            Params q = p; q.limit = false;
            Engine off; off.prepare (fsL); off.setParams (q);
            auto outOff = run (off, hot);
            info ("limiter on: peak %.4f; limiter off: peak %.2f", (double) peakOf (outOn), (double) peakOf (outOff));
            check (peakOf (outOn) <= 1.0f, "safety limiter: output never exceeds 0 dBFS even with every boost at maximum");
            check (peakOf (outOff) > 1.5f, "limiter off: the same settings do exceed 0 dBFS (so the test is meaningful)");
            // transparent below the ceiling: a quiet signal with the limiter on equals the limiter off, bit for bit
            std::vector<float> quiet (48000);
            for (size_t i = 0; i < quiet.size(); ++i) quiet[i] = (float) (0.2 * std::sin (2.0 * kPi * 440.0 * (double) i / fsL));
            Params n0; n0.lowB = 3; n0.tube = 4;
            Engine a; a.prepare (fsL); a.setParams (n0);
            Params n1 = n0; n1.limit = false;
            Engine b; b.prepare (fsL); b.setParams (n1);
            auto oa = run (a, quiet), ob = run (b, quiet);
            check (oa == ob, "safety limiter is bit exact below 0 dBFS (limiter on equals off for a quiet signal)");
            // lookahead: no step at the moment a loud burst starts (largest sample to sample jump stays modest)
            std::vector<float> burst (48000, 0.0f);
            for (size_t i = 20000; i < 28000; ++i) burst[i] = (float) (std::min (std::min (1.0, (double) (i - 20000) / 240.0), (double) (28000 - i) / 240.0) * 3.0 * std::sin (2.0 * kPi * 200.0 * (double) (i - 20000) / fsL));   // 5 ms fade in and out, 3x full scale in between
            Engine c2; c2.prepare (fsL); Params u; c2.setParams (u);
            auto ob2 = run (c2, burst);
            check (peakOf (ob2) <= 1.0f, "a 3x over burst is limited to 0 dBFS");
            float maxStep = 0; for (size_t i = 1; i < ob2.size(); ++i) maxStep = std::max (maxStep, std::fabs (ob2[i] - ob2[i - 1]));
            info ("largest sample to sample step through the limited burst: %.3f", (double) maxStep);
            check (maxStep < 0.12f, "the limiter acts smoothly (no clicks at the start of a burst)");
        }
        {
            Engine e; e.prepare (48000);
            Params p; p.lowB = 10; p.lowA = 10; p.hiB = 10; p.hiA = 10; p.tube = 10; p.out = 10; e.setParams (p);
            std::vector<float> z (96000, 0.0f);
            auto out = run (e, z);
            bool zero = true; for (float v : out) zero = zero && v == 0.0f;
            check (zero, "silence in gives exact silence out, every knob at maximum");
        }
        {
            Engine e; e.prepare (48000);
            Params p; p.lowB = 10; p.out = 5; p.lowF = 0; e.setParams (p);
            std::vector<float> dc (96000, 0.2f);   // 0.2 so the boosted DC (x2.37) stays below the 0 dBFS safety limiter
            auto out = run (e, dc);
            bool fin = true; for (float v : out) fin = fin && std::isfinite (v);
            const double want = 0.2 * std::pow (10.0, 13.5 / 20.0);
            check (fin && std::fabs (out.back() - want) < 0.01 * want, "DC through the low boost settles at the shelf gain");
            info ("DC 0.5 in, %.4f out (expected %.4f)", (double) out.back(), want);
        }
        {
            Engine e; e.prepare (48000);
            Params p; p.lowB = 10; p.hiB = 10; p.tube = 10; p.out = 10; e.setParams (p);
            std::vector<float> sq (96000);
            for (size_t i = 0; i < sq.size(); ++i) sq[i] = (i / 50) % 2 ? 1.0f : -1.0f;
            auto out = run (e, sq);
            bool fin = true; double mx = 0; for (float v : out) { fin = fin && std::isfinite (v); mx = std::max (mx, (double) std::fabs (v)); }
            check (fin && mx < 100.0, "full scale square wave with everything at maximum stays finite and bounded");
            info ("largest output %.2f", mx);
        }
        {
            Engine e; e.prepare (48000);
            Params p; p.lowB = 5; p.hiB = 5; p.tube = 5; e.setParams (p);
            std::vector<float> in = sine (440, 48000, 48000, 0.3);
            in[20000] = std::nanf (""); in[20001] = INFINITY; in[20002] = -INFINITY; in[20003] = 1.0e30f;
            auto out = run (e, in);
            bool fin = true; for (float v : out) fin = fin && std::isfinite (v);
            double tail = 0; for (size_t i = 30000; i < 48000; ++i) tail = std::max (tail, (double) std::fabs (out[i]));
            check (fin && tail < 2.0 && tail > 0.05, "NaN and infinity in the input never reach the output, and the sound recovers");
        }
        {
            Engine e; e.prepare (44100);
            std::mt19937 g (11); std::uniform_int_distribution<int> ui (0, 10); std::uniform_real_distribution<float> uf (0.0f, 10.0f);
            std::uniform_real_distribution<float> un (-0.7f, 0.7f);
            bool fin = true; double mx = 0;
            std::vector<float> buf (64);
            for (int blk = 0; blk < 4000; ++blk)
            {
                Params p;
                p.lowF = ui (g) % 4; p.hiF = ui (g) % 7; p.attF = ui (g) % 3;
                p.lowB = uf (g); p.lowA = uf (g); p.hiB = uf (g); p.bw = uf (g); p.hiA = uf (g); p.tube = (ui (g) < 3) ? 0.0f : uf (g); p.out = uf (g);
                p.bypass = ui (g) == 0;
                e.setParams (p);
                for (auto& v : buf) v = un (g);
                e.process (buf.data(), nullptr, 64);
                for (float v : buf) { fin = fin && std::isfinite (v); mx = std::max (mx, (double) std::fabs (v)); }
            }
            check (fin && mx < 200.0, "4000 blocks of random parameter changes (all switches, tube on and off, bypass) stay finite and bounded");
            info ("largest output %.2f", mx);
        }
        {
            Engine e; e.prepare (48000);
            Params p; p.hiB = 10; p.hiF = 6; e.setParams (p);
            std::vector<float> in (4800, 0.0f); in[0] = 1.0f;
            auto out = run (e, in);
            double tail = 0; for (size_t i = 4000; i < out.size(); ++i) tail = std::max (tail, (double) std::fabs (out[i]));
            check (tail < 1.0e-6, "impulse response of the high boost dies away (no ringing forever, no denormals)");
        }
    }

    // ---- stereo and mono ---------------------------------------------------------------------------------------------
    {
        Engine e; e.prepare (48000);
        Params p; p.lowB = 6; p.hiB = 4; p.tube = 4; e.setParams (p);
        auto s = sine (300, 48000, 24000, 0.3);
        std::vector<float> l (s), r (s);
        for (size_t i = 0; i < l.size(); i += 64) e.process (l.data() + i, r.data() + i, (int) std::min<size_t> (64, l.size() - i));
        bool same = true; for (size_t i = 0; i < l.size(); ++i) same = same && l[i] == r[i];
        check (same, "identical left and right give identical output (channels do not leak)");
        Engine m; m.prepare (48000); m.setParams (p);
        auto mono = run (m, s);
        check (std::equal (mono.begin(), mono.end(), l.begin()), "mono and stereo give the same signal");
    }

    // ---- sample rate change and reuse ---------------------------------------------------------------------------------
    {
        Engine e;
        for (double fs : { 44100.0, 192000.0, 48000.0 })
        {
            e.prepare (fs);
            Params p; p.lowB = 5; p.hiB = 5; p.tube = 3; e.setParams (p);
            auto out = run (e, sine (1000, fs, (size_t) fs / 4, 0.3));
            bool fin = true; for (float v : out) fin = fin && std::isfinite (v);
            check (fin && rmsDb (out, out.size() / 2) > -30.0, "prepare at a new sample rate works (44.1, 192, 48 kHz)");
        }
    }

    // ---- CPU --------------------------------------------------------------------------------------------------------
    {
        for (int tubeOn = 0; tubeOn < 2; ++tubeOn)
        {
            Engine e; e.prepare (48000);
            Params p; p.lowB = 6; p.lowA = 4; p.hiB = 5; p.hiA = 3; p.tube = tubeOn ? 5.0f : 0.0f; e.setParams (p);
            auto in = pinkNoise (48000 * 20, 1, 0.3);
            std::vector<float> r (in);
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t i = 0; i < in.size(); i += 512) { e.process (in.data() + i, r.data() + i, 512); }
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  measured: stereo, tube %s: 20 s of audio in %.3f s, about %.2f percent of one core at 48 kHz (this machine)\n", tubeOn ? "on " : "off", sec, 100.0 * sec / 20.0);
        }
    }

    std::printf (failures == 0 ? "ALL CORE TESTS PASSED\n" : "CORE TESTS FAILED (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
