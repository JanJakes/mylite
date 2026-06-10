# Baseline Select No-op Modifiers

## Summary

This phase adds a narrow top-level and source-`SELECT` modifier slice for:

- `HIGH_PRIORITY`
- `STRAIGHT_JOIN`
- `SQL_SMALL_RESULT`
- `SQL_BIG_RESULT`
- `SQL_BUFFER_RESULT`
- `SQL_NO_CACHE`

The first implementation treats all of these as planner no-ops for MyLite's
current single-table and no-source `SELECT` surfaces, except that
`SQL_NO_CACHE` records MySQL's deprecation warning. `SQL_CALC_FOUND_ROWS`
remains owned by the previous `baseline-found-rows-function` slice and keeps
its existing restricted execution and warning behavior.

This is intentionally not full SELECT modifier compatibility. It is syntax and
diagnostics coverage for modifier words commonly emitted by MySQL-oriented
applications while MyLite still executes the same descriptor-built SQLite plans
as the corresponding unmodified supported `SELECT`.

## Compatibility Authority

Use these sources as the compatibility authority for this phase:

- Official MySQL 8.4 Reference Manual, `SELECT Statement`:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Observed MySQL 8.4.9 runtime behavior from
  `packages/libmylite/tests/mysql_baseline_select_noop_modifiers_expectations.sh`.

The MySQL manual documents these modifier words after `SELECT` and before the
select list. It describes `HIGH_PRIORITY`, statement-level `STRAIGHT_JOIN`,
`SQL_SMALL_RESULT`, `SQL_BIG_RESULT`, and `SQL_BUFFER_RESULT` as optimizer or
execution-strategy hints. It also documents `SQL_NO_CACHE` as deprecated and
without effect because the query cache has been removed in MySQL 8.4.

Runtime probes against MySQL 8.4.9 showed:

- `HIGH_PRIORITY`, statement-level `STRAIGHT_JOIN`, `SQL_SMALL_RESULT`,
  `SQL_BIG_RESULT`, and `SQL_BUFFER_RESULT` do not change result rows,
  `ROW_COUNT()`, or warning count for simple scalar, table, aggregate, grouped,
  and distinct queries used by this phase.
- `SQL_NO_CACHE` does not change result rows or found-row state, but records one
  warning:
  - code: `1681`
  - level: `Warning`
  - message: `'SQL_NO_CACHE' is deprecated and will be removed in a future release.`
- `SQL_NO_CACHE` warning rows are recorded before `SQL_CALC_FOUND_ROWS` and
  `FOUND_ROWS()` deprecation warnings when those modifiers/functions are used
  in the same accepted statement.
- MySQL accepts result-option modifiers such as `SQL_BIG_RESULT`,
  `SQL_SMALL_RESULT`, `SQL_BUFFER_RESULT`, and `SQL_CALC_FOUND_ROWS` before
  `DISTINCT` / `DISTINCTROW` / `ALL`, in addition to the canonical order.
- MySQL accepts many broader modifier permutations and duplicate no-op
  modifiers. This phase intentionally admits the canonical MyLite grammar below
  plus the documented canonical-order result-option-before-duplicate
  compatibility retry, and defers broader permutation compatibility.

## Ownership Boundaries

- Public API: no new public functions, structs, constants, or ABI changes.
- Statement context: no new statement-context state is required. Warnings use
  the existing diagnostics area and result warning-count finalization.
- Parser/AST: owns recognizing the admitted modifier words and storing them as
  `SELECT` statement flags. `ALL` / `DISTINCT` / `DISTINCTROW` remain the
  duplicate-mode modifier. `SQL_CALC_FOUND_ROWS` remains an independent flag.
- Analyzer/planner: owns copying modifier flags into planned select structures,
  rejecting unsupported combinations or unsupported source usage, and keeping
  no-op modifiers out of generated SQLite SQL.
- Runtime/result builder: owns appending the `SQL_NO_CACHE` warning before any
  later same-statement warning produced by `SQL_CALC_FOUND_ROWS` or scalar
  functions.
- Catalog: no catalog reads or writes beyond the existing selected descriptor
  resolution. Modifiers must not mutate descriptors, generation counters, or
  caches.
- Storage/VFS/file format: no `.mylite` preamble, VFS, or SQLite payload format
  changes.
- SQLite: no new SQLite fork patch, temp table, optimizer hint injection, or
  extension API call. SQLite receives the same descriptor-built SQL as the
  equivalent unmodified query.

## MyLite Syntax

The admitted grammar is intentionally canonical and independently authored for
MyLite. It is based on the observed MySQL feature surface and current MyLite
parser structure, not copied from MySQL parser sources.

The core modifier sequence is canonical:

```text
select_modifiers:
    duplicate_modifier_opt high_priority_opt straight_join_opt
    sql_small_result_opt sql_big_result_opt sql_buffer_result_opt
    sql_no_cache_opt sql_calc_found_rows_opt

duplicate_modifier_opt:
    /* empty */
  | ALL
  | DISTINCT
  | DISTINCTROW

high_priority_opt:
    /* empty */
  | HIGH_PRIORITY

straight_join_opt:
    /* empty */
  | STRAIGHT_JOIN

sql_small_result_opt:
    /* empty */
  | SQL_SMALL_RESULT

sql_big_result_opt:
    /* empty */
  | SQL_BIG_RESULT

sql_buffer_result_opt:
    /* empty */
  | SQL_BUFFER_RESULT

sql_no_cache_opt:
    /* empty */
  | SQL_NO_CACHE

sql_calc_found_rows_opt:
    /* empty */
  | SQL_CALC_FOUND_ROWS
```

Representative Lemon-style snippets:

```text
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, NULL, NULL, NULL, NULL, NULL, NULL);
}

select_statement(A) ::=
    SELECT(T) select_modifiers(M) select_item_list(B)
    FROM(F) table_name(N) table_alias_opt(AL)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL),
        W, G, H, O, L);
}

select_statement(A) ::=
    SELECT(T) select_modifiers(M) STAR(S)
    FROM(F) table_name(N) table_alias_opt(AL)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N, AL), W, G, H, O, L);
}
```

The implementation may reuse existing select-statement constructors internally,
but the AST must expose enough state to distinguish each admitted modifier.

For compatibility with MySQL-emitted statement text, the parser also accepts a
narrow leading result-option prefix before the duplicate-mode modifier. When
more than one result option appears in this prefix, the result options must use
the same relative order as the canonical grammar:

```text
select_modifiers_compat:
    result_option_prefix duplicate_modifier canonical_modifier_tail

result_option_prefix:
    sql_small_result_opt sql_big_result_opt sql_buffer_result_opt
    sql_no_cache_opt sql_calc_found_rows_opt

duplicate_modifier:
    ALL
  | DISTINCT
  | DISTINCTROW
```

The accepted compatibility form is normalized into the same AST flags as the
canonical form. The implementation may do this as a bounded parser retry over
the initial token prefix rather than by expanding the Lemon grammar, because
the broad order-independent modifier grammar is not worth the generated-parser
state growth for this narrow slice. Arbitrary ordering inside the result-option
prefix remains deferred with the rest of the broad modifier permutation surface.

## Supported Surface

The no-op modifiers are admitted on existing MyLite-supported `SELECT` shapes:

- no-source and `FROM DUAL` scalar/session-value/literal projections;
- descriptor-backed single persistent base-table wildcard and column-list
  `SELECT`;
- existing one-column `DISTINCT` / `DISTINCTROW` descriptor-backed `SELECT`;
- existing one-item aggregate and limited grouped aggregate forms;
- existing descriptor-backed source `SELECT` paths used by `CREATE TABLE ...
  SELECT`, `INSERT ... SELECT`, and `REPLACE ... SELECT`, except that
  `SQL_CALC_FOUND_ROWS` remains rejected for those source forms.

The result-option-before-duplicate compatibility form is admitted on the same
supported shapes as the equivalent canonical form. For example,
`SELECT SQL_BIG_RESULT DISTINCT column FROM table` and
`SELECT SQL_CALC_FOUND_ROWS DISTINCT column FROM table LIMIT n` are normalized
to the canonical `SELECT DISTINCT SQL_BIG_RESULT ...` /
`SELECT DISTINCT SQL_CALC_FOUND_ROWS ...` internal representation before
planning.

`SQL_CALC_FOUND_ROWS` is admitted only where the previous found-rows slice
admits it: descriptor-backed column-list, wildcard, and limited `DISTINCT`
table selects with the existing `WHERE`, `ORDER BY`, and `LIMIT` subset.
Combining `SQL_NO_CACHE` with admitted `SQL_CALC_FOUND_ROWS` emits both
warnings.

## Semantics

### `HIGH_PRIORITY`

Accepted as an embedded no-op. MyLite has no table-lock scheduling behavior for
the current SQLite-backed execution path, so this modifier must not change
generated SQLite SQL, result rows, affected rows, warning count, found-row
state, or file contents.

### Statement-level `STRAIGHT_JOIN`

Accepted as a no-op on current no-join select surfaces. It must not enable join
syntax, table-reference `STRAIGHT_JOIN`, or join-order planning. Once joins
exist, this statement-level flag will need a new planner contract.

### `SQL_SMALL_RESULT` and `SQL_BIG_RESULT`

Accepted as no-ops. MyLite must not create different temporary structures,
indexes, or sorting strategies solely because these modifiers are present.
Current descriptor-built `ORDER BY`, `DISTINCT`, aggregate, and grouped
execution paths remain authoritative.

### `SQL_BUFFER_RESULT`

Accepted as a no-op. MyLite must not materialize a temporary result table or
change lock-release behavior for this phase. This is consistent with current
embedded execution because there is no MySQL server cursor or table-lock
scheduling surface to expose.

### `SQL_NO_CACHE`

