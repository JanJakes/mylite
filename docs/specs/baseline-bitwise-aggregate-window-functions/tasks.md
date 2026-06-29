# Baseline Bitwise Aggregate Window Functions Tasks

- [x] Research MySQL 8.4 aggregate-window behavior and official documentation.
- [x] Capture MySQL 8.4.9 runtime expectations for bitwise aggregate windows,
  neutral frame behavior, metadata, and syntax boundaries.
- [x] Add SQLite window callbacks for MyLite bitwise aggregates.
- [x] Add runtime planning, lowering, and metadata for `BIT_AND()`, `BIT_OR()`,
  and `BIT_XOR()` aggregate windows only when `OVER` is present.
- [x] Preserve non-window bitwise aggregate behavior.
- [x] Add focused C runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused and full verification, then review.
- [x] Commit and push.
