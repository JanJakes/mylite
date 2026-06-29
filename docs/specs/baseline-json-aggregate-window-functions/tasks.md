# Baseline JSON Aggregate Window Functions Tasks

- [x] Research MySQL 8.4 aggregate-window behavior and official documentation.
- [x] Capture MySQL 8.4.9 runtime expectations for JSON aggregate windows,
  empty frame behavior, duplicate object keys, metadata, and unsupported
  `GROUP_CONCAT()` windows.
- [x] Add SQLite window callbacks for MyLite JSON aggregates.
- [x] Add runtime planning, lowering, and metadata for `JSON_ARRAYAGG()` and
  `JSON_OBJECTAGG()` aggregate windows only when `OVER` is present.
- [x] Preserve non-window JSON aggregate behavior.
- [x] Add focused C runtime coverage.
- [x] Update compatibility documentation.
- [x] Run focused and full verification, then review.
- [ ] Commit and push.
