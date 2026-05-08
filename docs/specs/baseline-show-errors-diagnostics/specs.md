# Baseline SHOW Errors Diagnostics

## Status

This feature specifies a narrow diagnostics introspection slice for
`SHOW ERRORS` and `SHOW COUNT(*) ERRORS`. It builds on
`baseline-show-warnings-diagnostics`, including the connection-local previous
diagnostics snapshot that is preserved across successful diagnostic statements
and replaced after ordinary statements or statement failures.

The feature is intentionally not a full MySQL diagnostics-area
implementation. MyLite exposes the previous statement's stored error
condition through MySQL's `SHOW ERRORS` result shape, but it does not yet
implement notes, `max_error_count`, `sql_notes`, `GET DIAGNOSTICS`,
`@@error_count`, multiple error conditions from stored programs, condition
handlers, or warning-producing conversion paths.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline diagnostics snapshot:
  `docs/specs/baseline-show-warnings-diagnostics/specs.md`
- Baseline `SHOW PROCESSLIST` warning source:
  `docs/specs/baseline-show-processlist-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `SHOW ERRORS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-errors.html
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- MySQL 8.4 Reference Manual, the diagnostics area:
  https://dev.mysql.com/doc/refman/8.4/en/diagnostics-area.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- Official syntax admits `SHOW ERRORS [LIMIT [offset,] row_count]` and
  `SHOW COUNT(*) ERRORS`.
- `SHOW ERRORS` returns columns `Level`, `Code`, and `Message`.
- `SHOW COUNT(*) ERRORS` returns one column named `@@session.error_count`.
- `SHOW ERRORS` is a diagnostic statement. It reads the current diagnostics
  area and does not clear it when successful. A later ordinary nondiagnostic
  statement, such as `SELECT 1`, clears the message list.
- A parse error followed by `SHOW ERRORS` in the same session displays one
  row with `Level = Error`, code `1064`, and the parse-error message.
- A warning-only previous statement, such as `SHOW PROCESSLIST`, yields
  `SHOW COUNT(*) ERRORS = 0` and an empty `SHOW ERRORS` result, while a
  following `SHOW WARNINGS` still displays the warning row. `SHOW ERRORS`
  filters the displayed conditions without clearing the underlying list.
- `SHOW ERRORS` itself returns a result set, so `ROW_COUNT()` is `-1`.
- `SHOW ERRORS LIMIT 0` succeeds and displays no rows.
- `SHOW ERRORS LIMIT 1`, `LIMIT 0,1`, and `LIMIT 1 OFFSET 0` display the
  first error row when one error exists.
- `SHOW ERRORS LIMIT 1,1` and `LIMIT 1 OFFSET 1` display no rows when only
  one error exists.
- `SHOW ERRORS LIMIT 18446744073709551615` is accepted.
- `SHOW ERRORS LIMIT -1`, `LIMIT +1`, and
  `LIMIT 18446744073709551616` are syntax errors.
- `SHOW COUNT (*) ERRORS`, `SHOW ERRORS LIKE 'pattern'`, and
  `SHOW ERRORS WHERE ...` are syntax errors.

The local MySQL container used for expectation scripts can start a new client
session with a preexisting warning from its initialization path. Expectation
scripts first execute a harmless nondiagnostic statement before checking an
empty diagnostics area.

## Scope

The implementation must add:

- parser and AST support for `SHOW ERRORS`, `SHOW ERRORS LIMIT ...`, and
  `SHOW COUNT(*) ERRORS`;
- result-set rendering for the previous error condition when present;
- `SHOW COUNT(*) ERRORS` total-count rendering for the previous stored error
  condition only;
- MySQL-compatible `LIMIT` display slicing for unsigned decimal integer
  literal row-count and offset forms through `UINT64_MAX`;
- diagnostics-statement lifecycle integration: successful `SHOW ERRORS` and
  `SHOW COUNT(*) ERRORS` preserve the previous diagnostics snapshot;
- result-set row-count behavior matching existing MyLite conventions
  (`ROW_COUNT() = -1`);
- `warning_count == 0` on successful `SHOW ERRORS` and
  `SHOW COUNT(*) ERRORS`, because those successful diagnostic statements do
  not create new warning records;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `GET DIAGNOSTICS`, `@@error_count`, diagnostics-area stacking, stored
  program condition handlers, `SIGNAL`, `RESIGNAL`, or condition item names;
- notes, `max_error_count`, `sql_notes`, counted-but-not-stored messages, or
  multiple stored error conditions beyond the single current MyLite error
  condition;
