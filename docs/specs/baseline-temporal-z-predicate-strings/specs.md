# Baseline Temporal Z Predicate Strings

## Status

This feature extends the existing descriptor-backed `DATETIME` and `TIMESTAMP`
predicate slice to admit canonical datetime strings with a single trailing
`Z` or `z` suffix:

```sql
WHERE datetime_col = '2016-01-15T00:00:00Z'
WHERE timestamp_col BETWEEN '2016-01-15T00:00:00Z' AND '2016-01-16T00:00:00Z'
```

The suffix is handled as MySQL 8.4.9 handles it in predicate comparisons:
the datetime value is truncated to the first 19 canonical characters and a
warning is recorded. It is not treated as a UTC time-zone designator. Numeric
`+HH:MM` / `-HH:MM` offsets remain owned by the existing predicate-offset
slice.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline `DATETIME` and `TIMESTAMP` specs:
  `docs/specs/baseline-datetime-type/specs.md` and
  `docs/specs/baseline-timestamp-type/specs.md`
- Baseline zero temporal SQL modes:
  `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
- Existing predicate, DML, result, diagnostics, and statement-context specs
  under `docs/specs/`
- MySQL 8.4 Reference Manual, date and time literals:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-literals.html
- MySQL 8.4 Reference Manual, `DATE`, `DATETIME`, and `TIMESTAMP`:
  https://dev.mysql.com/doc/refman/8.4/en/datetime.html
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_temporal_z_predicate_strings_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes against the local MySQL 8.4.9 container establish these
expectations for this slice:

- The documented `T` separator is accepted for datetime/timestamp contexts.
- MySQL also accepts predicate strings such as
  `'2024-01-01T00:00:00Z'`, `'2024-01-01 00:00:00Z'`, and lowercase
  `'2024-01-01T00:00:00z'` by comparing the value as
  `2024-01-01 00:00:00`.
- Each accepted trailing-suffix comparison records warning `1292` / `22007`
  with message shape
  `Incorrect datetime value: '<literal>' for column '<column>' at row 1`.
- For `DATETIME`, the trailing `Z` does not apply UTC conversion. With
  `time_zone = '+02:00'`, comparison to `'2024-01-01T00:00:00Z'` still matches
  stored `DATETIME '2024-01-01 00:00:00'`.
- For `TIMESTAMP`, the trailing `Z` is likewise not a UTC designator. MySQL's
  normal session time-zone timestamp comparison rules still apply.
- MySQL duplicates warnings for `BETWEEN` bounds during row evaluation. The
  count depends on row evaluation and short-circuiting, so this MyLite slice
  records deterministic conversion warnings once per admitted trailing-`Z`
  literal and leaves exact duplicate-warning multiplication for a later
  row-evaluator compatibility pass.
- MySQL accepts broader trailing garbage, for example a final `Q` or
  `Z+00:00`, by truncating with warning. MyLite deliberately admits only a
  single trailing `Z` or `z` in this slice.

## Scope

The implementation must add:

- semantic conversion for `DATETIME` and `TIMESTAMP` predicate string literals
  whose decoded text is exactly `YYYY-MM-DD[T ]HH:MM:SSZ` or
  `YYYY-MM-DD[T ]HH:MM:SSz`;
- support in the existing comparison, null-safe equality, inequality, range,
  `BETWEEN`, `NOT BETWEEN`, `IN`, and `NOT IN` predicate surfaces used by
  descriptor-backed `SELECT`, `DELETE`, and single-table `UPDATE`;
- conversion by copying the first 19 characters, normalizing a `T` separator to
  a space, validating the resulting canonical datetime against the target
  descriptor, and binding the converted canonical text through the existing
  prepared-statement path;
- warning `1292` / `22007` for each admitted trailing-`Z` predicate literal,
  exposed through `mylite_result_warning_count()`, `@@warning_count`,
  `SHOW WARNINGS`, and `SHOW COUNT(*) WARNINGS`;
- unchanged behavior for canonical predicates, existing `T`-separator
  predicates without suffix, and existing numeric-offset predicate strings;
- deterministic rejection of malformed numeric offsets and general trailing
  garbage outside the single-`Z` suffix admitted here;
- no parser or AST expansion, because the existing string-literal predicate
  grammar already admits the SQL surface.

## Non-Goals

This feature must not implement:

- treating `Z` / `z` as UTC or as a named time-zone designator;
- broader relaxed temporal parsing, fractional seconds, two-digit years,
  date/time literal introducers, numeric temporal values, or arbitrary trailing
  garbage truncation;
- `DATE`, `TIME`, assignment, default, `CAST`, scalar function, generated
  column, grouping, ordering-expression, or projection conversion for
  trailing-`Z` strings;
- exact MySQL duplicate warning multiplication for `BETWEEN` or complex
  short-circuited predicates;
- SQLite fork patches or SQLite schema changes.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns result lifetime,
  diagnostics snapshotting, and public warning-count exposure.
- Statement context and session state own the current SQL mode and time-zone
  fields. This feature observes string literal decoding and current
  descriptor/session behavior but does not add session state.
- Lexer/parser/AST own only existing string literal syntax. They do not
  interpret the trailing suffix.
- Analyzer/planner/runtime predicate conversion owns detection, truncation,
  canonical validation, warning generation, and prepared-statement values.
- Catalog descriptors remain authoritative for logical type and target column
  name used in diagnostics. SQLite metadata remains non-authoritative.
- SQLite physical storage remains ordinary descriptor-owned text columns in
  stable MyLite physical tables. Generated SQL still uses quoted identifiers
  and bound parameters.
- Storage/VFS and the `.mylite` preamble are untouched.

## Grammar

No new grammar is added. The existing predicate grammar already admits string
literal operands:

```lemon
comparison_predicate ::= column_reference comparison_operator string_literal.
between_predicate ::= column_reference BETWEEN string_literal AND string_literal.
in_predicate ::= column_reference IN LPAREN predicate_value_list RPAREN.
predicate_value_list ::= predicate_value_list COMMA string_literal.
predicate_value_list ::= string_literal.
```

The runtime conversion below applies only after descriptor resolution proves
that the target column is `DATETIME` or `TIMESTAMP`.

## Runtime Semantics

For a `DATETIME` or `TIMESTAMP` predicate string literal:

1. Decode the SQL string using the current statement SQL mode, preserving the
   existing `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES` behavior.
2. If the decoded text has 19 characters and a `T` separator, normalize the
   separator to a space and validate the canonical datetime as existing code
   already does.
3. If the decoded text has 25 characters and a numeric offset, reuse the
   existing offset conversion path. `DATETIME` uses the current session offset;
   `TIMESTAMP` uses the current fixed-UTC baseline.
4. If the decoded text has 20 characters, the date/time separator is a space
   or `T`, and the final character is `Z` or `z`, copy the first 19 characters,
   normalize a `T` separator to a space, validate the canonical datetime for
   the target descriptor, append warning `1292` / `22007`, and bind the
   canonical text.
5. Otherwise, use the existing canonical validation path or existing
   deterministic error path.

The trailing `Z` path never applies time-zone shifting. For `DATETIME`, this
matches the observed MySQL 8.4.9 predicate behavior. For `TIMESTAMP`, this
keeps MyLite's current fixed-UTC timestamp baseline instead of broadening into
full mutable session time-zone timestamp semantics.

## Diagnostics

Supported trailing-`Z` predicate literals append warning:

```text
Warning 1292 22007 Incorrect datetime value: '<literal>' for column '<column>' at row 1
```

The target descriptor column name is used in the message. The warning is
recorded once per converted literal. For example, a simple equality predicate
records one warning, an `IN` list with two trailing-`Z` values records two
warnings, and a `BETWEEN` predicate with two trailing-`Z` bounds records two
warnings in MyLite even though MySQL may duplicate those warnings during row
evaluation.

Malformed numeric offsets such as `+1:00`, rejected offsets such as `-00:00`,
and offsets outside the current `-13:59` to `+14:00` envelope continue to fail
with existing MyLite diagnostics. General trailing garbage such as `Q` or
`Z+00:00` remains unsupported in this slice.

## Performance

The feature stays on the existing descriptor-planned path: constants are
decoded once during planning, converted once, warned once per literal, and then
bound into SQLite prepared statements. It does not introduce row
materialization, expression evaluation outside SQLite, temporary tables, or
extra scans. This preserves the current efficient pushdown for predicate
filtering while documenting the remaining warning-count compatibility gap.

## Test Plan

Tests must cover:

- MySQL 8.4.9 expectation script coverage for equality, comparison,
  `BETWEEN`, `NOT BETWEEN`, `IN`, `NOT IN`, uppercase and lowercase suffixes,
  space and `T` separators, `DATETIME` session time zone behavior,
  `TIMESTAMP` behavior, and warning rows/counts;
- MyLite `DATETIME` C coverage for equality, `<`, `BETWEEN`, `IN`, update and
  delete predicate reuse, warning count, `SHOW WARNINGS`, lowercase `z`, and
  malformed trailing garbage rejection;
- MyLite `TIMESTAMP` C coverage for equality, `BETWEEN`, `IN`, warning count,
  `SHOW WARNINGS`, and malformed trailing garbage rejection;
- compatibility-doc updates that narrow the previous unsupported
  trailing-`Z` statement without claiming broad relaxed temporal parsing;
- focused datetime/timestamp tests, the MySQL expectation script, and the full
  `cmake --workflow --preset check`.
