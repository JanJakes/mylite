# SQL partitioning

Partitioning clauses, options, and partition maintenance compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `PARTITION` selection | ❌ | Explicit partition selection syntax and errors |
| Partition maintenance | ❌ | Partition maintenance ALTER actions |
| `PARTITION BY RANGE` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY RANGE COLUMNS` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY LIST` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY LIST COLUMNS` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY HASH` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY LINEAR HASH` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY KEY` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| `PARTITION BY LINEAR KEY` | ⚪ | `CREATE TABLE` storage-layout suffix is accepted and ignored; table remains nonpartitioned |
| Subpartitioning | ⚪ | `CREATE TABLE` subpartition suffix syntax is accepted and ignored; no subpartition metadata |
| Partition options | ⚪ | `CREATE TABLE` partition definitions/options are accepted and ignored; no partition metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
