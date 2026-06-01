# Baseline Functional and Multi-Valued Index Diagnostics

## Summary

This phase moves MyLite's baseline coverage for MySQL functional key parts and
multi-valued indexes from parse rejection to explicit, documented limited
support:

```sql
CREATE TABLE t (a INT, b INT, KEY k ((a + b)))
ALTER TABLE t ADD INDEX k ((a + b) DESC)
CREATE INDEX k ON t ((a + b))
CREATE TABLE t (j JSON, KEY mv ((CAST(j->'$.ids' AS UNSIGNED ARRAY))))
```

MySQL implements these as real secondary indexes over expression values or JSON
array elements. MyLite does not yet have expression-key-part descriptors,
generated hidden virtual columns, JSON array element index storage, or optimizer
support. The baseline behavior is therefore parser admission followed by a
deterministic unsupported-feature diagnostic before any catalog or SQLite
mutation.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing index lifecycle specifications:
  `docs/specs/baseline-secondary-index-lifecycle/specs.md`,
  `docs/specs/baseline-create-index-lifecycle/specs.md`,
  `docs/specs/baseline-alter-table-add-index-lifecycle/specs.md`,
  `docs/specs/baseline-descending-index-key-parts/specs.md`,
  `docs/specs/baseline-index-options-metadata/specs.md`
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, JSON data type:
  <https://dev.mysql.com/doc/refman/8.4/en/json.html>
- Observed MySQL 8.4.9 behavior recorded by
  `packages/libmylite/tests/mysql_baseline_functional_multivalued_index_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- `CREATE TABLE`, `ALTER TABLE ... ADD INDEX`, and standalone `CREATE INDEX`
  accept functional key parts written as parenthesized expressions.
- Functional key parts may appear in composite indexes and may mix with ordinary
  column key parts.
- `ASC` and `DESC` are accepted for functional key parts.
- `SHOW CREATE TABLE` renders functional key parts with nested parentheses, and
  `INFORMATION_SCHEMA.STATISTICS` reports `COLUMN_NAME = NULL` plus the
  expression text.
- A functional key part consisting only of a column name fails with
  `3762 / HY000`.
- A primary key cannot be a functional index and fails with `3756 / HY000`.
- Multi-valued indexes are functional indexes over `CAST(json_expression AS
  type ARRAY)` and are accepted in `CREATE TABLE`, `ALTER TABLE ... ADD INDEX`,
  and standalone `CREATE INDEX`.
- `INFORMATION_SCHEMA.STATISTICS` reports multi-valued key parts as expression
  rows with `COLUMN_NAME = NULL`.
- Explicit `ASC` or `DESC` on a multi-valued key part fails with
  `1221 / HY000`.
- Only one multi-valued key part is allowed per index; two multi-valued key
  parts fail with `1235 / 42000`.
- Direct ordinary indexes on a JSON column remain a separate rejected surface
  with error `3152 / 42000`.

## Scope

Supported:

- parser admission for table-level secondary, unique, fulltext, and spatial key
  part lists using functional syntax `((expression))`;
- parser admission for the same key-part lists using the common multi-valued
  shape `((CAST(json_expression AS UNSIGNED ARRAY)))`, including JSON path
  extraction expressions already admitted by MyLite;
- parser admission for `ALTER TABLE ... ADD [UNIQUE] INDEX`, `ALTER TABLE ...
  ADD [FULLTEXT|SPATIAL] INDEX`, and standalone `CREATE [UNIQUE|FULLTEXT|SPATIAL]
  INDEX` entry points that share secondary key-part lists;
- optional `ASC` or `DESC` after admitted functional key parts;
- explicit unsupported diagnostics before name generation, catalog descriptor
  mutation, generated SQLite DDL, or physical index creation;
- preservation of existing ordinary column, prefix, direction, option,
  fulltext, spatial, direct-JSON, and duplicate-name behavior.

Deferred:

- functional key-part descriptor storage;
- hidden virtual column metadata used by MySQL to implement functional indexes;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and `INFORMATION_SCHEMA.STATISTICS`
  metadata for persisted functional or multi-valued indexes;
- expression validation rules for deterministic functions, column-only
  functional expressions, disallowed return types, casts, collations, prefix
  references, foreign keys, and primary keys;
- multi-valued index storage, JSON array element extraction, uniqueness
  semantics, optimizer use for `MEMBER OF()`, `JSON_CONTAINS()`, or
  `JSON_OVERLAPS()`;
- named windows, generated columns, loaded zoneinfo, and any SQLite fork hook.

## Ownership Boundaries

- Public API: no new public ABI.
- Lexer/parser/AST: owns syntax admission and marks expression key parts as
  non-column key parts without consulting descriptors or SQLite.
- Analyzer/planner/runtime: detects non-column key parts and reports the
  explicit unsupported diagnostic before descriptor mutation.
- Catalog: unchanged. Existing index descriptors remain column-key-part based.
- SQLite integration: unchanged. No SQLite fork patch is required for this
  baseline diagnostic slice.

## Grammar

MyLite Lemon-syntax sketch:

```lemon
secondary_index_part ::= identifier index_key_direction_opt.
secondary_index_part ::= identifier LPAREN INTEGER RPAREN index_key_direction_opt.
secondary_index_part ::= functional_index_part.
secondary_index_part ::= multi_valued_index_part.

primary_key_part ::= identifier index_key_direction_opt.
primary_key_part ::= functional_index_part.
primary_key_part ::= multi_valued_index_part.

functional_index_part ::=
    LPAREN expression RPAREN index_key_direction_opt.

multi_valued_index_part ::=
    LPAREN CAST LPAREN expression AS cast_basic_target ARRAY RPAREN RPAREN
    index_key_direction_opt.
```

The `multi_valued_index_part` production is intentionally narrow. It admits the
common MySQL JSON-array index syntax and lets MyLite return a precise diagnostic
without broadly expanding cast target grammar or key-part descriptor semantics.

## Runtime Behavior

When any admitted functional key part is encountered in a supported index DDL
path, MyLite returns:

- code: `1064`
- SQLSTATE: `42000`
- message contains: `Functional key parts are not yet supported`

When any admitted multi-valued key part is encountered in a supported index DDL
path, MyLite returns:

- code: `1064`
- SQLSTATE: `42000`
- message contains: `Multi-valued indexes are not yet supported`

These errors are reported before index-name generation from key parts. For
example, `ALTER TABLE t ADD INDEX ((a + b))` must fail with the same functional
diagnostic rather than deriving an invalid omitted index name.

If the syntax remains outside the admitted baseline, MyLite may still return
the existing generic parser diagnostic. Direct JSON-column key parts continue to
use the existing MySQL-compatible `3152 / 42000` diagnostic.

## Tests

MySQL-runtime expectations cover:

- successful MySQL functional index creation and metadata expression rows;
- successful MySQL multi-valued index creation and expression metadata;
- functional-column-only, functional-primary-key, multi-valued explicit order,
  and two-multi-valued-part diagnostics.

MyLite runtime tests cover:

- table-level functional and multi-valued key parts fail with the new
  deterministic diagnostics;
- `ALTER TABLE ... ADD INDEX` and standalone `CREATE INDEX` fail before catalog
  mutation;
- primary-key functional syntax is admitted and fails before mutation;
- existing direct JSON index diagnostics remain unchanged.
