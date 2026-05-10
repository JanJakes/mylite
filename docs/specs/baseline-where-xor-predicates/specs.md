# Baseline WHERE XOR Predicates

## Summary

This phase adds the next narrow descriptor-backed `WHERE` predicate composition
operator:

```sql
predicate XOR predicate
```

The supported surface is limited to the existing descriptor-backed predicate
atoms and parenthesized predicate expressions already admitted for table
`SELECT`, aggregate source filters, `CREATE TABLE ... SELECT`, `INSERT ...
SELECT`, `REPLACE ... SELECT`, single-table `DELETE`, and single-table
`UPDATE`.

This phase does not add general scalar expression evaluation. It does not add
bare truth operands such as `WHERE column_name`, `WHERE TRUE`, literal-left
logical operators, `HAVING` boolean composition, symbolic operators, joins,
subqueries, row constructors, string predicates, or arbitrary SQLite
pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - Logical operators: <https://dev.mysql.com/doc/refman/8.4/en/logical-operators.html>
  - Operator precedence: <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - Operator table: <https://dev.mysql.com/doc/refman/8.4/en/non-typed-operators.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_where_xor_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

The official manual states the logical operator family evaluates to `TRUE`,
`FALSE`, or `NULL`, implemented by MySQL as `1`, `0`, and SQL `NULL`. It also
documents `XOR` as returning `NULL` when either operand is `NULL`; otherwise
the result is true when an odd number of operands is true. MySQL precedence is
`NOT`, then `AND`, then `XOR`, then `OR`.

Runtime probes confirm for MySQL 8.4.9:

- `1 XOR 1 = 0`, `1 XOR 0 = 1`, `1 XOR NULL = NULL`,
  `0 XOR NULL = NULL`, `NULL XOR NULL = NULL`.
- `1 XOR 1 XOR 1 = 1`, with left-to-right evaluation at the `XOR` precedence
  level.
- `AND` binds tighter than `XOR`, and `XOR` binds tighter than `OR`.
- `XOR` produces no warnings for the supported forms.
- Existing DML behavior applies when `XOR` is used in supported `DELETE` and
  `UPDATE` `WHERE` clauses.

## Ownership Boundaries

- Public API: no ABI or public-header changes.
- Statement context: no new statement-level state. Existing diagnostics and
  warning-count behavior are reused.
- Lexer/parser/AST: recognize keyword `XOR`, build an explicit logical
  predicate node, and preserve source spans for parser diagnostics and tests.
- Analyzer/planner: resolve only the leaf predicate columns through MyLite
  descriptors. `XOR` itself owns no schema, table, or column lookup.
- Catalog: no schema/table/column descriptor mutation and no descriptor-version,
  catalog-generation, or SQLite-schema-generation changes.
- Result builder: successful statements use the existing row-result and
  non-row-result conventions for their statement kind.
- Storage/VFS/file format: no changes. `.mylite` preamble and shifted SQLite
  payload invariants remain unchanged.
- SQLite physical execution: generated SQLite continues to operate over stable
  MyLite-owned physical table names and quoted descriptor-derived identifiers.
  `XOR` lowering is a MyLite wrapper/translation using ordinary SQLite SQL
  comparison semantics. No public SQLite extension function or SQLite fork
  patch is required.

## Supported SQL

`XOR` is admitted only inside the existing `where_clause_opt` contexts that
already accept descriptor-backed predicate boolean expressions:

```sql
SELECT ... FROM table_name WHERE predicate XOR predicate
CREATE TABLE target AS SELECT ... FROM table_name WHERE predicate XOR predicate
INSERT INTO target SELECT ... FROM table_name WHERE predicate XOR predicate
REPLACE INTO target SELECT ... FROM table_name WHERE predicate XOR predicate
DELETE FROM table_name WHERE predicate XOR predicate
UPDATE table_name SET column_name = value WHERE predicate XOR predicate
```

Each side of `XOR` may be any currently supported predicate expression:

- descriptor-column comparisons to supported decimal integer or boolean literal
  right operands;
- descriptor-column `IS NULL`, `IS NOT NULL`, `IS TRUE`, `IS NOT TRUE`,
  `IS FALSE`, `IS NOT FALSE`, `IS UNKNOWN`, and `IS NOT UNKNOWN`;
- descriptor-column `BETWEEN` / `NOT BETWEEN`;
- descriptor-column `IN` / `NOT IN`;
- keyword `NOT`;
- `AND` / `&&`;
- `OR` / `||`;
- parentheses;
- nested `XOR`.

`SELECT` predicate columns keep the existing source-qualified descriptor
reference policy. `DELETE` and `UPDATE` predicates remain limited to
unqualified descriptor columns.

## Unsupported SQL

This phase deliberately does not admit:

- bare truth operands: `WHERE column_name`, `WHERE TRUE`, `WHERE 1`;
- literal-left or expression-left XOR forms: `1 XOR column_name = 1`,
  `column_name + 1 XOR ...`;
- scalar `SELECT 1 XOR 0`, table-backed projection expressions, or assignment
  expressions;
- `HAVING` `XOR` composition;
- symbolic bitwise XOR `^`;
- SQL-mode-dependent `!` or `||` behavior beyond the current baseline;
- joins, subqueries, row constructors, CTEs, parameters, functions, arbitrary
  expressions, string/decimal/float/hex/bit predicates, collations, triggers,
  privileges, or foreign-key side effects.

## Grammar

The MyLite grammar is independently authored and deliberately smaller than
MySQL's full expression grammar. The intended Lemon-style shape is:

