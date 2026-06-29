# Baseline Statistical Aggregate Window Functions Tasks

- [x] Research MySQL 8.4 aggregate-window behavior and official documentation.
- [x] Capture MySQL 8.4.9 runtime expectations for statistical aggregate
  windows, neutral frame behavior, metadata, aliases, and syntax boundaries.
- [x] Add SQLite window callbacks for MyLite statistical aggregates.
- [x] Add runtime planning, lowering, and metadata for `STD()`, `STDDEV()`,
  `STDDEV_POP()`, `STDDEV_SAMP()`, `VAR_POP()`, `VAR_SAMP()`, and
  `VARIANCE()` aggregate windows only when `OVER` is present.
- [x] Preserve non-window statistical aggregate behavior.
- [x] Add focused C runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused and full verification, then review.
- [x] Commit and push.
