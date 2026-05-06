# WordPress SQLite Test Experiment

This experiment runs the upstream `WP_SQLite_*.php` tests from
`WordPress/sqlite-database-integration` against the MyLite-backed `mysqli`
extension.

The upstream tests target the SQLite integration driver's PHP API, not mysqli
directly, so the runner provides a small `WP_SQLite_Driver`-shaped shim backed by
MyLite. This keeps the original test SQL and assertions intact while making the
database calls go through the PHP extension.

The runner excludes `WP_SQLite_Driver_Translation_Tests` and
`WP_SQLite_Information_Schema_Reconstructor_Tests` because those suites exercise
the upstream parser/translator and schema reconstructor internals rather than
MySQL-compatible SQL execution through mysqli.

Run from the repository root:

```sh
experiments/wp-sqlite-tests/run.sh
```

The script builds the PHP extension in Docker when needed, clones the upstream
test suite into `build/wp-sqlite-integration-src`, and writes a JSON result file
to `build/wp-sqlite-mylite-results.json`.
