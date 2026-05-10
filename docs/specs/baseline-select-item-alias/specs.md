# Baseline SELECT Item Alias

## Summary

This feature adds select-item aliases to the `SELECT` forms MyLite already
supports. The alias is projection metadata: it changes the public result column
label and can be used by the supported table-backed `ORDER BY` resolver, but it
does not change catalog descriptors, physical SQLite column names, or row
storage.

The supported surface is:

```sql
SELECT expression [AS] alias
SELECT column_name [AS] alias[, column_name [AS] alias ...] FROM table_name ...
SELECT DISTINCT column_name [AS] alias FROM table_name ...
SELECT COUNT(...) [AS] alias FROM table_name ...
SELECT MIN(column_name) [AS] alias FROM table_name ...
SELECT MAX(column_name) [AS] alias FROM table_name ...
... ORDER BY alias [ASC | DESC]
```

`alias` may be an identifier, quoted identifier, or string literal. Expression
support is still exactly the existing baseline expression support: aliases do
not admit new projection expressions.

## Sources And Runtime Evidence

Normative sources:

- MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- MySQL 8.4 Reference Manual, problems with column aliases:
  <https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html>

The MySQL documentation describes optional select-expression aliases using
`AS alias_name`, states that aliases may be used in `ORDER BY`, and documents
that quoted string aliases are accepted in the select list. It also documents
that aliases are not visible in `WHERE` and that a string-quoted alias must be
referenced with identifier quoting in later clauses; string quotes in `ORDER BY`
are parsed as a string literal.

Observed MySQL 8.4.9 behavior, using local Docker runtime
`mylite-mysql-849`:

```sql
SELECT VERSION();
-- 8.4.9

CREATE DATABASE mylite_select_alias_probe;
USE mylite_select_alias_probe;
CREATE TABLE t(id INT NOT NULL, n INT NULL, nn INT NOT NULL);
INSERT INTO t VALUES (1,10,5),(2,NULL,6),(3,20,7);

DO 0;
SELECT n AS x, nn y FROM t ORDER BY id;
SELECT @@warning_count, ROW_COUNT();

SELECT n AS `Customer identity` FROM t ORDER BY id LIMIT 1;
SELECT n AS 'Customer identity' FROM t ORDER BY `Customer identity` DESC LIMIT 1;
SELECT n 'Customer identity' FROM t ORDER BY id LIMIT 1;

SELECT n AS X FROM t ORDER BY x DESC;
SELECT n AS id FROM t ORDER BY id;
SELECT n AS id FROM t WHERE id = 2;
SELECT n AS x FROM t WHERE x = 10;
SELECT id AS x, n AS x FROM t ORDER BY x;

SELECT DISTINCT n AS x FROM t ORDER BY x;
SELECT COUNT(*) AS c FROM t;
SELECT COUNT(n) cn FROM t;
SELECT COUNT(DISTINCT n) AS cd FROM t;
SELECT MIN(n) AS mn FROM t;
SELECT MAX(n) mx FROM t;
SELECT DATABASE() AS d, USER() u, CURRENT_USER AS cu, @@warning_count AS wc;
```

Observed results:

- Identifier, quoted-identifier, and string-literal aliases set the result
  column label.
- Bare aliases without `AS` are accepted for identifiers and string literals.
- Successful row-returning statements leave `@@warning_count = 0` and the
  following `ROW_COUNT() = -1`.
- `ORDER BY alias` resolves aliases case-insensitively.
- An alias shadows a source column name in `ORDER BY`: `SELECT n AS id ...
  ORDER BY id` sorts by `n`, not by the source `id` column.
- `WHERE` does not resolve select-item aliases. `WHERE id` in
  `SELECT n AS id ...` still resolves the source `id` column, while an alias
  with no source-column match reports `1054 (42S22) Unknown column ... in
  'where clause'`.
- Duplicate aliases in `ORDER BY` are ambiguous and report `1052 (23000)
  Column '<name>' in order clause is ambiguous`.
- `ORDER BY 'alias'` is a constant string expression in MySQL, not an alias
  reference. MyLite does not admit string-literal order keys in this slice.
- `SELECT * AS alias` is a syntax error.

