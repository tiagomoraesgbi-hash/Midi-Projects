#!/bin/bash
# Compila OriginalVerb (VST3 + AU + Standalone, universal arm64/x86_64) e gera OriginalVerb-mac.zip
# Requisitos: Xcode Command Line Tools (xcode-select --install) e CMake (brew install cmake)
set -e
cd "$(dirname "$0")/.."
cmake -S plugin -B build -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target OriginalVerb_VST3 OriginalVerb_AU OriginalVerb_Standalone -j 8
OUT=dist; rm -rf $OUT; mkdir -p $OUT
A=build/OriginalVerb_artefacts/Release
cp -R $A/VST3/OriginalVerb.vst3 $OUT/
cp -R $A/AU/OriginalVerb.component $OUT/
cp -R $A/Standalone/OriginalVerb.app $OUT/ 2>/dev/null || true
cp mac/Install.command $OUT/; chmod +x $OUT/Install.command
for f in $OUT/*.vst3 $OUT/*.component $OUT/*.app; do [ -e "$f" ] && codesign --force --deep -s - "$f"; done
(cd $OUT && zip -qry ../OriginalVerb-mac.zip .)
echo "Pronto: OriginalVerb-mac.zip"
