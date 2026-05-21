# Baseline Spatial Index Type Options

## Summary

This phase fills one narrow gap in the existing spatial-index metadata
baseline: ordinary nonunique index definitions that name an index type while
their first key part is a spatial descriptor column.

```sql
CREATE TABLE t (g GEOMETRY NOT NULL, KEY g_key USING RTREE (g))
CREATE TABLE t (g GEOMETRY NOT NULL, KEY g_key (g) USING RTREE)
ALTER TABLE t ADD INDEX g_key USING RTREE (g)
ALTER TABLE t ADD INDEX g_key (g) USING RTREE
CREATE INDEX g_key USING RTREE ON t (g)
CREATE INDEX g_key ON t (g) USING RTREE
```

For supported persistent base tables, `USING RTREE` on an otherwise ordinary
nonunique one-column spatial index normalizes to the existing metadata-only
`SPATIAL` descriptor. Ordinary `USING BTREE` or `USING HASH` over a spatial
column is rejected with MySQL's spatial-index-type diagnostic. `USING RTREE`
over a non-spatial column is rejected like an attempted spatial index over a
non-geometric column. Ordinary unique RTREE forms remain unsupported as
spatial indexes, but this phase makes their diagnostics match MySQL's observed
spatial-index path.

This does not add physical R-tree storage, spatial search, broader `USING`
syntax, or explicit `SPATIAL ... USING ...` support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing spatial metadata specification:
  `docs/specs/baseline-spatial-index-metadata/specs.md`
- Existing index-option metadata specification:
  `docs/specs/baseline-index-options-metadata/specs.md`
- MySQL 8.4 Reference Manual, spatial data types:
  <https://dev.mysql.com/doc/refman/8.4/en/spatial-types.html>
- MySQL 8.4 Reference Manual, creating spatial indexes:
  <https://dev.mysql.com/doc/refman/8.4/en/creating-spatial-indexes.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- Observed MySQL 8.4.9 behavior added to
  `packages/libmylite/tests/mysql_baseline_spatial_index_metadata_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite source. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

Runtime probes establish:

- Ordinary nonunique table-level `KEY` / `INDEX` definitions over one
  `NOT NULL` `GEOMETRY`-family column accept leading or trailing
  `USING RTREE`. `SHOW CREATE TABLE` renders the result as `SPATIAL KEY`
  without a `USING RTREE` clause.
- Ordinary nonunique `ALTER TABLE ... ADD INDEX` and standalone
  `CREATE INDEX` over one `NOT NULL` `GEOMETRY`-family column accept leading
  or trailing `USING RTREE`. The stored metadata is spatial, emits warning
  `3674 / HY000` while SRID metadata is absent, and renders as `SPATIAL KEY`.
- Ordinary `USING BTREE` and `USING HASH` over a `GEOMETRY`-family column fail
  with `3729 / HY000` and message shape
  `The index type BTREE|HASH is not supported for spatial indexes.`
- Ordinary `USING RTREE` over a non-geometric column fails with
  `1687 / 42000`, `A SPATIAL index may only contain a geometrical type column`.
- Explicit `SPATIAL KEY ... USING RTREE`, `SPATIAL KEY ... USING BTREE`,
  `SPATIAL KEY ... USING HASH`, and standalone
  `CREATE SPATIAL INDEX ... USING RTREE` remain syntax errors in the observed
  admitted forms.

## Scope

Supported:

- persistent MyLite base tables through the existing table-level `CREATE TABLE`,
  single-action `ALTER TABLE ... ADD INDEX|KEY`, and standalone
  `CREATE INDEX` paths;
- ordinary nonunique one-column index definitions with leading or trailing
  `USING RTREE` over a supported `NOT NULL` spatial descriptor column;
- descriptor normalization to the existing metadata-only `SPATIAL` index kind;
- existing spatial diagnostics for nullable spatial columns, multi-part
  spatial indexes, explicit key-part direction, prefixes, duplicate names,
  missing columns, and unsupported object kinds;
- MySQL-compatible rejection of ordinary `USING BTREE` / `USING HASH` over
  spatial descriptor columns;
- MySQL-compatible rejection of ordinary `USING RTREE` over non-spatial
  descriptor columns;
- MySQL-compatible rejection of ordinary unique `USING RTREE`,
  `USING BTREE`, and `USING HASH` forms when they resolve through spatial-index
  semantics;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW INDEX`, and limited
  `INFORMATION_SCHEMA.STATISTICS` metadata through the existing spatial
  metadata path;
- existing no-result DDL result conventions, zero affected rows, and warning
  `3674` for each supported spatial index created without SRID metadata;
- persistence, independent handles, `CREATE TABLE ... LIKE`, rename/drop, and
  visibility behavior inherited from the existing spatial-index descriptor
  lifecycle.

Deferred:

- explicit `SPATIAL ... USING ...` syntax;
- physical support for unique, primary, fulltext, temporary, expression,
  table-qualified, multi-part, ordered, or prefix key-part definitions with
  `USING RTREE`;
- physical SQLite R-tree indexes or optimizer behavior;
- geometry value parsing, spatial functions, SRID attributes, spatial search,
  or `INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS`;
- broader index type names, parser plugins, `KEY_BLOCK_SIZE`, engine
  attributes, standalone `ALGORITHM` / `LOCK`, and other index options not
  already admitted by the surrounding index-option baseline.

## Ownership Boundaries

