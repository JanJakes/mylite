# SQLite Fork Diagnostics Bridge

## Status

This slice adds the first structured diagnostics bridge from MyLite-owned
SQLite-fork bytecode to the public MyLite diagnostics area. It is intentionally
small: fork opcodes can publish a MySQL condition for the most recent
fork-owned VDBE failure, and MyLite consumes that condition when mapping the
SQLite error back to its own warning/error list.

Implemented scope:

- connection-local fork condition storage on the private SQLite handle
- public fork APIs to read and clear the most recent fork condition
- `OP_MyliteTypeCheck` publishes MySQL condition codes and SQLSTATE values for
  the first strict assignment failures
- SQLite-native `NOT NULL`, `UNIQUE`, and `PRIMARY KEY` constraint halts
  publish MySQL condition codes and SQLSTATE values through the same bridge
- MyLite's SQLite-error mapping consumes the fork condition and appends a MySQL
  error condition instead of falling back to generic error 1105
- direct fork and public MyLite tests cover out-of-range integer, over-length
  `VARCHAR`, invalid `DOUBLE`, over-length binary string, invalid decimal,
  out-of-range decimal, invalid temporal, datetime overflow, invalid `YEAR`,
  out-of-range `YEAR` assignment conditions, and native `NOT NULL`, `UNIQUE`,
  and `PRIMARY KEY` constraint conditions

Deferred scope:

- full diagnostics-area shape, including row numbers, column names in MySQL text
  form, condition item fields, and stacked diagnostics
- warning demotion for `IGNORE` and non-strict SQL modes
- multiple warning records from one SQLite statement
- SQLSTATE exposure through the public MyLite API and wire protocol
- exact MySQL message rendering for native SQLite constraints
- structured conditions for `CHECK`, foreign-key, generated-column, parser,
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
```

## Design

SQLite's public error surface is text plus SQLite result code. That is not
enough for MySQL compatibility because MySQL behavior depends on the structured
condition code, SQLSTATE, severity, SQL mode, and `IGNORE` handling. Public
SQLite hooks also run too late or not at all for VDBE opcode failures.

The fork therefore owns a small connection-local condition slot:

- `mylite_sqlite_fork_last_condition()` copies the most recent fork condition.
- `mylite_sqlite_fork_clear_condition()` clears it after the caller consumes it.
- internal fork code calls `sqlite3MyliteSetCondition()` immediately before
  raising the SQLite error.

For this first slice, the condition slot contains one condition. This matches
the current type-check path, where the VDBE aborts on the first strict
assignment failure. Later `IGNORE` and non-strict SQL mode work should replace
the single slot with a statement-owned condition collector that can append
multiple warnings without aborting execution.

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
- direct fork condition readback after native `NOT NULL`, `UNIQUE`, and
  `PRIMARY KEY` constraint failures
- existing type-coercion success and failure behavior continuing to pass

## Compatibility Status

This feature is `🟡` because the first structured bridge exists for fork-owned
type-check failures and common native constraint failures, but the full MySQL
diagnostics area, warning demotion, exact message rendering, and wire-protocol
condition metadata remain incomplete.
