# Google Input Tools – Modern Windows

An independent modernization project for Windows 11, starting with Tamil transliteration.

> This is a community-led, independent project. It is not an official Google product and is
> not endorsed by Google. The archived upstream repository remains unchanged.

## Current status

- Modern CMake + C++20 foundation.
- Platform-neutral transliteration engine API.
- Tamil starter word overrides and a conservative roman-token fallback.
- Command-line demo and automated CTest smoke tests.
- Windows CI workflow using Visual Studio 2022.
- Initial Windows TSF COM activation and key-event scaffold with commit-on-delimiter typing.

## Build on Windows 11

Install Visual Studio 2022 with **Desktop development with C++** and CMake.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\tamil-ime-demo.exe vanakkam
```

The expected demo output for `vanakkam` is `வணக்கம்`.

## Current Tamil examples

| Roman input | Tamil output |
|---|---|
| `vanakkam` | வணக்கம் |
| `amma` | அம்மா |
| `tamizh` | தமிழ் |
| `nandri` | நன்றி |
| `ulagam` | உலகம் |

The TSF layer is currently a commit-on-delimiter prototype: it buffers roman keys and inserts
Tamil text when a delimiter is pressed. It has no live pre-edit view or candidate window yet.
This is only a starter dictionary, not a complete transliteration standard. Tamil spelling,
vowel-sign composition, ambiguity handling, candidate ranking, and native-speaker review
remain active development work.

## Project plan

See [docs/UPGRADE_PLAN.md](docs/UPGRADE_PLAN.md) and
[docs/TSF_DESIGN.md](docs/TSF_DESIGN.md).

## License and attribution

The repository retains its existing Apache License 2.0 in [LICENSE](LICENSE). The original
Google Input Tools project is documented in [NOTICE](NOTICE). No upstream source files or
language data have been copied into this initial modernization scaffold; future reuse will
be reviewed file-by-file and preserve applicable notices.
