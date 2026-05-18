# Baseline SHOW Metadata REGEXP Filters

## Summary

This phase extends the existing descriptor-driven `SHOW ... WHERE` metadata
filters with the same narrow `REGEXP` / `RLIKE` predicate subset already used
for table row predicates. The admitted statements are:

```sql
SHOW [FULL] {COLUMNS | FIELDS} {FROM | IN} table_name ... WHERE output_column REGEXP 'pattern'
SHOW {INDEX | INDEXES | KEYS} {FROM | IN} table_name ... WHERE output_column RLIKE 'pattern'
SHOW TABLE STATUS ... WHERE output_column [NOT] REGEXP 'pattern'
```

The feature evaluates the predicate over the displayed metadata cells already
built by MyLite. It does not query SQLite schema text, does not add a general
metadata expression engine, and does not widen `SHOW VARIABLES`, `SHOW STATUS`,
or other `SHOW` statements.

## Compatibility Authority

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `COMPATIBILITY.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-show-columns-where/specs.md`
  - `docs/specs/baseline-show-index-where/specs.md`
  - `docs/specs/baseline-show-table-status-where/specs.md`
  - `docs/specs/baseline-regexp-rlike-predicates/specs.md`
- Official MySQL 8.4 documentation:
  - `SHOW INDEX`: <https://dev.mysql.com/doc/refman/8.4/en/show-index.html>
  - `SHOW TABLE STATUS`: <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
  - extensions to `SHOW` statements:
    <https://dev.mysql.com/doc/refman/8.4/en/extended-show.html>
  - regular expressions: <https://dev.mysql.com/doc/refman/8.4/en/regexp.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_show_metadata_regexp_filters_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against MySQL 8.4.9 establish these expectations for this
slice:

- `SHOW COLUMNS`, `SHOW FULL COLUMNS`, `SHOW INDEX`, and `SHOW TABLE STATUS`
  accept `REGEXP` and `RLIKE` inside their trailing `WHERE` clauses.
- `REGEXP` and `RLIKE` are synonyms. `NOT REGEXP` and `NOT RLIKE` behave as
  logical negation of the regex predicate.
- The left operand resolves against the displayed output column names for the
  current `SHOW` statement.
- A SQL `NULL` displayed cell produces SQL `UNKNOWN` for `REGEXP`; it is not
  selected by either `REGEXP` or `NOT REGEXP` in a `WHERE` filter.
- The default metadata collation makes the verified ASCII regex matches
  case-insensitive, except `SHOW TABLE STATUS` `Name` follows the current
  `lower_case_table_names = 0` catalog policy and matches table names
  case-sensitively.
- Non-`NULL` numeric metadata cells are matched as their displayed text.
- Invalid regex patterns are diagnosed before returning a result, including
  `3696 / HY000` for an unclosed bracket expression and `3697 / HY000` for an
  invalid character range.
- Successful supported filters leave `@@warning_count == 0`,
  `@@error_count == 0`, and make `ROW_COUNT()` return `-1`.

## Ownership Boundaries

- Public API: no ABI or public-header change. Successful statements return
  through the existing result-set conventions for metadata statements.
- Statement context: successful filters are result-producing statements with
  affected rows `0`, warning count `0`, and previous row count `-1`.
- Lexer/parser/AST: no new grammar is required. Existing predicate grammar
  already represents `REGEXP`, `RLIKE`, `NOT REGEXP`, and `NOT RLIKE`.
- Runtime/analyzer: the existing `SHOW` result builders continue to resolve
  schema/table targets, build descriptor-owned metadata rows, and evaluate
  filter predicates before appending rows. This phase only widens comparison
  predicate evaluation for displayed metadata cells.
- Catalog: descriptors remain authoritative for table, column, index, and
  status metadata. This feature is read-only and does not mutate catalog rows,
  descriptor versions, caches, catalog generation, or SQLite schema generation.
- SQLite physical storage: no generated SQLite SQL is added. Regex matching is
  performed in MyLite over metadata cell strings using the existing MyLite
  baseline regex engine, not by SQLite.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are unaffected.

## Syntax

This phase reuses the existing `SHOW COLUMNS WHERE`, `SHOW INDEX WHERE`, and
`SHOW TABLE STATUS WHERE` grammar. The independently authored added predicate
surface is:

