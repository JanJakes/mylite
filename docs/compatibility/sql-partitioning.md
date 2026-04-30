# SQL partitioning

Partitioning clauses, options, and partition maintenance compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `PARTITION` selection | ❌ | Explicit partition selection syntax and errors |
| Partition maintenance | ❌ | Partition maintenance ALTER actions |
| `PARTITION BY RANGE` | ❌ | Range partition syntax |
| `PARTITION BY RANGE COLUMNS` | ❌ | Range columns syntax and comparison semantics |
| `PARTITION BY LIST` | ❌ | List partition syntax and value matching |
| `PARTITION BY LIST COLUMNS` | ❌ | List columns syntax and tuple matching |
| `PARTITION BY HASH` | ❌ | Hash partition syntax and function semantics |
| `PARTITION BY LINEAR HASH` | ❌ | Linear hash partition syntax |
| `PARTITION BY KEY` | ❌ | Key partition syntax and default key selection |
| `PARTITION BY LINEAR KEY` | ❌ | Linear key partition syntax |
| Subpartitioning | ❌ | HASH/KEY subpartition syntax and metadata |
| Partition options | ❌ | Partition options and metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
