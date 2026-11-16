# Midwest - guitar processor for midwest emo (VST3 / AU, Apple Silicon)

Signal chain: Gate -> Compressor -> Drive -> Tremolo -> Chorus -> Delay -> Reverb -> Tape.
12 presets, resizable window (drag the corner).

Build: push this folder to GitHub, the workflow in .github/workflows builds Midwest.vst3 and Midwest.component.
Install: copy Midwest.vst3 to /Library/Audio/Plug-Ins/VST3 and run  xattr -cr  on it, rescan plugins in FL Studio.
