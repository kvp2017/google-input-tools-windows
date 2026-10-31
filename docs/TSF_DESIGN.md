# Windows Text Services Framework design

## First target

Windows 11 x64, native C++ COM in-process text service. The TSF layer must remain a thin
adapter around the platform-neutral `input_engine` library.

## Components

1. **COM text service** — implements `ITfTextInputProcessor`, activates/deactivates with
   the TSF thread manager, and owns per-thread service state.
2. **Key event sink** — handles roman key input, backspace, space/enter commit, and escape cancel.
3. **Composition controller** — requests TSF edit sessions and updates the active document range.
4. **Candidate UI** — later milestone for ambiguous roman input and alternate Tamil spellings.
5. **Language profile** — registers Tamil (India, LANGID 0x0449) and exposes a user-selectable
   TSF profile.
6. **Installer** — registers/unregisters the COM server and TSF profile safely; never uses the
   upstream repository's test certificate.

## Current implementation

`src/windows-tsf/tsf_provider.cpp` now implements the initial COM activation/deactivation
lifecycle, class factory exports, and an `ITfKeyEventSink`. The key sink buffers A–Z keystrokes,
supports backspace and escape in the pending roman buffer, and commits the transliterated word
when the user presses space, enter, period, comma, semicolon, or slash.

This first input path is deliberately a **commit-on-delimiter prototype**: it does not display
live pre-edit text or a candidate window while the user types. It uses
`ITfContext::RequestEditSession` with a synchronous read/write edit session to replace the
current selection with UTF-16 text. It does not yet register the service as a Tamil TSF language
profile, so it is not yet selectable through Windows Settings.

## Required next steps

- Add COM registration and TSF profile registration/unregistration.
- Replace commit-on-delimiter with a visible TSF composition and candidate list.
- Add layout-aware key handling, uppercase/shift handling, punctuation and hotkey settings.
- Handle read-only contexts, denied edit sessions, focus loss, dead keys, IME coexistence,
  selection changes and shutdown cleanup.
- Add automated Windows tests and manual application tests on Windows 11.
