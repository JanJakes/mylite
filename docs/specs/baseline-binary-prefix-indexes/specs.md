# Baseline Binary Prefix Indexes Specification

## Summary

This phase extends the existing descriptor-owned prefix-index baseline from
nonbinary string descriptors to binary string descriptors for nonunique
secondary indexes:

```sql
CREATE TABLE t (payload BLOB, KEY payload_prefix (payload(32)))
ALTER TABLE t ADD KEY payload_prefix (payload(32))
CREATE INDEX payload_prefix ON t (payload(32))
```

The goal is to accept common MySQL DDL that indexes `BINARY`, `VARBINARY`, and
`BLOB` family columns by byte prefix, preserve the prefix in MyLite catalog
descriptors, render metadata through descriptor-driven `SHOW` and limited
`INFORMATION_SCHEMA` paths, and create physical SQLite indexes from generated
descriptor SQL.

This is not complete binary string index support. Full-column binary indexes,
unique binary prefix indexes, primary binary indexes, optimizer behavior, and
binary duplicate-key diagnostic formatting remain separate features.

## Sources

- MyLite architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing prefix-index design:
  `docs/specs/baseline-index-prefix-key-parts/specs.md`
- Existing binary string design:
  `docs/specs/baseline-binary-string-types/specs.md`
- MySQL 8.4 Reference Manual, Column Indexes:
  <https://dev.mysql.com/doc/refman/8.4/en/column-indexes.html>
- MySQL 8.4 Reference Manual, `CREATE INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/create-index.html>
- MySQL 8.4 Reference Manual, `SHOW INDEX` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.STATISTICS` table:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-statistics-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_binary_prefix_indexes_expectations.sh`.

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. It does not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or other restrictively licensed implementation sources.

## MySQL 8.4.9 Observations

The MySQL 8.4 manual documents that prefix lengths are interpreted as
characters for `CHAR`, `VARCHAR`, and `TEXT`, but as bytes for `BINARY`,
`VARBINARY`, and `BLOB`. `SHOW INDEX.Sub_part` and
`INFORMATION_SCHEMA.STATISTICS.SUB_PART` report the indexed prefix length.

Runtime probes for this phase establish:

- `KEY k_b (b(2))` succeeds for `BINARY(4)` and renders as
  ``KEY `k_b` (`b`(2))``.
- `KEY k_vb (vb(3))` succeeds for `VARBINARY(10)` and renders as
  ``KEY `k_vb` (`vb`(3))``.
- `KEY k_bl (bl(4))` succeeds for `BLOB`.
- `KEY k_tb (tb(255))` succeeds for `TINYBLOB`.
- `SHOW INDEX` and `INFORMATION_SCHEMA.STATISTICS` report the binary prefix
  length in `Sub_part` / `SUB_PART`.
- `BLOB` without a prefix fails with `1170 / 42000`.
- `TINYBLOB(256)` prefix fails with `1071 / 42000` and reports the 255-byte
  type-family key cap.
- Prefixes longer than a bounded `BINARY(n)` or `VARBINARY(n)` descriptor fail
  with `1089 / HY000`.
- Prefix length `0` fails with `1391 / HY000`.
- `BLOB` prefixes and composite binary prefix indexes whose total indexed
  bytes exceed the observed 3072-byte InnoDB key cap fail with
  `1071 / 42000`.

## Scope

Supported:

- persistent MyLite base tables only;
- create-time nonunique table-level `KEY` / `INDEX [index_name] (...)`;
- single-action `ALTER TABLE table_name ADD INDEX|KEY [index_name] (...)`;
- standalone `CREATE INDEX index_name ON table_name (...)`;
- unqualified and schema-qualified target table names using existing
  selected/default schema policy;
- explicit or generated nonunique index names using existing descriptor policy;
- ordered key-part lists containing any existing nonunique full-column or
  prefix key parts plus the binary prefix parts admitted here;
- optional positive decimal integer prefix lengths for `BINARY`,
  `VARBINARY`, `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and `LONGBLOB` descriptor
  columns;
