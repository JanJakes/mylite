# Baseline Integer Type Aliases

## Status

This feature specifies a narrow parser and descriptor-mapping slice for MySQL
integer-family aliases observed in MySQL 8.4.9: `INT1`, `INT2`, `INT3`,
`INT4`, and `INT8`. These aliases normalize to the integer families MyLite
already supports:

| Alias | Normalized family |
| --- | --- |
| `INT1` | `TINYINT` |
| `INT2` | `SMALLINT` |
| `INT3` | `MEDIUMINT` |
| `INT4` | `INT` |
| `INT8` | `BIGINT` |

The feature intentionally does not add `BOOL` / `BOOLEAN`, `SERIAL`, display
widths, `ZEROFILL`, auto-increment behavior, expression casts, compact storage,
or protocol-grade metadata.

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

- parser support for `INT1`, `INT2`, `INT3`, `INT4`, and `INT8` in every
  current column-definition position;
- optional single `SIGNED` or `UNSIGNED` support after each alias, matching the
  current integer-family attribute grammar;
- AST representation that normalizes each alias to the existing integer-family
  payload and `is_unsigned` flag;
- descriptor-driven DDL, DML, predicate, ordering, and introspection behavior
  identical to the normalized target family;
- support in `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`,
  `ALTER TABLE ... MODIFY [COLUMN]`, and
  `ALTER TABLE ... CHANGE [COLUMN]`;
- MySQL 8.4.9 expectation coverage for accepted alias forms, rendered type
  text, range checks, alter behavior, and explicitly deferred accepted MySQL
  syntax.

## Non-Goals

This feature must not implement:

- `BOOL` or `BOOLEAN`, which normalize to `TINYINT(1)` and therefore require
  display-width-aware metadata;
- `SERIAL`, which expands to unsigned, not-null, auto-increment, and unique-key
  behavior that MyLite does not support yet;
- display width syntax such as `INT1(1)`;
- `ZEROFILL`, including its unsigned implication and metadata rendering;
- combined or repeated `SIGNED` / `UNSIGNED` attribute lists;
- aliases for non-integer types;
- expression type names, casts, parameters, function arguments, or standalone
  identifier behavior beyond the existing lexer policy;
- protocol metadata changes, compact storage, or SQLite physical storage
  changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own result
  allocation, diagnostics exposure, and public misuse behavior.
- Lexer/parser/AST own syntax admission for the alias names. They do not
  consult descriptors, storage, or SQLite.
- Analyzer/planner maps aliases to the same normalized logical descriptors used
  for their target integer families.
- Catalog descriptors remain authoritative. The original alias spelling is not
  stored because MySQL 8.4.9 renders the normalized family in the admitted
  subset.
- Result and introspection builders continue to render descriptor logical types
  to lower-case MySQL-style normalized type text.
- SQLite remains the physical row storage and execution engine. This feature
  requires no SQLite fork patch, extension point, or file-format change.

## Supported SQL Grammar

This feature expands the existing `integer_type` nonterminal used by supported
column definitions. It admits each alias as a type name, optionally followed by
one `SIGNED` or one `UNSIGNED`:

```sql
INT1
INT1 SIGNED
INT1 UNSIGNED
INT2
INT2 SIGNED
INT2 UNSIGNED
INT3
INT3 SIGNED
INT3 UNSIGNED
INT4
INT4 SIGNED
INT4 UNSIGNED
INT8
INT8 SIGNED
INT8 UNSIGNED
```

The existing canonical integer-family forms remain unchanged.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
integer_type ::= INT1.
integer_type ::= INT2.
integer_type ::= INT3.
integer_type ::= INT4.
integer_type ::= INT8.

integer_type ::= INT1 SIGNED.
integer_type ::= INT2 SIGNED.
integer_type ::= INT3 SIGNED.
integer_type ::= INT4 SIGNED.
integer_type ::= INT8 SIGNED.

