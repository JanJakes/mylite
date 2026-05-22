# Baseline Replace Select Row-Scalar Source Tasks

- [x] Read current architecture, compatibility docs, `REPLACE ... SELECT`,
      keyed `REPLACE`, row-scalar `INSERT ... SELECT`, parser, runtime, and
      test context.
- [x] Verify official MySQL 8.4 documentation for `REPLACE`, `SELECT`, and
      `ROW_COUNT()`.
- [x] Probe MySQL 8.4.9 runtime behavior for no-source, `FROM DUAL`,
      `WHERE EXISTS`, zero-row, required-column, keyed replacement,
      auto-increment, and diagnostics.
- [x] Write the independent feature spec with ownership boundaries, grammar
      snippet, physical SQLite handling, diagnostics, and tests.
- [x] Add MySQL runtime expectation script for row-scalar `REPLACE ... SELECT`.
- [x] Update compatibility documentation for the expanded limited subset.
- [x] Remove the row-scalar source rejection for `REPLACE ... SELECT` while
      keeping unsupported `UNION` sources deferred.
- [x] Add focused runtime tests for row-scalar replacement, affected rows,
      warnings, diagnostics, auto-increment, keys, and continued unsupported
      forms.
- [x] Run focused build/tests and the MySQL expectation script.
- [x] Run `cmake --workflow --preset check`.
- [x] Review the final diff for MySQL behavior, architecture, performance,
      descriptor authority, file-format safety, cleanup, scope control, and
      docs.
- [x] Commit and push the completed slice.
