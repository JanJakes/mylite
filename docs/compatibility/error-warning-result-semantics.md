# Error, warning, and result semantics

| Feature | Status | Notes |
| --- | --- | --- |
| Error code catalog | ❌ | Error numbers, SQLSTATE, text |
| Warning code catalog | ❌ | Warning numbers, SQLSTATE, order |
| Diagnostics area | 🟡 | Limited previous-statement diagnostics snapshot for `SHOW WARNINGS`, `SHOW ERRORS`, `SHOW COUNT(*) WARNINGS`, `SHOW COUNT(*) ERRORS`, and scalar `@@warning_count` / `@@error_count`; no notes, diagnostics stacks, `GET DIAGNOSTICS`, `max_error_count`, or counted-but-not-stored conditions |
| SQL warnings variable | 🟡 | Limited scalar `@@sql_warnings` reads report fixed disabled value `0`; no mutable warning-reporting state, warning-producing DML conversions, single-row insert information strings, or protocol status changes |
| Strict-mode errors | ❌ | Escalation from warnings to errors under strict SQL modes |
| IGNORE warning demotion | ❌ | DML IGNORE demotion rules |
| Metadata flags | ❌ | Column flags and metadata |
| Result ordering guarantees | ❌ | Required ordering guarantees |
| Floating-point edge cases | ❌ | NaN, Inf, rounding, comparison |
| Temporal edge cases | ❌ | Zero dates, DST, fractional seconds |
| JSON edge cases | ❌ | Keys, ordering, paths, partial updates |
| Spatial edge cases | ❌ | SRID, invalid geometry, units |
| Privilege-sensitive metadata | ❌ | Grant-sensitive metadata visibility |

[Back to compatibility overview](../../COMPATIBILITY.md)
