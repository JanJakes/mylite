# Baseline Integer Display Width

## Status

This feature specifies a narrow metadata and warning slice for deprecated
integer display width syntax in MySQL 8.4.9. It builds on MyLite's descriptor
integer families, optional `SIGNED` / `UNSIGNED` attributes, integer aliases,
row-value conversion, descriptor-backed DML, and table introspection.

The feature intentionally does not add `ZEROFILL`, display padding,
protocol-grade column metadata, compact integer storage, non-integer display
widths, or expression semantics. Display width is accepted only where MySQL
8.4.9 accepts it for currently supported integer type syntax.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline small integer types:
  `docs/specs/baseline-small-integer-types/specs.md`
- Baseline integer `SIGNED` attribute:
  `docs/specs/baseline-integer-signed-attribute/specs.md`
- Baseline integer type aliases:
  `docs/specs/baseline-integer-type-aliases/specs.md`
- Current DDL, row-value, select, delete, update, alter, and show specs under
  `docs/specs/`
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, numeric type attributes:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-attributes.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence Summary

Observed MySQL 8.4.9 behavior:

- `TINYINT(M)`, `SMALLINT(M)`, `MEDIUMINT(M)`, `INT(M)`,
  `INTEGER(M)`, `BIGINT(M)`, and the supported `INT1` / `INT2` / `INT3` /
  `INT4` / `INT8` aliases accept unsigned decimal display widths in `0..255`.
- Each accepted display width clause appends warning `1681` with message
  `Integer display width is deprecated and will be removed in a future release.`
- Width `256` fails with error `1439` / SQLSTATE `42000` and message
  `Display width out of range for column 'c' (max = 255)`.
- Signed width tokens such as `INT(+1)` and `INT(-1)`, empty width lists, and
  width after `UNSIGNED` such as `INT UNSIGNED(1)` are syntax errors.
- Display width does not change stored integer ranges, inserted values, update
  conversion, predicate conversion, ordering, or selected value text.
- In `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`,
  MySQL 8.4.9 normalizes all admitted integer display widths away except
  signed `TINYINT(1)` and `INT1(1)`, which render as `tinyint(1)`.
- `TINYINT(1) UNSIGNED` renders as `tinyint unsigned`, not
  `tinyint(1) unsigned`.
- `TINYINT(0)` and `TINYINT(2)` render as `tinyint`.
- `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` admit the same width syntax in
  supported column definitions and emit one warning per display width clause.
- Altering between `TINYINT` and `TINYINT(1)` reports `ROW_COUNT() == 0` in
  MySQL 8.4.9 and changes only metadata visible through introspection.

The executable evidence script is
`packages/libmylite/tests/mysql_baseline_integer_display_width_expectations.sh`.

## Scope

The implementation must add:

- parser support for `integer_type ( unsigned_decimal_width )` before optional
  `SIGNED` or `UNSIGNED` in the existing supported column-definition positions;
- support for display width on canonical integer families and supported
  integer aliases only;
- validation that display width is an unsigned decimal integer in `0..255`;
- one deprecation warning `1681` for each admitted display width clause;
- descriptor normalization that preserves only MySQL-visible signed
  `TINYINT(1)` / `INT1(1)` metadata and normalizes every other width away;
- descriptor-driven `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]` support;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering of `tinyint(1)` for descriptors that preserve it;
- existing integer range conversion, nullable handling, predicate conversion,
  ordering, DML execution, reopen persistence, file-format preamble, and
  independent-handle behavior for width-declared columns;
- no SQLite fork patch, public API change, physical storage change, or compact
  integer representation.

## Non-Goals

This feature must not implement:

- `ZEROFILL`, including its unsigned implication, zero-padding display behavior,
  warnings, or metadata rendering;
- display width for `BIT`, `DECIMAL`, `FLOAT`, `DOUBLE`, string, temporal,
  binary, enum, set, JSON, or spatial types;
- display width in casts, function arguments, parameters, expressions, or
  non-column-definition grammar;
- signed, negative, empty, decimal-point, exponent, hex, bit, string, or
  parameter display-width tokens;
- display widths above `255`;
- combined or repeated attribute lists beyond what the current integer grammar
  already admits;
- protocol-grade result metadata length changes;
- changed storage ranges, selected value formatting, padding, arithmetic, or
  expression truth semantics.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns result allocation,
  diagnostics exposure, and public misuse behavior.
- Statement context and diagnostics own warning persistence and result
  `warning_count`. This feature appends MySQL-shaped warning records during
  statement planning for admitted display width syntax.
- Lexer/parser/AST own syntax admission, source spans, and display-width token
  capture. They stay independent of catalog, runtime, storage, and SQLite.
- Analyzer/planner validates display-width range, emits deprecation warnings,
  maps aliases to normalized integer descriptors, and decides whether width is
  metadata-preserving or normalized away.
- The catalog remains the durable authority for logical descriptors. This
  feature stores signed `TINYINT(1)` as descriptor text distinct from
  `TINYINT`; all other admitted display widths store the same descriptor text
  as the no-width form.
- Result and introspection builders render descriptor logical types to
  MySQL-shaped lower-case type text. SQLite schema text is not user-visible
  authority.
- SQLite remains the physical b-tree storage and execution engine. Generated
  physical tables still use SQLite `INTEGER` columns and prepared statements.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

