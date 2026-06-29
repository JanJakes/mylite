# SQL full-text search

Full-text search expression, mode, parser, and FULLTEXT index interaction compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `MATCH ... AGAINST` | ⚪ | Parser-admitted expression placeholder for parenthesized `MATCH(column[, ...]) AGAINST (...)` and shorthand `MATCH column[, ...] AGAINST (...)` forms with search expressions and mode modifiers; runtime full-text search remains unsupported |
| Natural language full-text search | ⚪ | Parser admits explicit natural-language modifiers; no default-mode ranking or search behavior |
| Boolean full-text search | ⚪ | Parser admits `IN BOOLEAN MODE`; no boolean operator, ranking, or index scan behavior |
| Query expansion full-text search | ⚪ | Parser admits `WITH QUERY EXPANSION`; no expansion or ranking behavior |
| FULLTEXT index metadata | 🟡 | Limited metadata-only table-level `FULLTEXT [KEY\|INDEX] [name] (column[, ...])` definitions inside persistent `CREATE TABLE` for nonzero-length character and text descriptor columns; descriptors render through supported `SHOW` and `INFORMATION_SCHEMA` surfaces but do not provide search behavior |
| FULLTEXT index interaction | 🟡 | Limited metadata-only interaction: supported `FULLTEXT` descriptors affect `SHOW`, limited `INFORMATION_SCHEMA`, clone/drop/rename/visibility, and ordinary DML metadata, but `MATCH ... AGAINST` still returns the documented unsupported diagnostic and there is no optimizer/index lookup, ranking, tokenizer, SQLite FTS, or search behavior |

[Back to compatibility overview](../../COMPATIBILITY.md)
