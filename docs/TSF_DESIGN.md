# Windows Text Services Framework design

## Target

Windows 11 x64, native C++ COM in-process text service. TSF remains a thin adapter around
the platform-neutral \`input_engine\` library.

## Implemented prototype

- \`ITfTextInputProcessor\` activation/deactivation and \`ITfKeyEventSink\`.
- Roman A–Z key capture and live update of an active TSF composition.
- Backspace updates the pre-edit word; Escape cancels the composition.
- A simple non-activating Win32 popup displays alternate dictionary candidates.
- Number keys 1–9 and Up/Down move the candidate selection.
- Space, Enter, period, comma, semicolon, or slash commits the selected candidate.
- All document mutations occur through synchronous read/write TSF edit sessions.
- UTF-8 engine output is converted to UTF-16 with strict invalid-sequence checking.

## Known limitations

- The candidate popup is a lightweight Win32 window, not a full
  \`ITfCandidateListUIElement\`; UI-less application mode is not supported.
- Candidate coverage is a small starter dictionary, not a statistical language model.
- The first key handler assumes a conventional Latin A–Z virtual-key layout.
- Display-attribute underlining, reconversion, preserved keys, touch keyboard integration,
  and context-specific composition recovery are not yet implemented.
- Focus-loss recovery and composition teardown need further real-application testing.
- The TSF registration tool and installer build successfully only after validation on a
  supported Windows SDK; actual activation must be checked on Windows 11.

## Registration

\`TamilImeRegistration.exe\` writes the COM in-process server registration under the current
user's \`HKCU\\Software\\Classes\\CLSID\` and uses
\`ITfInputProcessorProfiles::Register\`, \`AddLanguageProfile\`, and
\`EnableLanguageProfile\` for Tamil (India), LANGID 0x0449. The uninstall operation removes
the language profile and per-user COM registration.

## Microsoft references

- [TSF compositions](https://learn.microsoft.com/windows/win32/tsf/compositions)
- [ITfContextComposition::StartComposition](https://learn.microsoft.com/windows/win32/api/msctf/nf-msctf-itfcontextcomposition-startcomposition)
- [ITfInsertAtSelection::InsertTextAtSelection](https://learn.microsoft.com/windows/win32/api/msctf/nf-msctf-itfinsertatselection-inserttextatselection)
- [Text service registration](https://learn.microsoft.com/windows/win32/tsf/text-service-registration)
