# Baseline Non-Strict Implicit Default Families

## Summary

This phase hardens the existing non-strict DML coercion slice across the
descriptor families that were not covered by the original fast runtime test:
`DECIMAL`, `FLOAT`, `DOUBLE`, `BINARY`, `VARBINARY`, BLOB-family, `BIT`,
`YEAR`, `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, `ENUM`, `SET`, and `JSON`.

The goal is not broader expression conversion. The goal is to make MyLite's
documented implicit-default behavior demonstrably MySQL-compatible for ordinary
`INSERT`, `REPLACE`, and matched single-table `UPDATE` paths when neither
`STRICT_TRANS_TABLES` nor `STRICT_ALL_TABLES` is active, and to fix the verified
`ENUM` warning/value exceptions.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-nonstrict-dml-coercion/specs.md`
  - `docs/specs/baseline-dml-default-keyword-values/specs.md`
  - descriptor-family specs for numeric, binary, `BIT`, temporal, `ENUM`,
    `SET`, and `JSON` columns
- Official MySQL 8.4 Reference Manual:
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - Data type default values:
    <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
  - Server SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_nonstrict_dml_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for this slice under
`SET sql_mode = ''`:

- Omitted no-explicit-default `NOT NULL` `DECIMAL`, approximate numeric,
  binary string, BLOB-family, `BIT`, `YEAR`, `DATE`, `TIME`, `DATETIME`,
  `TIMESTAMP`, `SET`, and `JSON` columns materialize the type's implicit value
  and append warning `1364`.
- Omitted no-explicit-default `ENUM NOT NULL` columns materialize the first
  enum label and do not append warning `1364`.
- Explicit DML `DEFAULT` for no-explicit-default targets follows the same value
  and warning policy as omitted columns: warning `1364` for the families above,
  but no warning for `ENUM NOT NULL`.
- Matched `UPDATE ... SET column = NULL` into `NOT NULL` targets appends warning
  `1048` once per matched row per adjusted assignment and stores the target
  implicit value.
- For `ENUM NOT NULL`, adjusted explicit `NULL` stores the enum error value
  exposed as the empty string, not the first enum label.
- `UPDATE` conversion still happens only for matched rows. No-match updates and
  `LIMIT 0` updates must not emit these conversion warnings.
- Successful adjusted statements return through the existing non-row DML result
  shape and preserve changed-row affected-count semantics.

The verified implicit values are:

| Descriptor family | Omitted / `DEFAULT` value | Adjusted explicit `NULL` value |
| --- | --- | --- |
| `DECIMAL(M,D)` | zero at descriptor scale, for example `0.00` | same |
| `FLOAT` / `DOUBLE` | `0` | same |
| `BINARY(N)` | `N` zero bytes | same |
| `VARBINARY(N)` / BLOB-family | zero-length bytes | same |
| `BIT(N)` | zero bit value | same |
| `YEAR` | `0000` | same |
| `DATE` | `0000-00-00` | same |
| `TIME` | `00:00:00` | same |
| `DATETIME` / `TIMESTAMP` | `0000-00-00 00:00:00` | same |
| `ENUM('a', ...)` | first label, no warning | empty enum error value with warning |
| `SET(...)` | empty string with warning | empty string with warning |
| `JSON` | JSON `null` with warning | JSON `null` with warning |

## Scope

This feature covers only the statement shapes already supported by
`baseline-nonstrict-dml-coercion`:

- `INSERT ... VALUES` and `INSERT ... SET`;
- `REPLACE ... VALUES` and `REPLACE ... SET`;
- matched single-table `UPDATE ... SET ... [WHERE ...] [ORDER BY ...] [LIMIT ...]`.

For those shapes, this phase adds runtime verification and fixes for:

- omitted-column allocation;
- omitted-column warning emission;
- explicit DML `DEFAULT` conversion;
- explicit `NULL` conversion in matched `UPDATE`;
- existing `INSERT IGNORE` explicit `NULL` conversion where it shares the same
  descriptor helper.

## Non-Goals

This feature does not add:

- new SQL grammar;
- `UPDATE IGNORE`;
- broader non-strict expression conversion;
- invalid `ENUM` or `SET` literal demotion beyond the verified explicit
  `NULL` adjustment;
