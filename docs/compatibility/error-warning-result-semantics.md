# Error, warning, and result semantics

| Feature | Status | Notes |
| --- | --- | --- |
| Error code catalog | ❌ | Error numbers, SQLSTATE, text |
| Warning code catalog | ❌ | Warning numbers, SQLSTATE, order |
| Diagnostics area | 🟡 | Limited previous-statement diagnostics snapshot for `SHOW WARNINGS`, `SHOW ERRORS`, `SHOW COUNT(*) WARNINGS`, `SHOW COUNT(*) ERRORS`, and scalar `@@warning_count` / `@@error_count`; supports warning records and note records currently produced by limited table-existence DDL no-ops, existing-schema `CREATE DATABASE IF NOT EXISTS`, and limited `INSERT IGNORE ... VALUES` / `SET` adjustment paths; missing-schema `DROP DATABASE IF EXISTS` exposes a statement warning count but intentionally stores no diagnostics row; no diagnostics stacks, `GET DIAGNOSTICS`, `max_error_count`, or broader counted-but-not-stored conditions |
| SQL notes variable | 🟡 | Limited scalar `@@sql_notes` reads report fixed enabled value `1`; note-level diagnostics are currently produced only by limited table/schema-existence DDL no-ops; no mutable note state, note suppression, `max_error_count`, diagnostics stacks, or changed `SHOW WARNINGS` / `@@warning_count` behavior beyond the documented missing-schema drop exception |
| SQL warnings variable | 🟡 | Limited scalar `@@sql_warnings` reads report fixed disabled value `0`; limited `INSERT IGNORE ... VALUES` / `SET` adjustment warnings are still recorded in diagnostics like MySQL; no mutable warning-reporting state, broader warning-producing DML conversions, single-row insert information strings, or protocol status changes |
| Strict-mode errors | ❌ | Escalation from warnings to errors under strict SQL modes |
| IGNORE warning demotion | 🟡 | Limited `INSERT IGNORE ... VALUES` / `SET` demotion for descriptor integer, decimal, temporal, string, `NULL`, and `DEFAULT` row inputs: explicit `NULL` into numeric/string/temporal `NOT NULL`, omitted no-explicit-default columns, explicit `DEFAULT` for no-default columns, out-of-range descriptor integer/decimal inputs, invalid temporal inputs, and duplicate-key rows become warnings with adjusted or skipped values; omitted or explicit-`DEFAULT` numeric `NOT NULL` no-default columns store `0`, omitted or explicit-`DEFAULT` `DATE NOT NULL` stores `0000-00-00`, omitted or explicit-`DEFAULT` `DATETIME NOT NULL` or `TIMESTAMP NOT NULL` stores `0000-00-00 00:00:00`, string `NOT NULL` stores empty string, and nullable dropped-default columns store `NULL`; no broader type conversion, `INSERT IGNORE ... SELECT`, `UPDATE IGNORE`, `DELETE IGNORE`, or `LOAD DATA IGNORE` demotion |
| Metadata flags | ❌ | Column flags and metadata |
| Result ordering guarantees | ❌ | Required ordering guarantees |
| Floating-point edge cases | ❌ | NaN, Inf, rounding, comparison |
| Temporal edge cases | ❌ | Zero dates, DST, fractional seconds |
| JSON edge cases | ❌ | Keys, ordering, paths, partial updates |
| Spatial edge cases | ❌ | SRID, invalid geometry, units |
| Privilege-sensitive metadata | ❌ | Grant-sensitive metadata visibility |

[Back to compatibility overview](../../COMPATIBILITY.md)
