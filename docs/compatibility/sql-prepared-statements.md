# SQL prepared statements

SQL-level prepared statement lifecycle compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `PREPARE` | 🟡 | Limited SQL-level `PREPARE name FROM 'sql'` and `PREPARE name FROM @source` over one currently supported MyLite statement; statement names are handle-local and case-insensitive, replacement failure removes the old handler, `?` markers are counted outside comments/literals/quoted identifiers, and prepared lifecycle commands inside prepared SQL return the verified MySQL unsupported-command diagnostic. No binary protocol prepare, marker metadata, status counters, performance-schema rows, `max_prepared_stmt_count`, persistent handlers, identifier markers, multiple source statements, or additional SQL compatibility beyond the expanded statement |
| `EXECUTE` | 🟡 | Limited `EXECUTE name` and `EXECUTE name USING @var[, ...]`; `USING` accepts user variables only, count must match marker count, uninitialized/`NULL` variables bind as `NULL`, integer/boolean variables bind as decimal integer text, and string variables bind as MyLite-escaped string literals before dispatch through the same internal statement execution path as direct SQL. No constants in `USING`, parameters outside prepared SQL, protocol metadata, binary values with embedded `NUL`, server-side type metadata, or identifier/table-name substitution |
| `DEALLOCATE PREPARE` / `DROP PREPARE` | 🟡 | Limited handle-local cleanup with MySQL-compatible unknown-handler diagnostics; successful cleanup returns a non-row result with affected rows `0`. No cross-handle cleanup, metadata visibility, or persistence across close/reopen |

[Back to compatibility overview](../../COMPATIBILITY.md)
