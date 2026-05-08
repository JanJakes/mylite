# Error, warning, and result semantics

| Feature | Status | Notes |
| --- | --- | --- |
| Error code catalog | ❌ | Error numbers, SQLSTATE, text |
| Warning code catalog | ❌ | Warning numbers, SQLSTATE, order |
| Diagnostics area | 🟡 | Limited previous-statement diagnostics snapshot for `SHOW WARNINGS`, `SHOW ERRORS`, `SHOW COUNT(*) WARNINGS`, `SHOW COUNT(*) ERRORS`, and scalar `@@warning_count` / `@@error_count`; supports warning records and note records currently produced by limited table-existence DDL no-ops only; no diagnostics stacks, `GET DIAGNOSTICS`, `max_error_count`, or counted-but-not-stored conditions |
| SQL notes variable | 🟡 | Limited scalar `@@sql_notes` reads report fixed enabled value `1`; note-level diagnostics are currently produced only by limited table-existence DDL no-ops; no mutable note state, note suppression, `max_error_count`, diagnostics stacks, or changed `SHOW WARNINGS` / `@@warning_count` behavior |
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
