# Baseline Joined Alias HAVING Comparison

## Status

This slice executes a narrow non-grouped joined `SELECT ... HAVING` form where
the predicate compares two unique selected output aliases that each map directly
to descriptor columns:

```sql
SELECT left_source.column AS left_alias, right_source.column AS right_alias
FROM left_source JOIN right_source ON left_source.id = right_source.id
HAVING left_alias = right_alias;

SELECT left_source.column AS left_alias, right_source.column AS right_alias
FROM left_source JOIN right_source ON left_source.id = right_source.id
HAVING left_alias != right_alias;
```

It turns the remaining parser-corpus SELECT-clause residual HAVING probe into
an executable baseline without claiming broad MySQL HAVING expression support.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_parser_corpus_select_clause_residuals_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes verify:

- a non-grouped joined `SELECT` may use selected output aliases in `HAVING`;
- `HAVING alias = alias` keeps rows whose selected values compare equal;
- `HAVING alias != alias` keeps rows whose selected values compare unequal;
- rows where the comparison evaluates to `NULL` are not returned; and
- the output aliases continue to define result column names.

## Supported Surface

Supported predicate shape:

```sql
HAVING selected_alias = selected_alias
HAVING selected_alias != selected_alias
HAVING selected_alias <> selected_alias
```

Each alias must resolve uniquely through the current select list. The selected
item behind each alias must be a descriptor column reference, either qualified
or unqualified. The underlying descriptor columns must be compatible with the
existing MyLite same-scope column comparison rules.

The MyLite Lemon grammar for this slice is:

```lemon
having_predicate_atom ::= having_operand EQUAL qualified_identifier.
having_predicate_atom ::= having_operand NOT_EQUAL qualified_identifier.
```

## Runtime Semantics

The joined `SELECT` planner resolves each HAVING operand as a selected output
name, validates that the name is unique, maps the selected item back to its
descriptor column, and plans the predicate as a SQLite column-to-column
comparison. This keeps filtering inside SQLite and avoids materializing joined
rows in MyLite.

Unknown aliases return MySQL-shaped unknown-HAVING-column diagnostics.
Duplicate selected output names return MySQL-shaped ambiguous-HAVING-column
diagnostics. Unsupported expressions return deterministic MyLite unsupported
diagnostics.

No SQLite fork hook, catalog change, file-format change, VFS change, or public
ABI change is needed.

## Non-Goals

This slice does not add:

- arbitrary HAVING expressions;
- literal RHS predicates for joined non-grouped HAVING;
- function, aggregate, row-scalar, subquery, parameter, system-variable, or
  user-variable HAVING operands;
- broad MySQL group-resolution or alias-shadowing rules;
- selected expression alias execution beyond direct descriptor columns;
- comparison operators beyond `=`, `!=`, and `<>`; or
- new collation, type-conversion, aggregate, or warning behavior.

Unsupported forms continue to use deterministic MyLite diagnostics.

## Validation

Coverage includes:

- MySQL 8.4.9 expectation rows for equality and inequality alias HAVING probes;
- parser-corpus movement from placeholder classification to normal parse;
- runtime parser-corpus execution over a joined descriptor source; and
- focused inner-join runtime coverage that equal and `NULL` comparison rows are
  filtered out while the unequal row remains.
