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
| Primary keys | ❌ | Definitions, metadata, errors |
| Unique indexes | ❌ | NULLs, prefixes, functional parts |
| Nonunique indexes | ❌ | BTREE/HASH clauses, visibility, comments, and parser options |
| Descending indexes | ❌ | DESC key-part syntax and ordering semantics |
| Prefix indexes | ❌ | Prefix length parsing, byte/character semantics, and limits |
| Functional key parts | ❌ | Expression key parts and metadata |
| Multi-valued indexes | ❌ | JSON array index behavior |
| FULLTEXT indexes | ❌ | Parser options and MATCH metadata |
| SPATIAL indexes | ❌ | Geometry column requirements and spatial metadata |
| Foreign keys | ❌ | Cascades, checks, metadata |
| Foreign key checks variable | 🟡 | Limited scalar `@@foreign_key_checks` reads report fixed enabled value `1`; no mutable checking state, foreign key DDL, enforcement, cascades, metadata, or dependency checks |
| Unique checks variable | 🟡 | Limited scalar `@@unique_checks` reads report fixed enabled value `1`; no mutable checking state, unique index DDL, duplicate-key enforcement, index metadata, optimizer effects, or import optimizations |
| CHECK constraints | ❌ | Expression validation, enforcement, names, and metadata |
| Constraint naming | ❌ | Names, scope, SHOW CREATE |
| CREATE INDEX options | ❌ | ALGORITHM, LOCK, visibility |
| `ADD PRIMARY KEY` | ❌ | Primary key addition and data validation |
| `DROP PRIMARY KEY` | ❌ | Primary key removal and auto-increment restrictions |
| `ADD UNIQUE` | ❌ | Unique index addition and duplicate validation |
| `ADD INDEX` / `ADD KEY` | ❌ | Secondary index addition and metadata |
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
