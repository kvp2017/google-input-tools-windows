# Upgrade plan: Google Input Tools – Modern Windows

## Goal

An independent, maintainable Windows 11 x64 input-method platform. Tamil is the first language
implementation; Hindi, Telugu, and other Indian languages follow as independent packs.

## The five requested milestones

### 1. Live Tamil composition
**Prototype implemented:** A–Z keystrokes update a TSF composition immediately. Backspace edits
the pending roman word, Escape cancels, and delimiters commit. Real Windows 11 desktop validation
and broader keyboard-layout handling remain required.

### 2. Candidate suggestions
**Prototype implemented:** the engine returns multiple candidate strings for selected ambiguous
words. A non-activating popup shows options; digits 1–9 and Up/Down change the selected candidate.
The dictionary needs broader coverage and native-speaker review; a formal TSF candidate-list
UI element is a future improvement.

### 3. Tamil TSF profile registration
**Implementation added:** a per-user registration utility writes COM registration and calls
\`ITfInputProcessorProfiles\` to register and enable the Tamil (India) language profile.
Install/uninstall and profile visibility must still be checked on a Windows 11 desktop.

### 4. Settings and language switching
**Implementation added:** a Win32 settings app stores Enabled and ShowCandidates for the current
user. Use Windows' built-in **Win + Space** input switcher to select the installed profile.
Shortcut customization and UI/accessibility polish are future enhancements.

### 5. Windows installer
**Implementation added:** an Inno Setup script and PowerShell build script create a per-user x64
installer and call the registration utility. Signing, upgrade/rollback testing and actual
machine install/uninstall validation remain release gates.

## Technical foundation

- C++20 + CMake.
- Platform-neutral UTF-8 engine API and tests.
- No Python 2, GYP, old WTL, or unneeded legacy dependency downloads.
- Windows-specific integration stays under \`src/windows-tsf/\`.

## Future languages

Hindi and Telugu are next after the shared engine contract stabilizes. Kannada, Malayalam,
Bengali, Gujarati, Marathi, Punjabi, Odia, Assamese, and Urdu are planned later.

## Release gates

- Windows x64 CI configure/build/test/package is green.
- Manual activation in Notepad and Microsoft Word on Windows 11 x64.
- Composition, cancel, backspace, punctuation, candidate selection and focus changes tested.
- Per-user install, upgrade, rollback and uninstall tested.
- Production language data reviewed by fluent native speakers.
- Installer signed with a trusted code-signing certificate before public release.

## Attribution

This is a separate community project, not an official Google product. The upstream
\`google/google-input-tools\` repository remains untouched. Any future imported code or data
requires individual license review and retained notices.
