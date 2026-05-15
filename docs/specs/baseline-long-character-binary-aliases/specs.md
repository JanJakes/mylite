# Baseline LONG Character Binary Aliases

## Status

This feature specifies a narrow vendor-type alias slice for persistent MyLite
base tables. It admits MySQL's legacy `LONG`, `LONG VARCHAR`, and
`LONG VARBINARY` column type spellings and normalizes them to existing MyLite
`MEDIUMTEXT` and `MEDIUMBLOB` descriptor paths.

The feature intentionally does not add new storage semantics. The aliases are
discarded at planning time, so catalog descriptors, row conversion, metadata,
introspection, and physical SQLite storage remain owned by the existing
`MEDIUMTEXT` and `MEDIUMBLOB` implementations.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline `TEXT` type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline binary string types:
  `docs/specs/baseline-binary-string-types/specs.md`
- Baseline table charset and collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- Baseline column charset and collation attributes:
  `docs/specs/baseline-column-charset-collation-attributes/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, string data types:
  https://dev.mysql.com/doc/refman/8.4/en/string-types.html
- MySQL 8.4 Reference Manual, using data types from other database engines:
  https://dev.mysql.com/doc/refman/8.4/en/other-vendor-data-types.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_long_character_binary_aliases_expectations.sh`
records the runtime probes for this feature. Observed behavior that shapes this
slice:

- MySQL maps `LONG` and `LONG VARCHAR` to `MEDIUMTEXT` at table creation time.
- MySQL maps `LONG VARBINARY` to `MEDIUMBLOB` at table creation time.
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA.COLUMNS` report
  the normalized `mediumtext` / `mediumblob` shape; the input alias spelling is
  not retained.
- `LONG` and `LONG VARCHAR` accept column-level character set and collation
  attributes exactly like the normalized `MEDIUMTEXT` target. `LONG VARBINARY`
  remains a binary string descriptor with SQL `NULL` charset and collation
  metadata.
- `LONG`, `LONG VARCHAR`, and `LONG VARBINARY` values use the existing
  normalized target behavior for `INSERT`, `UPDATE`, `SELECT`, `NULL`,
  `NOT NULL`, `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`,
  descriptor copying, and persistence.
- `ALTER TABLE ... ADD COLUMN` accepts the aliases and renders the normalized
  type in later introspection.
- `LONG VARCHAR(10)` and `LONG VARBINARY(10)` are syntax errors. The aliases do
  not accept a length argument.
- `LONG BINARY` is accepted by MySQL as `MEDIUMTEXT` with a binary collation.
  MyLite defers that spelling because it belongs to the broader `BINARY`
  character-column attribute surface rather than the binary-string alias.
- Literal defaults on normalized `MEDIUMTEXT` and `MEDIUMBLOB` columns are
  rejected by MySQL. Expression defaults are accepted upstream but remain
  deferred in MyLite because the current `TEXT` and BLOB-family default slices
  defer large-object expression defaults.

## Scope

The implementation must add:

- parser support for `LONG` as a column type alias for the existing
  `MEDIUMTEXT` AST and descriptor path;
- parser support for `LONG VARCHAR` as a column type alias for `MEDIUMTEXT`;
- parser support for `LONG VARBINARY` as a column type alias for
  `MEDIUMBLOB`;
- use of the aliases anywhere the current grammar accepts compatible column
  types, including `CREATE TABLE` and `ALTER TABLE ... ADD [COLUMN]`;
- normalized descriptor storage, physical storage, metadata, readback,
  defaults policy, DML assignment, descriptor copying, and diagnostics through
  the existing `MEDIUMTEXT` / `MEDIUMBLOB` code paths;
- column `CHARACTER SET` / `CHARSET` / `COLLATE` support for `LONG` and
  `LONG VARCHAR` through the existing `TEXT` family attribute path;
- parser tests for alias AST shape and rejected length forms;
- fast runtime tests for create/add-column metadata, DML, clone/copy,
  persistence, and deterministic unsupported forms;
- MySQL 8.4.9 expectation coverage for accepted aliases, normalized metadata,
  DML behavior, and deferred wider syntax.

## Non-Goals

This feature must not implement:

- `LONG BINARY`, `LONG TEXT`, `LONG VARCHAR(length)`,
  `LONG VARBINARY(length)`, signed or expression alias lengths, or other
  vendor aliases;
- new `TEXT` or BLOB-family value semantics beyond the existing normalized
  target paths;
- large-object literal defaults or expression defaults;
- binary-string charset/collation attributes, `CHARACTER SET binary` rewrites,
  or character-column `BINARY` collation attributes;
- new comparisons, ordering, grouping, distinct, functions, casts, parameters,
  or expression evaluation;
- new public API, catalog columns, physical storage formats, indexes, or SQLite
  fork patches.

## Ownership Boundary

- Public API behavior is unchanged. `mylite_execute()` continues to own call
  validation, result ownership, diagnostics exposure, and cleanup.
- Statement context behavior is unchanged. Supported in-range alias operations
  use existing statement result, warning, and affected-row reporting for the
  normalized target type.
- Lexer/parser/AST own alias syntax admission. They normalize aliases to the
  existing `MEDIUMTEXT` or `MEDIUMBLOB` AST node shape so later layers do not
  need alias-specific branching.
- Analyzer/planner code remains descriptor-driven. Alias AST nodes are mapped
  by the existing `TEXT` and binary-string descriptor builders and validators.
- Catalog descriptors remain authoritative and store normalized logical type
  text such as `MEDIUMTEXT` or `MEDIUMBLOB`. Alias spelling is not persisted.
- Result and introspection builders render normalized descriptor metadata. They
  do not inspect SQLite schema text for type names.
- SQLite owns physical `TEXT` and `BLOB` row storage for normalized
  descriptors. MyLite continues to bind values through prepared statements and
  does not add a SQLite fork hook for aliases.
- Storage/VFS behavior is unchanged. Alias DDL writes only descriptor rows and
  generated SQLite objects inside the shifted SQLite payload.

## Supported SQL Grammar

The feature extends the existing limited column-type grammar:

```sql
column_type:
    existing_integer_or_decimal_or_temporal_or_string_or_binary_type
  | long_alias_type

