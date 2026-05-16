# Baseline Key-Aware ALTER CHANGE/MODIFY

## Status

This feature extends the existing descriptor-driven
`ALTER TABLE ... CHANGE [COLUMN]` and `ALTER TABLE ... MODIFY [COLUMN]`
single-action lifecycle. It removes the broad primary-key and secondary-index
table rejection for the current replacement subset when the existing key
descriptors remain valid after the replacement.

The goal is narrow and practical for WordPress-style schema maintenance:
single-column replacements on keyed persistent base tables should preserve
MyLite-owned key metadata, physical indexes, row values, column ids, table ids,
and stable physical table names. This is not full MySQL `ALTER TABLE`; it does
not add multi-action planning, foreign-key-aware column replacement, CHECK
revalidation, prefix-length rewriting, generated columns, invisible columns, or
algorithm/lock clauses.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing `CHANGE COLUMN`, `MODIFY COLUMN`, and temporal positioning specs:
  `docs/specs/baseline-alter-table-change-column/specs.md`,
  `docs/specs/baseline-alter-table-modify-column/specs.md`, and
  `docs/specs/baseline-alter-change-modify-temporal-positioning/specs.md`
- Baseline key and index lifecycle specs under `docs/specs/`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `CREATE INDEX`:
  https://dev.mysql.com/doc/refman/8.4/en/create-index.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_key_aware_alter_change_modify_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase.

- `ALTER TABLE keyed MODIFY COLUMN non_key BIGINT` on a table with a primary
  key and secondary indexes succeeds, preserves keys, and reports the copied
  row count for the verified integer-family rebuild.
- `ALTER TABLE keyed CHANGE COLUMN indexed_name new_name VARCHAR(n) NOT NULL`
  with an otherwise identical descriptor succeeds, updates key metadata to the
  new column name, preserves rows, and reports `ROW_COUNT() = 0`.
- `ALTER TABLE pk MODIFY COLUMN id BIGINT` where `id` is the primary key
  succeeds, keeps the column `NOT NULL`, preserves the primary key, and reports
  the copied row count for the verified integer-family rebuild.
- `ALTER TABLE pk CHANGE COLUMN id id BIGINT` where `id` is the primary key
  follows the same omitted-nullability rule: the primary-key part remains
  `NOT NULL`, the key descriptor is preserved, and the verified integer-family
  rebuild reports the copied row count.
- `ALTER TABLE pk MODIFY COLUMN id BIGINT NULL` fails with MySQL error
  `1171 / 42000`, because primary-key parts cannot be nullable.
- Shrinking `VARCHAR(20)` to `VARCHAR(8)` under `KEY k_idx (k(10))` succeeds in
  MySQL and renders the key as the full column key. MyLite does not rewrite
  stored prefix descriptors in this phase; it rejects key-preserving replacement
  when an existing prefix would no longer be valid.
- Metadata-only `FULLTEXT` descriptors survive compatible character-column
  replacement in MySQL. MyLite preserves metadata-only full-text descriptors
  only when their existing key parts remain valid for the replacement
  descriptor.

## Scope

Supported:

- persistent MyLite base tables only;
- one existing `CHANGE [COLUMN]` or `MODIFY [COLUMN]` action only;
- the current admitted replacement types, defaults, nullability, charset,
  collation, temporal attributes, and optional `FIRST` / `AFTER` positioning
  already supported by the existing `CHANGE`/`MODIFY` lifecycle;
- target tables with existing primary keys, secondary indexes, unique indexes,
  prefix secondary indexes, and metadata-only full-text indexes when every
  existing key descriptor remains valid after applying the replacement column
  descriptor;
- descriptor-driven key validation for the replacement column and for every
  existing key part;
- primary-key nullability handling verified against MySQL: an omitted
  nullability attribute on a changed or modified primary-key part remains
  `NOT NULL`, and an explicit `NULL` attribute is rejected with the
  primary-key-null diagnostic;
- physical table rebuilds that recreate non-fulltext physical indexes after
  the stable table physical name is restored;
- metadata-only replacements and pure renames that continue to use the
  existing physical rename path and let SQLite update physical index SQL;
- no-row DDL result objects, zero warnings for successful in-range operations,
  and the affected-row behavior already defined by the current
  `CHANGE`/`MODIFY` subset.

Deferred:

- multiple alter actions or combined `CHANGE`/`MODIFY` plus key operations;
- indexed-table replacement that would require MyLite to rewrite stored prefix
  lengths, key part order, key expressions, or any other key descriptor;
- foreign-key child or parent tables;
- CHECK-constrained tables;
- direct modification of generated, invisible, auto-increment, or otherwise
  unsupported column attributes beyond the existing `CHANGE`/`MODIFY` surface;
- spatial indexes, full-text search behavior, expression indexes, functional
  indexes, invisible indexes, descending physical-key semantics beyond existing
  metadata, and online DDL algorithm or lock semantics.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse validation,
  result ownership, and failure cleanup.
- Statement context: owns diagnostics, warnings, affected rows, and statement
  boundary state.
- Lexer/parser/AST: unchanged by this feature. The already admitted
  `CHANGE`/`MODIFY` grammar remains the syntax authority and does not inspect
  catalog or storage state.
- Analyzer/planner: resolves the target table, replacement column, optional
  position, existing columns, existing key descriptors, and key-part
  compatibility from MyLite descriptors before generated SQLite SQL exists.
- Catalog: remains authoritative for table, column, primary-key, secondary-key,
  unique-key, prefix-key, and full-text metadata. This feature updates only the
  replacement column descriptor and optional column ordinals; existing index
  catalog rows and index-column catalog rows keep their ids and key metadata.
