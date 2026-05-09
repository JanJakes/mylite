# Baseline Integer SIGNED Attribute

## Status

This feature specifies a narrow parser and descriptor-mapping slice for the
explicit `SIGNED` attribute on integer column definitions. MySQL permits
`SIGNED` for numeric data types that permit `UNSIGNED`, but integer columns are
signed by default, so the attribute has no semantic effect for the integer
families MyLite currently supports.

The feature intentionally does not add display widths, `ZEROFILL`, combined
`SIGNED`/`UNSIGNED` attribute lists, repeated attributes, boolean aliases,
integer aliases, compact storage, expression casts, or protocol-grade metadata.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline small integer types:
  `docs/specs/baseline-small-integer-types/specs.md`
- Current DDL, row-value, select, delete, update, alter, and show specs under
  `docs/specs/`
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html
- MySQL 8.4 Reference Manual, integer types:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser support for a single explicit `SIGNED` attribute after each currently
  supported integer-family type name;
- support in all existing column-definition positions:
  `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]`;
- AST representation that preserves the signed logical family as the same
  signed descriptor MyLite already uses when no attribute is present;
- descriptor-driven DDL, DML, predicate, ordering, and introspection behavior
  identical to the corresponding bare signed type;
- MySQL 8.4.9 expectation coverage for accepted `SIGNED` forms, rendered type
  text, range checks, alter behavior, and explicitly deferred accepted MySQL
  syntax combinations.

## Non-Goals

This feature must not implement:

- display width syntax, such as `INT(11) SIGNED`;
- `ZEROFILL`;
- `UNSIGNED SIGNED`, `SIGNED UNSIGNED`, repeated `SIGNED`, or repeated
  `UNSIGNED` attribute lists, even though MySQL accepts some combinations;
- `SIGNED` after unsupported numeric types such as `DECIMAL`, `FLOAT`, or
  `DOUBLE`;
- `SIGNED` as a cast target, expression type, function argument, keyword in
  arbitrary expressions, or standalone identifier behavior beyond the existing
  lexer policy;
- `BOOL`, `BOOLEAN`, `INT1`, `INT2`, `INT3`, `INT4`, `INT8`, `SERIAL`, or
  other aliases;
- protocol metadata changes or SQLite physical storage changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own result
  allocation, diagnostics exposure, and public misuse behavior.
- Lexer/parser/AST own syntax admission for the new explicit attribute. They do
  not consult descriptors, storage, or SQLite.
- Analyzer/planner maps `SIGNED` forms to the same signed logical descriptors
  used for bare signed integer families.
- Catalog descriptors remain authoritative. No descriptor stores a separate
  signed flag for this phase because the signed attribute has no visible effect
  after type normalization.
- Result and introspection builders continue to render descriptor logical types
  to lower-case MySQL-style text without the word `signed`.
- SQLite remains the physical row storage and execution engine. This feature
  requires no SQLite fork patch, extension point, or file-format change.

## Supported SQL Grammar

This feature expands the existing `integer_type` nonterminal used by supported
column definitions. It admits a single trailing `SIGNED` keyword after the
currently supported signed integer families:

```sql
TINYINT SIGNED
SMALLINT SIGNED
MEDIUMINT SIGNED
INT SIGNED
INTEGER SIGNED
BIGINT SIGNED
```

The existing bare and `UNSIGNED` forms remain unchanged.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
integer_type ::= TINYINT SIGNED.
integer_type ::= SMALLINT SIGNED.
integer_type ::= MEDIUMINT SIGNED.
integer_type ::= INT SIGNED.
integer_type ::= INTEGER SIGNED.
integer_type ::= BIGINT SIGNED.
```

`SIGNED` in these productions sets `is_unsigned == 0` in the existing integer
type AST payload. The source span should include both the type keyword and the
`SIGNED` attribute for diagnostics/debugging consistency.

The parser must continue to reject display width, `ZEROFILL`, unsupported
aliases, and combined/repeated attribute lists in this phase.

## Runtime Semantics

For every admitted `SIGNED` form, MyLite must store the same logical descriptor
as the corresponding bare signed type:

| SQL input | Logical descriptor | Rendered type |
| --- | --- | --- |
| `TINYINT SIGNED` | `TINYINT` | `tinyint` |
| `SMALLINT SIGNED` | `SMALLINT` | `smallint` |
| `MEDIUMINT SIGNED` | `MEDIUMINT` | `mediumint` |
| `INT SIGNED` | `INT` | `int` |
| `INTEGER SIGNED` | `INT` | `int` |
| `BIGINT SIGNED` | `BIGINT` | `bigint` |

Assignment range checks, existing-row validation, nullable handling, predicate
conversion, ordering, `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and
`SHOW CREATE TABLE` all follow the existing signed descriptor behavior.

`ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` treat `SIGNED` as part
of the target type syntax, but not as a distinct descriptor attribute. Changing
`INT` to `INT SIGNED`, for example, is a same-definition no-op under the current
descriptor model.

## Diagnostics

Supported `SIGNED` forms produce no warnings. Out-of-range assignments and
existing-row validation failures use the same diagnostics as bare signed
integer descriptors.

Unsupported syntax should fail deterministically through the existing parser or
unsupported-feature diagnostics. This includes:

- `type_name SIGNED UNSIGNED`;
- `type_name UNSIGNED SIGNED`;
- repeated `SIGNED`;
- display width plus `SIGNED`;
- `ZEROFILL`;
- unsupported type aliases or non-integer numeric types.

## MySQL Runtime Evidence

`packages/libmylite/tests/mysql_baseline_integer_signed_attribute_expectations.sh`
captures the MySQL 8.4.9 evidence for this feature. The probes verify:

- accepted `SIGNED` forms for all currently supported integer families;
- `SHOW COLUMNS` and `SHOW CREATE TABLE` normalize `SIGNED` away;
- in-range signed lower-bound insertion and signed out-of-range diagnostics;
- `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` with `SIGNED`;
- MySQL accepts combined/repeated attribute forms that MyLite intentionally
  defers in this baseline.

## Test Plan

Implementation tests must cover:

- parser acceptance for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
  `CHANGE` with `SIGNED`;
- parser rejection for display width, `ZEROFILL`, combined/repeated attribute
  lists, aliases, and unsupported non-integer types;
- runtime descriptor logical type text and physical `INTEGER` storage;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering;
- successful `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` using signed
  descriptor ranges;
- out-of-range assignment diagnostics;
- `SELECT`, `DELETE`, and `UPDATE` predicates over columns declared with
  `SIGNED`;
- `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` behavior, including no-op
  descriptor equality where applicable and existing-row validation on narrowing;
- reopen persistence and `.mylite` preamble preservation through existing
  file-backed runtime tests where new runtime coverage is added.

## Compatibility Documentation

Compatibility docs should state that MyLite supports an explicit single
`SIGNED` attribute only for the limited integer-family column definitions. They
must not claim support for display widths, `ZEROFILL`, `SIGNED` with
`UNSIGNED`, repeated attributes, unsupported numeric types, casts, aliases,
metadata flags, or expression semantics.
