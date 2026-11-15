# EmoBoard — midwest emo pedalboard (VST3 / AU, Apple Silicon)

Цепочка: Comp -> Drive -> Chorus -> Delay -> Reverb. У каждой педали есть вкл/выкл.

## Сборка на Mac M2

1. Поставь Xcode (из App Store), открой его один раз и прими лицензию.
2. Поставь инструменты: `xcode-select --install` и `brew install cmake`.
3. В папке проекта:

```
cmake -B build -G Xcode
cmake --build build --config Release
```

JUCE скачается сам (нужен интернет). После сборки плагин копируется в:
- `~/Library/Audio/Plug-Ins/VST3/EmoBoard.vst3`
- `~/Library/Audio/Plug-Ins/Components/EmoBoard.component`

## FL Studio

Options -> Manage plugins -> Find installed plugins (или Start scan).
Дальше: Add -> EmoBoard. Ставь плагин на гитарный канал Mixer.

Если FL не видит плагин, проверь, что в Manage plugins добавлена папка `~/Library/Audio/Plug-Ins/VST3`.
Если macOS блокирует загрузку: `xattr -cr ~/Library/Audio/Plug-Ins/VST3/EmoBoard.vst3`

## Быстрый твик под звук

- Comp ~0.4, Drive gain ~0.2 (лёгкий кранч), Tone ~0.5
- Chorus rate ~0.5, depth ~0.5 — блестящий клин
- Delay 350-450 мс, feedback 0.3-0.4
- Reverb size ~0.6, mix ~0.3
