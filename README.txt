GuitarDSP-Pro Phase 0
=====================

目的:
- 新規プロジェクトとして土台から再構築
- Phase 0 では音を変えない
- Raspberry Pi 4B + Linux + JUCE standalone 前提

ビルド:
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j2
  ./build/GuitarDSPPro_artefacts/Release/GuitarDSPPro

想定:
- JUCE は ~/JUCE に存在
- CMake 3.22+
- C++17

Phase 0 実装:
- AudioEngine
- SignalChain
- SafetyLimiter
- LevelMeter
- MainView
- NavigationBar
- Amp/Pedal/Cab/Effects/Settings page skeleton
- audio device selector
- 48 kHz / 256 samples request
- mono input -> stereo output
- startup fade-in
- output limiter
- bypass
- meters

Phase 0 で意図的に未実装:
- Amp DSP
- Pedal DSP
- Cab IR
- Chorus/Delay/Reverb
- Presets

最初の確認:
1. build が通る
2. 起動する
3. Settings ページにオーディオ設定が出る
4. WAVIO を選べる
5. Input 1 にギターを入れる
6. L/R から同じ音が出る
7. INPUT/OUTPUT メーターが動く
8. BYPASS が効く
9. 音量が危険に跳ねない
10. ページ切替で落ちない
