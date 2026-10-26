// reverb_core.h — original stereo reverb engine (header-only, no dependencies).
//
// Topologies are built from published, public-domain DSP literature:
//   * Plate : figure-eight "tank" with modulated all-pass diffusers
//             (J. Dattorro, "Effect Design Part 1", JAES 1997)
//   * Room / Hall : 8-line feedback delay network with a Householder
//             feedback matrix and Jot-style per-line damping
//             (J.-M. Jot & A. Chaigne, 1991; Stautner & Puckette, 1982)
// Everything here is written from scratch for this project.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vv {

constexpr float kPi = 3.14159265358979323846f;

inline float flush(float x) { return (std::fabs(x) < 1e-20f) ? 0.f : x; }

inline bool isPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; ++i) if (n % i == 0) return false;
    return true;
}
inline int nearestPrime(int n) {
    for (int d = 0;; ++d) {
        if (isPrime(n + d)) return n + d;
        if (n - d > 1 && isPrime(n - d)) return n - d;
    }
}

// ---------------------------------------------------------------- delay line
class DelayLine {
public:
    void init(int maxSamples) {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf_.assign(n, 0.f);
        mask_ = n - 1;
        w_ = 0;
    }
    void clear() { std::fill(buf_.begin(), buf_.end(), 0.f); }
    inline void push(float x) { w_ = (w_ + 1) & mask_; buf_[w_] = x; }
    // delay in samples (>=1 for fractional read); 0 = most recent sample
    inline float read(int d) const { return buf_[(w_ - d) & mask_]; }
    inline float read(float d) const {  // 4-point Hermite
        int   i = (int)d;
        float f = d - (float)i;
        float xm1 = buf_[(w_ - i + 1) & mask_];
        float x0  = buf_[(w_ - i) & mask_];
        float x1  = buf_[(w_ - i - 1) & mask_];
        float x2  = buf_[(w_ - i - 2) & mask_];
        float c = (x1 - xm1) * 0.5f;
        float v = x0 - x1;
        float w = c + v;
        float a = w + v + (x2 - x0) * 0.5f;
        float b = w + a;
        return ((a * f - b) * f + c) * f + x0;
    }
private:
    std::vector<float> buf_;
    int mask_ = 0, w_ = 0;
};

// ----------------------------------------------------------------- one pole
struct OnePoleLP {
    float a = 0.f, z = 0.f;
    void setCutoff(float fc, float fs) { a = std::exp(-2.f * kPi * std::min(fc, 0.49f * fs) / fs); }
    inline float process(float x) { z = flush((1.f - a) * x + a * z); return z; }
    void reset() { z = 0.f; }
};

