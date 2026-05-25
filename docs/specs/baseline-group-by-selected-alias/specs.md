# Baseline GROUP BY Selected Alias

## Purpose

This phase extends the existing descriptor-driven grouped `SELECT` baseline so
`GROUP BY` keys may name selected descriptor-column aliases when no descriptor
column with that unqualified name exists in the source descriptor scope.

The target shape is common application SQL such as:

```sql
SELECT option_name AS name, COUNT(*) AS c
FROM wp_options
GROUP BY name
ORDER BY name
```

This is not full MySQL grouping expression support. It only maps an admitted
grouping alias back to the selected descriptor column and then uses the
existing grouped planner and SQLite execution path.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing grouped aggregate specs:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`,
  `docs/specs/baseline-group-by-string-column/specs.md`,
  `docs/specs/baseline-group-by-multiple-keys/specs.md`, and
  `docs/specs/baseline-group-by-primary-key-projection/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_group_by_selected_alias_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted subset:

- `GROUP BY selected_alias` is accepted when `selected_alias` names an alias
  for a selected descriptor column and no source descriptor column has that
  unqualified name.
- Multiple group keys may mix selected aliases and ordinary descriptor column
  references.
- A grouped alias may enable primary-key functional-dependence projection when
  the alias maps to the complete primary-key descriptor column.
- If a source descriptor column has the same unqualified name as the `GROUP BY`
  identifier, MySQL resolves that identifier as the source column before the
  selected alias. Under default `ONLY_FULL_GROUP_BY`, this can produce
  `1055 / 42000` if the projection is not functionally dependent on that source
  column.
- Duplicate selected aliases matching the `GROUP BY` identifier fail with
  `1052 / 23000` and an ambiguous-column diagnostic.
- MySQL accepts expression aliases such as `SELECT id + 1 AS k ... GROUP BY k`.
  This phase intentionally defers that broader expression grouping surface.
- Successful admitted statements produce `@@warning_count = 0` and make
  `ROW_COUNT()` return `-1`.

## Supported Surface

MyLite supports the current grouped aggregate `SELECT` shape with one to four
group keys:

```sql
SELECT descriptor_column [AS alias]
       [, descriptor_column [AS alias] ...]
       [, aggregate [AS alias] ...]
FROM source
[WHERE baseline_predicate]
GROUP BY group_key [, group_key ...]
[HAVING baseline_grouped_having_predicate]
[ORDER BY baseline_grouped_order_key [ASC | DESC]]
[LIMIT baseline_select_limit]
```

For this phase, each `group_key` may be:

- an existing descriptor group column reference; or
- an unqualified identifier that uniquely matches a selected alias whose select
  item expression is a descriptor column reference.

The alias target descriptor column must pass the existing grouped-key
validation: integer-family or ASCII nonbinary string (`CHAR`, `VARCHAR`, or
baseline `TEXT` family). A selected alias can target a source-qualified
descriptor column such as `t.name AS n`; the `GROUP BY` key itself must be the
one-part alias name.

Descriptor resolution remains source-column-first for unqualified `GROUP BY`
identifiers. Alias lookup runs only after descriptor resolution reports that no
source descriptor column matches the `GROUP BY` identifier.

## Explicit Non-Goals

This phase does not add:

- grouping aliases when the alias name is shadowed by a source descriptor column;
- selected expression aliases such as `id + 1 AS k`;
- selected aggregate aliases as group keys;
- aliases for wildcards or qualified wildcards as group keys;
- ordinal, literal, function-call, arithmetic, parenthesized, or general
  expression group keys;
- aliases in `ROLLUP`, grouping sets, windows, derived tables, CTEs, subqueries,
  or arbitrary SQLite pass-through;
- broader functional-dependence inference beyond the existing complete
  primary-key rule.

## Ownership And Architecture

- Public API: unchanged. Applications continue to call `mylite_execute()` and
  inspect the existing row-result object.
- Statement context: unchanged. Grouped result statements keep existing
  diagnostics, warning count, and `ROW_COUNT()` behavior.
- Lexer/parser/AST: no grammar expansion is needed. Existing select-item
  aliases and `GROUP BY` identifier nodes are reused.
- Analyzer/planner: owns alias matching, duplicate-alias detection,
  descriptor-column target validation, and source-column-first resolution.
- Catalog: remains authoritative for source schemas, tables, aliases, columns,
  type families, and primary keys. SQLite metadata is not consulted for logical
  name resolution.
- Runtime SQL generation: remains descriptor-built. A grouped alias is lowered
  to the target descriptor column's quoted physical SQLite identifier, not to a
  generated SQL alias.
- SQLite physical row storage: unchanged. SQLite still owns scanning,
  filtering, grouping, aggregation, ordering, and limiting.
- Storage/VFS: unchanged. This read-only feature does not mutate catalog rows,
  descriptor caches, SQLite schema generation, user rows, or the `.mylite`
  preamble.