integer_type ::= INT1 UNSIGNED.
integer_type ::= INT2 UNSIGNED.
integer_type ::= INT3 UNSIGNED.
integer_type ::= INT4 UNSIGNED.
integer_type ::= INT8 UNSIGNED.
```

Alias productions map directly to the existing AST integer-family enum values:
`INT1` to `TINYINT`, `INT2` to `SMALLINT`, `INT3` to `MEDIUMINT`, `INT4` to
`INT`, and `INT8` to `BIGINT`. `SIGNED` sets `is_unsigned == 0`; `UNSIGNED`
sets `is_unsigned == 1`. The source span should include the alias and attribute
when an attribute is present.

The parser must continue to reject display width, `ZEROFILL`, `BOOL`,
`BOOLEAN`, `SERIAL`, unsupported aliases, and combined/repeated attribute lists
in this phase.

## Runtime Semantics

For every admitted alias form, MyLite must store the normalized logical
descriptor:

| SQL input | Logical descriptor | Rendered type |
| --- | --- | --- |
| `INT1` / `INT1 SIGNED` | `TINYINT` | `tinyint` |
| `INT1 UNSIGNED` | `TINYINT UNSIGNED` | `tinyint unsigned` |
| `INT2` / `INT2 SIGNED` | `SMALLINT` | `smallint` |
| `INT2 UNSIGNED` | `SMALLINT UNSIGNED` | `smallint unsigned` |
| `INT3` / `INT3 SIGNED` | `MEDIUMINT` | `mediumint` |
| `INT3 UNSIGNED` | `MEDIUMINT UNSIGNED` | `mediumint unsigned` |
| `INT4` / `INT4 SIGNED` | `INT` | `int` |
| `INT4 UNSIGNED` | `INT UNSIGNED` | `int unsigned` |
| `INT8` / `INT8 SIGNED` | `BIGINT` | `bigint` |
| `INT8 UNSIGNED` | `BIGINT UNSIGNED` | `bigint unsigned` |

Assignment range checks, existing-row validation, nullable handling, predicate
conversion, ordering, `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and
`SHOW CREATE TABLE` all follow the normalized descriptor behavior.

`ALTER TABLE ... MODIFY [COLUMN]` and `CHANGE [COLUMN]` treat alias names as
target type syntax, but not as distinct descriptor attributes. Changing
`TINYINT` to `INT1`, for example, is a same-definition no-op under the current
descriptor model.

## Diagnostics

Supported alias forms produce no warnings. Out-of-range assignments and
existing-row validation failures use the same diagnostics as the normalized
integer descriptors.

Unsupported syntax should fail deterministically through the existing parser or
unsupported-feature diagnostics. This includes:

- `alias_name SIGNED UNSIGNED`;
- `alias_name UNSIGNED SIGNED`;
- repeated `SIGNED` or `UNSIGNED`;
- display width plus an alias;
- `ZEROFILL`;
- `BOOL`, `BOOLEAN`, `SERIAL`, and unsupported non-integer type aliases.

## MySQL Runtime Evidence

`packages/libmylite/tests/mysql_baseline_integer_type_aliases_expectations.sh`
captures the MySQL 8.4.9 evidence for this feature. The probes verify:

- accepted `INT1`, `INT2`, `INT3`, `INT4`, and `INT8` forms;
- accepted `SIGNED` and `UNSIGNED` attributes after those aliases;
- `SHOW COLUMNS` and `SHOW CREATE TABLE` normalize aliases to canonical type
  text;
- in-range insertion and out-of-range diagnostics for normalized families;
- `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` with aliases;
- MySQL accepts deferred alias forms that MyLite intentionally does not support
  in this baseline.

## Test Plan

Implementation tests must cover:

- parser acceptance for `CREATE TABLE`, `ALTER TABLE ADD`, `MODIFY`, and
  `CHANGE` with aliases;
- parser acceptance for optional single `SIGNED` and `UNSIGNED` after aliases;
- parser rejection for display width, `ZEROFILL`, combined/repeated attribute
  lists, `BOOL`, `BOOLEAN`, `SERIAL`, and unsupported type names;
- runtime descriptor logical type text and physical `INTEGER` storage;
- `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, and `SHOW CREATE TABLE`
  rendering;
- successful `INSERT ... VALUES`, `INSERT ... SET`, and `UPDATE` using
  normalized descriptor ranges;
- out-of-range assignment diagnostics;
- `SELECT`, `DELETE`, and `UPDATE` predicates over alias-declared columns;
- `ALTER TABLE ... ADD`, `MODIFY`, and `CHANGE` behavior, including no-op
  descriptor equality where applicable and existing-row validation on
  narrowing;
- reopen persistence and `.mylite` preamble preservation through existing
  file-backed runtime tests where new runtime coverage is added.

## Compatibility Documentation

Compatibility docs should state that MyLite supports only the limited
`INT1`/`INT2`/`INT3`/`INT4`/`INT8` aliases with optional single `SIGNED` or
`UNSIGNED` in integer-family column definitions. They must not claim support
for `BOOL`, `BOOLEAN`, `SERIAL`, display widths, `ZEROFILL`, combined/repeated
attributes, unsupported aliases, casts, metadata flags, or expression
semantics.
