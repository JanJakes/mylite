# Baseline Core Aggregate Window Functions Tasks

- [x] Research MySQL 8.4 aggregate-window behavior and official documentation.
- [x] Capture MySQL 8.4.9 runtime expectations for core aggregate windows,
  named windows, frames, literal/count behavior, and diagnostics.
- [x] Add runtime planning for `COUNT()`, `SUM()`, `AVG()`, `MIN()`, and
  `MAX()` aggregate windows only when `OVER` is present.
- [x] Lower supported core aggregate windows through SQLite built-in window
  functions and a MyLite-owned `AVG()` result formatter.
- [x] Preserve deterministic unsupported diagnostics for non-core aggregate
  window families.
- [x] Add focused C runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused and full verification, then review.
- [x] Commit and push.