```ebnf
show_metadata_regexp_predicate:
    output_column REGEXP string_literal
  | output_column RLIKE string_literal
  | output_column NOT REGEXP string_literal
  | output_column NOT RLIKE string_literal
```

The predicate may be combined with existing parenthesized predicates, `NOT`,
`AND`, and `OR` in these `SHOW` contexts. `XOR`, functions, column-to-column
comparisons, subqueries, and general expressions remain outside the current
metadata filter surface.

### MyLite Lemon-Syntax Snippet

No new parser rule is required; this phase relies on the existing predicate
rules:

```lemon
predicate_atom(A) ::= qualified_identifier(C) REGEXP(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}

predicate_atom(A) ::= qualified_identifier(C) RLIKE(O) STRING(T). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING));
}
```

The snippets describe MyLite's admitted subset and are not MySQL grammar text.

## Semantics

Evaluation is row-local over displayed metadata cells:

1. Resolve the left operand as an output column supported by the current
   `SHOW` statement.
2. Decode the right operand as an ordinary string literal using current MyLite
   SQL-mode string-literal rules.
3. Reject decoded `NUL` bytes, non-ASCII pattern bytes, unsupported regex
   constructs, and invalid supported-subset regex syntax before matching.
4. For a SQL `NULL` left cell, return `UNKNOWN`.
5. For a non-`NULL` left cell, match the displayed cell text using the existing
   baseline ASCII case-insensitive regex implementation, except `SHOW TABLE
   STATUS` `Name` uses the same case-sensitive table-name policy as existing
   equality and `LIKE` filters.

The supported pattern subset is exactly the current
`baseline-regexp-rlike-predicates` subset: ASCII literals, `.`, `^`, `$`,
bracket classes and ranges, negated bracket classes, and `*`, `+`, and `?`
quantifiers. Alternation, grouping, counted repetition, Unicode/ICU-specific
syntax, backreferences, match-control arguments, and binary-string semantics
remain deferred.

`NOT REGEXP` and `NOT RLIKE` use the existing three-valued `NOT` logic in the
metadata predicate evaluator, so `NULL NOT REGEXP 'x'` remains `UNKNOWN`.

## Diagnostics

Supported runtime diagnostics:

- unknown output columns and qualified output-column references: existing
  `1054 / 42S22` unknown-column diagnostics;
- non-output expressions where an output column is required: existing
  syntax-error or deterministic unsupported diagnostics;
- non-string pattern operands: deterministic unsupported diagnostics for the
  current `SHOW ... WHERE REGEXP` context;
- decoded `NUL` bytes or non-ASCII pattern bytes: deterministic unsupported
  diagnostics;
- unsupported regex constructs: deterministic unsupported diagnostics for
  MyLite's baseline ASCII regex subset;
- invalid supported-subset regex syntax: MySQL-compatible regex diagnostics
  where already verified by the row-predicate slice, including `3696 / HY000`
  and `3697 / HY000`;
- allocation failures: existing out-of-memory diagnostics.

Successful supported statements return the normal metadata result set, warning
count `0`, affected rows `0`, and no catalog or file-format mutation.

## Tests

Coverage is provided by:

- MySQL 8.4.9 expectation artifact:
  `packages/libmylite/tests/mysql_baseline_show_metadata_regexp_filters_expectations.sh`;
- runtime C tests extending:
  - `packages/libmylite/tests/runtime_show_columns_introspection_test.c`;
  - `packages/libmylite/tests/runtime_show_full_columns_introspection_test.c`;
  - `packages/libmylite/tests/runtime_show_index_empty_introspection_test.c`;
  - `packages/libmylite/tests/runtime_show_table_status_introspection_test.c`.

Tests must cover `REGEXP`, `RLIKE`, `NOT REGEXP`, SQL `NULL` displayed cells,
numeric displayed cells, invalid pattern diagnostics, result metadata,
warning count, row count, no-match filters, reopen persistence already covered
by the underlying `SHOW ... WHERE` suites, and no `.mylite` preamble mutation.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` to
remove the old explicit `REGEXP` exclusion only for the admitted `SHOW COLUMNS`,
`SHOW INDEX`, and `SHOW TABLE STATUS` metadata filter surfaces. Do not mark
arbitrary `SHOW ... WHERE` expressions, full ICU regex behavior, or other
`SHOW` statements as supported.