## Grammar

No parser expansion is required. The independently authored subset remains:

```lemon
select_item ::= expression alias_opt.
expression ::= qualified_identifier.

group_clause_opt(A) ::= .
group_clause_opt(A) ::= GROUP(G) BY group_key_list(K). {
    A = mylite_sql_parser_make_group_by_clause(parser, G, K);
}

group_key_list(A) ::= qualified_identifier(K).
group_key_list(A) ::= group_key_list(L) COMMA qualified_identifier(K).
```

Runtime validation interprets a one-part `qualified_identifier` as a selected
alias only after ordinary descriptor-column resolution fails.

## Resolution Rules

For each `GROUP BY` key:

1. Resolve the key through the existing descriptor column resolver.
2. If descriptor resolution succeeds, use that descriptor column.
3. If descriptor resolution fails because no source column matches, and the key
   is a one-part identifier, scan the select list for aliases whose alias text
   matches case-insensitively.
4. If no matching selected alias exists, keep the existing descriptor resolver
   unknown-column diagnostic.
5. If more than one selected alias matches, return `1052 / 23000` with a
   MySQL-shaped ambiguous `GROUP BY` diagnostic.
6. If the unique selected alias points to a descriptor column select item,
   resolve that descriptor column through the catalog and use it as the group
   key.
7. If the unique selected alias points to anything else, return a deterministic
   unsupported-feature diagnostic for the deferred expression-alias grouping
   surface.

Alias text matching uses the existing select-item alias decoder and
case-insensitive identifier comparison. Alias lookup admits identifier, quoted
identifier, and string-literal aliases through the existing select-item alias
rules. Duplicate aliases are allowed elsewhere; they are rejected only when a
group key needs to resolve through that ambiguous alias name.

## Semantics

Once a `GROUP BY` alias resolves to a descriptor column, all existing grouped
semantics apply:

- integer and ASCII nonbinary string group keys use the current type-specific
  grouping and ordering rules;
- `NULL` key tuples form one group;
- string keys use MyLite's registered ASCII `utf8mb4_0900_ai_ci` collation in
  generated grouping and ordering expressions;
- selected descriptor columns remain legal when they are grouped or determined
  by the complete grouped primary key of their own source;
- `WHERE`, `HAVING`, `ORDER BY`, and `LIMIT` retain their current grouped
  subsets;
- no result row order is promised without an admitted `ORDER BY`;
- successful statements return a row result set, report `warning_count == 0`
  for supported in-range queries, and leave affected-row state at the existing
  result-set convention.

## SQLite Handling

This feature uses MyLite wrapper/planner translation over public SQLite APIs.
No SQLite fork patch is required.

For:

```sql
SELECT name AS n, COUNT(*) AS c FROM users GROUP BY n ORDER BY n
```

MyLite plans `n` as the `name` descriptor column and emits physical SQL shaped
like:

```sql
SELECT "name", COUNT(*)
FROM "_mylite_user_table_<table_id>"
GROUP BY "name" COLLATE "utf8mb4_0900_ai_ci"
ORDER BY "name" COLLATE "utf8mb4_0900_ai_ci" ASC
```

Every generated SQLite identifier is quoted. Predicate, `HAVING`, aggregate
option, limit, and offset values remain bound parameters.

## Diagnostics

Existing diagnostics remain for syntax errors, missing/default schema, unknown
schema, unknown table, reserved names, unsupported object kinds, unknown
ordinary descriptor columns, unsupported group-key descriptor types,
unsupported aggregate arguments, unsupported `HAVING`/`ORDER BY`/`LIMIT`
forms, physical SQLite failures, allocation failures, and public API misuse.

This phase adds or relies on:

- duplicate selected group aliases:
  `1052 / 23000 Column '<alias>' in group statement is ambiguous`;
- selected alias resolving to an unsupported expression:
  `GROUP BY supports selected descriptor-column aliases only`;
- selected alias resolving to an unsupported descriptor type: the existing
  grouped-key type diagnostic.

## Tests

Fast C tests must cover:

- basic selected string alias grouping with result labels;
- selected integer alias grouping;
- selected aliases with multiple group keys;
- alias-backed grouping with complete primary-key projection;
- descriptor-column-first behavior when an alias name matches a source
  descriptor column;
- duplicate selected aliases;
- selected expression aliases rejected deterministically;
- unknown group aliases preserving the existing unknown-column diagnostic;
- `HAVING`, `ORDER BY`, and `LIMIT` interaction with alias-backed groups;
- warning count `0`, `ROW_COUNT() = -1`, and no affected rows for successful
  result statements.

The MySQL expectation script records the MySQL 8.4.9 behavior used by these
tests. Existing grouped aggregate, parser, runtime, file-format, and workflow
checks must continue to pass.
