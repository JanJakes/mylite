# DML Integer String Prefix Coercion

## Summary

This phase broadens the existing quoted numeric string DML conversion for
integer-family storage targets. MyLite already accepts exact quoted integer
strings. This slice adds MySQL-compatible leading ASCII whitespace handling,
numeric-prefix scanning, decimal/exponent rounding into integer targets,
non-strict warning adjustment, and `INSERT IGNORE` warning adjustment for
ordinary SQL string literals assigned to supported integer-family columns.

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

Only integer-family target descriptors are expanded by this phase. Existing
`DECIMAL` and approximate numeric string handling remains unchanged.
`INSERT ... SELECT`, `REPLACE ... SELECT`, scalar subquery assignment, prepared
parameters, user variables, expression-level conversion, predicate conversion,
and full unsigned `BIGINT` values above MyLite's current signed-64 physical
integer envelope remain outside this slice.

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
- Official MySQL 8.4 Reference Manual:
  - Type conversion in expression evaluation:
    <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
  - Out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
  - Server SQL modes:
    <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_dml_int_string_prefix_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for this slice:

- The default MySQL 8.4.9 session mode includes `STRICT_TRANS_TABLES`.
- Strict DML accepts leading and trailing ASCII whitespace around a numeric
  string with no warning, including spaces, tabs, and newlines.
- Strict DML accepts quoted decimal and exponent numeric strings for integer
  targets, rounds half away from zero, and stores the rounded integer without a
  warning. Examples: `'1.5'` stores `2`, `'-2.5'` stores `-3`, and `'1e2'`
  stores `100`.
- Strict DML preserves exact integer-family boundary behavior for admitted
  decimal and exponent strings inside MyLite's physical integer envelope.
  Examples: `'9223372036854775807.0'` and
  `'9.223372036854775807e18'` store `9223372036854775807`.
- Strict DML accepts incomplete exponent markers at end-of-token with no
  warning. Examples: `'1e'`, `'1e+'`, and `'1e-'` store `1`.
- Strict DML rejects a numeric prefix followed by non-whitespace trailing text
  with error `1265 / 01000`, `Data truncated for column '<column>' at row <n>`.
  Examples include `'123abc'`, `'1.2abc'`, `'1e2x'`, `'1efoo'`, `'1.foo'`,
  and `'0x10'`.
- Strict DML rejects strings that do not start with a number after leading
  whitespace with error `1366 / HY000`,
  `Incorrect integer value: '<value>' for column '<column>' at row <n>`.
  Examples include `'abc123'`, `''`, only spaces, `'+'`, and `'+ 1'`.
- Strict signed and unsigned range errors use `1264 / 22003`,
  `Out of range value for column '<column>' at row <n>`.
- In non-strict mode (`SET sql_mode = ''`), invalid strings store `0` and
  record warning `1366`; truncated numeric prefixes store the parsed value and
  record warning `1265`; out-of-range values clip to the nearest supported
  endpoint and record warning `1264`.
- `INSERT IGNORE` in strict mode uses the same warning-adjusted storage behavior
  for invalid strings, truncated numeric prefixes, and out-of-range values.
- `UPDATE` assignment conversion is evaluated only for matched rows. A
  non-strict no-match `UPDATE ... SET i = 'abc' WHERE ...` records no warning;
  matched rows record one warning per adjusted row and assignment.
- Successful supported statements return the existing non-row DML result shape.
  `UPDATE` affected rows use changed-row semantics.

## Scope

This phase expands only ordinary SQL string literals assigned to integer-family
targets in the existing row-value DML paths:

- signed and unsigned `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT` / `INTEGER`,
  and `BIGINT`;
- `INT1`, `INT2`, `INT3`, `INT4`, and `INT8` aliases after descriptor
  normalization;
- supported `AUTO_INCREMENT` integer targets in the same row-value paths;
- admitted duplicate-key assignment values where the existing duplicate-key
  planner already reuses row-value conversion.

MyLite's current physical integer model remains authoritative:

- signed descriptors use their MySQL storage ranges;
- unsigned descriptors use their MySQL storage ranges where the endpoint fits
  the current signed-64 SQLite integer storage model;
- `BIGINT UNSIGNED` remains capped at `9223372036854775807` for row storage in
  this slice.

## Non-Goals

This phase does not add:

- `UPDATE IGNORE`;
- `INSERT ... SELECT`, `REPLACE ... SELECT`, or scalar subquery string-to-integer
  conversion;
- string-to-integer conversion for predicates, ordering, grouping, functions,
  default expressions, `DEFAULT(column_name)`, user variables, parameters, or
  arbitrary expressions;
- hexadecimal or bit-string numeric conversion;
- full unsigned `BIGINT` storage above MyLite's signed-64 physical range;
- locale-specific decimal separators, non-ASCII digits, `NaN`, `Infinity`, or
  MySQL's complete numeric scanner;
- SQLite type-affinity conversion as a compatibility authority;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result/diagnostic accessors keep
  the existing ABI and ownership rules.
- Statement context: owns diagnostic reset, warnings, affected rows, and the
  non-row result shape.
- Session state: owns strict versus non-strict mode and string-literal decoding
  modes such as `NO_BACKSLASH_ESCAPES`.
- Lexer/parser/AST: unchanged. Existing string literal nodes continue to be
  syntax-only; no grammar expansion is required.
- Analyzer/planner/runtime conversion: decodes the string literal, performs
  MyLite-owned numeric scanning and integer rounding, applies descriptor ranges,
  and binds the converted integer value.
- Catalog: remains the source of logical integer type, unsigned state,
  nullability, defaults, auto-increment state, and physical table names. This
  feature does not mutate catalog descriptors, descriptor versions, catalog
  generation, or SQLite schema generation.
