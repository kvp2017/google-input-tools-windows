# Upgrade plan: Google Input Tools – Modern Windows

## Product goal

Build an independent, maintainable Windows 11 input-method platform, beginning with
Tamil transliteration and adding Hindi, Telugu, and other Indian languages as separate
language packs when the shared engine is stable.

## Scope

- Primary target: Windows 11 x64.
- Native Windows Text Services Framework (TSF) integration.
- Modular transliteration, candidate ranking, dictionaries, and user vocabulary.
- Tamil first; Hindi and Telugu next; then additional Indian languages.
- Signed installer, safe upgrade/uninstall, accessibility and compatibility testing.

## Phases and acceptance criteria

### Phase 0 — Repository governance
- Maintain this as a separate repository; do not modify the archived upstream.
- Preserve Apache-2.0 and attribution notices.
- Record provenance/licenses for imported code and language data.
- Add CI for Windows x64 builds and tests.

**Exit:** repository policy, source provenance, and passing Windows CI.

### Phase 1 — Modern build foundation
- Replace legacy GYP/Python 2/MSVC-era build assumptions with CMake and C++20.
- Keep the transliteration core platform-neutral.
- Add deterministic tests and Unicode UTF-8 handling.
- Avoid old protobuf/gtest/zlib/WTL dependencies unless a specific need is demonstrated.

**Exit:** reproducible Visual Studio 2022 x64 configure/build/test.

### Phase 2 — Tamil transliteration engine
- Define a stable composing API for roman input to Tamil Unicode.
- Add longest-match tokenization, word overrides, candidate lists, and backspace/edit support.
- Create reviewed test data covering vowels, consonants, pulli, uyirmei combinations,
  grantha letters, common words, punctuation, and mixed-script input.
- Obtain native-speaker review before declaring the mapping production-ready.

**Exit:** test corpus passes, Unicode output is valid, native-speaker acceptance is recorded.

### Phase 3 — Windows TSF
- Complete the COM server lifecycle and TSF registration/profile setup.
- Implement key event handling, composition updates, commit/cancel, backspace, and focus changes.
- Add candidate UI and keyboard shortcuts.
- Test in Notepad, Microsoft Office, browsers, and other supported Windows text controls.

**Exit:** signed Windows 11 test build passes the TSF application compatibility matrix.

### Phase 4 — Settings and language packs
- Provide settings for input mode, key behavior, dictionary, and shortcuts.
- Stabilize Tamil.
- Add Hindi and Telugu with language-specific transliteration tests.
- Plan Kannada, Malayalam, Bengali, Gujarati, Marathi, Punjabi, Odia, Assamese, and Urdu.

### Phase 5 — Packaging and release
- Produce reproducible x64 artifacts.
- Create a signed MSI/MSIX package, update strategy, rollback, and complete uninstaller.
- Publish release notes, software bill of materials, and security reporting guidance.

## Initial architecture

```text
Windows TSF service
       |
Composition / Candidate UI
       |
Input Engine API
       |
Tokenizer + Candidate Ranker
       |
Language Pack Interface
       +-- Tamil (ta) [first]
       +-- Hindi (hi) [planned]
       +-- Telugu (te) [planned]
       +-- Other Indian languages [planned]
```

## Current implementation boundary

The repository currently has a platform-neutral Tamil prototype, CMake build, smoke tests,
Windows CI configuration, and a TSF COM activation scaffold. The TSF scaffold does not yet
consume keystrokes or insert text. Installer, candidate UI, settings app, and production
language data are not implemented.