- Public API: no new ABI. Applications continue to use `mylite_execute()` and
  existing result and diagnostic accessors.
- Statement context: owns diagnostics reset, warning count, affected rows, and
  cleanup. The RTREE-normalized spatial path reuses the existing SRID warning.
- Lexer/parser/AST: already admits `USING identifier` where ordinary index
  type options are supported. This phase recognizes `RTREE` in analyzer code
  only for the ordinary-index contexts described here.
- Analyzer/planner/runtime: resolves tables, columns, option placement, index
  kind, and MySQL diagnostics from MyLite descriptors before any catalog or
  SQLite mutation.
- Catalog: logical descriptors remain authoritative. RTREE-normalized indexes
  are stored as existing spatial-index descriptors, not as ordinary secondary
  indexes with a stored RTREE option.
- Result builder/introspection: renders metadata only from descriptors. Because
  the descriptor kind is spatial, `SHOW CREATE TABLE` renders `SPATIAL KEY`
  and omits `USING RTREE`.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: no SQLite R-tree table or physical index is created.
  No SQLite fork patch or new extension point is required.

## Grammar

The existing ordinary index grammar is sufficient for this slice:

```lemon
secondary_index_definition ::=
    KEY index_name_opt index_type_opt
    LPAREN secondary_index_part_list RPAREN index_option_list_opt.

secondary_index_definition ::=
    INDEX index_name_opt index_type_opt
    LPAREN secondary_index_part_list RPAREN index_option_list_opt.

create_index_statement ::=
    CREATE INDEX identifier index_type_opt ON table_name
    LPAREN secondary_index_part_list RPAREN index_option_list_opt.

index_type_opt ::= .
index_type_opt ::= index_type_option.
index_type_option ::= USING identifier.

index_option ::= index_type_option.
```

`identifier` is semantically limited by analyzer code. For this phase:

- `BTREE` and `HASH` retain existing non-spatial behavior;
- `RTREE` is admitted only when the ordinary nonunique index resolves to a
  supported spatial index;
- unknown type names continue to use the existing syntax-error path;
- explicit spatial-index grammar remains unchanged and does not admit leading
  `index_type_opt`.

## Resolution and Semantics

Target table resolution, reserved-name checks, information-schema write
diagnostics, missing default schema handling, unknown schema diagnostics, and
unknown table diagnostics are inherited from the existing index-add and
standalone-create-index planners.

Column resolution is descriptor-driven:

- key parts must be unqualified descriptor column names;
- missing columns fail with the existing `1072 / 42000` key-column diagnostic;
- ordinary RTREE normalization examines the first resolved key part before
  catalog mutation;
- if the first part is a supported spatial descriptor and the index is
  ordinary nonunique, the effective kind becomes spatial;
- if the first part is not spatial and the requested type is RTREE, the
  statement fails with `1687 / 42000`;
- if the effective kind is spatial and the requested type is BTREE or HASH,
  the statement fails with `3729 / HY000`.

Current descriptor catalog case-sensitivity and collation expectations are
unchanged: identifier matching follows the existing table and column resolver
rules for supported DDL paths, and no new collation behavior is introduced.

## Diagnostics and Warnings

The slice uses existing diagnostics except for the MySQL-compatible spatial
index-type error:

- `3729 / HY000`: `The index type BTREE is not supported for spatial indexes.`
- `3729 / HY000`: `The index type HASH is not supported for spatial indexes.`

Existing diagnostics continue to apply for syntax errors, unsupported grammar,
missing default schema, unknown schemas, unknown tables, reserved names,
unsupported object kinds, duplicate indexes, missing columns, nullable spatial
columns, non-geometric spatial columns, multi-part spatial indexes, prefixes,
explicit key-part order, unique or primary spatial indexes, allocation
failures, physical SQLite failures in non-spatial sibling paths, and public API
misuse.

Successful RTREE-normalized spatial index creation emits the existing warning
`3674 / HY000` once per created spatial index while SRID descriptors are absent.
Supported successful statements otherwise report zero affected rows through the
existing non-row DDL result conventions.

## SQLite Handling

The generated physical SQL shape is unchanged for non-spatial indexes. For
RTREE-normalized spatial indexes, MyLite mutates only catalog descriptors and
does not generate SQLite index DDL. Identifier quoting, stable generated
physical names, and statement binding behavior remain the responsibility of
the existing non-spatial index execution paths. This phase adds no SQLite
fork patches.

## Test Plan

Add coverage to the existing spatial metadata MySQL expectation script and C
runtime test:

- table-level leading and trailing `KEY ... USING RTREE` over a `GEOMETRY`
  column renders as `SPATIAL KEY`;
- `ALTER TABLE ... ADD INDEX ... USING RTREE` and standalone
  `CREATE INDEX ... USING RTREE` render as spatial metadata and emit warning
  `3674`;
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report `SPATIAL`;
- ordinary table-level, `ALTER TABLE ... ADD INDEX`, and standalone
  `CREATE INDEX` `USING BTREE` and `USING HASH` forms over a spatial column
  fail with `3729 / HY000`;
- ordinary table-level, `ALTER TABLE ... ADD INDEX`, and standalone
  `CREATE INDEX` `USING RTREE` forms over a non-spatial column fail with
  `1687 / 42000`;
- explicit `SPATIAL ... USING RTREE` and
  `CREATE SPATIAL INDEX ... USING RTREE` remain syntax errors;
- physical SQLite index counts stay zero for the normalized spatial paths.
