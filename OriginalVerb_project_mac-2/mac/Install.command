#!/bin/bash
# Duplo clique para instalar. Copia os plugins para as pastas do usuário e remove a quarentena.
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/OriginalVerb.vst3 ~/Library/Audio/Plug-Ins/Components/OriginalVerb.component
cp -R OriginalVerb.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R OriginalVerb.component ~/Library/Audio/Plug-Ins/Components/
xattr -cr ~/Library/Audio/Plug-Ins/VST3/OriginalVerb.vst3 ~/Library/Audio/Plug-Ins/Components/OriginalVerb.component
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Instalado. Reabra sua DAW e faça rescan de plugins."
C=~/Library/Audio/Plug-Ins/Components/OriginalVerb.component
echo "--- Info.plist do componente (códigos do AU):"
plutil -p "$C/Contents/Info.plist" | grep -A10 AudioComponents
echo "--- Arquiteturas:"; lipo -archs "$C/Contents/MacOS/OriginalVerb"
echo "Aguardando o macOS registrar o AU..."
killall -9 AudioComponentRegistrar coreaudiod 2>/dev/null; sleep 6
echo "--- AUs encontrados com este fabricante:"
auval -a 2>/dev/null | grep -i "Orig" 
LINE=$(auval -a 2>/dev/null | grep -i "Orig" | head -1)
if [ -z "$LINE" ]; then echo "NENHUM AU registrado. Mande a saída acima."; else
  T=$(echo $LINE | awk '{print $1,$2,$3}'); echo "Validando: $T"; auval -v $T 2>&1 | tail -n 8; fi