## Scope

In scope:

- aliases on existing no-source and `FROM DUAL` scalar/session `SELECT`
  expressions;
- aliases on existing descriptor-backed selected columns, including
  source-qualified selected columns;
- aliases on existing descriptor-backed `SELECT ALL` projection lists;
- aliases on the current one-column `SELECT DISTINCT` and `DISTINCTROW` forms;
- aliases on existing `COUNT(*)`, `COUNT(column)`, `COUNT(integer/NULL/boolean
  literal)`, `COUNT(DISTINCT column)`, `MIN(column)`, and `MAX(column)` forms;
- `AS alias` and bare alias syntax;
- identifier, quoted-identifier, and string-literal aliases;
- result column labels from the decoded alias text;
- one unqualified table-backed `ORDER BY` key resolving first against
  select-item aliases, then against the descriptor column resolver;
- existing `ASC`/`DESC`, `WHERE`, `LIMIT`/`OFFSET`, table-alias, qualified
  column, warning, and `ROW_COUNT()` behavior.

Out of scope:

- aliases on `SELECT *` and qualified wildcards;
- aliases as `WHERE` names;
- alias resolution in unsupported `GROUP BY`, `HAVING`, windows, set
  operations, or locking clauses;
- `ORDER BY` string literals, ordinals, expressions, qualified alias names,
  multiple keys, collations, or tie-order guarantees;
- aliases that admit new expression projections, arithmetic, column-to-column
  expressions, functions outside the existing scalar/session and aggregate
  slices, parameters, subqueries, CTEs, joins, derived tables, or arbitrary
  SQLite pass-through;
- protocol-grade origin metadata, charset metadata, and metadata flags.

## Architecture

The public API and statement context are unchanged. A successful aliased
`SELECT` returns through the existing public result object; non-row statements
and diagnostics are unaffected.

The parser stores an optional alias as the second child of
`MYLITE_SQL_AST_SELECT_ITEM`. Child `0` remains the expression node. The alias
child is either `MYLITE_SQL_AST_IDENTIFIER` or a string
`MYLITE_SQL_AST_LITERAL`. Existing AST nodes for expressions, table sources,
predicates, ordering, and limits remain unchanged.

The analyzer/runtime treats aliases as projection metadata:

- table sources still resolve through MyLite's selected/default schema policy
  and catalog descriptors;
- descriptor columns remain authoritative for projection values, predicates,
  aggregate arguments, distinct values, and fallback order keys;
- aliases are decoded into separate projection-label storage and do not mutate
  descriptor names;
- unsupported projection expressions remain unsupported even when aliased;
- `WHERE` keeps using descriptor column resolution only and never sees
  select-item aliases;
- table-backed `ORDER BY` resolves an unqualified key by checking projection
  aliases before descriptor columns, matching MySQL's alias-shadowing behavior;
- if more than one projected alias matches the unqualified order key under the
  current ASCII case-insensitive name matching policy, MyLite reports an
  ambiguous-order diagnostic;
- if no alias matches, existing descriptor order-column resolution runs;
- generated SQLite SQL uses stable physical table and column names such as
  `_mylite_user_table_<table_id>` and descriptor column names, never alias
  text;
- generated SQLite identifiers remain quoted and predicate/limit values remain
  bound parameters; and
- no catalog rows, descriptor generations, `sqlite_schema_generation`, file
  preamble bytes, VFS behavior, or SQLite fork code change.

This feature adds no row materialization beyond the existing public
`mylite_result` object. SQLite still performs table scans, filtering, sorting,
limiting, duplicate removal, and aggregate computation for supported
descriptor-backed statements.

## Syntax

MyLite grammar snippets, independently authored for this slice:

```lemon
select_alias ::= identifier.
select_alias ::= STRING.

select_item ::= expression.
select_item ::= expression AS select_alias.
select_item ::= expression select_alias.

order_clause_opt ::= ORDER BY qualified_identifier order_direction_opt.
```

The `order_clause_opt` grammar remains intentionally narrow. Alias resolution
is an analyzer rule for one-part `qualified_identifier` nodes; string-literal
order expressions and general order expressions are not admitted in this
slice.

