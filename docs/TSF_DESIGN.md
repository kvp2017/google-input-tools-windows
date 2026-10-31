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

## Current scaffold

`src/windows-tsf/tsf_provider.cpp` implements the initial COM activation/deactivation interface
and class factory exports. It is deliberately only a lifecycle scaffold: it does not yet
register a TSF language profile, advise an `ITfKeyEventSink`, or modify application text.

## Required next steps

- Add COM registration and TSF profile registration/unregistration.
- Implement `ITfKeyEventSink` and per-context composition state.
- Use `ITfContext::RequestEditSession` for all document mutations.
- Commit through an edit session; do not directly change document ranges from a key callback.
- Convert UTF-8 engine output to UTF-16 using Windows conversion APIs.
- Handle read-only contexts, denied edit sessions, focus loss, dead keys, IME coexistence,
  and shutdown cleanup.
- Add automated tests where possible and manual application tests on Windows 11.
