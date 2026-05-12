# SQL indexes and constraints

Standalone index statements plus index, key, constraint, foreign-key, and check DDL details.

## Statement surface

| Feature | Status | Notes |
| --- | --- | --- |
| `CREATE INDEX` | ❌ | Index type and attribute DDL |
| `DROP INDEX` | ❌ | Standalone index removal semantics |
| `CACHE INDEX` | ❌ | MyISAM key cache assignment syntax |
| `LOAD INDEX INTO CACHE` | ❌ | MyISAM index preload syntax |

## Index and constraint details

| Feature | Status | Notes |
| --- | --- | --- |
| Primary keys | 🟡 | Limited descriptor-owned integer-family primary keys declared in `CREATE TABLE`, plus keys added later with `ALTER TABLE ... ADD PRIMARY KEY (column[, ...])` and removed with `ALTER TABLE ... DROP PRIMARY KEY`. Create-time keys support inline single-column `PRIMARY KEY`, table-level `PRIMARY KEY (column)`, and table-level composite `PRIMARY KEY (column[, ...])`; all key parts become `NOT NULL`, and ordered key parts are stored in catalog descriptors. Added keys require existing unqualified integer-family columns with no `NULL` values or duplicate key tuples, make every key-part descriptor column `NOT NULL`, normalize implicit/explicit `NULL` defaults to no explicit default, preserve non-`NULL` integer defaults and supported secondary indexes, and create a generated SQLite unique index. Dropped keys remove the descriptor rows and generated physical index, preserve rows, column descriptors, and supported secondary indexes, leave former key columns `NOT NULL`, report the current table row count, and keep an auto-increment column only if a supported secondary index still indexes it. Existing DML enforces duplicate keys for current `INSERT ... VALUES` / `INSERT ... SET`, `INSERT IGNORE`, and single-assignment `UPDATE`; `CREATE TABLE ... LIKE`, `SHOW COLUMNS` / `SHOW INDEX` / `SHOW CREATE TABLE`, and limited `INFORMATION_SCHEMA.COLUMNS.COLUMN_KEY`, `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and `INFORMATION_SCHEMA.KEY_COLUMN_USAGE` metadata observe the descriptor. No string primary keys, named constraints, foreign keys, auto-increment conversion, or optimizer/index-use guarantees |
| Unique indexes | 🟡 | Limited descriptor-owned single-column unique indexes declared inside `CREATE TABLE` with inline `UNIQUE` / `UNIQUE KEY` or table-level `UNIQUE [KEY\|INDEX] [name] (column)` on integer-family, exact `DECIMAL`, canonical `DATE`, canonical `DATETIME`, and canonical `TIMESTAMP` descriptors. The descriptor is backed by a generated SQLite unique index, cloned by `CREATE TABLE ... LIKE`, omitted by `CREATE TABLE ... SELECT`, rendered in `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and limited `INFORMATION_SCHEMA.STATISTICS`, `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`, and `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`, and enforced for `INSERT ... VALUES`, `INSERT ... SET`, `INSERT IGNORE ... VALUES` / `SET`, and single-assignment `UPDATE`. Multiple `NULL` values are allowed. No standalone `CREATE UNIQUE INDEX`, `ALTER TABLE ADD/DROP/RENAME UNIQUE`, composite indexes, string-family unique indexes, prefix lengths, descending key parts, functional key parts, named constraints, visibility toggles, comments, parser options, or optimizer-use guarantees |
| Nonunique indexes | 🟡 | Limited table-level `KEY` / `INDEX [name] (column)` definitions inside `CREATE TABLE` plus limited single-action `ALTER TABLE table_name ADD INDEX\|KEY [name] (column)` for one supported non-`TEXT` descriptor column on persistent base tables, including integer-family, exact `DECIMAL`, canonical `DATE`, canonical `DATETIME`, canonical `TIMESTAMP`, `CHAR`, and `VARCHAR` descriptors. The descriptor is backed by a generated SQLite index, cloned by `CREATE TABLE ... LIKE`, omitted by `CREATE TABLE ... SELECT`, rendered in `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SHOW INDEX`, and limited `INFORMATION_SCHEMA.STATISTICS`. No standalone `CREATE INDEX`, `ALTER TABLE DROP/RENAME INDEX`, fulltext/spatial indexes, composite indexes, prefix lengths, descending key parts, functional key parts, visibility toggles, comments, parser options, or optimizer-use guarantees |
| Descending indexes | ❌ | DESC key-part syntax and ordering semantics |
| Prefix indexes | ❌ | Prefix length parsing, byte/character semantics, and limits |
| Functional key parts | ❌ | Expression key parts and metadata |
| Multi-valued indexes | ❌ | JSON array index behavior |
| FULLTEXT indexes | ❌ | Parser options and MATCH metadata |
| SPATIAL indexes | ❌ | Geometry column requirements and spatial metadata |
| Foreign keys | ❌ | Cascades, checks, metadata |
| Foreign key checks variable | 🟡 | Limited scalar `@@foreign_key_checks` reads report fixed enabled value `1`; no mutable checking state, foreign key DDL, enforcement, cascades, metadata, or dependency checks |
| Unique checks variable | 🟡 | Limited scalar `@@unique_checks` reads report fixed enabled value `1`; no mutable checking state, toggleable enforcement, optimizer effects, or import optimizations |
| CHECK constraints | ❌ | Expression validation, enforcement, names, and metadata |
| Constraint naming | ❌ | Names, scope, SHOW CREATE |
| CREATE INDEX options | ❌ | ALGORITHM, LOCK, visibility |
| `ADD PRIMARY KEY` | 🟡 | Limited `ALTER TABLE table_name ADD PRIMARY KEY (column_name[, ...])` for persistent base tables and existing unqualified integer-family descriptor columns; validates existing rows for `NULL` with `1138 / 22004`, validates duplicate key tuples with `1062 / 23000`, reports zero affected rows and warnings, and updates ordered descriptor/physical index metadata atomically. No named constraints, string keys, key options, multi-action `ALTER`, generated invisible keys, or auto-increment conversion |
| `DROP PRIMARY KEY` | 🟡 | Limited single-action `ALTER TABLE table_name DROP PRIMARY KEY` for persistent base tables; deletes primary-key descriptors and the generated SQLite unique index atomically, preserves rows, columns, and supported secondary indexes, leaves former key columns `NOT NULL`, reports the table's current row count, and rejects tables without a primary key or drops that would leave an auto-increment column unindexed. No `DROP CONSTRAINT`, `DROP INDEX PRIMARY`, multi-action `ALTER`, temporary tables, views, algorithms, locks, foreign keys, cascades, triggers, or implicit-commit emulation |
| `ADD UNIQUE` | ❌ | Unique index addition and duplicate validation |
| `ADD INDEX` / `ADD KEY` | 🟡 | Limited single-action `ALTER TABLE table_name ADD INDEX\|KEY [index_name] (column_name)` for persistent base tables and one existing unqualified supported non-`TEXT` descriptor column; creates secondary-index and index-column descriptors plus one generated SQLite index, supports omitted-name generation with `_2`, `_3`, ... suffixes, reports zero affected rows/warnings, and updates descriptor-driven `SHOW`, `CREATE TABLE ... LIKE`, limited `INFORMATION_SCHEMA.STATISTICS`, DML, and auto-increment key checks. No `ADD UNIQUE`, multi-action `ALTER`, standalone `CREATE INDEX`, composite/prefix/descending/functional/table-qualified key parts, index options, comments, visibility, temporary tables, views, or optimizer guarantees |
| `ADD FULLTEXT` | ❌ | Full-text index addition |
| `ADD SPATIAL` | ❌ | Spatial index addition |
| `DROP INDEX` / `DROP KEY` | ❌ | Index removal and constraint dependencies |
| `RENAME INDEX` / `RENAME KEY` | ❌ | Index rename semantics |
| `ALTER INDEX VISIBLE` / `INVISIBLE` | ❌ | Index visibility metadata and optimizer behavior |
| `ADD CONSTRAINT CHECK` | ❌ | Check constraint addition and validation |
| `DROP CHECK` | ❌ | Check constraint removal |
| `ALTER CHECK ENFORCED` / `NOT ENFORCED` | ❌ | Check enforcement toggling |
| `ADD CONSTRAINT FOREIGN KEY` | ❌ | Foreign key addition, validation, indexes, and actions |
| `DROP FOREIGN KEY` | ❌ | Foreign key removal and metadata cleanup |
| `DISABLE KEYS` / `ENABLE KEYS` | ❌ | MyISAM-style key maintenance syntax, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