- selected-row conversion for descriptor families not already covered by the
  current `INSERT ... SELECT` non-strict slice;
- changes to `INSERT ... ON DUPLICATE KEY UPDATE`;
- protocol info-string parity, triggers, generated columns, privileges, or
  SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` keeps result-handle ownership,
  diagnostics exposure, and failure cleanup.
- Session state: owns the handle-local SQL-mode bitset that decides strict
  versus non-strict behavior.
- Parser/AST: unchanged. Existing `NULL` and `DEFAULT` value nodes are reused.
- Analyzer/planner/runtime: own descriptor-based omitted-value materialization,
  assignment conversion, and warning emission before generated SQLite SQL is
  prepared.
- Catalog: remains authoritative for logical type, physical type, nullability,
  default kind, stable physical table names, and descriptor metadata. This
  feature does not mutate catalog rows or descriptor versions.
- SQLite physical storage: receives quoted descriptor-built SQL and bound
  values. SQLite does not decide MySQL strictness, implicit defaults, or warning
  counts.
- Storage/VFS/file format: unchanged. Adjusted rows are normal SQLite payload
  writes and must not touch the `.mylite` preamble.

## Grammar

No new grammar is required. The relevant independently authored MyLite
Lemon-syntax sketch remains:

```lemon
insert_value ::= NULL.
insert_value ::= DEFAULT.

replace_value ::= insert_value.

update_assignment ::= qualified_identifier EQUAL update_value.
update_value ::= NULL.
update_value ::= DEFAULT.
```

## Semantics

Strictness is evaluated from the session SQL mode at execution time:

- strict if `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` is active;
- non-strict otherwise.

For omitted columns and explicit DML `DEFAULT`:

- `ENUM NOT NULL` with no explicit default materializes the first enum label
  without warning;
- every other covered no-explicit-default `NOT NULL` family materializes its
  implicit value and appends warning `1364`;
- nullable dropped-default columns keep the existing SQL `NULL` plus warning
  `1364` behavior.

For matched `UPDATE ... SET column = NULL` and shared `INSERT IGNORE` explicit
`NULL` adjustment:

- `ENUM NOT NULL` stores the empty enum error value and appends warning `1048`;
- other covered families store the same implicit value used by the current
  `INSERT IGNORE` policy and append warning `1048`;
- nullable columns store SQL `NULL` without adjustment warning.

## Diagnostics

This phase reuses existing diagnostics:

| Case | Code | SQLSTATE | Message |
| --- | --- | --- | --- |
| missing explicit default adjusted | `1364` | `HY000` | `Field '<column>' doesn't have a default value` |
| explicit `NULL` into `NOT NULL` adjusted | `1048` | `23000` | `Column '<column>' cannot be null` |

Strict-mode errors, unsupported syntax, unknown names, physical SQLite errors,
allocation failures, and public API misuse keep their existing diagnostics.

## Physical SQLite Handling

No SQLite fork patch or new public SQLite extension point is needed.

The implementation remains in MyLite's descriptor-driven runtime layer:

- generated SQL uses stable physical table names and quoted identifiers;
- generated SQL uses prepared statements with bound integer, real, text, blob,
  and `NULL` values;
- BLOB and `BIT` values are bound byte-safely;
- JSON `null` is stored through the existing JSON descriptor value path;
- no SQLite schema default clause or conflict algorithm is treated as the
  compatibility authority.

## Tests

Extend the existing non-strict DML test coverage and expectation script to
cover:

- MySQL 8.4.9 expectation probes for all covered descriptor families;
- omitted-column `INSERT` values and warning rows;
- explicit DML `DEFAULT` values and warning rows;
- matched `UPDATE ... SET column = NULL` values and warning rows;
- `ENUM NOT NULL` first-label omitted/default behavior without warning;
- `ENUM NOT NULL` explicit-`NULL` adjustment to the empty enum value with
  warning `1048`;
- byte-safe binary, BLOB-family, and `BIT` readback;
- changed-row affected counts, warning counts, no row result sets, and
  persistence through the existing file-backed non-strict test coverage.
