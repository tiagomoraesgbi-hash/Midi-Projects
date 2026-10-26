#!/usr/bin/env python3
"""Fit engine parameters so its impulse response matches a target IR you recorded
from a plugin you are licensed to use (black-box measurement only).

usage: fit.py target.wav mode=plate|room|hall [fs=48000]
Prints the best parameter set found (render with: tools/render ir out.wav k=v ...)."""
import sys, subprocess, tempfile, os, numpy as np
from scipy.optimize import differential_evolution
sys.path.insert(0, os.path.dirname(__file__))
from analyze import load, rt60, band, echo_density, BANDS
from scipy.signal import welch

HERE = os.path.dirname(os.path.abspath(__file__))
target_path = sys.argv[1]
kv = dict(a.split('=') for a in sys.argv[2:])
mode = kv.get('mode', 'hall'); fs_t, tgt = load(target_path); tm = tgt.mean(1)

def feats(m, fs):
    rt = np.array([rt60(band(m, fs, b), fs) for b in BANDS])
    ed = echo_density(m, fs)[[1, 3, 5, 10, 20]]
    f, P = welch(m[:int(2 * fs)], fs, nperseg=4096)
    idx = np.unique(np.geomspace(5, len(f) - 1, 40).astype(int))
    spec = 10 * np.log10(P[idx] + 1e-20); spec -= spec.mean()
    env = 10 * np.log10(np.convolve(m ** 2, np.ones(fs // 20) / (fs // 20), 'valid')[::fs // 20][:40] + 1e-20)
    env -= env[0]
    return rt, ed, spec, env

ft = feats(tm, fs_t)
names = ['decay', 'size', 'diffusion', 'damp', 'bass', 'moddepth', 'predelay']
bounds = [(0.3, 12), (0.0, 1.0), (0.2, 1.0), (1500, 18000), (0.5, 1.6), (0.0, 1.0), (0, 80)]

def cost(x):
    args = [f"{n}={v:.4f}" for n, v in zip(names, x)]
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as t: path = t.name
    subprocess.run([os.path.join(HERE, 'render'), 'ir', path, f'mode={mode}', f'fs={fs_t}',
                    f'len={len(tm)/fs_t:.2f}'] + args, check=True)
    _, y = load(path); os.unlink(path)
    fe = feats(y.mean(1), fs_t)
    c = np.nansum(np.abs(np.log(fe[0] / ft[0]))) * 4
    c += np.abs(fe[1] - ft[1]).sum() * 1.0
    c += np.sqrt(np.mean((fe[2] - ft[2]) ** 2)) * 0.15
    n = min(len(fe[3]), len(ft[3])); c += np.sqrt(np.mean((fe[3][:n] - ft[3][:n]) ** 2)) * 0.05
    return c if np.isfinite(c) else 1e6

r = differential_evolution(cost, bounds, maxiter=25, popsize=10, seed=1, disp=True, polish=False)
print("\nBEST (cost %.3f):" % r.fun)
print(' '.join(f"{n}={v:.3f}" for n, v in zip(names, r.x)), f"mode={mode}")
