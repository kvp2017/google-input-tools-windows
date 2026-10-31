# Tamil language pack (starter)

Language code: `ta`.

The first prototype stores a small word override table in
`src/tamil_transliterator.cpp`. Before expanding it, move the data into a versioned
language-pack format. Each entry should include:

- Roman input and expected Tamil Unicode output.
- Provenance/source and license.
- Native-speaker review status.
- Alternate spellings and candidate notes.
- Regression test reference.

Do not treat the current fallback mapping as a standardized or complete Tamil IME scheme.