Accepted as a deprecated no-op. It must append warning `1681` / `HY000`:

```text
'SQL_NO_CACHE' is deprecated and will be removed in a future release.
```

The warning count on the successful result object must include this warning.
`SHOW WARNINGS` and `SHOW COUNT(*) WARNINGS` must observe it through the
existing diagnostics machinery.

When `SQL_NO_CACHE` appears with another admitted warning-producing construct in
the same statement, MyLite records the `SQL_NO_CACHE` warning first. For this
slice that means:

- `SELECT SQL_NO_CACHE FOUND_ROWS()` warns first for `SQL_NO_CACHE`, then for
  `FOUND_ROWS()`;
- `SELECT SQL_NO_CACHE SQL_CALC_FOUND_ROWS ...` warns first for
  `SQL_NO_CACHE`, then for `SQL_CALC_FOUND_ROWS`.

### Found-Row State

No-op modifiers do not change found-row state semantics. Successful `SELECT`
statements continue to update connection-local `FOUND_ROWS()` state exactly as
their unmodified equivalent would. Non-`SELECT` statements using a source
`SELECT`, such as `INSERT ... SELECT`, remain non-`SELECT` statements for
outer statement row-count and found-row state purposes.

### Diagnostics and Unsupported Forms

This phase must keep deterministic behavior for:

- unsupported modifier permutations or repeated modifiers beyond the
  canonical-order result-option-before-duplicate compatibility form;
- `SQL_CALC_FOUND_ROWS` outside its existing supported subset;
- `SQL_CALC_FOUND_ROWS` inside `CREATE TABLE ... SELECT`, `INSERT ... SELECT`,
  and `REPLACE ... SELECT`;
- `SQL_CACHE`, which remains outside this feature;
- statement-level `STRAIGHT_JOIN` as a join operator or table reference;
- joins, CTEs, subqueries, set operations, locking clauses, `INTO`, partitions,
  and arbitrary SQLite pass-through.

Where MySQL accepts a broader permutation that MyLite deliberately defers, the
compatibility docs must describe the limitation rather than implying full
modifier compatibility.

## Physical SQLite Handling

Generated SQLite SQL must be identical to the SQL generated for the equivalent
supported `SELECT` without these modifiers. In particular:

- do not interpolate modifier text into SQLite SQL;
- continue quoting descriptor-built identifiers and binding values through
  existing prepared-statement paths;
- do not use SQLite optimizer hints;
- do not create temporary tables for `SQL_BUFFER_RESULT`,
  `SQL_SMALL_RESULT`, or `SQL_BIG_RESULT`;
- do not add indexes or storage objects;
- do not touch `.mylite` preamble bytes or shift-VFS behavior.

## Tests

Add MySQL-runtime expectation coverage for:

- each admitted no-op modifier on simple scalar `SELECT`;
- `SQL_NO_CACHE` warning code/message/count;
- all admitted no-op modifiers together on a descriptor-backed table select;
- `SQL_NO_CACHE` with `FOUND_ROWS()` warning order;
- `SQL_NO_CACHE` with `SQL_CALC_FOUND_ROWS` warning order and found-row result;
- canonical distinct, aggregate, and grouped forms with no-op modifiers;
- result-option-before-duplicate forms such as `SQL_BIG_RESULT DISTINCT` and
  `SQL_CALC_FOUND_ROWS DISTINCT`;
- no-op modifiers in `CREATE TABLE ... SELECT`, `INSERT ... SELECT`, and
  `REPLACE ... SELECT` source selects, including `SQL_NO_CACHE` warnings;
- existing row results, affected-row/result-row conventions, warning counts,
  and `FOUND_ROWS()` state where visible;
- deterministic MyLite diagnostics for explicitly unsupported syntax kept
  outside this slice.

Fast C tests should cover parser AST flags and runtime behavior. Prefer a new
runtime test binary if it keeps warning and source-select cases readable.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/sql-query-expressions.md` to say SELECT modifiers support:

- existing `ALL`;
- existing limited `DISTINCT` / `DISTINCTROW`;
- limited `SQL_CALC_FOUND_ROWS`;
- limited canonical no-op `HIGH_PRIORITY`, statement-level `STRAIGHT_JOIN`,
  `SQL_SMALL_RESULT`, `SQL_BIG_RESULT`, `SQL_BUFFER_RESULT`, and deprecated
  no-op `SQL_NO_CACHE` with warning `1681`;
- the narrow canonical-order result-option-before-duplicate compatibility form for
  `SQL_SMALL_RESULT`, `SQL_BIG_RESULT`, `SQL_BUFFER_RESULT`, `SQL_NO_CACHE`,
  and `SQL_CALC_FOUND_ROWS` before `ALL`, `DISTINCT`, or `DISTINCTROW`.

Do not claim full modifier permutations, repeated modifiers, joins, source
optimizer behavior, locking, buffering, temporary-table strategy, query cache
semantics, `SQL_CACHE`, set operations, subqueries, or protocol metadata.
