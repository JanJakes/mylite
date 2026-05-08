# Embedded-design compatibility decisions

Some MySQL server features do not naturally map to an in-process single-file database. They still belong in the compatibility matrix because applications may issue the syntax or inspect related metadata. Each row needs an explicit design decision before it can move out of ❌.

| Feature | Status | Notes |
| --- | --- | --- |
| Replication and binary logs | ❌ | Embedded replication/binlog policy |
| Account management and privileges | ❌ | Embedded account/privilege policy |
| Resource groups | ❌ | Resource group diagnostics |
| Components and plugins | ❌ | Component/plugin metadata policy |
| Server lifecycle commands | ❌ | Lifecycle command diagnostics |
| Storage engines | 🟡 | Explicit InnoDB-only embedded surface for `CREATE TABLE ... ENGINE [=] InnoDB`, fixed `SHOW CREATE TABLE` suffixes, and one-row `SHOW [STORAGE] ENGINES`; no plugin architecture or alternate engines |
| `InnoDB` engine surface | 🟡 | MyLite maps its current persistent base-table storage to a limited InnoDB-compatible default-engine surface for application detection |
| `MyISAM` engine surface | ❌ | MyISAM surface mapping |
| `MEMORY` engine surface | ❌ | MEMORY surface mapping |
| `CSV` engine surface | ❌ | Decide log-table and ENGINE=CSV compatibility behavior |
| `ARCHIVE` engine surface | ❌ | Decide archive-engine syntax, diagnostics |
| `BLACKHOLE` engine surface | ❌ | Decide blackhole-engine syntax and embedded behavior |
| `MERGE` engine surface | ❌ | Decide MERGE/MyISAM union table syntax, diagnostics |
| `FEDERATED` engine surface | ❌ | FEDERATED surface mapping |
| `NDB` engine surface | ❌ | NDB surface mapping |
| `PERFORMANCE_SCHEMA` engine surface | ❌ | Performance Schema engine mapping |
| Tablespaces and logfile groups | ❌ | Single-file storage diagnostics |
| Performance Schema | ❌ | Embedded metrics policy |
| sys schema | ❌ | Useful views vs placeholders |
| File import/export | ❌ | Secure embedded file I/O policy |
| User-defined/loadable functions | ❌ | Extension registration policy |
| X Protocol and Document Store | ❌ | X Protocol scope decision |

[Back to compatibility overview](../../COMPATIBILITY.md)
