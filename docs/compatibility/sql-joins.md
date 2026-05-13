# SQL joins

Table reference, join syntax, join semantics, lateral derived table, and join metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Table references | 🟡 | Limited two-source descriptor-backed base-table references in `SELECT` with optional `AS alias` or bare aliases; no comma source lists, derived tables, table functions, parenthesized table references, partitions, or index hints |
| Inner joins | 🟡 | Limited two-source `JOIN`, `INNER JOIN`, and `CROSS JOIN` in descriptor-backed `SELECT`, with optional one-column descriptor equality `ON` or no `ON` for cartesian products; supports projection, `WHERE`, one-column `ORDER BY`, and `LIMIT` over joined sources; no comma join, `USING`, arbitrary `ON` predicates, more than two sources, aggregates, grouping, distinct joins, or expression projection |
| Outer joins | ❌ | LEFT/RIGHT OUTER JOIN null-extension and predicate placement |
| Natural joins | ❌ | NATURAL INNER/LEFT/RIGHT JOIN column matching and metadata |
| `STRAIGHT_JOIN` | ❌ | Join-order forcing syntax and optimizer interaction |
| Lateral derived tables | ❌ | LATERAL derived table correlation rules |

[Back to compatibility overview](../../COMPATIBILITY.md)
