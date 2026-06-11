# SQL partitioning

Partitioning clauses, options, and partition maintenance compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `PARTITION` selection | ⚪ | `SELECT`, supported `INSERT` / `REPLACE` / `UPDATE` / `DELETE`, supported joined source references, and `LOAD DATA` target suffix syntax are accepted and ignored; table remains nonpartitioned and no partition pruning or write-set validation is performed; see [baseline table partition selection](../specs/baseline-table-partition-selection/specs.md) |
| Partition maintenance | ⚪ | `ALTER TABLE` partition operations such as repartitioning, add/drop/reorganize/rebuild/coalesce/truncate/exchange partition, including MySQL-accepted bare `ADD PARTITION` and `REORGANIZE PARTITION` forms, and partition maintenance syntax are parsed as unsupported utility statements returning `1064 / 42000`; no descriptors, data movement, deletion, repair, pruning, or partition metadata mutation; see [parser corpus ALTER TABLE partition surfaces](../specs/parser-corpus-alter-table-partition-surfaces/specs.md) and [parser corpus DDL default/order residuals](../specs/parser-corpus-ddl-default-order-residuals/specs.md) |
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
