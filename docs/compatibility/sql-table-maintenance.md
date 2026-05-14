# SQL table maintenance

Table maintenance and table-checking statements.

| Feature | Status | Notes |
| --- | --- | --- |
| `ANALYZE TABLE` | 🟡 | Limited MySQL-shaped result rows for persistent and shadowing temporary base tables, including optional `NO_WRITE_TO_BINLOG` / `LOCAL`, comma-separated targets in input order, missing target rows, implicit user-transaction commit, `ROW_COUNT() = -1`, and no warnings. No histogram syntax, partition maintenance, real statistics refresh, optimizer effects, privileges, or binary logging |
| `CHECK TABLE` | 🟡 | Limited MySQL-shaped result rows for persistent and shadowing temporary base tables, including `QUICK`, `FAST`, `MEDIUM`, `EXTENDED`, `CHANGED`, and `FOR UPGRADE` as accepted no-op options, comma-separated targets in input order, missing target rows, implicit user-transaction commit, `ROW_COUNT() = -1`, and no warnings. No physical integrity scan, partition checks, checksum validation, privileges, or repair advice |
| `CHECKSUM TABLE` | ❌ | Table checksum syntax and result metadata |
| `OPTIMIZE TABLE` | 🟡 | Limited MySQL-shaped result rows for persistent and shadowing temporary base tables, including optional `NO_WRITE_TO_BINLOG` / `LOCAL`, InnoDB-style unsupported-optimize note plus `OK` status rows, comma-separated targets in input order, missing target rows, implicit user-transaction commit, `ROW_COUNT() = -1`, and no warnings. No table rebuild, real analysis, free-space recovery, optimizer effects, privileges, or binary logging |
| `REPAIR TABLE` | 🟡 | Limited MySQL-shaped result rows for persistent and shadowing temporary base tables, including optional `NO_WRITE_TO_BINLOG` / `LOCAL` and `QUICK`, `EXTENDED`, and `USE_FRM` as accepted no-op options, InnoDB-style unsupported-repair note rows, comma-separated targets in input order, missing target rows, implicit user-transaction commit, `ROW_COUNT() = -1`, and no warnings. No physical repair, partition repair, `.frm` handling, privileges, or binary logging |

[Back to compatibility overview](../../COMPATIBILITY.md)
