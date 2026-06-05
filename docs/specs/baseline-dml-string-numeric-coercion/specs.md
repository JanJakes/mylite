# Baseline DML String Numeric Coercion

## Summary

This phase admits a narrow, descriptor-driven subset of MySQL's string-to-number
storage conversion for the DML paths that already write rows:

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
    SET column_name = value
    [WHERE ...] [ORDER BY column_name [ASC | DESC]] [LIMIT row_count]
```

For target numeric descriptors, `value` may now be an ordinary SQL string
literal containing an exact supported numeric text. MyLite decodes the string
literal using the active string-literal SQL mode, converts it through MyLite's
descriptor-owned conversion rules, and binds the converted value into generated
SQLite statements. SQLite metadata, affinity, and text-to-number conversion are
not the compatibility authority.

This is not general expression coercion. It does not add arbitrary expression
assignments, column-to-column conversion, scalar subquery implicit conversion,
predicate string-to-number conversion, function result conversion, expression
defaults, protocol binary-value conversion, or MySQL's full permissive
string-scanning rules.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-row-values-lifecycle/specs.md`
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-dml-default-keyword-values/specs.md`
  - `docs/specs/baseline-nonstrict-dml-coercion/specs.md`
  - `docs/specs/baseline-decimal-type/specs.md`
  - `docs/specs/baseline-float-double-type/specs.md`
- Official MySQL 8.4 Reference Manual:
  - Type conversion in expression evaluation:
    <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
  - Data type default values:
    <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
  - Out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
  - Server SQL modes:
    <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_dml_string_numeric_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

The MySQL manual documents implicit string-to-number conversion, strict-mode
handling of invalid data-changing values, and warning demotion for ignored
out-of-range numeric storage. Runtime probes against MySQL 8.4.9 define the
exact user-visible behavior in this limited slice.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for this slice:

- The default MySQL 8.4.9 session SQL mode includes `STRICT_TRANS_TABLES`.
- Strict `INSERT` / `REPLACE` / `UPDATE` of exact quoted integer strings into
  compatible signed integer columns stores the integer value with
  `warning_count == 0`.
- Strict quoted signed minimum `BIGINT` stores
  `-9223372036854775808`.
- Strict quoted positive values within MyLite's currently supported signed
  physical range store into `BIGINT UNSIGNED`; values above signed 64-bit remain
  outside this slice.
- Strict quoted fixed decimal strings store into `DECIMAL` descriptors using
  MySQL's existing fixed-point rounding behavior. Discarded nonzero fractional
  digits record note `1265`.
- Strict quoted approximate strings in ordinary decimal or scientific notation
  store into `FLOAT` / `DOUBLE` descriptors when finite and in range.
- Strict invalid quoted integer strings fail with `1366 / HY000` and message
  `Incorrect integer value: '<value>' for column '<column>' at row <n>`.
- Strict quoted integer strings with nonnumeric trailing text fail with
  `1265 / 01000` and message `Data truncated for column '<column>' at row <n>`.
  This phase keeps that trailing-text behavior outside the admitted subset and
  rejects it deterministically.
- Strict quoted decimal strings with invalid content fail with
  `1366 / HY000` and message
  `Incorrect decimal value: '<value>' for column '<column>' at row <n>`.
  This phase admits only strings accepted by MyLite's fixed decimal parser.
- Strict out-of-range numeric storage fails with `1264 / 22003`.
- `INSERT IGNORE` continues to clip descriptor integer and decimal out-of-range
  values through the existing warning-demotion path once the quoted numeric
  text has been parsed.
- Non-strict invalid text-to-number and range conversion are broader in MySQL
  than this phase. MyLite still rejects unsupported string forms and does not
  add ordinary non-strict range clipping unless this spec explicitly admits the
  behavior. This phase admits non-strict clipping for already-parsed integer
  numeric literals assigned to integer-family targets, including negative
  literals assigned to unsigned columns.
- Successful supported `UPDATE` statements use MySQL changed-row affected-row
  behavior and do not return result rows.

## Scope

This feature supports only ordinary SQL string literals in numeric DML value
positions for existing descriptor-backed base tables:

- `INSERT ... VALUES`
- `INSERT ... SET`
- `REPLACE ... VALUES`
- `REPLACE ... SET`
- single-table `UPDATE ... SET`
- admitted `ON DUPLICATE KEY UPDATE` assignment values only where the existing
  duplicate-key planner already reuses the insert-value conversion path.

The target descriptor families are:

- supported signed integer descriptors;
- supported unsigned integer descriptors within MyLite's current signed-64
  physical storage range;
- supported `DECIMAL`, `DEC`, `NUMERIC`, and `FIXED` descriptors;
- supported `FLOAT`, `DOUBLE`, `DOUBLE PRECISION`, and `REAL` descriptors,
  honoring the current `REAL_AS_FLOAT` descriptor state.

The admitted quoted numeric text is intentionally narrow:

- integer targets: optional leading `+` or `-`, followed by one or more ASCII
  decimal digits;
- decimal targets: optional leading `+` or `-`, followed by MyLite's current
  fixed decimal literal form: digits with an optional decimal point and digits;
- approximate targets: optional leading sign and a finite C-locale decimal or
  scientific-notation floating literal accepted by MyLite's existing
  approximate parser.

The slice does not admit leading/trailing whitespace, prefix numeric strings
with trailing nonnumeric bytes, hexadecimal or bit strings as numeric values,
binary strings, non-ASCII digits, locale-specific decimal punctuation,
`NaN`/`Infinity`, blank strings, parameters, user variables, column references,
function calls, arithmetic expressions, scalar subqueries, `DEFAULT(col_name)`,
or generated/default expression evaluation.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to expose existing result
  and diagnostics handles. No public header or ABI change is required.
- Statement context: owns diagnostic reset, warning collection, affected rows,
  and non-row DML result shape.
- Session state: owns string literal mode effects such as `NO_BACKSLASH_ESCAPES`
  and DML strictness for existing clipping policies.
- Lexer/parser/AST: no grammar expansion is required; the parser already
  represents ordinary string literals in admitted insert and update value
  positions. The AST remains syntax-only.
- Analyzer/planner: resolves target tables and columns from descriptors,
  chooses the descriptor conversion routine, decodes string literals, parses the
  admitted numeric text, applies target range/nullability rules, and builds
  bound planned values.
- Catalog: remains authoritative for logical type, unsigned state, precision,
  scale, physical storage type, stable physical table names, and defaults. This
  feature must not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- SQLite physical storage: receives generated SQLite SQL over stable physical
  table names with quoted identifiers and bound integer, text-decimal, or real
  values. SQLite affinity is not used for MySQL conversion.
- Storage/VFS/file format: unchanged. Updated or inserted rows live only in the
  shifted SQLite payload; the `.mylite` preamble is not touched.

## Grammar

No new grammar is required. The relevant existing MyLite grammar shape is:

```lemon
insert_value ::= STRING.
replace_value ::= insert_value.

