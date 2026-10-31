# Google Input Tools – Modern Windows

An independent modernization project for Windows 11 x64, beginning with Tamil transliteration.

> This is a community-led independent project. It is not an official Google product and is not
> endorsed by Google. The upstream archived repository remains unchanged.

## Current implementation

- CMake + C++20 transliteration core.
- Tamil word dictionary with a candidate API.
- Live TSF composition prototype with backspace, cancel, and delimiter commit.
- Simple candidate popup with number-key and Up/Down selection.
- Per-user Tamil TSF registration utility.
- Win32 settings application.
- Inno Setup per-user installer script.
- Windows CI build, tests and installer artifact.

This is an early prototype. The dictionary is small, the candidate popup is not yet a formal
TSF candidate-list UI element, and the TSF profile still needs manual validation on a real
Windows 11 desktop. No production signing certificate or signed installer is included.

## Build on Windows 11

Install Visual Studio 2022 or newer with **Desktop development with C++** and CMake.

\`\`\`powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\\build\\Release\\tamil-ime-demo.exe vanakkam
\`\`\`

## Build the installer

Install Inno Setup 6, then run:

\`\`\`powershell
.\\installer\\build-installer.ps1
\`\`\`

The output is written to \`artifacts\\TamilTransliteration-Setup-x64.exe\`.

## Install and use

Run the installer as the current Windows user. It registers the Tamil (India) TSF profile
for that user. Select **Tamil Transliteration (Preview)** using Windows **Win + Space**.
Open **Tamil Input Settings** to enable/disable conversion and candidate suggestions.

While typing, the composition text updates as Roman letters are entered. For ambiguous words,
choose a candidate with **1–9** or **Up/Down**. Press **Space** or **Enter** to commit and
**Escape** to cancel the current composition.

## Starter examples

| Roman input | Tamil output |
|---|---|
| \`vanakkam\` | வணக்கம் |
| \`amma\` | அம்மா |
| \`tamizh\` | தமிழ் |
| \`nandri\` | நன்றி |
| \`ulagam\` | உலகம் |

Ambiguous examples such as \`kalam\`, \`malai\`, and \`palli\` expose multiple candidates.

## Documentation

- [Upgrade plan](docs/UPGRADE_PLAN.md)
- [TSF design](docs/TSF_DESIGN.md)
- [Contribution guidance](CONTRIBUTING.md)
- [Tamil language pack notes](languages/ta/README.md)

## License and attribution

The repository retains its Apache License 2.0 in [LICENSE](LICENSE). See [NOTICE](NOTICE) for
upstream project attribution. Code, language data, and assets will be reviewed individually
before any upstream material is incorporated.
