# Baseline SHOW STATUS

## Summary

This phase adds a small MyLite-owned `SHOW STATUS` result surface:

```sql
SHOW [GLOBAL | SESSION | LOCAL] STATUS [LIKE 'pattern']
```

The statement exposes a limited registry of common status rows with embedded
MyLite values. It is intended for clients that probe basic MySQL server state
or feature availability. It does not implement MySQL's complete status-variable
catalog, status counters, Performance Schema status tables, privilege checks,
`mysqladmin extended-status`, `FLUSH STATUS`, or `WHERE` filtering.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `SHOW STATUS`: <https://dev.mysql.com/doc/refman/8.4/en/show-status.html>
  - server status variable reference:
    <https://dev.mysql.com/doc/refman/8.4/en/server-status-variable-reference.html>
- Observed MySQL 8.4.9 runtime behavior from the local Homebrew
  `mysql@8.4` server (`mysqld 8.4.9`) started with a temporary datadir and
  Unix socket.

Runtime probes against MySQL 8.4.9 establish these expectations for the
admitted subset:

- result columns are `Variable_name` and `Value`;
- `SHOW STATUS`, `SHOW SESSION STATUS`, and `SHOW LOCAL STATUS` use session
  scope; `LOCAL` is a session synonym;
- `SHOW GLOBAL STATUS` uses global scope;
- session-scope status includes global-only rows such as `Connections`;
- global-scope status omits session-only rows such as `Compression`;
- `LIKE` filters match status variable names case-insensitively using `%`, `_`,
  and backslash escapes;
- `SHOW STATUS LIKE 'Threads\_%'` returns the thread rows whose names match the
  pattern;
- `SHOW STATUS LIKE 'Threads%' WHERE ...`, `SHOW STATUS ORDER BY ...`,
  `SHOW STATUS LIMIT ...`, `SHOW FULL STATUS`, and non-string `LIKE` operands
  are syntax errors;
- successful supported statements leave warning count `0`, error count `0`,
  and make `ROW_COUNT()` return `-1`.

The exact live values of MySQL counters vary by server startup and connection
history. MyLite therefore verifies MySQL shape, scope, and filtering behavior
against MySQL 8.4.9, while MyLite's own C tests assert the deterministic
embedded values documented below.

## Ownership Boundaries

- Public API: no ABI or public-header change. `mylite_execute()` returns a
  normal row result through the existing result handle.
- Statement context: successful `SHOW STATUS` is a result-producing statement.
  It reports affected rows `0`, warning count `0`, no stored warnings, and
  updates the previous row count to `-1`.
- Lexer/parser/AST: add a `SHOW STATUS` statement node with an optional scope
  token and optional string-only `LIKE` clause. Reuse the existing identifier
  and string-literal AST helpers.
- Runtime/analyzer: resolve the optional scope and iterate a static
  MyLite-owned status registry. Apply `LIKE` while appending rows.
- Catalog: not involved. Status variables are runtime/embedded metadata, not
  schema or table descriptors.
- Result builder: build rows directly through the existing result API.
- Storage/VFS/file format: no file reads beyond normal handle state, no writes,
  no catalog mutation, no SQLite schema generation change, and no `.mylite`
  preamble change.
- SQLite physical storage: no generated SQLite SQL and no SQLite fork patch.

## Syntax

The independent MyLite subset is:

```ebnf
show_status_statement:
    SHOW show_status_scope_opt STATUS show_status_filter_opt

show_status_scope_opt:
    empty
  | GLOBAL
  | SESSION
  | LOCAL

show_status_filter_opt:
    empty
  | LIKE string_literal
```

The grammar intentionally excludes `FULL`, `WHERE`, `ORDER BY`, `LIMIT`,
schema qualifiers, parameters, non-string `LIKE` operands, and arbitrary
expressions. `LOCAL` is admitted as a session-scope synonym because MySQL 8.4.9
accepts it for this statement.

### MyLite Lemon-Syntax Snippet

