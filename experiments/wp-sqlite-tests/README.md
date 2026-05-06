# WordPress SQLite Test Experiment

This experiment runs the upstream `WP_SQLite_*.php` tests from
`WordPress/sqlite-database-integration` against the MyLite-backed `mysqli`
extension.

The upstream tests target the SQLite integration driver's PHP API, not mysqli
directly, so the runner provides a small `WP_SQLite_Driver`-shaped shim backed by
MyLite. This keeps the original test SQL and assertions intact while making the
database calls go through the PHP extension.

Run from the repository root:

```sh
experiments/wp-sqlite-tests/run.sh
```

The script builds the PHP extension in Docker when needed, clones the upstream
test suite into `build/wp-sqlite-integration-src`, and writes a JSON result file
to `build/wp-sqlite-mylite-results.json`.
