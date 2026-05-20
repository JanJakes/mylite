# DML Decimal And Approximate String Prefix Coercion

## Summary

This phase broadens quoted numeric string DML conversion for `DECIMAL`,
`FLOAT`, and `DOUBLE` / `REAL` storage targets. MyLite already accepts exact
quoted fixed decimal strings for `DECIMAL` and exact quoted finite numeric
strings for approximate targets. This slice adds MySQL-compatible leading and
trailing ASCII whitespace handling, decimal/exponent numeric-prefix scanning,
strict trailing-junk diagnostics, non-strict warning adjustment, and
`INSERT IGNORE` warning adjustment for ordinary SQL string literals assigned to
supported decimal and approximate numeric descriptors.

The supported statement shapes are the existing descriptor-backed row-value
paths:

```sql
INSERT [IGNORE] [INTO] table_name [(column_name[, ...])]
    VALUES (value[, ...])[, ...]

INSERT [IGNORE] [INTO] table_name
    SET column_name = value[, ...]

REPLACE [INTO] table_name [(column_name[, ...])]
    VALUES (value[, ...])[, ...]

REPLACE [INTO] table_name
    SET column_name = value[, ...]

UPDATE table_name
    SET column_name = value[, ...]
    [WHERE ...] [ORDER BY column_name [ASC | DESC]] [LIMIT row_count]
```

