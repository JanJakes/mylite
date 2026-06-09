# SQL full-text search

Full-text search expression, mode, parser, and FULLTEXT index interaction compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `MATCH() ... AGAINST()` | ⚪ | Parser-admitted expression placeholder with column lists, search expressions, and mode modifiers; runtime full-text search remains unsupported |
| Natural language full-text search | ⚪ | Parser admits explicit natural-language modifiers; no default-mode ranking or search behavior |
| Boolean full-text search | ⚪ | Parser admits `IN BOOLEAN MODE`; no boolean operator, ranking, or index scan behavior |
| Query expansion full-text search | ⚪ | Parser admits `WITH QUERY EXPANSION`; no expansion or ranking behavior |
| FULLTEXT index metadata | 🟡 | Limited metadata-only table-level `FULLTEXT [KEY\|INDEX] [name] (column[, ...])` definitions inside persistent `CREATE TABLE` for nonzero-length character and text descriptor columns; descriptors render through supported `SHOW` and `INFORMATION_SCHEMA` surfaces but do not provide search behavior |
| FULLTEXT index interaction | ❌ | Index requirements and optimizer behavior |

[Back to compatibility overview](../../COMPATIBILITY.md)
