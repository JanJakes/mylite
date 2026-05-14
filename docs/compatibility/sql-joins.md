# SQL joins

Table reference, join syntax, join semantics, lateral derived table, and join metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Table references | 🟡 | Limited two-source descriptor-backed base-table references in `SELECT` with optional `AS alias` or bare aliases; no comma source lists, derived tables, table functions, parenthesized table references, partitions, or index hints |
| Inner joins | 🟡 | Limited two-source `JOIN`, `INNER JOIN`, and `CROSS JOIN` in descriptor-backed `SELECT`, with optional one-column same-family integer or ASCII string descriptor equality `ON` or no `ON` for cartesian products; supports projection, `WHERE`, one-column `ORDER BY`, and `LIMIT` over joined sources, plus limited grouped aggregates with one integer descriptor group column and one aggregate result; no comma join, `USING`, mixed-type `ON` comparisons, arbitrary `ON` predicates, more than two sources, distinct joins, full grouping, or expression projection |
| Left outer joins | 🟡 | Limited two-source `LEFT JOIN` and `LEFT OUTER JOIN` in descriptor-backed `SELECT`, with required one-column same-family integer or ASCII string descriptor equality `ON`; preserves left rows, null-extends unmatched right descriptor columns, and supports projection, `WHERE`, one-column `ORDER BY`, and `LIMIT` over joined sources, plus limited grouped aggregates with one integer descriptor group column and one aggregate result; no `USING`, natural joins, right/full outer joins, mixed-type `ON` comparisons, arbitrary `ON` predicates, more than two sources, distinct joins, full grouping, or expression projection |
| Right/full outer joins | ❌ | RIGHT OUTER JOIN and FULL OUTER JOIN null-extension and predicate placement |
| Natural joins | ❌ | NATURAL INNER/LEFT/RIGHT JOIN column matching and metadata |
| `STRAIGHT_JOIN` | ❌ | Join-order forcing syntax and optimizer interaction |
| Lateral derived tables | ❌ | LATERAL derived table correlation rules |

[Back to compatibility overview](../../COMPATIBILITY.md)
