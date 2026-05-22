# SQL joins

Table reference, join syntax, join semantics, lateral derived table, and join metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Table references | 🟡 | Limited two-source descriptor-backed base-table references in `SELECT` with optional `AS alias` or bare aliases, limited validated/no-op index hints, and limited two-source comma source lists; no more than two sources, mixed comma/explicit join precedence, derived tables, table functions, parenthesized table references, or partitions |
| Inner joins | 🟡 | Limited two-source `JOIN`, `INNER JOIN`, `CROSS JOIN`, and comma joins in descriptor-backed `SELECT`, with optional one-column same-family integer or ASCII string descriptor equality `ON`, no `ON` for cartesian products, or limited same-family descriptor-column equality in `WHERE` for comma/inner join filtering; supports projection, `WHERE`, descriptor-column `ORDER BY`, and `LIMIT` over joined sources, plus limited grouped aggregates with one integer descriptor group column and one aggregate result; no `USING`, mixed-type `ON`/column-to-column comparisons, arbitrary `ON` predicates, arbitrary column-to-column `WHERE`, more than two sources, distinct joins, full grouping, or expression projection |
| Left outer joins | 🟡 | Limited two-source `LEFT JOIN` and `LEFT OUTER JOIN` in descriptor-backed `SELECT`, with required one-column same-family integer or ASCII string descriptor equality `ON`; preserves left rows, null-extends unmatched right descriptor columns, and supports projection, `WHERE`, descriptor-column `ORDER BY`, and `LIMIT` over joined sources, plus limited grouped aggregates with one integer descriptor group column and one aggregate result; no `USING`, natural joins, right/full outer joins, mixed-type `ON` comparisons, arbitrary `ON` predicates, more than two sources, distinct joins, full grouping, or expression projection |
| Right/full outer joins | ❌ | RIGHT OUTER JOIN and FULL OUTER JOIN null-extension and predicate placement |
| Natural joins | ❌ | NATURAL INNER/LEFT/RIGHT JOIN column matching and metadata |
| `STRAIGHT_JOIN` | ❌ | Join-order forcing syntax and optimizer interaction |
| Lateral derived tables | ❌ | LATERAL derived table correlation rules |

[Back to compatibility overview](../../COMPATIBILITY.md)