- descriptor-backed `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`,
  limited `INFORMATION_SCHEMA.STATISTICS`, `CREATE TABLE ... LIKE`, index
  drops, index rename/visibility metadata operations, reopen persistence,
  independent handles, and `.mylite` preamble preservation;
- no-result DDL result shape with `affected_rows == 0` and
  `warning_count == 0` for supported successful forms.

Deferred:

- unique binary prefix indexes and unique duplicate-key enforcement over
  binary prefixes;
- full-column binary secondary indexes;
- binary primary-key parts;
- fulltext or spatial binary indexes;
- prefix parts on `BIT`, `ENUM`, `SET`, `JSON`, numeric, temporal, generated,
  functional, table-qualified, or expression key parts;
- parser options outside the current index option subset;
- optimizer/index-use guarantees.

## Ownership Boundaries

- Public API: no ABI change. Callers continue through `mylite_execute()` and
  existing result/diagnostic APIs.
- Statement context: owns diagnostics reset, warning count, row-count state,
  and result cleanup. This feature adds no new session state.
- Lexer/parser/AST: existing secondary key-part AST already preserves an
  optional prefix length literal independent of descriptors and SQLite.
- Analyzer/planner/runtime: resolves schemas, target tables, index names,
  key-part columns, binary-prefix admissibility, byte caps, and diagnostics
  from MyLite descriptors before generating SQLite SQL.
- Catalog: MyLite index and index-column descriptors remain authoritative
  logical metadata. The existing nullable `prefix_length` descriptor field is
  reused; a positive value means a byte prefix for binary string descriptors.
- Result builder/introspection: descriptor-driven `SHOW`,
  `CREATE TABLE ... LIKE`, and limited `INFORMATION_SCHEMA` paths render the
  same prefix length without consulting SQLite schema text.
- Storage/VFS: `.mylite` preamble and shifted SQLite payload invariants are
  unchanged.
- SQLite physical storage: MyLite generates ordinary SQLite index DDL using
  public SQLite expression-index support. No SQLite fork patch is required.

## Supported SQL Grammar

This phase reuses the existing secondary key-part grammar:

```sql
KEY [index_name] (column_name[(prefix_length)][, ...])
INDEX [index_name] (column_name[(prefix_length)][, ...])
ALTER TABLE table_name ADD KEY [index_name] (column_name[(prefix_length)][, ...])
ALTER TABLE table_name ADD INDEX [index_name] (column_name[(prefix_length)][, ...])
CREATE INDEX index_name ON table_name (column_name[(prefix_length)][, ...])
```

`prefix_length` is a positive decimal integer literal without sign. The parser
continues to reject parameters, expressions, string literals, floating-point
literals, hex literals, bit literals, qualified columns, and functional key
parts in this slice.

MyLite Lemon-syntax sketch, already present for secondary key parts:

```lemon
secondary_index_part ::= identifier index_key_direction_opt.
secondary_index_part ::= identifier LPAREN integer_literal RPAREN index_key_direction_opt.
```

## Resolution and Validation

Target table resolution follows existing policy:

- an unqualified target table requires the selected/default schema;
- a schema-qualified target uses the explicit schema and does not require a
  selected schema;
- unknown schemas fail with `1049 / 42000`;
- unknown tables fail with `1146 / 42S02`;
- reserved `_mylite_*` schema or table names are rejected before physical SQL
  generation;
- target objects must be persistent base tables.

Each binary prefix key part resolves against MyLite column descriptors:

- key-part names must be unqualified descriptor column names;
- unknown columns fail with `1072 / 42000`;
- duplicate key parts fail with `1060 / 42S21`;
- prefix length `0` fails with `1391 / HY000`;
- prefixes on `BINARY(n)` and `VARBINARY(n)` must be at most `n` bytes or fail
  with `1089 / HY000`;
