# Parser Corpus CREATE TABLE Attribute Order Tasks

- [x] Research MySQL 8.4 `CREATE TABLE` column definitions and defaults.
- [x] Verify representative MySQL 8.4.9 runtime behavior for legacy column
      attribute order.
- [x] Relax MyLite post-parse column-attribute order validation without
      weakening duplicate or charset/collation checks.
- [x] Add parser and runtime tests for admitted order variants.
- [x] Add MySQL expectation script.
- [x] Update compatibility docs.
- [x] Run focused build/tests and parser corpus benchmark.
- [x] Run `git diff --check`, static/style checks, and release-gate review.