Only string-literal assignment into `DECIMAL`, `FLOAT`, and `DOUBLE` / `REAL`
targets is expanded. `INSERT ... SELECT`, `REPLACE ... SELECT`, scalar subquery
assignment, prepared parameters, user variables, expression-level conversion,
predicate conversion, quoted decimal/approximate defaults, casts, hexadecimal
numeric conversion, bit-string numeric conversion, `NaN`, `Infinity`, and full
MySQL expression coercion remain outside this slice.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-dml-string-numeric-coercion/specs.md`
  - `docs/specs/baseline-nonstrict-dml-coercion/specs.md`
  - `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/dml-int-string-prefix-coercion/specs.md`
- Official MySQL 8.4 Reference Manual:
  - Type conversion in expression evaluation:
    <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
  - Out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
  - Server SQL modes:
    <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_dml_decimal_approx_string_prefix_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for this slice:

- The default MySQL 8.4.9 session mode includes `STRICT_TRANS_TABLES`.
- Strict DML accepts leading and trailing ASCII whitespace around decimal and
  approximate numeric strings with no warning.
- Strict `DECIMAL` DML accepts ordinary fixed decimal strings, strings with a
  leading decimal point, and exponent strings when the exponent is complete.
  Examples: `' 12.30 '`, `'.5'`, `'1e2'`, and `'1.e2'`.
- Strict approximate DML accepts the same complete numeric-token forms that fit
  the target range, including `'.5'`, `'1e2'`, and `'1.e2'`.
- Strict `DECIMAL` DML rejects a numeric prefix followed by non-whitespace
  trailing bytes with error `1366 / HY000`,
  `Incorrect decimal value: '<value>' for column '<column>' at row <n>`.
  Examples include `'12.3abc'`, `'1e2abc'`, `'1e'`, `'1e+'`, `'1e-'`,
  `'0x10'`, and `'+ 1'`.
- Strict approximate DML rejects a numeric prefix followed by non-whitespace
  trailing bytes with error `1265 / 01000`,
  `Data truncated for column '<column>' at row <n>`. Examples include
  `'1e2abc'`, `'1e'`, `'1e+'`, `'1e-'`, `'0x10'`, and `'+ 1'`.
- Strict approximate strings with no numeric prefix, such as `'abc123'`, also
  fail with `1265 / 01000`, `Data truncated ...`.
- Strict range errors use `1264 / 22003`,
  `Out of range value for column '<column>' at row <n>`.
- In non-strict mode and in strict `INSERT IGNORE`:
  - trailing text after a `DECIMAL` prefix stores the parsed prefix and records
    `Note 1265`;
  - invalid `DECIMAL` strings store decimal zero and record `Warning 1366`;
  - out-of-range `DECIMAL` prefixes clip to the descriptor endpoint and record
    `Warning 1264`;
  - trailing text after an approximate prefix stores the parsed prefix and
    records `Warning 1265`;
  - invalid approximate strings store `0` and record `Warning 1265`;
  - out-of-range approximate values clip to the target endpoint and record
    `Warning 1264`;
  - unsigned negative decimal and approximate values clip to zero and record
    range warnings after any trailing-text warnings.
- A non-strict matched `UPDATE` records one warning or note per adjusted row and
  assignment. A no-match `UPDATE` performs no assignment conversion and records
  no warnings.
- Successful supported statements return the existing non-row DML result shape.
  `UPDATE` affected rows use changed-row semantics.

## Scope

This phase expands only ordinary SQL string literals assigned to:

- `DECIMAL(M,D)`, `DECIMAL`, `DEC`, `NUMERIC`, and `FIXED` descriptors already
  supported by MyLite, including optional `UNSIGNED`;
- `FLOAT`, `FLOAT(0..24)`, and `FLOAT4` descriptors, including optional
  deprecated `UNSIGNED`;
- `DOUBLE`, `DOUBLE PRECISION`, default-mode `REAL`, `FLOAT(25..53)`, and
  `FLOAT8` descriptors, including optional deprecated `UNSIGNED`;
- admitted duplicate-key assignment values where the existing duplicate-key
  planner already reuses row-value conversion.

Existing descriptor limits remain authoritative. `DECIMAL` values stay stored
as canonical fixed-point text. Approximate values stay stored as SQLite `REAL`
after MyLite-owned range conversion and `FLOAT` single-precision rounding.

## Non-Goals

This phase does not add:

- `UPDATE IGNORE`;
- `INSERT ... SELECT`, `REPLACE ... SELECT`, or scalar subquery string-to-number
  conversion;
- string-to-number conversion for predicates, ordering, grouping, functions,
  default expressions, `DEFAULT(column_name)`, user variables, parameters,
  prepared parameter binding, or arbitrary expressions;
- bare approximate numeric literal conversion into `DECIMAL` targets beyond the
  existing subset;
- hexadecimal or bit-string numeric conversion;
- locale-specific decimal separators, non-ASCII digits, `NaN`, `Infinity`, or
  MySQL's complete numeric scanner;
- SQLite type-affinity conversion as a compatibility authority;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result/diagnostic accessors keep
  the existing ABI and ownership rules.
- Statement context: owns diagnostic reset, warning/note collection, affected
  rows, and the non-row result shape.
- Session state: owns strict versus non-strict mode and string-literal decoding
  modes such as `NO_BACKSLASH_ESCAPES`.
- Lexer/parser/AST: unchanged. Existing string literal nodes continue to be
  syntax-only; no grammar expansion is required.
- Analyzer/planner/runtime conversion: decodes string literals, performs
  MyLite-owned numeric scanning, applies descriptor range and precision rules,
  and materializes the storage value before SQL generation.
- Catalog: remains the source of logical type, unsigned state, precision, scale,
  nullability, defaults, and physical table names. This feature does not mutate
  catalog descriptors, descriptor versions, catalog generation, or SQLite
  schema generation.
- SQLite physical storage: receives generated SQL using quoted stable physical
  identifiers and bound text or real parameters. SQLite affinity is not used
  for MySQL conversion.
- Storage/VFS/file format: unchanged. Row changes remain in the shifted SQLite
  payload and must not touch the `.mylite` preamble.

## Grammar

No new grammar is required. Existing MyLite grammar already admits string
literals in the relevant row-value positions:

```lemon
insert_value ::= STRING.
replace_value ::= insert_value.
update_value ::= STRING.
```

The runtime narrows these parsed values by resolved target descriptor.

## Conversion Semantics

### String Decoding

The SQL string literal is decoded once through MyLite's existing string-literal
decoder before numeric scanning. Decoding observes the current string SQL mode.
Embedded `NUL` bytes remain rejected for numeric storage.

### Shared Numeric Scanner

The decoded byte string is scanned as follows:

1. Skip leading ASCII whitespace: space, tab, newline, vertical tab, form feed,
   and carriage return.
2. Accept an optional `+` or `-` sign only when the next byte begins a numeric
   token. A sign followed by whitespace is invalid.
3. Accept a decimal numeric token over ASCII bytes:
   - decimal digits;
   - optional decimal point;
   - optional exponent marker `e` or `E`, optional exponent sign, and at least
     one exponent digit.
4. If no numeric token is found, the string is invalid.
5. If the remaining suffix contains only ASCII whitespace, conversion is exact
   for diagnostic purposes.
6. If the remaining suffix contains any non-whitespace byte, conversion is
   truncated.

The scanner is MyLite-owned and uses descriptor conversion after scanning.
SQLite numeric conversion, locale conversion, and MySQL implementation internals
are not compatibility authorities.

### DECIMAL Targets

`DECIMAL` conversion uses decimal digit and exponent arithmetic, not binary
floating-point conversion:

- complete exponent strings are converted to fixed decimal text before the
  existing decimal canonicalizer applies scale padding, half-away-from-zero
  rounding, truncation notes, unsigned checks, and range checks;
- incomplete exponent markers are treated as trailing text for diagnostics;
- strict truncated `DECIMAL` strings fail with `1366 / HY000`,
  `Incorrect decimal value: '<value>' for column '<column>' at row <n>`;
- adjusted truncated `DECIMAL` strings append `Note 1265` before range handling;
- invalid adjusted `DECIMAL` strings append `Warning 1366` and store decimal
  zero;
- adjusted out-of-range `DECIMAL` strings append `Warning 1264` and clip to the
  decimal endpoint, or zero for unsigned negative inputs.

When both truncation and range adjustment apply, MySQL 8.4.9 reports the
truncation note before the range warning. MyLite must preserve that order.

### FLOAT And DOUBLE Targets

Approximate conversion uses MyLite's existing C-locale finite numeric parsing
for the scanned token, followed by descriptor range handling:

- strict truncated or invalid approximate strings fail with `1265 / 01000`,
  `Data truncated for column '<column>' at row <n>`;
- strict finite values outside the target range fail with `1264 / 22003`,
  `Out of range value for column '<column>' at row <n>`;
- adjusted truncated or invalid approximate strings append `Warning 1265`;
- adjusted out-of-range approximate strings append `Warning 1264` and clip to
  the finite target endpoint, or zero for unsigned negative inputs;
- `FLOAT` targets round through single precision before storage/readback;
- `DOUBLE` / `REAL` targets store the double-precision endpoint or value.

When both truncation and range adjustment apply, MySQL 8.4.9 can report both
warnings. MyLite must preserve the verified warning order for the admitted
cases.

## Diagnostics

Supported strict failures:

- `DECIMAL` truncated or invalid string:
  `1366 / HY000`, `Incorrect decimal value: '<value>' for column '<column>' at
  row <n>`.
- approximate truncated or invalid string:
  `1265 / 01000`, `Data truncated for column '<column>' at row <n>`.
- decimal or approximate range failure:
  `1264 / 22003`, `Out of range value for column '<column>' at row <n>`.
- unsupported literal shape:
  existing MyLite unsupported-value diagnostics for the target family.
- allocation failure: existing `MYLITE_NOMEM` diagnostics.

Supported adjusted warnings and notes:

- `DECIMAL` truncated prefix: `Note 1265`.
- `DECIMAL` invalid string: `Warning 1366`.
- approximate truncated or invalid string: `Warning 1265`.
- decimal or approximate range clipping: `Warning 1264`.

Diagnostics use MyLite's existing connection diagnostics API. Public API misuse
behavior is unchanged because no public surface changes.

## Physical SQLite Handling

The generated SQLite statement shapes do not change. Insert, replace,
duplicate-key update, and update planners continue to:

- resolve target descriptors before value conversion;
- generate SQL only against stable MyLite physical table names such as
  `_mylite_user_table_<table_id>`;
- quote generated SQLite identifiers;
- bind converted decimal text or approximate real values through prepared
  statement parameters;
- keep MyLite catalog descriptors authoritative over SQLite schema text.

No SQLite fork patch or optional SQLite syntax is required.

## Performance

The scanner runs once per string-literal assignment for matched rows. It is a
single pass over the decoded string plus bounded decimal canonicalization for
the resolved descriptor. It does not materialize row sets, does not inspect
SQLite metadata, and does not move filtering, ordering, or duplicate probing
out of the existing execution paths.

## Tests

Add MySQL-runtime expectation coverage for:

- strict whitespace, fixed decimal, exponent, leading-dot, and `1.e2` strings;
- strict `DECIMAL` truncated, invalid, incomplete exponent, and range errors;
- strict approximate truncated, invalid, and range errors;
- non-strict and `INSERT IGNORE` truncation, invalid, range, and unsigned
  negative adjustment for `DECIMAL`, `FLOAT`, and `DOUBLE`;
- matched `UPDATE` warnings across multiple rows and no warnings for no-match
  updates;
- warning/note order and counts;
- stored rows and readback text;
- existing non-row DML result shape and affected-row behavior;
- persistence and independent file-backed handles in C runtime tests.

Regression checks must continue to cover existing lexer, parser, runtime,
catalog, row-value, update, duplicate-key, SQL-mode, diagnostics, VFS, and file
format behavior.
