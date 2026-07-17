# Storage File Initialization Lifecycle Tasks

- [x] Specify the identity-bound open and preamble lifecycle protocol.
- [x] Add version-2 lifecycle fields while retaining version-1 reads.
- [x] Move exclusive creation and validation into the offset VFS handle.
- [x] Publish committed and recovery-required states through the exact main
      VFS file object.
- [x] Remove pathname-based failed-open deletion.
- [x] Add lifecycle, truncation, concurrent-open, and pathname-replacement
      tests.
- [x] Run focused Release and sanitizer coverage.
- [x] Update the remediation ledger and review the complete change.
