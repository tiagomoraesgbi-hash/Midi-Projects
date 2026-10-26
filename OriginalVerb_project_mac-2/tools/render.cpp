// render.cpp — offline renderer: impulse-response generator and file processor.
// usage: render ir  out.wav [key=value ...]
//        render proc in.wav out.wav [key=value ...]
#include "../core/reverb_core.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>

static void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int fs) {
    FILE* f = fopen(path, "wb"); if (!f) { perror(path); exit(1); }
    uint32_t n = (uint32_t)l.size(), dataBytes = n * 2 * 4, v;
    auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, f); };
    auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(3); w16(2); w32(fs); w32(fs * 8); w16(8); w16(32);
    fwrite("data", 1, 4, f); w32(dataBytes);
    for (uint32_t i = 0; i < n; ++i) { fwrite(&l[i], 4, 1, f); fwrite(&r[i], 4, 1, f); }
    (void)v; fclose(f);
}
static bool readWav(const char* path, std::vector<float>& l, std::vector<float>& r, int& fs) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    char id[4]; uint32_t sz; fread(id, 1, 4, f); fread(&sz, 4, 1, f); fread(id, 1, 4, f);
    uint16_t fmt = 0, ch = 0, bits = 0; uint32_t rate = 0;
    while (fread(id, 1, 4, f) == 4 && fread(&sz, 4, 1, f) == 1) {
        if (!memcmp(id, "fmt ", 4)) {
            uint16_t x; fread(&fmt, 2, 1, f); fread(&ch, 2, 1, f); fread(&rate, 4, 1, f);
            fseek(f, 6, SEEK_CUR); fread(&bits, 2, 1, f); fseek(f, sz - 16, SEEK_CUR); (void)x;
        } else if (!memcmp(id, "data", 4)) {
            std::vector<uint8_t> raw(sz); fread(raw.data(), 1, sz, f);
            int bps = bits / 8; size_t n = sz / (bps * ch);
            l.resize(n); r.resize(n); fs = (int)rate;
            for (size_t i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) {
                const uint8_t* p = &raw[(i * ch + std::min<int>(c, ch - 1)) * bps]; float v = 0;
                if (fmt == 3) memcpy(&v, p, 4);
                else if (bits == 16) v = *(const int16_t*)p / 32768.f;
                else if (bits == 24) v = ((int32_t)(p[0] << 8 | p[1] << 16 | p[2] << 24)) / 2147483648.f;
                else if (bits == 32) v = *(const int32_t*)p / 2147483648.f;
                (c ? r : l)[i] = v;
            }
            fclose(f); return true;
        } else fseek(f, sz, SEEK_CUR);
    }
    fclose(f); return false;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: render ir out.wav [k=v..] | render proc in.wav out.wav [k=v..]\n"); return 1; }
    std::string cmd = argv[1]; int argi = 2;
    std::string inPath, outPath;
    if (cmd == "proc") { inPath = argv[argi++]; }
    outPath = argv[argi++];
    std::map<std::string, float> kv; std::string modeStr = "hall";
    for (; argi < argc; ++argi) {
        std::string a = argv[argi]; auto e = a.find('=');
        if (e == std::string::npos) continue;
        std::string k = a.substr(0, e), v = a.substr(e + 1);
        if (k == "mode") modeStr = v; else kv[k] = (float)atof(v.c_str());
    }
    vv::Params p;
    p.mode = modeStr == "plate" ? vv::Mode::Plate : modeStr == "room" ? vv::Mode::Room : vv::Mode::Hall;
    auto g = [&](const char* k, float& dst) { if (kv.count(k)) dst = kv[k]; };
    g("mix", p.mix); g("predelay", p.preDelay); g("decay", p.decay); g("size", p.size);
    g("diffusion", p.diffusion); g("damp", p.dampHz); g("bass", p.bassMult); g("basshz", p.bassHz);
    g("modrate", p.modRate); g("moddepth", p.modDepth); g("er", p.erLevel);
    g("lowcut", p.lowCut); g("highcut", p.highCut);
    int fs = (int)(kv.count("fs") ? kv["fs"] : 48000);

    std::vector<float> l, r;
    if (cmd == "ir") {
        float secs = kv.count("len") ? kv["len"] : std::min(30.f, p.decay * 1.6f + 1.f);
        p.mix = kv.count("mix") ? p.mix : 1.f;
        l.assign((size_t)(secs * fs), 0.f); r = l; l[0] = r[0] = 1.f;
    } else if (!readWav(inPath.c_str(), l, r, fs)) { fprintf(stderr, "cannot read %s\n", inPath.c_str()); return 1; }
    else {  // add tail
        size_t tail = (size_t)(std::min(30.f, p.decay * 1.5f) * fs);
        l.resize(l.size() + tail, 0.f); r.resize(r.size() + tail, 0.f);
    }
    vv::Reverb rv; rv.prepare((float)fs); rv.setParams(p);
    rv.process(l.data(), r.data(), (int)l.size());
    writeWav(outPath.c_str(), l, r, fs);
    return 0;
}
