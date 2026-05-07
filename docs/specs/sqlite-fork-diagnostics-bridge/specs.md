# SQLite Fork Diagnostics Bridge

## Status

This slice adds the first structured diagnostics bridge from MyLite-owned
SQLite-fork bytecode and configured scalar callbacks to the public MyLite
diagnostics area. It is intentionally small: fork code can publish one MySQL
condition for the most recent fork-owned error or warning, and MyLite consumes
that condition when mapping covered SQLite errors back to its own warning/error
list.

Implemented scope:

- connection-local fork condition storage on the private SQLite handle
- public fork APIs to read, clear, and publish the most recent fork condition
- connection-local condition-list storage with public count and indexed read
  APIs for statements that publish more than one warning
- `OP_MyliteTypeCheck` publishes MySQL condition codes and SQLSTATE values for
  the first strict assignment failures
- SQLite-native `NOT NULL`, `UNIQUE`, `PRIMARY KEY`, `CHECK`, and immediate
  foreign-key constraint failures publish MySQL condition codes and SQLSTATE
  values through the same bridge
- configured scalar callbacks can publish successful-statement warning
  conditions; implemented first for `IF()` numeric-condition truncation warning
  `1292`
- MyLite's SQLite-error mapping consumes the fork condition and appends a MySQL
  error condition instead of falling back to generic error 1105
- direct fork and public MyLite tests cover out-of-range integer, over-length
  `VARCHAR`, invalid `DOUBLE`, over-length binary string, invalid decimal,
  out-of-range decimal, invalid temporal, datetime overflow, invalid `YEAR`,
  out-of-range `YEAR` assignment conditions, native `NOT NULL`, `UNIQUE`,
  `PRIMARY KEY`, `CHECK`, and foreign-key constraint conditions, and direct
  scalar warning readback after successful `IF()` calls

Deferred scope:

- full diagnostics-area shape, including row numbers, column names in MySQL text
  form, condition item fields, and stacked diagnostics
- warning demotion for `IGNORE` and non-strict SQL modes
- exact message text for fork-collected warnings and errors
- SQLSTATE exposure through the public MyLite API and wire protocol
- exact MySQL message rendering for native SQLite constraints
- structured conditions for deferred foreign-key, generated-column, parser,
  trigger, and future fork-opcode errors

## Sources

- MySQL 8.4 Reference Manual, Server Error Message Reference:
  `https://dev.mysql.com/doc/mysql-errors/8.4/en/server-error-reference.html`
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  `https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html`
- MySQL 8.4 Reference Manual, Diagnostics Area:
  `https://dev.mysql.com/doc/refman/8.4/en/diagnostics-area.html`
- SQLite result codes:
  `https://www.sqlite.org/rescode.html`
- SQLite VDBE execution model from the vendored SQLite source tree

This specification is independently authored from official MySQL and SQLite
documentation, observed MySQL 8.4.9 runtime behavior, and the current MyLite
codebase.

## MySQL 8.4.9 Behavior Baseline

Runtime probes for this slice were executed on 2026-05-06 against the official
`mysql:8.4.9` Docker image in container `mylite-mysql-849`, using MySQL's
default strict SQL mode.

```text
INSERT INTO t(i TINYINT NOT NULL) VALUES (128)
ERROR 1264 (22003): Out of range value for column 'i' at row 1
SHOW WARNINGS: Error 1264

INSERT INTO t(s VARCHAR(4) NOT NULL) VALUES ('abcde')
ERROR 1406 (22001): Data too long for column 's' at row 1
SHOW WARNINGS: Error 1406

INSERT INTO t(d DOUBLE NOT NULL) VALUES ('bad')
ERROR 1265 (01000): Data truncated for column 'd' at row 1
SHOW WARNINGS: Error 1265

SELECT IF('abc','yes','no')
returns 'no'
SHOW WARNINGS: Warning 1292, truncated incorrect DOUBLE value
```

## Design