- new error or warning producers, new DML conversion warnings, protocol
  warning/error packet changes, or `mysql_warning_count()`;
- `SHOW ERRORS WHERE`, `LIKE`, `ORDER BY`, aliases, expressions, functions,
  parameters, string/decimal/float/hex/bit limit literals, signed limit
  literals, or arbitrary SELECT-like clauses;
- catalog mutations, descriptor mutations, physical table mutations,
  arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result-handle ownership, statement-boundary
  row-count state, diagnostics snapshot replacement, and failure cleanup.
- Statement context owns the live diagnostics reset at statement start and the
  active statement lifecycle. It continues to reset live diagnostics for every
  statement; preservation is handled by the connection-owned previous snapshot.
- Connection/session state owns the previous diagnostics snapshot because
  MySQL diagnostics are session-local and `SHOW ERRORS` reads the previous
  statement's conditions.
- Lexer/parser/AST own syntax admission, source spans, and `COUNT(*)`
  no-space enforcement. They remain independent of runtime, catalog, storage,
  and SQLite.
- Runtime execution builds `SHOW ERRORS` results directly from MyLite-owned
  diagnostics records. It does not query SQLite, catalog tables,
  `INFORMATION_SCHEMA`, Performance Schema, or system variables.
- The result builder owns the `Level`, `Code`, `Message`, and
  `@@session.error_count` result shapes.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This diagnostics introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW ERRORS
SHOW ERRORS LIMIT row_count
SHOW ERRORS LIMIT offset, row_count
SHOW ERRORS LIMIT row_count OFFSET offset
SHOW COUNT(*) ERRORS
```

`row_count` and `offset` are unsigned decimal integer literals with no sign.
The parser rejects signed literals and non-integer literals.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_errors_statement.

show_errors_statement ::= SHOW ERRORS limit_clause_opt.

show_errors_statement ::= SHOW COUNT LPAREN STAR RPAREN ERRORS.
```

The parser builder for the `SHOW COUNT(*) ERRORS` production must verify that
`COUNT` and `(` are adjacent in the source text, matching the existing
`COUNT(*)` scalar-expression policy and observed MySQL behavior.

`ERRORS` is tokenized as an unreserved keyword by the lexer. This slice maps
it to an explicit parser token only for the admitted `SHOW` grammar; it
remains available as an identifier where the parser permits ordinary
identifiers.

## Diagnostics Snapshot Semantics

This feature uses the two diagnostics areas introduced by
`baseline-show-warnings-diagnostics`:

- the live diagnostics area for the statement currently being executed;
- a previous-diagnostics snapshot used by diagnostic `SHOW` statements.

Starting any statement resets the live area as it does today. After a
successful nondiagnostic statement, MyLite replaces the previous snapshot with
the final live diagnostics from that statement. After a failed statement,
including parse errors and unsupported syntax, MyLite replaces the previous
snapshot with the final error condition. After a successful `SHOW ERRORS` or
`SHOW COUNT(*) ERRORS`, MyLite leaves the previous snapshot unchanged.

Successful diagnostic statements leave live `mylite_errcode()` as `MYLITE_OK`
and live public result warning count as `0`; preserving the previous snapshot
does not mean preserving the previous live error condition.

`SHOW ERRORS` is a display filter over the previous snapshot. If the previous
snapshot contains only warning records, `SHOW ERRORS` returns no rows and
`SHOW COUNT(*) ERRORS` returns `0`, but subsequent `SHOW WARNINGS` still sees
the warning records because the snapshot was not cleared.

The snapshot is connection-local and in-memory. It is not stored in the
`.mylite` file and is not shared between independent handles.

## Result Semantics

`SHOW ERRORS` returns:

| Ordinal | Column | Type in this slice |
| --- | --- | --- |
| 1 | `Level` | `Error` for a previous error condition |
| 2 | `Code` | decimal MySQL diagnostic code text |
| 3 | `Message` | MyLite's diagnostic message text |

The current MyLite diagnostics model stores at most one error condition plus
zero or more warning records. This slice displays only the stored error
condition. Warning records are ignored for `SHOW ERRORS` output and
`SHOW COUNT(*) ERRORS` counts.

`SHOW COUNT(*) ERRORS` returns one row and one column:

| Column | Value |
| --- | --- |
| `@@session.error_count` | decimal count of previous stored error conditions |

The count is `0` or `1` in this slice. This does not implement the
`@@error_count` system variable or counted-but-not-stored condition behavior.

