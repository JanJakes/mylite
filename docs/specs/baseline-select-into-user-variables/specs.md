# Baseline SELECT INTO User Variables

## Scope

This slice implements top-level `SELECT ... INTO @user_variable[, ...]` for the
currently supported MyLite `SELECT` result surfaces.

Supported:

- `SELECT select_list INTO @var[, ...]`
- `SELECT select_list INTO @var[, ...] FROM ...`
- `SELECT select_list FROM ... [ORDER BY ...] [LIMIT ...] [locking] INTO @var[, ...]`
- Assignment into handle-local MyLite user variables.
- Empty public result metadata with MySQL-compatible row-count, warning, and
  found-rows side effects.

Out of scope:

- Stored-program local variables, routine block scope, cursors, handlers, and
  diagnostics areas.
- `SELECT ... INTO OUTFILE` and `SELECT ... INTO DUMPFILE`.
- `TABLE ... INTO`, `VALUES ... INTO`, compound query `INTO`, and CTE-specific
  placement.
- Deprecated middle placement before a trailing locking clause, such as
  `SELECT ... FROM t INTO @v FOR UPDATE`.
- MySQL placement deprecation warnings.
- Expressions or predicates beyond the existing supported MyLite `SELECT`
  executor.

## References

- Official MySQL 8.4 Reference Manual, `SELECT ... INTO`:
  <https://dev.mysql.com/doc/refman/8.4/en/select-into.html>
- Official MySQL 8.4 Reference Manual, user variables:
  <https://dev.mysql.com/doc/refman/8.4/en/user-variables.html>

Runtime expectations were verified against the local `mysql:8.4.9` comparison
container `mylite-mysql-849` with direct `mysql -uroot` probes.

Observed MySQL 8.4.9 diagnostics:

- zero selected rows succeeds, does not change target variables, and emits
  warning `1329`, SQLSTATE `02000`, message
  `No data - zero rows fetched, selected, or processed`;
- more than one selected row errors with `1172`, SQLSTATE `42000`, message
  `Result consisted of more than one row`;
- variable count and column count mismatch errors with `1222`, SQLSTATE
  `21000`, message `The used SELECT statements have a different number of columns`;
- `ROW_COUNT()` after `SELECT ... INTO` is `1` for one assigned row and `0` for
  zero rows;
- `FOUND_ROWS()` follows the selected row count, or the `SQL_CALC_FOUND_ROWS`
  count where that existing SELECT path supplies it.

## MyLite Grammar

Independent Lemon-shape grammar for this slice:

```lemon
select_statement ::= SELECT select_modifiers select_item_list INTO select_into_list ...
select_statement ::= SELECT select_modifiers select_item_list ... select_locking_clause_opt INTO select_into_list

select_into_list ::= user_variable
select_into_list ::= select_into_list COMMA user_variable
```

The AST appends `MYLITE_SQL_AST_SELECT_INTO_LIST` as an extra direct child of
the existing `MYLITE_SQL_AST_SELECT_STATEMENT`. Existing fixed children for
select list, source, predicates, grouping, ordering, and limits keep their
indexes unchanged.

## Runtime Semantics

The runtime executes the underlying supported `SELECT` normally, then applies
MySQL `SELECT ... INTO` post-processing:

1. Column count must equal target variable count.
2. Zero selected rows append warning `1329` and leave variables unchanged.
3. More than one selected row errors with `1172` and leaves variables unchanged.
4. Exactly one selected row copies each selected cell into the corresponding
   session user variable.
5. The public result has zero columns and zero rows.
6. `ROW_COUNT()` records the number of assigned rows, `0` or `1`.
7. `FOUND_ROWS()` records the selected count from the underlying result.

User variable values remain session-owned and nonpersistent. This is a MyLite
wrapper/runtime feature; it uses no SQLite fork hook and does not create
additional storage.

## Tests

Focused tests:

- `libmylite.parser.select_into_user_variables`
- `libmylite.runtime.select_into_user_variables`

The tests cover scalar and table-backed assignment, pre-`FROM` and trailing
placements, post-locking trailing placement, empty public results, `ROW_COUNT()`,
`FOUND_ROWS()`, zero-row warnings, column-count mismatch, multi-row errors, and
variable preservation on warning/error paths.
