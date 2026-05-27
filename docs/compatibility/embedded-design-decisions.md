# Embedded-design compatibility decisions

Some MySQL server features do not naturally map to an in-process single-file database. They still belong in the compatibility matrix because applications may issue the syntax or inspect related metadata. Each row needs an explicit design decision before it can move out of ❌.

| Feature | Status | Notes |
| --- | --- | --- |
| Replication and binary logs | ❌ | Embedded replication/binlog policy |
| Account management and privileges | ❌ | Embedded account/privilege policy |
| Built-in system schema writes | 🟡 | `information_schema`, `mysql`, `performance_schema`, and `sys` are synthetic metadata-only schemas in MyLite. `USE` and metadata listing are supported, but schema/table/index/rename/truncate/single-table DML writes are rejected before catalog mutation; `mysql` and `sys` write protection is deliberately stricter than MySQL 8.4.9 `root` temporary-table behavior. |
| Resource groups | ❌ | Resource group diagnostics |
| Components and plugins | ❌ | Component/plugin metadata policy |
| Server lifecycle commands | ❌ | Lifecycle command diagnostics |
| Storage engines | 🟡 | Explicit InnoDB-only embedded surface for `CREATE TABLE ... ENGINE [=] InnoDB`, SQL-mode-aware unavailable-engine diagnostics or substitution to InnoDB for current `CREATE TABLE` / `CREATE TEMPORARY TABLE` forms, fixed `SHOW CREATE TABLE` suffixes, one-row `SHOW [STORAGE] ENGINES`, limited one-row `INFORMATION_SCHEMA.ENGINES`, and scalar/`SHOW VARIABLES` `@@default_storage_engine` plus `@@default_tmp_storage_engine`; no plugin architecture, mutable engine state, or real alternate engines |
| `InnoDB` engine surface | 🟡 | MyLite maps its current persistent and temporary base-table storage to a limited InnoDB-compatible default-engine surface for application detection, including fixed `@@default_storage_engine = InnoDB` and `@@default_tmp_storage_engine = InnoDB` |
| `MyISAM` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no MyISAM storage, metadata, or behavior |
| `MEMORY` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no MEMORY storage, metadata, or behavior |
| `CSV` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no CSV storage, log-table metadata, or behavior |
| `ARCHIVE` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no ARCHIVE storage, metadata, or behavior |
| `BLACKHOLE` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no BLACKHOLE storage, metadata, or behavior |
| `MERGE` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no MERGE/MyISAM union-table storage, metadata, or behavior |
| `FEDERATED` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no FEDERATED storage, metadata, or behavior |
| `NDB` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no NDB storage, metadata, or behavior |
| `PERFORMANCE_SCHEMA` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no Performance Schema engine storage, metadata, or behavior |
| Tablespaces and logfile groups | ❌ | Single-file storage diagnostics |
| Performance Schema | ❌ | Embedded metrics policy |
| sys schema | ❌ | Useful views vs placeholders |
| File import/export | ❌ | Secure embedded file I/O policy |
| User-defined/loadable functions | ❌ | Extension registration policy |
| X Protocol and Document Store | ❌ | X Protocol scope decision |

[Back to compatibility overview](../../COMPATIBILITY.md)