SQLite's public error surface is text plus SQLite result code. That is not
enough for MySQL compatibility because MySQL behavior depends on the structured
condition code, SQLSTATE, severity, SQL mode, and `IGNORE` handling. Public
SQLite hooks also run too late or not at all for VDBE opcode failures.

The fork therefore owns a small connection-local diagnostics area:

- `mylite_sqlite_fork_last_condition()` copies the most recent fork condition.
- `mylite_sqlite_fork_condition_count()` returns the number of collected
  fork conditions.
- `mylite_sqlite_fork_condition_at()` copies a collected fork condition by
  zero-based index.
- `mylite_sqlite_fork_clear_condition()` clears the last-condition slot and
  collected condition list after the caller consumes them.
- `mylite_sqlite_fork_set_condition()` lets configured fork callbacks publish
  a warning or error condition when no VDBE opcode is raising a SQLite error.
- internal fork code calls `sqlite3MyliteSetCondition()` immediately before
  raising the SQLite error.

The most-recent condition slot is preserved for simple SQLite-error mapping and
compatibility with existing callers. The condition list is the foundation for
MySQL diagnostics-area behavior and for later `IGNORE` and non-strict SQL mode
work, where row-level write failures must be demoted, recorded, and execution
must continue. The first collector stores level, MySQL errno, and SQLSTATE; exact
condition messages and row/column metadata remain later work.

## Initial Semantics

`OP_MyliteTypeCheck` now publishes:

| Failure | MySQL code | SQLSTATE |
| --- | ---: | --- |
| signed or supported unsigned integer out of range | 1264 | `22003` |
| `VARCHAR(n)` value too long | 1406 | `22001` |
| invalid `DOUBLE` assignment text | 1265 | `01000` |
| over-length `BINARY(n)`, `VARBINARY(n)`, text family, or blob family | 1406 | `22001` |
| invalid `DECIMAL(p,s)` assignment text | 1366 | `HY000` |
| out-of-range `DECIMAL(p,s)` assignment | 1264 | `22003` |
| invalid `DATE`, `DATETIME(fsp)`, or `TIME(fsp)` assignment | 1292 | `22007` |
| post-round `DATETIME(fsp)` overflow | 1441 | `22008` |
| invalid `YEAR` assignment text | 1366 | `HY000` |
| out-of-range `YEAR` assignment | 1264 | `22003` |
| native `NOT NULL` constraint failure | 1048 | `23000` |
| native `UNIQUE` or `PRIMARY KEY` constraint failure | 1062 | `23000` |
| native `CHECK` constraint failure | 3819 | `HY000` |
| native child-side foreign-key insert/update failure | 1452 | `23000` |
| native parent-side foreign-key delete/update failure | 1451 | `23000` |
| `IF()` condition text truncated during numeric conversion | 1292 | `22007` |
| invalid internal MyLite descriptor | 1105 | `HY000` |

MyLite still uses SQLite's error message as the public text in this slice. The
structured code is now correct for the covered failures, while exact MySQL text,
duplicate-key interpolation, and row-number interpolation remain deferred.

## Tests

The executable tests must cover:

- direct fork condition readback after a descriptor-owned VDBE assignment error
- clearing the fork condition after it is consumed
- public MyLite diagnostics receiving covered MySQL error codes from fork
  type-check failures
- direct fork condition readback after native `NOT NULL`, `UNIQUE`,
  `PRIMARY KEY`, `CHECK`, and immediate foreign-key constraint failures
- direct fork warning readback after successful scalar callback evaluation
- direct fork condition-count and indexed-read coverage for multiple warnings
  produced by one SQLite statement
- existing type-coercion success and failure behavior continuing to pass

## Compatibility Status

This feature is `🟡` because the first structured bridge exists for fork-owned
type-check failures, common native constraint failures, and scalar callback
warnings, and the fork can now collect multiple condition records. The full
MySQL diagnostics area, warning demotion, exact message rendering, and
wire-protocol condition metadata remain incomplete.
