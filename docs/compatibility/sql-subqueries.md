# SQL subqueries

Subquery, quantified comparison, correlation, and derived table materialization compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Scalar subqueries | 🟡 | Limited no-source and `FROM DUAL` scalar subqueries as projection operands and as current `CONCAT()` operands, with one inner scalar value from the supported `DATABASE()`/`SCHEMA()`, integer/boolean/`NULL`, and system-variable read subset; limited uncorrelated table-backed scalar subqueries are supported only as single-table `UPDATE` assignment values, with one descriptor source column and the current descriptor `SELECT` source envelope; multi-column inner projections return MySQL-compatible `1241 / 21000`, matched multi-row assignment subqueries return `1242 / 21000`, and same-table update sources return `1093 / HY000`; no correlated, predicate, derived-table, row, `EXISTS`, `IN`, quantified, general DML-assignment, or arbitrary expression subqueries |
| Row subqueries | ❌ | Row constructors and multi-column comparison semantics |
| `EXISTS` subqueries | ❌ | Existence semantics and correlation behavior |
| `IN` subqueries | ❌ | NULL-aware membership semantics and type conversion |
| `ANY` / `SOME` / `ALL` subqueries | ❌ | Quantified comparison semantics |
| Derived table materialization/merge | ❌ | Optimizer-visible semantics and metadata results |

[Back to compatibility overview](../../COMPATIBILITY.md)