```lemon
where_predicate(A) ::= or_predicate(B). {
    A = B;
}

or_predicate(A) ::= xor_predicate(B). {
    A = B;
}
or_predicate(A) ::= or_predicate(B) OR(O) xor_predicate(C). {
    A = mylite_sql_parser_make_logical_predicate(
        state, B, O, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, C);
}

xor_predicate(A) ::= and_predicate(B). {
    A = B;
}
xor_predicate(A) ::= xor_predicate(B) XOR(X) and_predicate(C). {
    A = mylite_sql_parser_make_logical_predicate(
        state, B, X, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, C);
}
```

The actual nonterminal names may differ from this snippet, but parse precedence
must match MySQL for the admitted subset:

1. predicate atoms and parenthesized predicates;
2. keyword `NOT`;
3. `AND` / `&&`;
4. `XOR`;
5. `OR` / `||`.

`XOR` is left-associative within its own precedence level.

## Semantics

For two predicate operands `left` and `right`:

| Operand values | Result |
| --- | --- |
| one true and one false | true |
| both true | false |
| both false | false |
| either operand is SQL `NULL` / unknown | SQL `NULL` |

Rows pass a `WHERE` filter only when the final expression is true. Therefore,
unknown `XOR` results do not match rows.

Examples over the baseline `numbers` fixture:

- `i = 1 XOR nn = 8` matches rows 2 and 4.
- `n IS NULL XOR nn = 5` matches row 3.
- `i = 1 XOR n = 9` matches row 4 only because row 1 and row 3 produce
  unknown results and row 2 has two true operands.
- `i = 1 XOR nn = 8 AND n = 9` parses as
  `i = 1 XOR (nn = 8 AND n = 9)`.
- `i = 1 XOR nn = 8 OR id = 1` parses as
  `(i = 1 XOR nn = 8) OR id = 1`.

`XOR` itself records no warnings. Existing deprecated `&&` and `||` warning
behavior is preserved when those operators appear inside a larger `XOR`
predicate.

## Runtime Planning And SQLite Handling

Planning adds a logical `XOR` node to the existing planned predicate tree. The
node stores left and right child indexes and has no descriptor payload.

Generated SQLite uses ordinary SQLite comparison semantics over MyLite-generated
predicate subexpressions:

```sql
((left_sql) <> (right_sql))
```

The existing admitted predicate leaves and logical predicate nodes already
produce integer `1`, integer `0`, or SQL `NULL`. SQLite `<>` over those values
therefore preserves the admitted MySQL `XOR` truth table: different non-`NULL`
truth values return `1`, equal non-`NULL` truth values return `0`, and any
`NULL` operand returns `NULL`. This shape keeps predicate evaluation inside
SQLite, avoids materializing rows in MyLite, and avoids duplicating predicate
SQL or parameter binding through a `CASE` expansion. No public SQLite extension
function, physical table metadata change, index, trigger, constraint, or SQLite
fork patch is introduced.

## Diagnostics

Diagnostics reuse existing behavior:

- syntax errors for unsupported grammar report MySQL parse error `1064`
  / SQLSTATE `42000`;
- missing default schema, unknown schema, unknown table, reserved names, and
  unsupported object kinds are resolved by the enclosing statement planner;
- unknown predicate columns from either side of `XOR` report the existing
  MySQL-compatible `Unknown column ... in 'where clause'`;
- out-of-range predicate literals under either side report the existing
  descriptor-range diagnostics;
- unsupported qualified DML predicate columns remain rejected with the existing
  MyLite-specific diagnostic;
- physical SQLite failures and allocation failures propagate through the
  existing execution paths.

## Result And Side Effects

Successful `SELECT` statements return ordinary row result sets. Successful
`CREATE TABLE ... SELECT`, `INSERT ... SELECT`, `REPLACE ... SELECT`,
`DELETE`, and `UPDATE` statements use their existing non-row result object
conventions, affected-row semantics, and warning-count behavior.

`XOR` does not mutate catalogs, descriptor caches, catalog generation,
`sqlite_schema_generation`, storage format, or `.mylite` preamble bytes.

## Tests

Add MySQL-runtime expectation coverage and matching MyLite C coverage for:

- result labels and simple `XOR` result sets;
- MySQL three-valued `NULL` behavior;
- `NOT`, `AND`, `XOR`, `OR`, and parenthesized precedence;
- nested and repeated `XOR`;
- composition with `IS TRUE` / `IS UNKNOWN`, `BETWEEN`, and `IN`;
- aggregate-source filters, grouped aggregates, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, and `REPLACE ... SELECT`;
- `UPDATE` and `DELETE`, including ordered limited DML;
- warning count zero for supported forms and existing `&&` / `||` warnings
  when nested under `XOR`;
- missing schema/table, unknown predicate column, out-of-range nested predicate
  literals, table-qualified DML predicate columns, and unsupported broader
  forms such as bare operands, scalar projection `XOR`, symbolic `^`, and
  `HAVING` `XOR`;
- close/reopen persistence, file preamble preservation, independent file-backed
  handles, and existing parser/runtime lifecycle regressions.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` for the `XOR` operator and the root `WHERE` row.
- `docs/compatibility/operators.md` for limited logical `XOR`.
- `docs/compatibility/sql-query-expressions.md` for `WHERE`.
- `docs/compatibility/sql-table-dml.md` only to clarify that `DELETE` and
  `UPDATE` inherit the expanded baseline predicate boolean expression.

Do not claim support for bitwise XOR `^`, scalar expression `XOR`, `HAVING XOR`,
literal-left logical operands, or full MySQL boolean expression evaluation.