// ------------------------------------------------------------------ biquad
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setLP(float fc, float fs, float q = 0.7071f) {
        float w = 2.f * kPi * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), s = std::sin(w);
        float al = s / (2.f * q), a0 = 1.f + al;
        b0 = (1.f - c) * 0.5f / a0; b1 = (1.f - c) / a0; b2 = b0;
        a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
    }
    void setHP(float fc, float fs, float q = 0.7071f) {
        float w = 2.f * kPi * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), s = std::sin(w);
        float al = s / (2.f * q), a0 = 1.f + al;
        b0 = (1.f + c) * 0.5f / a0; b1 = -(1.f + c) / a0; b2 = b0;
        a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
    }
    inline float process(float x) {
        float y = b0 * x + z1;
        z1 = flush(b1 * x - a1 * y + z2);
        z2 = flush(b2 * x - a2 * y);
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// --------------------------------------------------------- Schroeder all-pass
// w[n] = x[n] + g*w[n-D];  y[n] = w[n-D] - g*w[n]
class Allpass {
public:
    void init(int maxSamples) { dl_.init(maxSamples); }
    void clear() { dl_.clear(); }
    inline float process(float x, float d, float g) {
        float wd = dl_.read(d);
        float w  = x + g * wd;
        dl_.push(flush(w));
        return wd - g * w;
    }
    inline float tap(float d) const { return dl_.read(d); }
private:
    DelayLine dl_;
};

// ------------------------------------------------------------------- LFO
struct Lfo {
    float phase = 0.f;
    inline float tick(float inc) {
        phase += inc; if (phase >= 1.f) phase -= 1.f;
        // parabolic sine approximation
        float x = phase * 2.f - 1.f;
        float y = 4.f * x * (1.f - std::fabs(x));          // ~ -sin
        return -y * (0.775f + 0.225f * std::fabs(y));
    }
};

// ============================================================== parameters
enum class Mode { Plate = 0, Room = 1, Hall = 2 };

struct Params {
    Mode  mode      = Mode::Hall;
    float mix       = 0.35f;     // 0..1
    float preDelay  = 0.f;       // ms
    float decay     = 2.5f;      // seconds (RT60, mid band)
    float size      = 0.5f;      // 0..1
    float diffusion = 0.8f;      // 0..1
    float dampHz    = 7000.f;    // high-frequency damping corner inside the loop
    float bassMult  = 1.0f;      // low-frequency decay multiplier (0.4..2)
    float bassHz    = 250.f;
    float modRate   = 0.6f;      // Hz
    float modDepth  = 0.4f;      // 0..1
    float erLevel   = 0.5f;      // early reflections (room / hall)
    float lowCut    = 20.f;      // Hz (wet)
    float highCut   = 20000.f;   // Hz (wet)
};

// ================================================================ the engine
class Reverb {
public:
    void prepare(float sampleRate) {
        fs_ = sampleRate;
        float k = fs_ / 29761.f;                    // Dattorro reference rate
        auto S = [&](float n) { return (int)std::ceil(n * k); };

        // pre-delay (max 500 ms)
        preL_.init((int)(0.5f * fs_) + 4);
        preR_.init((int)(0.5f * fs_) + 4);

        // ---- input diffusion (shared by all modes)
        const float inLen[4] = {142, 107, 379, 277};
        for (int i = 0; i < 4; ++i) { inApL_[i].init(S(inLen[i] * 2.2f)); inApR_[i].init(S(inLen[i] * 2.2f)); }

        // ---- plate tank
        const float apm[2] = {672, 908}, d1[2] = {4453, 4217}, ap2[2] = {1800, 2656}, d2[2] = {3720, 3163};
        for (int b = 0; b < 2; ++b) {
            pApMod_[b].init(S(apm[b] * 3.2f) + 64);
            pD1_[b].init(S(d1[b] * 3.2f));
            pAp2_[b].init(S(ap2[b] * 3.2f));
            pD2_[b].init(S(d2[b] * 3.2f));
            pBase_[b][0] = apm[b] * k; pBase_[b][1] = d1[b] * k; pBase_[b][2] = ap2[b] * k; pBase_[b][3] = d2[b] * k;
        }

        // ---- FDN
        static const int baseLen[kN] = {1087, 1283, 1427, 1613, 1789, 1993, 2237, 2503};
        for (int i = 0; i < kN; ++i) {
            fdnBase_[i] = nearestPrime((int)(baseLen[i] * fs_ / 44100.f));
            fdn_[i].init((int)(fdnBase_[i] * 3.4f) + 200);
        }
        const float erL[12] = {0.0043f, 0.0097f, 0.0151f, 0.0212f, 0.0278f, 0.0341f,
                               0.0409f, 0.0476f, 0.0551f, 0.0623f, 0.0711f, 0.0803f};
        for (int i = 0; i < 12; ++i) {
            erTapL_[i] = (int)(erL[i] * fs_);
            erTapR_[i] = (int)(erL[(i * 5 + 3) % 12] * 1.07f * fs_);
        }
        erBuf_[0].init((int)(0.2f * fs_)); erBuf_[1].init((int)(0.2f * fs_));

        reset();
        smInit_ = false;
    }

    void reset() {
        preL_.clear(); preR_.clear();
        for (int i = 0; i < 4; ++i) { inApL_[i].clear(); inApR_[i].clear(); }
        for (int b = 0; b < 2; ++b) { pApMod_[b].clear(); pD1_[b].clear(); pAp2_[b].clear(); pD2_[b].clear(); pDamp_[b].reset(); pFb_[b] = 0.f; bassSt_[b] = 0.f; }
        for (int i = 0; i < kN; ++i) { fdn_[i].clear(); fdnLp_[i].reset(); fdnBass_[i].reset(); fdnOut_[i] = 0.f; }
        erBuf_[0].clear(); erBuf_[1].clear();
        for (auto& l : bwL_) l.reset();
        bwR_.reset();
        for (int c = 0; c < 2; ++c) { hp_[c].reset(); lp_[c].reset(); }
        for (int i = 0; i < kN; ++i) lfo_[i].phase = (float)i / kN;
        lfoP_[0].phase = 0.f; lfoP_[1].phase = 0.5f;
    }

    void setParams(const Params& p) { target_ = p; if (!smInit_) { cur_ = p; smInit_ = true; updateStatic(); } }

    void process(float* l, float* r, int n) {
        int pos = 0;
        while (pos < n) {
            int blk = std::min(kBlock, n - pos);
            smooth(blk);
            for (int i = 0; i < blk; ++i) tick(l[pos + i], r[pos + i]);
            pos += blk;
        }
    }

private:
    static constexpr int kN = 8;
    static constexpr int kBlock = 16;

    // -------------------------------------------------------- control smoothing
    void smooth(int blk) {
        float a = 1.f - std::exp(-(float)blk / (0.06f * fs_));   // ~60 ms
        auto sm = [&](float& c, float t) { c += (t - c) * a; };
        cur_.mode = target_.mode;
        sm(cur_.mix, target_.mix);       sm(cur_.preDelay, target_.preDelay);
        sm(cur_.decay, target_.decay);   sm(cur_.size, target_.size);
        sm(cur_.diffusion, target_.diffusion);
        sm(cur_.dampHz, target_.dampHz); sm(cur_.bassMult, target_.bassMult);
        sm(cur_.modRate, target_.modRate); sm(cur_.modDepth, target_.modDepth);
        sm(cur_.erLevel, target_.erLevel);
        sm(cur_.lowCut, target_.lowCut); sm(cur_.highCut, target_.highCut);
        updateStatic();
    }

    void updateStatic() {
        const Params& p = cur_;
        sizeScale_ = std::pow(2.f, (p.size - 0.5f) * 3.2f);              // ~0.33 .. 3.0
        float T = std::max(0.05f, p.decay);
        diffG_ = 0.35f + 0.4f * p.diffusion;                              // 0.35 .. 0.75
        dampA_ = std::exp(-2.f * kPi * std::min(p.dampHz, 0.49f * fs_) / fs_);
        bassA_ = std::exp(-2.f * kPi * p.bassHz / fs_);
        pDamp_[0].a = pDamp_[1].a = dampA_;
        modInc_ = p.modRate / fs_;
        preSamp_ = p.preDelay * 0.001f * fs_;

        if (p.mode == Mode::Plate) {
            float k = fs_ / 29761.f;
            float loopB = (pBase_[0][0] + pBase_[0][1] + pBase_[0][2] + pBase_[0][3]) * sizeScale_ * 0.5f
                        + (pBase_[1][0] + pBase_[1][1] + pBase_[1][2] + pBase_[1][3]) * sizeScale_ * 0.5f;
            (void)k;
            // two branches in series form one loop of length loopB
            plateG_ = std::min(0.9995f, std::pow(10.f, -3.f * loopB / (fs_ * T)));
            plateGlow_ = std::min(0.9995f, std::pow(10.f, -3.f * loopB / (fs_ * T * p.bassMult)));
        } else {
            float modeScale = (p.mode == Mode::Room) ? 0.55f : 1.0f;
            for (int i = 0; i < kN; ++i) {
                float len = fdnBase_[i] * sizeScale_ * modeScale;
                len = std::max(len, 24.f);
                fdnLen_[i] = len;
                fdnG_[i] = std::min(0.9998f, std::pow(10.f, -3.f * len / (fs_ * T)));
                fdnGlow_[i] = std::min(0.9998f, std::pow(10.f, -3.f * len / (fs_ * T * p.bassMult)));
            }
        }
        float lc = std::max(10.f, p.lowCut), hc = std::min(p.highCut, 0.45f * fs_);
        for (int c = 0; c < 2; ++c) {
            if (lc != lastLc_) hp_[c].setHP(lc, fs_);
            if (hc != lastHc_) lp_[c].setLP(hc, fs_);
        }
        lastLc_ = lc; lastHc_ = hc;
    }

    // --------------------------------------------------------------- per-sample
    inline void tick(float& inL, float& inR) {
        const Params& p = cur_;
        float dryL = inL, dryR = inR;

        preL_.push(inL); preR_.push(inR);
        float xl = preSamp_ < 1.f ? inL : preL_.read(preSamp_);
        float xr = preSamp_ < 1.f ? inR : preR_.read(preSamp_);

        float wl = 0.f, wr = 0.f;
        if (p.mode == Mode::Plate) plateTick(xl, xr, wl, wr);
        else                        fdnTick(xl, xr, wl, wr);

        wl = lp_[0].process(hp_[0].process(wl));
        wr = lp_[1].process(hp_[1].process(wr));

        inL = dryL * (1.f - p.mix) + wl * p.mix;
        inR = dryR * (1.f - p.mix) + wr * p.mix;
    }

    // ----------------------------------------------------------------- Plate
    inline void plateTick(float xl, float xr, float& oL, float& oR) {
        float in = 0.5f * (xl + xr);
        // bandwidth: gentle one-pole
        in = bwR_.process(in);
        float g1 = diffG_ + 0.1f, g2 = diffG_ - 0.1f;
        float k = fs_ / 29761.f;
        static const float inLen[4] = {142, 107, 379, 277};
        in = apMono(inApL_[0], in, inLen[0] * k, g1);
        in = apMono(inApL_[1], in, inLen[1] * k, g1);
        in = apMono(inApL_[2], in, inLen[2] * k, g2);
        in = apMono(inApL_[3], in, inLen[3] * k, g2);

        float mod0 = lfoP_[0].tick(modInc_), mod1 = lfoP_[1].tick(modInc_ * 1.11f);
        float depth = (0.0009f * fs_) * cur_.modDepth * std::min(1.f, sizeScale_ + 0.3f);  // up to ~0.9 ms
        float modv[2] = {mod0 * depth, mod1 * depth};

        float sz = sizeScale_, newFb[2];
        for (int b = 0; b < 2; ++b) {
            float x = in + pFb_[1 - b];                          // cross-feedback
            float m = pBase_[b][0] * sz * 0.5f + 16.f;          // keep modulated AP well inside its buffer
            x = pApMod_[b].process(x, m + modv[b], -(diffG_ + 0.05f));
            pD1_[b].push(x);
            float y = pD1_[b].read(std::max(2.f, pBase_[b][1] * sz));
            y = pDamp_[b].process(y);
            // bass: lift low band so low frequencies decay slower/faster than mids
            float lowB = bassSt_[b] = flush((1.f - bassA_) * y + bassA_ * bassSt_[b]);
            y = plateG_ * y + (plateGlow_ - plateG_) * lowB;   // decay: mid gain + low-band offset
            y = pAp2_[b].process(y, std::max(2.f, pBase_[b][2] * sz), diffG_ - 0.05f);
            pD2_[b].push(y);
            newFb[b] = pD2_[b].read(std::max(2.f, pBase_[b][3] * sz));
        }
        pFb_[0] = flush(newFb[0]); pFb_[1] = flush(newFb[1]);

        // output taps: spread over both branches, opposite polarity mixes for width
        auto T = [&](DelayLine& d, float frac, float base) { return d.read(std::max(2.f, base * sz * frac)); };
        oL = 0.6f * (T(pD1_[1], 0.06f, pBase_[1][1]) + T(pD1_[1], 0.67f, pBase_[1][1])
                     - pAp2_[1].tap(std::max(2.f, pBase_[1][2] * sz * 0.72f))
                     + T(pD2_[1], 0.54f, pBase_[1][3])
                     - T(pD1_[0], 0.45f, pBase_[0][1])
                     - pAp2_[0].tap(std::max(2.f, pBase_[0][2] * sz * 0.10f))
                     - T(pD2_[0], 0.29f, pBase_[0][3]));
        oR = 0.6f * (T(pD1_[0], 0.05f, pBase_[0][1]) + T(pD1_[0], 0.67f, pBase_[0][1])
                     - pAp2_[0].tap(std::max(2.f, pBase_[0][2] * sz * 0.70f))
                     + T(pD2_[0], 0.53f, pBase_[0][3])
                     - T(pD1_[1], 0.44f, pBase_[1][1])
                     - pAp2_[1].tap(std::max(2.f, pBase_[1][2] * sz * 0.09f))
                     - T(pD2_[1], 0.28f, pBase_[1][3]));
    }

    static inline float apMono(Allpass& a, float x, float d, float g) { return a.process(x, std::max(2.f, d), g); }

    // ------------------------------------------------------------------- FDN
    inline void fdnTick(float xl, float xr, float& oL, float& oR) {
        // input diffusion (4 stages per channel)
        static const float lenL[4] = {113, 179, 263, 401}, lenR[4] = {131, 191, 281, 419};
        float k = fs_ / 44100.f, g = diffG_;
        float l = xl, r = xr;
        for (int i = 0; i < 4; ++i) {
            l = apMono(inApL_[i], l, lenL[i] * k, g);
            r = apMono(inApR_[i], r, lenR[i] * k, g);
        }

        // early reflections
        erBuf_[0].push(xl); erBuf_[1].push(xr);
        float erl = 0.f, err = 0.f;
        float erScale = sizeScale_ * (cur_.mode == Mode::Room ? 0.6f : 1.f);
        for (int i = 0; i < 12; ++i) {
            float sgn = (i & 1) ? -1.f : 1.f, amp = std::pow(0.88f, (float)i);
            erl += sgn * amp * erBuf_[0].read(std::max(1, (int)(erTapL_[i] * erScale)));
            err += -sgn * amp * erBuf_[1].read(std::max(1, (int)(erTapR_[i] * erScale)));
        }
        erl *= 0.3f * cur_.erLevel; err *= 0.3f * cur_.erLevel;

        // read lines (with modulation)
        float depth = 0.0012f * fs_ * cur_.modDepth;                  // up to ~1.2 ms
        float y[kN], sum = 0.f;
        for (int i = 0; i < kN; ++i) {
            float m = lfo_[i].tick(modInc_ * (0.87f + 0.035f * i));
            float d = std::max(depth + 2.f, fdnLen_[i]) + m * depth;
            y[i] = fdn_[i].read(d);
            sum += y[i];
        }
        // Householder reflection:  v = y - (2/N) * sum(y)
        float h = sum * (2.f / kN);
        for (int i = 0; i < kN; ++i) {
            float v = y[i] - h;
            // loop filters: HF damping (one pole) + low-band decay adjust
            float s = (1.f - dampA_) * v + dampA_ * fdnLp_[i].z;
            fdnLp_[i].z = flush(s);
            float lowB = fdnBass_[i].z = flush((1.f - bassA_) * s + bassA_ * fdnBass_[i].z);
            s = fdnG_[i] * s + (fdnGlow_[i] - fdnG_[i]) * lowB;
            float inj = ((i & 1) ? l : r) * ((i & 2) ? 1.f : -1.f) * 0.5f
                      + ((i & 1) ? r : l) * ((i & 4) ? 0.35f : -0.35f);
            fdn_[i].push(flush(s + inj));
            fdnOut_[i] = y[i];
        }
        // decorrelated stereo taps
        float L = 0.f, R = 0.f;
        for (int i = 0; i < kN; ++i) {
            float sl = (i % 2 == 0) ? 1.f : -1.f;
            float sr = ((i / 2) % 2 == 0) ? 1.f : -1.f;
            L += sl * y[i]; R += sr * y[(i + 1) % kN];
        }
        const float norm = 0.5f;
        oL = norm * L + erl;
        oR = norm * R + err;
    }

    // ------------------------------------------------------------------ state
    float fs_ = 44100.f;
    Params cur_, target_;
    bool smInit_ = false;

    DelayLine preL_, preR_;
    Allpass inApL_[4], inApR_[4];
    OnePoleLP bwL_[1], bwR_;

    // plate
    Allpass pApMod_[2], pAp2_[2];
    DelayLine pD1_[2], pD2_[2];
    OnePoleLP pDamp_[2];
    float pBase_[2][4] = {};
    float pFb_[2] = {};
    float bassSt_[2] = {};
    float plateG_ = 0.5f, plateGlow_ = 0.5f;
    Lfo lfoP_[2];

    // fdn
    DelayLine fdn_[kN];
    OnePoleLP fdnLp_[kN], fdnBass_[kN];
    int   fdnBase_[kN] = {};
    float fdnLen_[kN] = {}, fdnG_[kN] = {}, fdnGlow_[kN] = {}, fdnOut_[kN] = {};
    Lfo   lfo_[kN];
    DelayLine erBuf_[2];
    int erTapL_[12] = {}, erTapR_[12] = {};

    // common derived
    float sizeScale_ = 1.f, diffG_ = 0.6f, dampA_ = 0.5f, bassA_ = 0.97f, modInc_ = 0.f, preSamp_ = 0.f;
    Biquad hp_[2], lp_[2];
    float lastLc_ = -1.f, lastHc_ = -1.f;
};

}  // namespace vv