Successful `SHOW ERRORS` and `SHOW COUNT(*) ERRORS` return result sets and
therefore set `ROW_COUNT()` to `-1` through the existing result-row-count
policy. The result object's warning count is `0`.

## LIMIT Semantics

The supported `LIMIT` forms behave like the corresponding `SHOW WARNINGS`
forms:

- no `LIMIT`: display every error row in the previous snapshot;
- `LIMIT row_count`: display at most `row_count` error rows from the start;
- `LIMIT offset, row_count`: skip `offset` error rows, then display at most
  `row_count` rows;
- `LIMIT row_count OFFSET offset`: display at most `row_count` rows after
  skipping `offset` rows.

`row_count` and `offset` must parse as unsigned decimal integer literals in
the range `0..UINT64_MAX`. `LIMIT 0` succeeds and displays no rows.

Signed forms such as `LIMIT +1` and `LIMIT -1`, literals above
`UINT64_MAX`, nondecimal forms, strings, parameters, functions, and
expressions are rejected deterministically. MyLite reports its existing parse
or unsupported-syntax diagnostics; tests verify the MySQL 8.4.9 behavior, but
this slice does not claim byte-identical parser messages.

## Physical SQLite Handling

No SQLite SQL is generated. No prepared statements are needed. No catalog rows,
descriptor versions, descriptor caches, physical user tables,
`sqlite_schema_generation`, or file preamble bytes are read or mutated.

The implementation is a MyLite result-builder path over connection-owned
diagnostics memory. SQLite fork patches are not needed.

## Diagnostics And Errors

Supported `SHOW ERRORS` statements fail only for allocation or internal
misuse failures. Allocation failures set `MYLITE_NOMEM`. Internal invariant
failures set a MyLite runtime error.

Unsupported grammar, including `SHOW COUNT (*) ERRORS`,
`SHOW ERRORS LIKE ...`, `SHOW ERRORS WHERE ...`, signed limits, and noninteger
limits, is rejected by the parser or by the narrow runtime limit validator.
Those failures replace the previous diagnostics snapshot with the new error
condition, so a following supported `SHOW ERRORS` displays that failure.

## Tests

Add `packages/libmylite/tests/mysql_baseline_show_errors_diagnostics_expectations.sh`
to record MySQL 8.4.9 expectations for:

- result headers for `SHOW ERRORS` and `SHOW COUNT(*) ERRORS`;
- empty diagnostics count and `ROW_COUNT()` behavior;
- warning-only previous diagnostics producing no error rows while preserving
  `SHOW WARNINGS` output;
- parse-error display and count behavior;
- diagnostic chaining through `SHOW ERRORS`, `SHOW COUNT(*) ERRORS`, and
  `SHOW WARNINGS`;
- ordinary statement clearing;
- `LIMIT 0`, exact one-row limits, offset forms, and `UINT64_MAX`;
- rejected `SHOW COUNT (*) ERRORS`, signed limits, too-large limits,
  `LIKE`, and `WHERE`.

Add fast C tests under `packages/libmylite/tests/` for:

- parser acceptance and rejection;
- empty, warning-only, and error previous snapshots;
- result columns, row counts, warning counts, and row-count state;
- diagnostic statement preservation and ordinary statement clearing;
- limit slicing;
- file preamble and generation invariants;
- independent handles.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` only
for the exact supported subset:

- `SHOW ERRORS`: limited previous-statement error-condition rows with
  `Level`, `Code`, and `Message`, plus unsigned decimal `LIMIT` slicing.
- `SHOW COUNT(*) ERRORS`: limited previous-statement error-condition count
  with column label `@@session.error_count`.

Do not claim support for `@@error_count`, full diagnostics-area behavior,
notes, max-error-count truncation, `GET DIAGNOSTICS`, filters, privileges, or
multiple stored errors.

## Verification

Before implementation is marked complete, run:

1. `cmake --build --preset dev`
2. `ctest --preset dev -R 'libmylite\.(parser|runtime\.(diagnostics|show_processlist_introspection|show_warnings_diagnostics|show_errors_diagnostics))$' --output-on-failure`
3. `packages/libmylite/tests/mysql_baseline_show_errors_diagnostics_expectations.sh`
4. `cmake --workflow --preset check`

Review the final diff for parser independence, diagnostics snapshot
preservation, result shape accuracy, count and limit semantics, public ABI
stability, absence of SQLite/catalog/storage side effects, file-format safety,
zero-init cleanup, compatibility-matrix accuracy, and test relevance.
