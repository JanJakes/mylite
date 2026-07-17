# File-Backed MyLite Opening VFS Tasks

- [x] Add `mylite_open()` with private SQLite ownership and shared bootstrap.
- [x] Shift main-database VFS I/O by the fixed 4096-byte preamble.
- [x] Delegate auxiliary files and non-offset VFS methods.
- [x] Translate size hints and disable WAL/mmap paths not yet qualified.
- [x] Create new files exclusively through the wrapped platform VFS.
- [x] Validate preamble, physical size, and SQLite header on the exact opened
      file object.
- [x] Publish committed/recovery-required lifecycle state on the owning file.
- [x] Remove pathname-based failed-open deletion.
- [x] Cover creation, reopen, legacy files, malformed and incomplete files,
      concurrent initialization, and pathname replacement.
- [x] Run focused Release and sanitizer tests.