```lemon
statement(A) ::= show_status_statement(B). {
    A = B;
}

show_status_statement(A) ::=
    SHOW(S) show_status_scope_opt(O) STATUS(T) show_status_filter_opt(F). {
    A = mylite_sql_parser_make_show_status_statement(state, S, O, T, F);
}

show_status_scope_opt(A) ::= . {
    A = NULL;
}
show_status_scope_opt(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_status_scope_opt(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
show_status_scope_opt(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

show_status_filter_opt(A) ::= . {
    A = NULL;
}
show_status_filter_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Status Registry

This phase exposes a limited common registry. Values are deterministic embedded
placeholders unless listed otherwise. All numeric values are returned as
decimal text.

| Variable | Default/session/LOCAL visibility | GLOBAL visibility | MyLite value |
| --- | --- | --- | --- |
| `Aborted_clients` | yes | yes | `0` |
| `Aborted_connects` | yes | yes | `0` |
| `Bytes_received` | yes | yes | `0` |
| `Bytes_sent` | yes | yes | `0` |
| `Com_begin` | yes | yes | `0` |
| `Com_commit` | yes | yes | `0` |
| `Com_delete` | yes | yes | `0` |
| `Com_insert` | yes | yes | `0` |
| `Com_replace` | yes | yes | `0` |
| `Com_rollback` | yes | yes | `0` |
| `Com_select` | yes | yes | `0` |
| `Com_set_option` | yes | yes | `0` |
| `Com_show_status` | yes | yes | `0` |
| `Com_show_variables` | yes | yes | `0` |
| `Com_update` | yes | yes | `0` |
| `Compression` | yes | no | `OFF` |
| `Connections` | yes | yes | `1` |
| `Created_tmp_disk_tables` | yes | yes | `0` |
| `Created_tmp_files` | yes | yes | `0` |
| `Created_tmp_tables` | yes | yes | `0` |
| `Handler_delete` | yes | yes | `0` |
| `Handler_read_first` | yes | yes | `0` |
| `Handler_read_key` | yes | yes | `0` |
| `Handler_read_next` | yes | yes | `0` |
| `Handler_read_rnd` | yes | yes | `0` |
| `Handler_read_rnd_next` | yes | yes | `0` |
| `Handler_update` | yes | yes | `0` |
| `Handler_write` | yes | yes | `0` |
| `Open_files` | yes | yes | `0` |
| `Open_streams` | yes | yes | `0` |
| `Open_table_definitions` | yes | yes | `0` |
| `Open_tables` | yes | yes | `0` |
| `Opened_files` | yes | yes | `0` |
| `Opened_table_definitions` | yes | yes | `0` |
| `Opened_tables` | yes | yes | `0` |
| `Prepared_stmt_count` | yes | yes | `0` |
| `Queries` | yes | yes | `0` |
| `Questions` | yes | yes | `0` |
| `Select_full_join` | yes | yes | `0` |
| `Select_full_range_join` | yes | yes | `0` |
| `Select_range` | yes | yes | `0` |
| `Select_scan` | yes | yes | `0` |
| `Slow_queries` | yes | yes | `0` |
| `Ssl_cipher` | yes | yes | empty string |
| `Ssl_version` | yes | yes | empty string |
| `Table_locks_immediate` | yes | yes | `0` |
| `Table_locks_waited` | yes | yes | `0` |
| `Threads_cached` | yes | yes | `0` |
| `Threads_connected` | yes | yes | `1` |
| `Threads_created` | yes | yes | `1` |
| `Threads_running` | yes | yes | `1` |
| `Uptime` | yes | yes | `0` |
| `Uptime_since_flush_status` | yes | yes | `0` |

The registry is intentionally not complete. Missing rows are omitted rather
than synthesized on demand. This mirrors MySQL's result behavior for unknown or
unavailable status names under `LIKE`, while documenting MyLite's partial
catalog.

## Scope Semantics

No explicit scope, `SESSION`, and `LOCAL` use session-visible status rows.
`GLOBAL` uses global-visible rows. The current registry marks all rows as
session-visible because MySQL exposes global-only rows through session-scope
status; `Compression` is the only session-only row in this slice and is omitted
from `SHOW GLOBAL STATUS`.

The `LIKE` filter is applied after scope filtering.

## LIKE Semantics

The optional `LIKE` operand must be an ordinary SQL string literal. Matching is
against `Variable_name` only. Matching is ASCII case-insensitive; `%` matches
any byte sequence; `_` matches one byte; a backslash escapes the next pattern
byte. This reuses the existing MyLite SHOW-pattern matcher.

## Result Semantics

Successful statements return a row result with columns:

```text
Variable_name
Value
```

Rows contain text values only and are emitted in the registry order above. The
order is MyLite-defined for the partial registry and stable for tests.

Successful statements report:

- affected rows `0`;
- warning count `0`;
- no stored warnings;
- `ROW_COUNT()` as `-1` for the following statement.

The statement does not change status values yet. In particular, it does not
increment `Com_show_status`, `Created_tmp_tables`, `Queries`, or `Questions`;
that broader status-counter lifecycle is deferred.

## Diagnostics

- Syntax errors for unsupported grammar use the existing parser diagnostic
  policy, currently `1064` / `42000`.
- Non-string `LIKE` operands remain parse-time syntax errors because the
  admitted grammar accepts only string literals.
- `SHOW STATUS WHERE ...` is intentionally a parser syntax error in this slice,
  even though MySQL accepts it. A later slice should either generalize the
  existing SHOW output-row predicate evaluator or add an independently
  specified `SHOW STATUS WHERE` evaluator.
- Allocation failures use the existing `MYLITE_NOMEM` / out-of-memory
  diagnostic policy.
- Physical SQLite failures are not expected because this statement generates no
  SQLite SQL. Any unexpected SQLite involvement is a bug.

## Performance

The statement is O(number of exposed status rows) over a small static array. It
does not query SQLite, materialize a temporary table, scan catalog descriptors,
or allocate an intermediate row set. Pattern filtering happens before each row
is appended.

## Tests

Fast C tests cover:

- parser acceptance for default, `GLOBAL`, `SESSION`, `LOCAL`, and `LIKE`
  forms;
- parser rejection for `WHERE`, `ORDER BY`, `LIMIT`, `FULL`, and non-string
  `LIKE` operands;
- result columns, fixed values, row ordering, scope filtering, and `LIKE`
  filtering;
- warning count, affected rows, absence of stored warnings, and `ROW_COUNT()`;
- no storage mutation for file-backed handles;
- independent handles.

The MySQL 8.4.9 expectation script verifies the MySQL reference behavior for
shape, scope, filters, and unsupported clauses. It does not require exact
counter values because those depend on the server process and prior probes.
