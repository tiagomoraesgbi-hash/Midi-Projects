#!/usr/bin/env python3
"""Impulse-response analysis: RT60 per band, echo density, stereo correlation, spectrum."""
import sys, numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfiltfilt

BANDS = [125, 250, 500, 1000, 2000, 4000, 8000]

def load(path):
    fs, x = wavfile.read(path)
    if x.dtype.kind == 'i': x = x / np.iinfo(x.dtype).max
    x = x.astype(np.float64)
    if x.ndim == 1: x = np.stack([x, x], 1)
    return fs, x

def edc_db(h):
    e = np.cumsum((h ** 2)[::-1])[::-1]
    return 10 * np.log10(e / e[0] + 1e-30)

def rt60(h, fs, lo=-5, hi=-35):
    d = edc_db(h)
    i0 = np.argmax(d <= lo); i1 = np.argmax(d <= hi)
    if i1 <= i0 + 10: return float('nan')
    t = np.arange(i0, i1) / fs
    s = np.polyfit(t, d[i0:i1], 1)[0]
    return -60.0 / s

def band(h, fs, fc):
    sos = butter(4, [fc / 2 ** 0.5, min(fc * 2 ** 0.5, 0.45 * fs)], 'band', fs=fs, output='sos')
    return sosfiltfilt(sos, h)

def echo_density(h, fs, win=0.02):
    n = int(win * fs); out = []
    for s in range(0, min(len(h) - n, int(1.0 * fs)), n):
        seg = h[s:s + n]; sd = seg.std()
        out.append(np.mean(np.abs(seg) > sd) / 0.3173 if sd > 0 else 0)
    return np.array(out)

def report(path):
    fs, x = load(path); m = x.mean(1)
    print(f"== {path}  fs={fs}  len={len(m)/fs:.2f}s  peak={np.abs(x).max():.3f}")
    print("RT60 by band (s): " + "  ".join(f"{b}Hz:{rt60(band(m, fs, b), fs):.2f}" for b in BANDS))
    c = np.corrcoef(x[:, 0], x[:, 1])[0, 1]
    print(f"L/R correlation: {c:.3f}")
    ed = echo_density(m, fs)
    print("echo density (norm.) @ 20,100,200,400 ms: " + ", ".join(f"{ed[min(i, len(ed)-1)]:.2f}" for i in (1, 5, 10, 20)))
    print(f"energy at end vs start: {10*np.log10((m[-fs//10:]**2).sum()/ (m**2).sum()+1e-30):.1f} dB;  finite={np.isfinite(x).all()}")

if __name__ == "__main__":
    for p in sys.argv[1:]: report(p)