- prefixes on `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, and `LONGBLOB` contribute
  exactly the requested byte count and must fit both the type-family byte cap
  and the current 3072-byte aggregate key-length cap;
- `BLOB` family columns without a prefix continue to fail with
  `1170 / 42000`;
- nonbinary prefix validation keeps its existing character-count behavior;
- successful supported forms produce zero warnings.

Unique binary prefix key parts are rejected with a deterministic MyLite
unsupported diagnostic until the duplicate-key lookup and byte-formatting paths
are specified and verified.

## Physical SQLite Handling

Generated physical SQLite indexes use stable MyLite physical table and index
names. For a full nonbinary string key part MyLite keeps the existing
`_mylite_string_key` collation annotation. For binary prefix key parts MyLite
must not append that text collation.

Physical key-part shape:

```sql
substr("binary_column", 1, <prefix_length>)
```

The prefix length is descriptor-validated before SQL generation and formatted
from the stored integer descriptor, not from user SQL text. Generated
identifiers are quoted through MyLite dynamic-string helpers. No user SQL
literals are interpolated into physical DML. Prefix expressions used for
internal duplicate probes and DML enforcement must apply text collation only
when the descriptor column is nonbinary.

## Metadata and Result Behavior

Successful nonunique binary prefix index creation:

- creates one secondary-index descriptor and one index-column descriptor per
  key part;
- stores the positive byte prefix in `prefix_length`;
- creates one generated SQLite index;
- updates descriptor/catalog generations and SQLite schema generation because a
  physical SQLite index changed;
- returns no row result set;
- reports `affected_rows == 0` and `warning_count == 0`.

`SHOW CREATE TABLE` renders binary prefix key parts with the stored prefix.
`SHOW INDEX.Sub_part` and `INFORMATION_SCHEMA.STATISTICS.SUB_PART` expose the
stored prefix. `SHOW COLUMNS` reports `MUL` for columns covered by nonunique
binary prefix indexes under the existing column-key rules.

## Diagnostics

Diagnostics reuse existing MySQL-compatible errors where the previous prefix
index baseline already defines them:

- syntax or unsupported grammar: existing parser diagnostic;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved target names: existing MyLite reserved-name diagnostic;
- unsupported object kind: existing MyLite object-kind diagnostic;
- unknown key column: `1072 / 42000`;
- duplicate key column: `1060 / 42S21`;
- duplicate index name: `1061 / 42000`;
- quoted `PRIMARY` secondary index name: `1280 / 42000`;
- `BLOB` family key without prefix: `1170 / 42000`;
- zero prefix: `1391 / HY000`;
- bounded binary prefix longer than the column: `1089 / HY000`;
- prefix or aggregate key bytes over the supported cap: `1071 / 42000`;
- binary unique prefix key part: deterministic MyLite unsupported diagnostic;
- physical SQLite failure: existing physical execution diagnostic;
- allocation failure: `MYLITE_NOMEM`.

## Tests

MySQL expectation coverage must include:

- create-time, alter-add, and standalone nonunique indexes over `BINARY`,
  `VARBINARY`, `TINYBLOB`, and `BLOB`;
- `SHOW CREATE TABLE`, `SHOW INDEX`, and
  `INFORMATION_SCHEMA.STATISTICS.SUB_PART` metadata;
- generated index names and composite prefix key parts;
- `CREATE TABLE ... LIKE`, drop, rename, visibility metadata where existing
  paths should observe descriptors;
- zero prefix, oversized bounded binary prefixes, BLOB-family byte caps,
  aggregate 3072-byte cap, duplicate key parts, duplicate index names, unknown
  columns, missing default schema, unknown schemas, and unknown tables;
- successful result shape with zero warnings.

Fast C tests must cover the same supported behavior, plus reopen persistence,
independent handles, `.mylite` preamble preservation, and cleanup on failure.
