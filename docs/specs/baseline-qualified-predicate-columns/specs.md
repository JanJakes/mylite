# Baseline Qualified Predicate Columns

## Status

Implemented in this slice.

## Sources

- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `DELETE`:
  https://dev.mysql.com/doc/refman/8.4/en/delete.html
- MySQL 8.4 Reference Manual, `UPDATE`:
  https://dev.mysql.com/doc/refman/8.4/en/update.html
- MySQL 8.4.9 runtime observations recorded in
  `packages/libmylite/tests/mysql_baseline_where_and_predicates_expectations.sh`.

## Scope

MySQL accepts column references in row conditions with optional qualifiers. This
slice extends MyLite's existing descriptor-backed `WHERE` predicate planner so
the current supported predicate atoms accept matching qualified descriptor
column subjects in single-source contexts:

- filtered single-table `SELECT`;
- aggregate-source filters that use the same single-source descriptor context;
- single-table `DELETE`;
- single-table `UPDATE`.

The supported qualifier shapes are, when the statement form exposes the corresponding source name:

- `column_name`;
- `table_name.column_name` when the source has no alias;
- `alias_name.column_name` when the source has an alias, such as supported `SELECT` and single-table `DELETE` forms;
- `schema_name.table_name.column_name` when the source has no alias.

The slice covers the current descriptor-backed predicate atoms, including
comparisons, `BETWEEN` / `NOT BETWEEN`, `IN` / `NOT IN`, `IS` / `IS NOT`,
`LIKE` / `REGEXP` where already supported, parenthesized boolean expressions,
`NOT`, `AND`, `XOR`, and `OR`.

## Semantics

Qualified column resolution is a name-resolution change, not a new expression
engine. After a qualified predicate subject resolves to the same descriptor
column that the unqualified predicate would have used, all existing conversion,
range, collation, warning, and generated-SQL behavior is reused.

For single-source statements without an alias, `table_name.column_name` and
`schema_name.table_name.column_name` resolve to the target/source table. For
aliased statements, the alias hides the base table name for two-part predicate
subjects, matching MySQL's alias-scoping behavior for the covered cases.

Examples:

```sql
SELECT id FROM numbers WHERE numbers.i = 1;
SELECT id FROM numbers WHERE app.numbers.i BETWEEN -2 AND 1;
DELETE FROM numbers WHERE numbers.i IN (-2, 1, 0);
UPDATE numbers SET n = 13 WHERE numbers.i = 0;
DELETE FROM numbers AS n WHERE n.i IS TRUE;
```

Unsupported qualifier targets return the existing MySQL-compatible unknown
column diagnostic for the `WHERE` context. They are no longer classified as a
generic "unqualified predicate columns only" limitation when a single source is
known.

## Parser Strategy

The primary grammar already admits qualified identifiers as descriptor
predicate subjects. This slice does not add new tokens, AST node kinds, or
general expression grammar.

## MyLite Lemon-Syntax Snippet

The implemented grammar shape for this slice is the already-owned predicate
identifier surface:

```lemon
predicate_atom ::= qualified_identifier comparison_operator predicate_comparison_value.
predicate_atom ::= qualified_identifier BETWEEN predicate_range_value AND predicate_range_value.
predicate_atom ::= qualified_identifier IN LP predicate_in_value_list RP.
predicate_atom ::= qualified_identifier IS is_predicate_value.
predicate_atom ::= qualified_identifier LIKE predicate_pattern_value.
predicate_atom ::= qualified_identifier REGEXP predicate_pattern_value.

qualified_identifier ::= identifier.
qualified_identifier ::= identifier DOT identifier.
qualified_identifier ::= identifier DOT identifier DOT identifier.
```

The parser acceptance above does not imply arbitrary expression support. Runtime
planning still admits only the documented descriptor-backed predicate atoms and
value forms.

## Runtime Strategy

The existing descriptor resolver already knows how to validate a qualifier
against a single `select_source_context`: unqualified names always remain
eligible, table-or-alias qualifiers must match the current source name, and
three-part qualifiers must match the selected schema and table when no alias is
present.

`SELECT` and `UPDATE` already pass that context to the predicate planner. This
slice makes single-table `DELETE` do the same for predicate planning. DML
`ORDER BY` keys keep the existing narrower unqualified descriptor-column
subset. No SQLite fork hook, public SQLite extension API, file-format change,
catalog mutation, or new dependency is required.

Generated SQL remains the existing single-table SQL for non-joined statements.
The qualifier is consumed during MyLite descriptor resolution; the physical
SQLite statement continues to reference the resolved column in the current
target table.

## Diagnostics

- Unknown qualified predicate columns report MySQL error `1054` / SQLSTATE
  `42S22` with the existing `where clause` context.
- Missing schema/table diagnostics for the enclosing statement are unchanged.
- Alias scoping remains strict: when a source alias is present, a two-part
  predicate subject must use that alias.
- Unsupported predicate value forms, unsupported column families, out-of-range
  literals, and parser-only expression forms continue to use their existing
  diagnostics.

## Deferred Work

- General table-backed expression predicates beyond documented row-scalar
  slices.
- Full joined-DML parity outside the already documented joined `DELETE` and
  `UPDATE` subsets.
- Qualifier behavior in unsupported `ON`, broad `HAVING`, window, CTE, trigger,
  stored-program, and routine contexts.
- Parameters, row constructors, and full MySQL expression metadata.
