# SQL joins

Table reference, join syntax, join semantics, lateral derived table, and join metadata compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Table references | ❌ | Aliases, derived tables, table functions |
| Inner joins | ❌ | JOIN, INNER JOIN, CROSS JOIN, comma join, ON, and USING |
| Outer joins | ❌ | LEFT/RIGHT OUTER JOIN null-extension and predicate placement |
| Natural joins | ❌ | NATURAL INNER/LEFT/RIGHT JOIN column matching and metadata |
| `STRAIGHT_JOIN` | ❌ | Join-order forcing syntax and optimizer interaction |
| Lateral derived tables | ❌ | LATERAL derived table correlation rules |

[Back to compatibility overview](../../COMPATIBILITY.md)
