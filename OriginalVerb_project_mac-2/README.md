# OriginalVerb — reverb original (Plate / Room / Hall)

Motor de reverb escrito do zero a partir de literatura pública de DSP
(Dattorro 1997 para a plate; FDN com matriz de Householder e amortecimento tipo Jot para room/hall).

## Estrutura
- core/reverb_core.h  — motor de DSP (header-only, sem dependências)
- tools/render.cpp    — renderizador offline (gera IRs, processa WAVs)
- tools/analyze.py    — RT60 por banda, densidade de ecos, correlação L/R
- tools/fit.py        — ajusta os parâmetros do motor para casar com um IR-alvo
- plugin/             — projeto JUCE/CMake (VST3 + Standalone)

## Compilar o plugin (Windows, Visual Studio 2022 + CMake)
    git clone --depth 1 https://github.com/juce-framework/JUCE.git plugin/JUCE
    cmake -S plugin -B plugin/build -G "Visual Studio 17 2022" -A x64
    cmake --build plugin/build --config Release --target OriginalVerb_VST3
(macOS/Linux: mesmo CMake; Linux precisa dos headers X11/ALSA/freetype.)

## Calibrar contra o plugin de referência (medição caixa-preta)
1. Na sua DAW, passe um impulso (1 amostra a 1.0) pelo plugin que você possui, 100% wet,
   e exporte cada ajuste (ex.: plate_decay2s.wav). Mesmo sample rate do motor (padrão 48 kHz).
2. g++ -O2 -std=c++17 -o tools/render tools/render.cpp
3. python3 tools/fit.py plate_decay2s.wav mode=plate      # demora; roda ~1 render por candidato
4. Use os valores impressos como preset desse modo.

## macOS (VST3 + AU + Standalone, Apple Silicon e Intel)
**No Mac:** `bash mac/build_mac.sh` → gera `OriginalVerb-mac.zip`; extraia e dê duplo clique em `Install.command`.
**Sem Mac:** suba esta pasta num repositório GitHub; a aba Actions → "build-mac" → Run workflow gera o zip pronto.
Os binários saem com assinatura ad-hoc (uso pessoal). Para distribuir a terceiros é preciso conta Apple Developer (assinar + notarizar).
Dica: se o macOS bloquear, `xattr -cr` já é feito pelo Install.command.

### Logic Pro
O Logic usa o formato AU (OriginalVerb.component, tipo Effect, código aufx/Ovrb/Orig). Aparece em Audio FX > Audio Units > OriginalDSP.
