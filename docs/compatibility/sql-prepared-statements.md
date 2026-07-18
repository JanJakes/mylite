# SQL prepared statements

SQL-level prepared statement lifecycle compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `PREPARE` | 🟡 | Limited SQL-level `PREPARE name FROM 'sql'` and `PREPARE name FROM @source` over one currently supported MyLite statement; statement names are handle-local and case-insensitive, replacement failure removes the old handler, `?` markers are counted outside comments/literals/quoted identifiers, prepared lifecycle commands inside prepared SQL return the verified MySQL unsupported-command diagnostic, and handlers capture prepare-time lexer modes, default database, connection character set, and literal collation. No binary protocol prepare, marker metadata, status counters, performance-schema rows, `max_prepared_stmt_count`, persistent handlers, identifier markers, multiple source statements, or additional SQL compatibility beyond the expanded statement |
| `EXECUTE` | 🟡 | Limited `EXECUTE name` and `EXECUTE name USING @var[, ...]`; `USING` accepts user variables only, count must match marker count, uninitialized/`NULL` variables bind as `NULL`, integer/boolean variables bind as decimal integer text, fixed-decimal variables bind as decimal source text, and string-backed variables, including current floating-source-text values, bind as MyLite-escaped string literals before dispatch through the same internal statement execution path as direct SQL. Prepared syntax, unqualified object resolution, `DATABASE()`, and literal metadata use captured prepare context, while parameter values, ordinary `@@session` readback, and runtime validation modes remain execute-time. No constants in `USING`, parameters outside prepared SQL, protocol metadata, binary values with embedded `NUL`, server-side type metadata, native floating parameter typing, or identifier/table-name substitution |
| `DEALLOCATE PREPARE` / `DROP PREPARE` | 🟡 | Limited handle-local cleanup with MySQL-compatible unknown-handler diagnostics; successful cleanup returns a non-row result with affected rows `0`. No cross-handle cleanup, metadata visibility, or persistence across close/reopen |

[Back to compatibility overview](../../COMPATIBILITY.md)
