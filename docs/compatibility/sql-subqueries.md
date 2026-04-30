# SQL subqueries

Subquery, quantified comparison, correlation, and derived table materialization compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Scalar subqueries | ❌ | Cardinality and NULL semantics |
| Row subqueries | ❌ | Row constructors and multi-column comparison semantics |
| `EXISTS` subqueries | ❌ | Existence semantics and correlation behavior |
| `IN` subqueries | ❌ | NULL-aware membership semantics and type conversion |
| `ANY` / `SOME` / `ALL` subqueries | ❌ | Quantified comparison semantics |
| Derived table materialization/merge | ❌ | Optimizer-visible semantics and metadata results |

[Back to compatibility overview](../../COMPATIBILITY.md)
