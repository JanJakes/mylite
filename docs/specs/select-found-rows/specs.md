# SELECT SQL_CALC_FOUND_ROWS and FOUND_ROWS()

## Scope

This slice implements the deprecated MySQL row-count compatibility pair used by
older pagination code:

- `SELECT SQL_CALC_FOUND_ROWS ...`
- `FOUND_ROWS()`
- per-connection state tracking for the previous successful `SELECT`
- MySQL warning 1287 for both deprecated surfaces

The feature applies to the `SELECT` surfaces MyLite already executes: no-table
scalar selects, single-table selects, joins, grouping, `DISTINCT`, `ORDER BY`,
`LIMIT`, and top-level `UNION` query expressions.

Out of scope:

- `SELECT SQL_CALC_FOUND_ROWS ... INTO`
- parser support for no-table `SELECT ... LIMIT`, which remains outside the
  existing scalar-select grammar
- replication notes and statement-based binary logging behavior
- optimizer behavior beyond externally visible rows, warnings, metadata, and
  `FOUND_ROWS()` values

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Information Functions:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849-regexp` using `mysql --table --force --show-warnings`.

This specification is independently authored from official MySQL
documentation and observed MySQL 8.4.9 behavior. It does not copy MySQL
grammar, documentation prose, or implementation sources.

## MySQL 8.4.9 Behavior Summary

MySQL accepts `SQL_CALC_FOUND_ROWS` as a `SELECT` modifier. The modifier and
`FOUND_ROWS()` are deprecated and each successful use emits warning 1287.

For a `SELECT SQL_CALC_FOUND_ROWS` statement with a `LIMIT`, MySQL stores the
number of rows the statement would have returned after `WHERE`, grouping,
`HAVING`, and `DISTINCT`, but before the final `LIMIT`/offset restriction.
The rows sent to the client are still limited.

Representative probe results:

| SQL sequence | `FOUND_ROWS()` value |
| --- | --- |
| `SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 2` | `5` |
| `SELECT id FROM t ORDER BY id LIMIT 2` | `2` |
| `SELECT id FROM t ORDER BY id LIMIT 2, 2` | `4` |
| `SELECT id FROM t ORDER BY id LIMIT 10, 2` over 3 matching rows | `3` |
| `SELECT id FROM t WHERE id > 100` | `0` |
| `SELECT SQL_CALC_FOUND_ROWS DISTINCT grp FROM t ORDER BY grp LIMIT 1` | `2` |
| `SELECT SQL_CALC_FOUND_ROWS grp, COUNT(*) FROM t GROUP BY grp LIMIT 1` | `2` |
| `SELECT SQL_CALC_FOUND_ROWS id FROM t UNION ALL SELECT id FROM u LIMIT 2` | pre-limit union row count |

If the most recent successful `SELECT` did not use `SQL_CALC_FOUND_ROWS`,
`FOUND_ROWS()` returns the row count visible up to any limit. For `LIMIT 50,10`,
that count is `60` when enough rows exist. When the offset extends beyond the
actual result set, the count is capped at the actual number of rows.

`FOUND_ROWS()` itself is a successful scalar `SELECT` when invoked as
`SELECT FOUND_ROWS()`. The returned value observes the previous `SELECT`; after
that result statement completes, a later `FOUND_ROWS()` returns `1` because the
`SELECT FOUND_ROWS()` result set contained one row.

On a fresh connection before any previous successful `SELECT`, direct MySQL
client probes return `0` for the first `SELECT FOUND_ROWS()`.

Behavior after a failed `SELECT` is undefined by MySQL documentation. MyLite
does not promise to update the stored value on failed statement preparation or
execution.

## MyLite Lemon Grammar Snippets

The intended MyLite grammar shape is:

```lemon
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
        opt_from_and_clauses.

select_modifiers(A) ::= .
select_modifiers(A) ::= select_modifiers(B) select_modifier(C).

select_modifier(A) ::= ALL(T).
select_modifier(A) ::= DISTINCT(T).
select_modifier(A) ::= DISTINCTROW(T).
select_modifier(A) ::= SQL_CALC_FOUND_ROWS(T).

scalar_function(A) ::= FOUND_ROWS LP RP.
```

The snippets describe MyLite's intended grammar and are not copied from MySQL.
If `FOUND_ROWS()` remains under generic function-call parsing, binding must
enforce the zero-argument surface.

## Runtime Semantics

MyLite stores the value returned by `FOUND_ROWS()` on the connection, not on a
statement handle. The state is updated when a successful `SELECT` completes.

- `mylite_db.previous_found_rows` stores the latest completed `SELECT` count.
- Table-backed custom SELECT execution records the count after rows are
  matched, filtered, grouped, and deduplicated.
- If `SQL_CALC_FOUND_ROWS` is present, the stored count ignores the final
  `LIMIT` and offset.
- If `SQL_CALC_FOUND_ROWS` is absent, the stored count is the number of rows
  returned up to the limit position: `min(pre_limit_count, offset + returned)`.
- For `UNION` query expressions, `SQL_CALC_FOUND_ROWS` is valid only on the
  first `SELECT` operand. The stored count is computed after set-operator
  duplicate handling and operand-local limits, but before the final global
  `LIMIT`/offset. Misplaced `SQL_CALC_FOUND_ROWS` raises error 1234.
- Result-set statements that execute directly through SQLite still update the
  stored count by counting rows as they are stepped.
- No-table scalar `SELECT` stores `1` after the scalar row is consumed.

For materialized rowsets, MyLite should compute the pre-limit count before
applying the final limit. For streaming early-stop plans, `SQL_CALC_FOUND_ROWS`
must disable early stop so the full pre-limit count can be observed.

## Warnings and Metadata

`SELECT SQL_CALC_FOUND_ROWS ...` appends warning 1287:

```text
SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release.
Consider using two separate queries instead.
```

`FOUND_ROWS()` appends warning 1287:

```text
FOUND_ROWS() is deprecated and will be removed in a future release.
Consider using COUNT(*) instead.
```

`FOUND_ROWS()` returns a non-null `LONGLONG` numeric result. MySQL 8.4.9
reports binary and numeric flags, but not the unsigned flag used by
`LAST_INSERT_ID()` and `CONNECTION_ID()`.

## Tests

Required coverage:

- parser acceptance for `SELECT SQL_CALC_FOUND_ROWS * FROM t`
- parser acceptance with `ALL`, `DISTINCT`, and `DISTINCTROW`
- parser rejection if the modifier appears after the select list
- `SELECT SQL_CALC_FOUND_ROWS ... LIMIT` returns limited rows and stores the
  full pre-limit count
- plain limited `SELECT` stores the row count up to limit/offset
- offset beyond the result set stores the actual row count
- empty result stores `0`
- `DISTINCT` and grouped queries store pre-limit counts with the modifier
- top-level `UNION` stores the final union row count before global limit with
  `SQL_CALC_FOUND_ROWS`
- later `UNION` operands reject `SQL_CALC_FOUND_ROWS` with error 1234
- first `FOUND_ROWS()` on a fresh connection returns `0`
- `FOUND_ROWS()` reads the previous value and then updates the previous-select
  value to `1`
- warning 1287 is observable for both deprecated surfaces
