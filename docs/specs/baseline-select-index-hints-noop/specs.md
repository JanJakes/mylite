# Baseline SELECT Index Hints No-Op

## Summary

This phase adds a narrow MySQL index-hint acceptance slice for supported
descriptor-backed `SELECT` table sources:

```sql
SELECT ... FROM table_name [AS alias] USE INDEX|KEY [FOR scope] (...)
SELECT ... FROM table_name [AS alias] FORCE INDEX|KEY [FOR scope] (...)
SELECT ... FROM table_name [AS alias] IGNORE INDEX|KEY [FOR scope] (...)
```

The hints are parsed, stored in the MyLite AST, validated against MyLite-owned
index descriptors, and ignored by the physical SQLite planner. The goal is to
accept common WordPress/MySQL query shapes without pretending that MyLite has a
cost-based optimizer or that secondary indexes already drive query execution.

This phase does not add optimizer behavior, SQLite index selection, query plan
metadata, comment optimizer hints, `JOIN_INDEX()` hints, `DELETE` hints,
partition hints, optimizer cost model state, or protocol-visible optimizer
diagnostics. Single-table `UPDATE` target hints are specified separately in
`docs/specs/baseline-update-index-hints-noop/specs.md`.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Existing query and index slices:
  - `docs/specs/baseline-select-where-lifecycle/specs.md`
  - `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
  - `docs/specs/baseline-inner-join-select/specs.md`
  - `docs/specs/baseline-group-by-single-column-aggregate/specs.md`
  - `docs/specs/baseline-secondary-index-lifecycle/specs.md`
  - `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - Index hints:
    <https://dev.mysql.com/doc/refman/8.4/en/index-hints.html>
  - Joined table syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/join.html>
  - `UPDATE` syntax, which is intentionally deferred for this phase:
    <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_select_index_hints_noop_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Observed behavior shaping this slice:

- MySQL accepts `USE INDEX`, `FORCE INDEX`, and `IGNORE INDEX` after a table
  reference alias in `SELECT`.
- `KEY` is accepted as a synonym for `INDEX`.
- Optional hint scopes are `FOR JOIN`, `FOR ORDER BY`, and `FOR GROUP BY`.
- `USE INDEX ()` is accepted on ordinary base tables and behaves as an empty
  usable-index list.
- `FORCE INDEX ()` and `IGNORE INDEX ()` are syntax errors
  (`1064 / 42000`).
- `PRIMARY` is accepted as an unquoted hint name for a primary key.
- Duplicate hint names are accepted.
- All-missing hint name lists fail with `1176 / 42000` and the message shape
  `Key '<name>' doesn't exist in table '<table>'`. MyLite additionally accepts
  the WordPress compatibility shape where at least one name in the list
  resolves and later stale names are ignored.
- Combining `USE INDEX` and `FORCE INDEX` on the same table reference fails
  with `1221 / HY000` and `Incorrect usage of USE INDEX and FORCE INDEX`,
  including when scopes differ.
- Supported successful `SELECT` statements return rows normally, report
  `ROW_COUNT() = -1`, and leave `@@warning_count = 0`.
- Single-table `UPDATE` accepts index hints in MySQL; MyLite covers that
  follow-up slice in `docs/specs/baseline-update-index-hints-noop/specs.md`.
- Single-table `DELETE ... USE INDEX(...)` is not accepted by MySQL in the
  tested shape and remains unsupported.

## Supported Scope

Supported:

- persistent and temporary MyLite base tables already visible to supported
  `SELECT` planning;
- unqualified and schema-qualified table names using existing selected-schema
  policy;
- table aliases with or without `AS`;
- joined `SELECT` table sources already admitted by the current join planner;
- current single-table row-scalar projections, column aggregates, `COUNT()`,
  grouped aggregate, and ordinary descriptor-backed `SELECT` paths when their
  table source already supports the rest of the query;
- hint kinds:
  - `USE INDEX`;
  - `USE KEY`;
  - `FORCE INDEX`;
  - `FORCE KEY`;
  - `IGNORE INDEX`;
  - `IGNORE KEY`;
- optional scopes:
  - omitted scope;
  - `FOR JOIN`;
  - `FOR ORDER BY`;
  - `FOR GROUP BY`;
- hint names from identifiers, quoted identifiers, and unquoted `PRIMARY`;
- empty hint name lists for `USE INDEX|KEY ()` only;
- descriptor-backed validation of named indexes, including primary keys and
  secondary indexes currently represented in the MyLite catalog;
- MySQL-compatible case-insensitive full-name and unambiguous prefix matching
  for hint names, including `PRIMARY` prefixes;
- WordPress-compatible mixed valid/stale name lists when at least one hint name
  resolves for the hinted table;
- MySQL-compatible diagnostics for unknown index names and combined
  `USE INDEX` plus `FORCE INDEX` on the same table reference;
- successful supported hints as no-op planner directives with no warnings.

Deferred:

- using hints to influence SQLite query plans or generated SQL;
- preserving hints in public metadata or exposing optimizer diagnostics;
- `DELETE` target hints;
- multi-table DML hints;
- index hints on `information_schema` virtual tables;
- optimizer comment hints such as `/*+ ... */`;
- new-style optimizer hints such as `JOIN_INDEX()`, `NO_JOIN_INDEX()`,
  `GROUP_INDEX()`, or `ORDER_INDEX()`;
- partition hints, table sampling, CTE hints, subquery hints, view expansion,
  query block naming, optimizer trace output, and privilege semantics;
- optimizer behavior changes based on the selected hint names. Ambiguous
  prefixes are rejected with MySQL's unknown-key diagnostic shape.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns result lifetime and public
  misuse behavior. Successful hinted `SELECT` returns through the same result
  object conventions as the equivalent unhinted statement.
- Statement context: owns diagnostics, warning count, affected rows, and
  `ROW_COUNT()`. Successful supported hinted `SELECT` leaves warning count `0`
  and row-count state identical to the unhinted `SELECT`.
- Lexer/parser/AST: admits the supported hint grammar after table aliases,
  records hint nodes in `FROM_TABLE`, keeps the table-name child stable, and
  preserves alias lookup through accessor helpers so existing SELECT code does
  not confuse a hint list for an alias.
- Analyzer/planner: resolves table sources through existing descriptor policy,
  validates hint names through MyLite catalog descriptors, rejects unsupported
  combinations before SQLite SQL is generated, and deliberately ignores valid
  hints when building physical query plans.
- Catalog module: remains the authority for logical table and index names.
  Hint validation reads descriptors only; it does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: unchanged. Generated SQL remains descriptor-built
  and uses current physical table naming and parameter binding rules. SQLite is
  not forked or patched for this phase.
- Storage/VFS/file format: unchanged. No hinted `SELECT` mutates `.mylite`
  preamble bytes or shifted SQLite payload invariants.

## Grammar

Supported SQL table-source shapes:

```sql
FROM table_name [AS alias] [index_hint ...]
FROM left_table [AS alias] [index_hint ...]
     JOIN right_table [AS alias] [index_hint ...] ON predicate
```

Supported hint shapes:

```sql
USE INDEX [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
USE KEY   [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
USE INDEX [FOR JOIN|ORDER BY|GROUP BY] ()
USE KEY   [FOR JOIN|ORDER BY|GROUP BY] ()
FORCE INDEX [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
FORCE KEY   [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
IGNORE INDEX [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
IGNORE KEY   [FOR JOIN|ORDER BY|GROUP BY] (index_name [, index_name] ...)
```

MyLite Lemon-syntax snippets:

```lemon
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
    FROM(F) table_name(N) table_alias_opt(AL) table_index_hints_opt(H)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(HV)
    order_clause_opt(O) limit_clause_opt(L) select_locking_clause_opt(K). {
    A = mylite_sql_parser_make_select_statement_with_modifiers(
        state, T, M, B, mylite_sql_parser_make_from_table(state, F, N, AL, H),
        W, G, HV, O, L, K);
}

table_source(A) ::= table_name(N) table_alias_opt(AL) table_index_hints_opt(H). {
    A = mylite_sql_parser_make_table_source(state, N, AL, H);
}

table_index_hints_opt(A) ::= . {
    A = NULL;
}

table_index_hints_opt(A) ::= table_index_hint_list(H). {
    A = H;
}

table_index_hint_list(A) ::= table_index_hint(H). {
    A = mylite_sql_parser_make_index_hint_list(state, H);
}

table_index_hint_list(A) ::= table_index_hint_list(L) table_index_hint(H). {
    A = mylite_sql_parser_append_index_hint(state, L, H);
}

table_index_hint(A) ::= USE(T) index_hint_keyword index_hint_scope_opt(S)
    LPAREN index_hint_name_list(N) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_USE_INDEX_HINT, T, S, N, R);
}

table_index_hint(A) ::= USE(T) index_hint_keyword index_hint_scope_opt(S) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_USE_INDEX_HINT, T, S,
        mylite_sql_parser_make_empty_identifier_list(state, L, R), R);
}

table_index_hint(A) ::= FORCE(T) index_hint_keyword index_hint_scope_opt(S)
    LPAREN index_hint_name_list(N) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_FORCE_INDEX_HINT, T, S, N, R);
}

table_index_hint(A) ::= IGNORE(T) index_hint_keyword index_hint_scope_opt(S)
    LPAREN index_hint_name_list(N) RPAREN(R). {
    A = mylite_sql_parser_make_index_hint(
        state, MYLITE_SQL_AST_IGNORE_INDEX_HINT, T, S, N, R);
}

index_hint_keyword ::= INDEX.
index_hint_keyword ::= KEY.

index_hint_scope_opt(A) ::= . {
    A = NULL;
}

index_hint_scope_opt(A) ::= FOR(F) JOIN(J). {
    A = mylite_sql_parser_make_index_hint_scope(
        state, MYLITE_SQL_AST_INDEX_HINT_FOR_JOIN, F, J);
}

index_hint_scope_opt(A) ::= FOR(F) ORDER BY(B). {
    A = mylite_sql_parser_make_index_hint_scope(
        state, MYLITE_SQL_AST_INDEX_HINT_FOR_ORDER_BY, F, B);
}

index_hint_scope_opt(A) ::= FOR(F) GROUP BY(B). {
    A = mylite_sql_parser_make_index_hint_scope(
        state, MYLITE_SQL_AST_INDEX_HINT_FOR_GROUP_BY, F, B);
}

index_hint_name_list(A) ::= index_hint_name(N). {
    A = mylite_sql_parser_make_identifier_list(state, N);
}

index_hint_name_list(A) ::= index_hint_name_list(L) COMMA index_hint_name(N). {
    A = mylite_sql_parser_append_identifier(state, L, N);
}

index_hint_name(A) ::= identifier(I). {
    A = I;
}

index_hint_name(A) ::= PRIMARY(P). {
    A = mylite_sql_parser_make_identifier(state, P);
}
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Semantics

For supported `SELECT` statements, hints are semantic no-ops after validation:

- A valid hinted statement returns the same rows, columns, aliases, warnings,
  and row-count state as the same statement with all hints removed.
- Hint scopes are accepted and retained in the AST, but the current planner
  does not distinguish join, order, and group hint effects.
- The table name, optional alias, and hint list are kept as separate AST
  concepts. Alias resolution remains identical to unhinted statements.
- Hint name lookup is descriptor-driven. MyLite resolves the table normally,
  then checks each named hint against the table's MyLite-owned index
  descriptors. SQLite metadata is not consulted.
- Lookup uses the current descriptor catalog's case-insensitive logical name
  comparison, matching other index lifecycle code. Quoted identifiers preserve
  their identifier spelling before comparison.
- `PRIMARY` names the table's primary-key descriptor. If the table has no
  primary key, it is an unknown key for this slice.
- `USE INDEX ()` and `USE KEY ()` are accepted without descriptor lookup.
- `FORCE INDEX ()` and `IGNORE INDEX ()` are syntax errors because the grammar
  requires at least one hint name for those hint kinds.
- Duplicate hint names are accepted once each name resolves.
- If a single table reference contains both `USE` and `FORCE` hints, MyLite
  reports MySQL-compatible `1221 / HY000`.

## Diagnostics

Supported MySQL-compatible diagnostics:

- unsupported grammar or malformed hint syntax:
  `1064 / 42000`;
- all-missing index name list:
  `1176 / 42000`, `Key '<name>' doesn't exist in table '<table>'`;
- combined `USE INDEX` and `FORCE INDEX` on one table source:
  `1221 / HY000`, `Incorrect usage of USE INDEX and FORCE INDEX`;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, unknown columns, ambiguous columns, unsupported query clauses, and
  allocation failures continue to use existing MyLite diagnostics for the
  underlying `SELECT` shape.

Explicit MyLite diagnostics:

- index hints on `information_schema` virtual tables are rejected as
  unsupported for this phase instead of being silently ignored.

## Physical SQLite Handling

The generated SQLite SQL is exactly the same shape as the equivalent unhinted
MyLite plan. No hint text is interpolated into SQLite SQL, no SQLite optimizer
directive is generated, and no extra row materialization is introduced.

Descriptor-backed `SELECT` continues to use current table scan, filtering,
ordering, grouping, and projection code. Future planner phases may use the
retained AST hint nodes when MyLite begins generating physical indexes or
driving SQLite query plans from descriptor metadata.

## Tests

Add MySQL-runtime-verified expectation coverage for:

- `USE INDEX`, `USE KEY`, `FORCE INDEX`, `FORCE KEY`, `IGNORE INDEX`, and
  `IGNORE KEY`;
- omitted scope, `FOR JOIN`, `FOR ORDER BY`, and `FOR GROUP BY`;
- alias and joined table-source placement;
- primary-key and secondary-index hint names;
- duplicate hint names;
- full and unambiguous prefix hint names, including primary-key prefixes;
- ambiguous hint prefixes;
- `USE INDEX ()`;
- unknown hint names;
- combined `USE` plus `FORCE`;
- syntax rejection for empty `FORCE` and `IGNORE` lists;
- no warnings and unchanged row-count state for successful hinted `SELECT`;
- parser rejection of unsupported `DELETE ... USE INDEX(...)`.

Add MyLite C coverage for:

- parser AST shape with table name, alias, and hint list kept distinct;
- ordinary single-table `SELECT` hint no-op rows;
- joined `SELECT` hint no-op rows;
- grouped/ordered/limited SELECT shapes already supported by existing planners;
- unknown hint names and `USE` plus `FORCE` diagnostics;
- syntax errors for empty `FORCE` and `IGNORE`;
- preservation of existing parser, SELECT, join, aggregate, information schema,
  index lifecycle, and full workflow checks.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-query-hints.md`.

The docs must say this is a partial `SELECT` table index-hint acceptance and
validation slice. They must not claim optimizer behavior, UPDATE hint support,
DELETE hint support, full optimizer hints, or full query-plan compatibility.
