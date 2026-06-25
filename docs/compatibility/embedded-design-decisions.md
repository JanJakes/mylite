# Embedded-design compatibility decisions

Some MySQL server features do not naturally map to an in-process single-file database. They still belong in the compatibility matrix because applications may issue the syntax or inspect related metadata. Rows marked as placeholders have an explicit embedded behavior rather than full server semantics.

| Feature | Status | Notes |
| --- | --- | --- |
| Replication and binary logs | 🟡 | Limited binary-log and replication metadata placeholders plus replication-control no-ops; no physical logs, channels, GTIDs, appliers, or topology |
| Account management and privileges | 🟡 | Synthetic embedded `root@%` identity, grant metadata, and no-op account/role/privilege statements; no authentication, stored accounts, roles, enforcement, or persistence |
| Built-in system schema writes | 🟡 | `information_schema`, `mysql`, `performance_schema`, and `sys` are synthetic metadata-only schemas in MyLite. `USE` and metadata listing are supported, but schema/table/index/rename/truncate/single-table DML writes are rejected before catalog mutation; `mysql` and `sys` write protection is deliberately stricter than MySQL 8.4.9 `root` temporary-table behavior. |
| Resource groups | ⚪ | Resource-group metadata placeholders and DDL/thread-assignment no-ops; no scheduler or thread-affinity state |
| Components and plugins | 🟡 | Limited metadata-only component and plugin surfaces: `mysql.component`, `mysql.plugin`, `SHOW PLUGINS`, and `INFORMATION_SCHEMA.PLUGINS` expose documented synthetic rows where implemented; no component or plugin loading, lifecycle, services, activation, persisted mutation, or extension execution |
| Server lifecycle commands | ⚪ | `RESTART` and `SHUTDOWN` are accepted as embedded no-ops with warning `1105`; no process lifecycle side effects |
| Storage engines | 🟡 | Explicit InnoDB-only embedded surface for `CREATE TABLE ... ENGINE [=] InnoDB`, SQL-mode-aware unavailable-engine diagnostics or substitution to InnoDB for current `CREATE TABLE` / `CREATE TEMPORARY TABLE` forms, fixed `SHOW CREATE TABLE` suffixes, one-row `SHOW [STORAGE] ENGINES`, limited one-row `INFORMATION_SCHEMA.ENGINES`, and scalar/`SHOW VARIABLES` `@@default_storage_engine` plus `@@default_tmp_storage_engine`; no plugin architecture, mutable engine state, or real alternate engines |
| `InnoDB` engine surface | 🟡 | MyLite maps its current persistent and temporary base-table storage to a limited InnoDB-compatible default-engine surface for application detection, including fixed `@@default_storage_engine = InnoDB` and `@@default_tmp_storage_engine = InnoDB` |
| `MyISAM` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no MyISAM storage, metadata, or behavior |
| `MEMORY` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no MEMORY storage, metadata, or behavior |
| `CSV` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no CSV storage or behavior beyond metadata-only `mysql` log-table status and empty read-only log-table rows |
| `ARCHIVE` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no ARCHIVE storage, metadata, or behavior |
| `BLACKHOLE` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no BLACKHOLE storage, metadata, or behavior |
| `MERGE` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no MERGE/MyISAM union-table storage, metadata, or behavior |
| `FEDERATED` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no FEDERATED storage, metadata, or behavior |
| `NDB` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no NDB storage, metadata, or behavior |
| `PERFORMANCE_SCHEMA` engine surface | 🟡 | Treated as an unavailable engine for current `CREATE TABLE` / `CREATE TEMPORARY TABLE`: strict mode rejects, loose mode substitutes InnoDB with warnings; no Performance Schema engine storage, metadata, or behavior |
| Tablespaces and logfile groups | ⚪ | Tablespace and logfile-group DDL is accepted as embedded no-op or unsupported file-operation diagnostics; no physical datafiles, undo files, or storage-engine state |
| Performance Schema | 🟡 | Built-in schema/catalog metadata, selected helper/status placeholders, and write protection; no live event tables, instrumentation, setup-table mutation, or queryable native Performance Schema data |
| sys schema | 🟡 | Selected MySQL-shaped views, helper functions, procedures, metadata, and write protection; no physical sys view definitions, definer enforcement, or complete Performance Schema-backed execution |
| File import/export | 🟡 | Limited server-side `LOAD DATA INFILE`; file export, `LOAD XML`, import-table, and tablespace file operations use explicit unsupported diagnostics; no unrestricted embedded file I/O |
| User-defined/loadable functions | ⚪ | Loadable UDF DDL is parsed and rejected with an explicit unsupported diagnostic; no native shared-library loading, extension registration, or `mysql.func` mutation |
| X Protocol and Document Store | ⚪ | SQL/status metadata placeholders only; no X Plugin listener, X Protocol packets, X DevAPI, collections, or document-store API |

[Back to compatibility overview](../../COMPATIBILITY.md)