long_alias_type:
    LONG
  | LONG VARCHAR
  | LONG VARBINARY
```

Equivalent MyLite Lemon-syntax snippets:

```lemon
text_type_name(A) ::= LONG(T). {
    A = text_type_tokens(T, MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT);
}

text_type_name(A) ::= LONG(T) VARCHAR(V). {
    A = text_type_tokens(span_join(T, V), MYLITE_SQL_AST_TEXT_TYPE_MEDIUMTEXT);
}

binary_string_type_name(A) ::= LONG(T) VARBINARY(V). {
    A = binary_type_tokens(span_join(T, V), MYLITE_SQL_AST_BINARY_STRING_TYPE_MEDIUMBLOB);
}
```

These snippets are descriptive. The generated parser must remain
independently authored and integrated with MyLite's actual Lemon helper
structures.

The aliases do not add value grammar. All row DML values are the existing
`MEDIUMTEXT` string/`NULL`/`DEFAULT` subset or `MEDIUMBLOB`
string/hex/`NULL`/`DEFAULT` subset.

## Semantics

### Normalization

`LONG` and `LONG VARCHAR` normalize to the existing `MEDIUMTEXT` descriptor:

- logical catalog type: `MEDIUMTEXT`;
- physical SQLite type: `TEXT`;
- metadata type text: `mediumtext`;
- character maximum and octet length: `16777215`;
- default effective character set and collation through the current `TEXT`
  family rules.

`LONG VARBINARY` normalizes to the existing `MEDIUMBLOB` descriptor:

- logical catalog type: `MEDIUMBLOB`;
- physical SQLite type: `BLOB`;
- metadata type text: `mediumblob`;
- character maximum and octet length: `16777215`;
- character set and collation metadata: SQL `NULL`.

The original alias spelling is not stored in the catalog and is not returned by
`SHOW CREATE TABLE`, `SHOW COLUMNS`, or `INFORMATION_SCHEMA.COLUMNS`.

### Defaults and Nullability

Alias columns use the existing normalized target rules:

- nullable columns with no explicit default report SQL `NULL` in `SHOW COLUMNS`;
- `SHOW CREATE TABLE` omits visible `DEFAULT NULL` clauses for normalized
  `MEDIUMTEXT` and `MEDIUMBLOB` columns;
- `NOT NULL` no-default columns use existing implicit empty-string or
  empty-byte behavior in supported row-value and add-column paths;
- large-object literal and expression defaults remain deferred by MyLite even
  when MySQL accepts expression defaults.

### Row Values and DML

All DML behavior is inherited from the normalized target descriptor:

- `LONG` and `LONG VARCHAR` values validate as UTF-8 non-`NUL` text and
  preserve ordinary trailing spaces;
- `LONG VARBINARY` values validate as bytes, accept admitted string and hex
  literals, and preserve embedded `NUL` bytes through the public byte result
  accessors;
- `INSERT`, `REPLACE`, `UPDATE`, `SELECT`, `WHERE IS NULL`,
  `WHERE IS NOT NULL`, clone/copy, and persistence reuse existing
  descriptor-driven behavior.

### Diagnostics

The feature uses existing diagnostics where possible:

- malformed alias syntax, length arguments, `LONG TEXT`, and deferred
  `LONG BINARY` produce deterministic parse or unsupported diagnostics;
- explicit large-object literal/default-expression attempts use existing
  normalized target diagnostics;
- invalid string/binary assignment, embedded `NUL` in text, overlength values,
  `NULL` into `NOT NULL`, unknown tables/columns, physical SQLite failures,
  allocation failures, and public API misuse use existing normalized target
  diagnostics.

## Physical SQLite Handling

No SQLite parser, planner, or storage changes are required. The aliases are
normalized before physical SQL is generated:

- `LONG` and `LONG VARCHAR` produce the same generated SQLite column shape as
  `MEDIUMTEXT`;
- `LONG VARBINARY` produces the same generated SQLite column shape as
  `MEDIUMBLOB`;
- generated identifiers continue to be descriptor-driven, quoted, and backed
  by stable physical table names;
- row values continue to use prepared statements and length-aware bindings.

## Tests

Add a focused runtime test, preferably
`packages/libmylite/tests/runtime_long_character_binary_aliases_test.c`,
covering:

- `CREATE TABLE` with `LONG`, `LONG VARCHAR`, and `LONG VARBINARY`;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `INFORMATION_SCHEMA.COLUMNS`, and result
  metadata normalized to `mediumtext` / `mediumblob`;
- `INSERT`, `UPDATE`, `NULL`, `NOT NULL`, affected rows, warning counts, and
  byte-safe `LONG VARBINARY` readback through the normalized target paths;
- `ALTER TABLE ... ADD [COLUMN]` with admitted aliases;
- `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, and reopen persistence;
- deterministic rejection of `LONG VARCHAR(length)`, `LONG VARBINARY(length)`,
  `LONG TEXT`, `LONG BINARY`, literal defaults, expression defaults, and
  unsupported binary-string charset/collation attributes;
- no `.mylite` preamble mutation.

Run the MySQL expectation script, targeted parser/runtime CTests, and
`cmake --workflow --preset check` before marking the feature complete.