## Semantics

For supported statements, an alias changes the visible result column name.

- Identifier aliases use their decoded identifier text. Quoted identifiers
  decode doubled backticks.
- String-literal aliases use the current MyLite string-literal decoder for
  SQL quoted text and escape sequences.
- Alias labels preserve the spelling and bytes of the decoded alias. Name
  matching for `ORDER BY alias` follows MyLite's current descriptor name
  policy, which is ASCII case-insensitive.
- Duplicate output labels are allowed.
- `SELECT * AS alias` remains a syntax error.
- Aliasing an expression does not make unsupported expression values
  executable.
- Successful row-returning statements return result rows, `affected_rows == 0`,
  `warning_count == 0`, and following `ROW_COUNT() == -1`.

For supported table-backed `ORDER BY`:

- only one unqualified order key can resolve as an alias;
- aliases take precedence over descriptor column names;
- an alias match must refer to a projected descriptor column expression in this
  slice, because general expression ordering is still out of scope;
- duplicate alias matches are ambiguous;
- if no alias matches, existing descriptor-column ordering applies;
- `ASC` is the default, and existing `NULL` ordering is preserved;
- no additional tie order is guaranteed.

For `SELECT DISTINCT column AS alias`, `ORDER BY alias` is equivalent to
ordering by that selected column. The existing rule that distinct ordering must
resolve to the selected descriptor column still applies after alias expansion.

## Diagnostics

Expected diagnostics for this slice:

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted grammar | existing parse error `1064` / `42000` |
| `SELECT * AS alias` | parse error `1064` / `42000` |
| Unsupported projection expression with alias | existing unsupported-expression diagnostic |
| Missing default schema for unqualified source table | existing `1046` / `3D000` |
| Unknown schema or unknown source table | existing table-resolution diagnostics |
| Reserved source schema/table name | existing reserved-name diagnostics |
| Unknown selected column | existing `1054` / `42S22`, field-list clause |
| Unknown predicate column, including attempted alias in `WHERE` | existing `1054` / `42S22`, where clause |
| Unknown order key after alias and descriptor fallback | existing `1054` / `42S22`, order clause |
| Duplicate matching order aliases | `1052` / `23000`, ambiguous column in order clause |
| Qualified alias in `ORDER BY` | unknown-column diagnostic after descriptor fallback |
| String-literal `ORDER BY` key | parse or unsupported diagnostic |
| Alias literal decoding failure or too-long alias | deterministic alias diagnostic or allocation diagnostic |
| Physical SQLite failure | existing internal SQLite row-operation diagnostic |
| Allocation failure | existing allocation diagnostic |

## Tests

Add MySQL-runtime expectation coverage and fast C tests for:

- identifier, bare, quoted-identifier, and string-literal aliases;
- result column labels for descriptor columns, multiple projections, `ALL`,
  `DISTINCT`, `DISTINCTROW`, aggregates, scalar/session functions, system
  variables, no-source `SELECT`, and `FROM DUAL`;
- `ORDER BY` alias default direction, `ASC`, `DESC`, nullable columns, and
  existing `NULL` ordering;
- alias shadowing of descriptor column names in `ORDER BY`;
- alias case-insensitive `ORDER BY` matching;
- duplicate alias ambiguity;
- `WHERE` ignoring aliases and still resolving descriptor columns;
- schema-qualified and table-aliased sources combined with select-item aliases;
- unknown selected, predicate, and order columns with aliases present;
- unsupported star alias, expression aliases outside the existing expression
  subset, ordinal order keys, string order keys, expression order keys, and
  qualified alias order keys;
- reopen persistence, table rename/drop interactions, independent handles,
  result conventions, and file-format preservation through representative
  aliased selects; and
- existing parser, lexer, runtime lifecycle, aggregate, scalar/session, table
  alias, qualified-column, select-order-limit, and select-where tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/functions-aggregate.md`, and
`docs/compatibility/functions-system.md` for the exact limited alias subset.
Do not claim support for general projection expressions, full result metadata,
qualified wildcards, alias use in `WHERE`, full `ORDER BY`, grouping, having,
joins, or protocol-grade metadata.