This feature expands the existing `integer_type` nonterminal. Width appears
immediately after the type name or alias and before optional `SIGNED` or
`UNSIGNED`.

Supported examples:

```sql
TINYINT(1)
TINYINT(1) SIGNED
TINYINT(1) UNSIGNED
SMALLINT(5)
MEDIUMINT(9)
INT(11)
INTEGER(10)
BIGINT(20)
INT1(1)
INT2(5)
INT3(7)
INT4(9)
INT8(20)
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
integer_type ::= integer_family_name display_width_opt integer_signedness_opt.
integer_family_name ::= TINYINT.
integer_family_name ::= SMALLINT.
integer_family_name ::= MEDIUMINT.
integer_family_name ::= INT.
integer_family_name ::= INTEGER.
integer_family_name ::= BIGINT.
integer_family_name ::= INT1.
integer_family_name ::= INT2.
integer_family_name ::= INT3.
integer_family_name ::= INT4.
integer_family_name ::= INT8.

display_width_opt ::= .
display_width_opt ::= LPAREN UNSIGNED_INTEGER RPAREN.

integer_signedness_opt ::= .
integer_signedness_opt ::= SIGNED.
integer_signedness_opt ::= UNSIGNED.
```

`UNSIGNED_INTEGER` here means the existing decimal digit token class used by
MyLite's literal parser, with no unary sign and no non-decimal notation.

The parser must continue to reject `INT(+1)`, `INT(-1)`, `INT()`,
`INT UNSIGNED(1)`, decimal width literals, string width literals, hex width
literals, bit width literals, parameters, and expression widths.

## Descriptor Semantics

MyLite must preserve only display width that remains visible in MySQL 8.4.9
metadata for the admitted subset:

| SQL input family | Width | Unsigned? | Stored descriptor |
| --- | --- | --- | --- |
| `TINYINT` | `1` | no | `TINYINT(1)` |
| `INT1` | `1` | no | `TINYINT(1)` |
| any other admitted integer family | any `0..255` | any | no-width normalized descriptor |
| `TINYINT` / `INT1` | any width other than `1` | no | `TINYINT` |
| `TINYINT` / `INT1` | any width | yes | `TINYINT UNSIGNED` |

`TINYINT(1)` has the same value range, physical type, conversion behavior,
nullability behavior, predicate behavior, ordering behavior, and DML behavior as
`TINYINT`.

Display-width-only changes, such as `ALTER TABLE t MODIFY c TINYINT(1)` when
`c` is already `TINYINT`, are catalog metadata changes only. They must not
rebuild physical SQLite tables or reread/rewrite user rows when physical type,
integer range, nullability, and name are unchanged.

## Diagnostics And Warnings

Supported display width syntax emits warning:

- level: `Warning`
- code: `1681`
- message: `Integer display width is deprecated and will be removed in a future release.`

One warning is emitted for each display width clause in the statement. A
successful statement with width clauses returns through the existing public
result conventions and reports `warning_count` equal to the number of width
warnings plus any existing statement warnings generated by the admitted
statement form.

Unsupported or invalid forms:

- width greater than `255`: error `1439` / SQLSTATE `42000`, message
  `Display width out of range for column 'name' (max = 255)`;
- signed width, empty width, non-decimal width, expression width, parameter
  width, or width after signedness: existing parse error behavior;
- `ZEROFILL`: remains unsupported for this slice;
- non-integer type widths: remain unsupported or syntax errors according to
  existing parser scope;
- allocation and physical SQLite failures: existing MyLite internal or physical
  error diagnostics.

## Runtime And Storage Semantics

`CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]` create the same SQLite
physical `INTEGER` columns as no-width integer definitions. Existing nullable
rows receive `NULL` on `ADD`; existing `NOT NULL` rows receive the current zero
backfill.

`ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` must validate existing
rows only when the normalized integer range or nullability changes. A
display-width-only change requires only a catalog descriptor mutation and
introspection must observe it after close/reopen.

`INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` assignment conversion use
the normalized integer range. `SELECT`, `DELETE`, and `UPDATE` predicates and
single-column ordering use the same descriptor-driven integer paths as the
corresponding no-width family.

The generated SQLite SQL must continue to quote identifiers, bind values as
parameters, use stable physical table names, and avoid interpolation of user
literals. No SQLite fork patch is required.

## Tests

Add or extend fast plain C tests under `packages/libmylite/tests/` and keep any
new test binary registered with dotted CTest names if a new binary is needed.
Coverage must include:

- parser acceptance for canonical integer families and `INT1` / `INT2` /
  `INT3` / `INT4` / `INT8` aliases with width before optional signedness;
- parser rejection for signed, empty, non-decimal, expression, parameter, and
  post-signedness widths;
- MySQL warning count and `SHOW WARNINGS` behavior for one and multiple width
  clauses;
- `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and `CHANGE` with display
  widths;
- descriptor persistence and introspection for `tinyint(1)` and normalized
  no-width forms;
- DML assignment and predicate behavior over width-declared columns;
- out-of-range display width diagnostics;
- unsupported `ZEROFILL` remains deterministic;
- metadata-only alter from `TINYINT` to `TINYINT(1)` and back without physical
  table rebuild when name, physical type, range, and nullability are unchanged;
- file-backed reopen persistence, preamble preservation, independent handles,
  cleanup paths, and existing parser/runtime lifecycle regression tests.

