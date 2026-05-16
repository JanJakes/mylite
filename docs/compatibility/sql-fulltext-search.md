# SQL full-text search

Full-text search expression, mode, parser, and FULLTEXT index interaction compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `MATCH() ... AGAINST()` | ❌ | Full-text search expression |
| Natural language full-text search | ❌ | Default mode and ranking |
| Boolean full-text search | ❌ | Boolean operators and ranking |
| Query expansion full-text search | ❌ | WITH QUERY EXPANSION behavior |
| FULLTEXT index metadata | 🟡 | Limited metadata-only table-level `FULLTEXT [KEY\|INDEX] [name] (column[, ...])` definitions inside persistent `CREATE TABLE` for nonzero-length character and text descriptor columns; descriptors render through supported `SHOW` and `INFORMATION_SCHEMA` surfaces but do not provide search behavior |
| FULLTEXT index interaction | ❌ | Index requirements and optimizer behavior |

[Back to compatibility overview](../../COMPATIBILITY.md)