update_assignment ::= qualified_identifier EQUAL update_value.
update_value ::= STRING.
```

Runtime narrows these parsed forms by target descriptor. A string literal that
is valid for a `VARCHAR` target remains a string value; the same literal may be
decoded and converted when the resolved target descriptor is numeric.

## Conversion Semantics

### String Decoding

The SQL string literal is decoded exactly once before numeric parsing:

- quote delimiters are removed by MyLite's existing string-literal decoder;
- ordinary backslash escapes use current MyLite string-literal policy;
- `NO_BACKSLASH_ESCAPES` disables backslash escape decoding for later
  statements;
- decoded embedded `NUL` bytes are rejected for numeric conversion.

The decoded byte string is parsed as ASCII numeric text. Empty strings and
strings containing non-ASCII bytes are outside the admitted subset.

### Integer Targets

For integer target descriptors, decoded text must match:

```text
[0-9]+
-[0-9]+
[0-9]+ with a leading plus sign
```

The optional sign is separated from the unsigned magnitude and passed to the
existing descriptor integer conversion helper. Current descriptor ranges remain
authoritative:

- signed `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`, `INTEGER`, and `BIGINT`
  use their MySQL storage ranges within MyLite's signed-64 physical model;
- unsigned integer descriptors use `0` through their MySQL unsigned maximum
  when the maximum fits in signed 64-bit; unsigned `BIGINT` values above
  `9223372036854775807` remain outside this slice.

Strict out-of-range values fail with the existing MySQL-compatible
out-of-range diagnostic. Existing `INSERT IGNORE` row-value paths and
non-strict already-parsed integer numeric literal assignments may clip and warn
where they already do for unquoted integer literals.

### DECIMAL Targets

For exact decimal target descriptors, decoded text may contain an optional sign
and the current MyLite fixed decimal form. The unsigned numeric text and sign
are passed to the existing decimal canonicalizer:

- scale is padded when needed;
- discarded nonzero fractional digits round half away from zero and record note
  `1265`;
- strict out-of-range values fail with `1264 / 22003`;
- existing `INSERT IGNORE` paths may clip and warn where they already do for
  unquoted decimal literals.

Scientific notation in quoted decimal strings is a known MySQL behavior but is
outside this slice until MyLite has a broader numeric-string parser.

### Approximate Targets

For approximate target descriptors, decoded text is parsed by the existing
finite C-locale approximate parser and then bound as a real planned value.
Signed decimal and scientific-notation forms are admitted when finite and
within the current descriptor range checks. `NaN`, infinities, blank strings,
and nonnumeric text are rejected.

### NULL, DEFAULT, Booleans, And Existing Values

`NULL`, `DEFAULT`, `TRUE`, `FALSE`, and unquoted numeric values keep their
existing behavior. This phase changes only ordinary string-literal values for
numeric targets.

## Diagnostics

For admitted exact quoted numeric text, diagnostics follow the same path as the
equivalent unquoted literal:

- out-of-range integer, decimal, or approximate storage:
  `1264 / 22003`, `Out of range value for column '<column>' at row <n>`;
- decimal fractional rounding note:
  `1265 / 01000`, `Data truncated for column '<column>' at row <n>`;
- nullability and missing-default errors remain unchanged.

For string forms that this phase does not admit, MyLite returns deterministic
diagnostics:

- nonnumeric integer text uses MySQL-compatible `1366 / HY000`
  `Incorrect integer value: '<value>' for column '<column>' at row <n>`;
- unsupported decimal string text uses MyLite's existing decimal unsupported
  diagnostic unless the string is accepted by the fixed decimal parser;
- unsupported approximate string text uses MyLite's existing out-of-range or
  unsupported approximate diagnostic;
- embedded `NUL` byte or string-decoding failure uses the existing string
  decoder diagnostic for invalid DML string literals;
- allocation failures return `MYLITE_NOMEM` and set the existing allocation
  diagnostic;
- physical SQLite failures remain reported through existing physical row
  diagnostics.

## Physical SQLite Handling

Generated SQLite SQL shape is unchanged. MyLite still builds `INSERT`,
`REPLACE`, duplicate-key, and `UPDATE` statements from descriptors and stable
physical table names such as `_mylite_user_table_<table_id>`. Every generated
identifier is quoted, and every converted value is bound through a prepared
statement parameter:

- integer conversions bind as SQLite integer values;
- `DECIMAL` conversions bind canonical decimal text, preserving the existing
  descriptor-owned decimal storage path;
- approximate conversions bind SQLite real values;
- `NULL` conversions bind SQLite `NULL`.

No SQLite fork patch is required. The feature is a MyLite wrapper/planner
conversion change over public SQLite prepared-statement binding APIs.

## Performance

The conversion is per DML value occurrence. It does not materialize result
sets, scan tables, or route row filtering through MyLite. `INSERT`, `REPLACE`,
and `UPDATE` continue to use generated SQLite statements and SQLite row
execution. The only new overhead is decoding and parsing a string literal before
binding the value.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` for the
exact supported subset. Update
`docs/compatibility/type-system-literals-conversion.md` because string literal
and type-conversion support expand for DML storage conversion. Do not document
general expression coercion, prefix-string numeric scanning, decimal scientific
notation storage, function-result conversion, or subquery conversion as
supported.

## Tests

Fast C tests cover:

- strict quoted integer insert, replace, update, and duplicate-key update paths;
- signed and unsigned integer descriptor families within MyLite's current
  physical range;
- strict quoted fixed decimal insert/update with fractional rounding notes;
- strict quoted approximate insert/update with decimal and exponent forms;
- strict invalid integer strings with `1366`;
- strict integer out-of-range strings with `1264`;
- current deterministic rejection of unsupported string forms;
- `INSERT IGNORE` and non-strict integer numeric-literal clipping where the
  existing unquoted paths already support clipping;
- affected rows, warning counts, absence of row result sets, persistence after
  close/reopen, and `.mylite` preamble preservation.

The MySQL expectation script covers the same user-visible behavior against
MySQL 8.4.9 and records broader MySQL behaviors that remain outside this slice.