- SQLite physical storage: receives generated SQL using quoted stable physical
  identifiers and bound integer parameters. SQLite affinity is not used for
  MySQL conversion.
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

The SQL string literal is decoded exactly once through MyLite's existing
string-literal decoder before numeric scanning. Decoding observes the current
string SQL mode. Embedded `NUL` bytes remain rejected for integer storage.

### Numeric Scanner

The decoded byte string is scanned as follows:

1. Skip leading ASCII whitespace: space, tab, newline, vertical tab, form feed,
   and carriage return.
2. Accept an optional `+` or `-` sign only when the next byte begins a numeric
   token. A sign followed by whitespace is invalid.
3. Accept a MyLite-owned decimal token over ASCII bytes:
   - decimal digits;
   - optional decimal point where MySQL's observed integer assignment accepts
     the token;
   - optional exponent with `e` or `E`, optional exponent sign, and exponent
     digits where MySQL's observed integer assignment accepts the token.
   Incomplete exponent suffixes at the end of the token are consumed for
   diagnostic purposes but do not change the numeric value.
4. If no numeric token is found, the string is invalid.
5. If the remaining suffix contains only ASCII whitespace, conversion is exact
   for diagnostic purposes.
6. If the remaining suffix contains any non-whitespace byte, conversion is
   truncated.

Integer, decimal, and exponent prefixes are parsed through MyLite's
precision-preserving decimal scanner so large integer-family boundary strings
do not lose precision before range checking. Decimal and exponent prefixes are
rounded half away from zero through decimal digit and exponent arithmetic, not
through binary floating-point conversion. The rounded absolute value must fit
MyLite's current unsigned 64-bit conversion envelope before it can be passed to
descriptor range conversion.

### Strict Mode

When either strict SQL mode is active:

- valid in-range conversions store the converted integer and record no warning;
- valid numeric prefixes with non-whitespace suffixes fail with `1265`;
- invalid strings fail with `1366`;
- rounded or exact values outside the target descriptor range fail with `1264`;
- the statement remains atomic.

### Non-Strict And IGNORE Adjustment

When neither strict SQL mode is active, or for supported `INSERT IGNORE`
row-value conversion:

- valid in-range conversions store the converted integer;
- valid numeric prefixes with non-whitespace suffixes store the converted
  integer and append warning `1265`;
- invalid strings store `0` and append warning `1366`;
- values outside the target descriptor range clip through the existing
  descriptor integer clipping path and append warning `1264`;
- multi-row `INSERT` / `REPLACE` warnings use MySQL row numbers;
- matched `UPDATE` warnings use MySQL row numbers for each matched row and
  assignment, while no-match updates and `LIMIT 0` updates record no warnings.

`INSERT IGNORE` duplicate-key, foreign-key, and other existing adjustments are
unchanged. `REPLACE` has no `IGNORE` syntax in this slice; ordinary non-strict
mode still applies.

## Diagnostics

Supported diagnostics are:

- strict truncated numeric prefix:
  `1265 / 01000`, `Data truncated for column '<column>' at row <n>`;
- adjusted truncated numeric prefix:
  warning `1265 / 01000`, same message text;
- strict invalid string:
  `1366 / HY000`, `Incorrect integer value: '<value>' for column '<column>' at
  row <n>`;
- adjusted invalid string:
  warning `1366 / HY000`, same message text;
- strict out-of-range:
  `1264 / 22003`, `Out of range value for column '<column>' at row <n>`;
- adjusted out-of-range:
  warning `1264 / 22003`, same message text;
- existing string-decoding, allocation, physical SQLite, and public API misuse
  diagnostics remain unchanged.

## Physical SQLite Handling

Generated SQL shape is unchanged. MyLite still builds descriptor-driven
`INSERT`, `REPLACE`, duplicate-key, and `UPDATE` statements over stable physical
table names such as `_mylite_user_table_<table_id>`. Every generated identifier
is quoted. Converted integer values are bound as SQLite integer parameters.

No SQLite fork patch is required. This is MyLite wrapper/planner conversion over
public SQLite prepared-statement binding APIs.

## Performance

The new work is per literal value occurrence. It does not materialize result
sets, scan table contents in MyLite, or route filtering through MyLite.
`INSERT`, `REPLACE`, and `UPDATE` continue to use generated SQLite statements
and SQLite row execution. `UPDATE` already reads the matched-row count before
conversion for non-strict adjustment behavior; this phase reuses that path for
row-numbered warnings.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-dml.md`, and
`docs/compatibility/type-system-literals-conversion.md` to state that
integer-family DML string storage conversion now supports leading ASCII
whitespace, numeric prefix scanning, decimal/exponent rounding, non-strict
warning adjustment, and `INSERT IGNORE` warning adjustment only for row-value
DML. Do not document selected-row conversion, expression conversion, predicate
conversion, decimal/approximate target changes, or full unsigned `BIGINT` as
supported.

## Tests

Fast C tests cover:

- strict leading/trailing whitespace conversion without warnings;
- strict decimal and exponent rounding into integer targets;
- strict truncation, invalid-string, and range errors;
- non-strict `INSERT`, `INSERT SET`, `REPLACE`, and `UPDATE` warning-adjusted
  invalid, truncated, and out-of-range string values;
- `INSERT IGNORE` warning-adjusted invalid, truncated, and out-of-range string
  values;
- signed and unsigned integer descriptor boundaries within MyLite's current
  physical range;
- `UPDATE` matched-row warning counts, no-match warning suppression, affected
  rows, and remaining row values;
- persistence after close/reopen and `.mylite` preamble preservation;
- independent handles keeping independent converted row state.

The MySQL expectation script covers the user-visible behavior against MySQL
8.4.9.
