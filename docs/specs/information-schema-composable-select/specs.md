# Composable `information_schema` SELECTs

## Scope

MyLite exposes supported `information_schema` tables as selectable system views
through the normal table-backed `SELECT` path. Queries should compose with the
same projection, alias, expression, `WHERE`, `IN`, aggregate, `GROUP BY`,
`HAVING`, `ORDER BY`, `DISTINCT`, and `LIMIT` behavior already implemented for
ordinary tables.

This batch focuses on `information_schema.TABLES`, while the architecture should
make the existing supported `information_schema` tables available through the
same route where their backing SQL exists.

## Behavior

- `information_schema` is matched case-insensitively as a system schema.
- Supported information-schema table names are matched case-insensitively.
- Unknown information-schema tables return MySQL-compatible unknown-table
  diagnostics for the system schema.
- Table aliases, qualified column references, wildcard expansion, projection
  aliases, expression projection, `WHERE`, `IN`, grouping, aggregate
  expressions, `HAVING`, `ORDER BY`, `DISTINCT`, and `LIMIT` are delegated to
  the regular `SELECT` binder and custom runtime.
- System-view columns are read-only. DDL and DML against system schemas return
  MySQL-style access-denied diagnostics before mutation.
- Metadata should be stable and good enough for normal client use. Exact MySQL
  metadata width and charset parity can continue to improve independently.

## Verified Expectations

The following behaviors were verified against MySQL 8.4.9 and should be covered
by MyLite tests:

| Query | Expected behavior |
| --- | --- |
| `SELECT t.TABLE_NAME AS n, t.ENGINE FROM information_schema.TABLES AS t WHERE t.TABLE_SCHEMA = DATABASE() AND t.TABLE_NAME IN ('wp_posts','wp_postmeta') ORDER BY n` | Returns both matching table names with `ENGINE='InnoDB'`. |
| `SELECT TABLE_SCHEMA, COUNT(*) AS c FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ('information_schema', DATABASE()) GROUP BY TABLE_SCHEMA ORDER BY c DESC, TABLE_SCHEMA` | Returns one grouped row per matching schema. |
| `SELECT COUNT(*) AS c FROM information_schema.TABLES AS t WHERE t.TABLE_SCHEMA = DATABASE() AND t.TABLE_NAME IN ('wp_posts','wp_postmeta')` | Returns `2` when both tables exist. |

## Compatibility Decisions

MyLite backs these tables with SQLite views over MyLite catalogs and generated
constant rows. This preserves the embedded single-file design while allowing the
normal MyLite query runtime to provide composability. Unsupported
information-schema tables remain absent until their rows and columns are
specified.
