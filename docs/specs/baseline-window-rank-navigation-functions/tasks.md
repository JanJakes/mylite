# Baseline Window Rank And Navigation Functions Tasks

- [x] Record MySQL 8.4.9 expectations for rank, distribution, bucket, navigation, and frame-value functions.
- [x] Extend parser and AST support for the ten remaining baseline window functions.
- [x] Generalize the existing `ROW_NUMBER()` row-scalar window planning path.
- [x] Lower supported functions to SQLite native window SQL through existing descriptor validation.
- [x] Add result metadata for rank/distribution and value-returning window functions.
- [x] Add C runtime tests and MySQL expectation script.
- [x] Update compatibility documentation.
- [x] Run focused verification and `cmake --workflow --preset check`.
- [x] Complete release-gate review, commit, and push.
