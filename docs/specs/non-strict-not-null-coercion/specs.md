# Non-Strict NOT NULL Coercion

This feature covers the first MySQL-compatible `NOT NULL` required-value
coercion slice for supported `INSERT` and `UPDATE` statements when the session
SQL mode does not include `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES`.

The behavior was verified against MySQL 8.4.9 using a session with
`SET SESSION sql_mode=''`, plus default-mode probes for strict error paths.

## Scope

Implemented surfaces:

- `INSERT ... VALUES` over MyLite base tables;
- `INSERT ... SET` over MyLite base tables;
- single-table `UPDATE`;
- joined `UPDATE`, through the shared update-assignment validation path.

The existing `INSERT IGNORE` required-value demotion remains unchanged and uses
the same implicit-default helpers.

Deferred surfaces:

- `UPDATE IGNORE`;
- non-`DUAL` `INSERT ... SELECT` sources;
- `LOAD DATA`, `CREATE TABLE ... SELECT`, triggers, generated columns, and
  binary protocol details;
- full type conversion, range clipping, string truncation, and invalid temporal
  demotion. Those remain owned by broader conversion and temporal tasks.

## SQL Mode Detection

MyLite treats a session as strict when `@@sql_mode` contains either
`STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES`. The default MySQL 8.4 mode is
strict. Clearing the mode with `SET SESSION sql_mode=''` activates this feature.

`TRADITIONAL` canonicalization already expands into strict mode names during SQL
mode assignment, so the strict check sees the expanded tokens.

## Implicit Defaults

When MySQL coerces a required `NOT NULL` column without an explicit default,
the stored value is the type family implicit default:

| Column family | Stored implicit default |
| --- | --- |
| integer, decimal, floating, boolean, year | `0` |
| char, varchar, text, binary, blob | empty string |
| `DATE` | `0000-00-00` |
| `TIME` | `00:00:00` |
| `DATETIME`, `TIMESTAMP` | `0000-00-00 00:00:00` |

The column metadata still reports no explicit default.

## INSERT Semantics

Under strict mode:

- omitting a required non-auto `NOT NULL` column with no explicit default fails
  with error 1364;
- assigning `DEFAULT` to such a column fails with error 1364;
- assigning explicit `NULL` to a `NOT NULL` column fails with error 1048.

Under non-strict mode:

- omitted required non-auto `NOT NULL` columns are stored with implicit defaults
  and warning 1364;
- explicit `DEFAULT` for a required non-auto `NOT NULL` column is stored with
  the implicit default and warning 1364;
- multi-row `INSERT ... VALUES` explicit `NULL` for a `NOT NULL` column is
  stored with the implicit default and warning 1048;
- single-row `INSERT ... VALUES` explicit `NULL` still fails with error 1048;
- single-row `INSERT ... SET` explicit `NULL` still fails with error 1048;
- `AUTO_INCREMENT` omitted, `NULL`, and `DEFAULT` behavior remains controlled by
  the existing auto-increment and `NO_AUTO_VALUE_ON_ZERO` rules.

MySQL warning cardinality differs by form:

- omitted missing-default warnings are emitted once per required column for a
  multi-row `INSERT ... VALUES` statement;
- explicit `DEFAULT` warnings are emitted per explicit default occurrence;
- multi-row explicit `NULL` warnings are emitted once per required column for
  the statement.

## UPDATE Semantics

Under strict mode, assigning `NULL` to a `NOT NULL` column fails with error 1048,
and assigning `DEFAULT` to a required non-auto column with no explicit default
fails with error 1364.

Under non-strict mode:

- `SET col = NULL` on a `NOT NULL` column stores the implicit default and records
  warning 1048 for each matched row and assignment;
- `SET col = DEFAULT` on a required non-auto `NOT NULL` column with no explicit
  default stores the implicit default and records warning 1364 for each matched
  row and assignment;
- affected rows still count changed rows, not matched rows. If the row already
  contains the implicit defaults, MySQL reports warnings but zero changed rows.

These rules apply before unique-key and foreign-key validation of the candidate
updated row. Coercing a key value to an implicit default can therefore expose
duplicate-key or referential errors, matching MySQL's ordering.

## Runtime Coverage

The runtime suite covers:

- non-strict omitted required columns in multi-row `INSERT ... VALUES`;
- non-strict explicit `NULL` in multi-row `INSERT ... VALUES`;
- single-row explicit `NULL` remaining an error in non-strict `INSERT`;
- non-strict omitted and explicit `DEFAULT` required columns in `INSERT ... SET`;
- explicit `NULL` remaining an error in non-strict `INSERT ... SET`;
- non-strict `UPDATE ... SET col = NULL`;
- non-strict `UPDATE ... SET col = DEFAULT`;
- unchanged non-strict `UPDATE` rows still producing warnings while reporting
  zero affected rows;
- numeric, text, `DATE`, `DATETIME`, and `TIME` implicit default storage.
