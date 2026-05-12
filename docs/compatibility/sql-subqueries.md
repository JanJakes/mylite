# SQL subqueries

Subquery, quantified comparison, correlation, and derived table materialization compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Scalar subqueries | 🟡 | Limited no-source and `FROM DUAL` scalar subqueries as projection operands and as current `CONCAT()` operands, with one inner scalar value from the supported `DATABASE()`/`SCHEMA()`, integer/boolean/`NULL`, and system-variable read subset; multi-column inner projections return MySQL-compatible `1241 / 21000`; no table-backed, correlated, predicate, DML-assignment, derived-table, row, `EXISTS`, `IN`, quantified, or arbitrary expression subqueries |
| Row subqueries | ❌ | Row constructors and multi-column comparison semantics |
| `EXISTS` subqueries | ❌ | Existence semantics and correlation behavior |
| `IN` subqueries | ❌ | NULL-aware membership semantics and type conversion |
| `ANY` / `SOME` / `ALL` subqueries | ❌ | Quantified comparison semantics |
| Derived table materialization/merge | ❌ | Optimizer-visible semantics and metadata results |

[Back to compatibility overview](../../COMPATIBILITY.md)