- Result and introspection builders: continue to render `SHOW COLUMNS`,
  `SHOW INDEX`, `SHOW CREATE TABLE`, and `INFORMATION_SCHEMA` rows from
  descriptors, not SQLite schema text.
- SQLite physical storage: stores durable rows and physical b-tree indexes.
  MyLite rebuilds tables through descriptor-built SQL, restores the stable
  physical table name, then recreates non-fulltext physical indexes from
  descriptor metadata. SQLite schema text is not MySQL-visible metadata
  authority.
- Storage/VFS: unchanged. The `.mylite` preamble remains untouched; generated
  SQLite writes occur only in the shifted SQLite payload.

## Grammar

This feature does not add new syntax. It relies on the existing independently
authored MyLite grammar for single-action `CHANGE` and `MODIFY`:

```lemon
alter_table_modify_column_statement ::=
    ALTER TABLE table_name MODIFY column_keyword_opt column_definition column_position_opt.

alter_table_change_column_statement ::=
    ALTER TABLE table_name CHANGE column_keyword_opt identifier column_definition column_position_opt.

column_position_opt ::= .
column_position_opt ::= FIRST.
column_position_opt ::= AFTER identifier.
```

All wider MySQL `ALTER TABLE` syntax remains governed by the existing parser
and unsupported-syntax diagnostics.

## Resolution And Key Compatibility

Schema, table, old-column, replacement-column, duplicate-name, position, and
reserved-name resolution stay identical to the existing `CHANGE`/`MODIFY`
lifecycle.

After resolving the replacement descriptor, MyLite loads existing key
descriptors for the target table and evaluates them against the resulting
column descriptor list:

- primary-key parts must still reference visible descriptors supported by the
  current primary-key subset;
- primary-key parts must be `NOT NULL`;
- secondary and unique key parts must still satisfy the current full-column or
  prefix-key descriptor validation rules;
- existing prefix lengths must remain valid for the replacement descriptor;
- metadata-only full-text key parts must still reference descriptors admitted
  by the current full-text metadata subset;
- all key ids, key names, physical key names, uniqueness flags, part ordinals,
  sort-direction metadata, prefix metadata, and referenced column ids are
  preserved.

If the replacement is incompatible with an existing key descriptor, MyLite
returns a deterministic MySQL-style key diagnostic when one already exists,
such as `1171` for nullable primary-key parts, an incorrect-prefix-key error
for invalid stored prefixes, key-too-long diagnostics, JSON-key diagnostics, or
the existing MyLite-specific unsupported-column-type message for deferred key
families.

Foreign-key child or parent tables and CHECK-constrained tables remain
unsupported for this feature and are rejected before physical SQLite SQL is
generated.

## Physical SQLite Handling

Metadata-only replacements keep using the existing catalog mutation plus native
SQLite physical column rename when the column spelling changes.

Physical rebuilds use the existing table-copy shape:

1. validate existing stored values against the replacement descriptor;
2. begin the catalog mutation;
3. create a temporary physical rowid table from the resulting descriptor list;
4. copy rows from the old physical table in stable rowid order;
5. drop the old physical table;
6. rename the temporary physical table back to the stable physical table name;
7. recreate every non-fulltext physical index from MyLite key descriptors and
   resulting column names;
8. update the table identity row to advance descriptor generation;
9. commit the catalog mutation and increment the connection-local SQLite schema
   generation.

Generated SQLite SQL must use only descriptor data and stable MyLite-generated
physical names, quote every SQLite identifier, and never reconstruct logical
metadata from SQLite schema text. No SQLite fork patch is required.

## Diagnostics

Existing `CHANGE`/`MODIFY` diagnostics are preserved for syntax errors,
missing default schema, unknown schema, unknown table, reserved target names,
unsupported object kinds, unknown columns, duplicate replacement columns,
unsupported column definitions, invalid defaults, unsupported row values,
integer out-of-range values, temporal conversion errors, `NULL` into `NOT
NULL`, allocation failures, and physical SQLite failures.

This phase changes only the former primary-key and secondary-index table-level
unsupported errors. Keyed tables are admitted when all existing key descriptors
remain valid. The following remain deterministic failures:

- explicit `NULL` on a primary-key part: MySQL error `1171`, SQLSTATE `42000`;
- replacement that makes an existing prefix key invalid: incorrect prefix key
  diagnostic;
- replacement that makes a key too wide: key-too-long diagnostic;
- replacement that makes a key part an unsupported descriptor kind: the current
  key-specific unsupported diagnostic;
- table has child or parent foreign-key descriptors: unsupported or referenced
  table diagnostics before physical SQL;
- table has CHECK descriptors: existing CHECK-constrained replacement
  diagnostic before physical SQL.

## Compatibility Notes

MySQL can rewrite some key metadata during column replacement. In particular,
when a column shrinks below an existing prefix length, MySQL renders the key as
a full-column key. MyLite does not update existing key-column descriptors in
this phase. That narrower behavior is deliberate so this feature can preserve
catalog authority without adding descriptor-rewrite semantics in the same
slice.

This feature also does not change broader `CHANGE`/`MODIFY` affected-row
semantics for character metadata-only changes. Tests for this phase focus on
verified integer rebuild counts and pure indexed-column renames that already
match the current lifecycle behavior.

## Verification

Required verification:

- `packages/libmylite/tests/mysql_baseline_key_aware_alter_change_modify_expectations.sh`
- focused runtime tests for keyed `CHANGE`/`MODIFY`
- existing parser and ALTER lifecycle tests
- `cmake --build --preset dev`
- `cmake --workflow --preset check`
