# Baseline ASCII and ORD Functions

## Summary

This phase adds a narrow MySQL-compatible `ASCII(expr)` and `ORD(expr)` scalar
function slice. The supported surface covers no-source scalar `SELECT`,
`SELECT ... FROM DUAL`, `DO`, and single-table row-scalar `SELECT` projection.

Core supported behavior:

- `ASCII(str)` returns the numeric value of the first byte of `str`;
- `ORD(str)` returns the first character code, using the MySQL byte-packing rule
  for nonbinary multibyte UTF-8 text and first-byte behavior for binary values;
- empty non-`NULL` input returns `0`;
- `NULL` input returns `NULL`;
- integer and boolean inputs use their visible decimal text before evaluation;
- table-backed row-scalar projection is descriptor-driven and uses private
  MyLite SQLite scalar helpers for row values.

This phase intentionally does not add `ASCII()` or `ORD()` in predicates, DML
assignments, ordering/grouping expressions, generated columns, defaults,
parameters, subqueries, arbitrary expression trees, or expression metadata.

## Sources and Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Related baseline expression slices:
  - `docs/specs/baseline-string-length-functions/specs.md`
  - `docs/specs/baseline-hex-function/specs.md`
  - `docs/specs/baseline-unhex-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_ascii_ord_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `ASCII('')` and `ORD('')` return `0`;
- `ASCII(NULL)` and `ORD(NULL)` return `NULL`;
- `ASCII('2')`, `ORD('2')`, `ASCII(2)`, and `ORD(2)` return `50`;
- `ASCII(TRUE)` returns `49` and `ORD(FALSE)` returns `48`;
- `ASCII('é')` returns `195`;
- `ORD('é')` returns `50089`, because `(0xc3 * 256) + 0xa9 = 50089`;
- `ORD(_utf8mb4 0xE282AC)` returns `14844588`;
- `ORD(_utf8mb4 0xF09F9982)` returns `4036991362`;
- binary values such as `X'C3A9'` and `_binary 0xC3A9` use first-byte behavior
  and return `195` for `ORD()`;
- string literals containing an initial NUL byte return `0` without becoming
  SQL `NULL`;
- `ASCII()` with zero arguments and `ASCII(expr, ...)` are syntax errors
  (`1064 / 42000`);
- `ORD()` with zero or multiple arguments reports native-function parameter
  count error `1582 / 42000`.

MySQL accepts broader behavior such as expression arguments, character-set
introducers beyond the current MyLite parser surface, use in predicates and DML
assignments, collations, parameters, and full expression metadata. Those forms
remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Successful `SELECT` returns normal row results with
  text integer values or SQL `NULL`; successful `DO` returns the existing
  non-row result shape.
- Statement context: owns diagnostics, warning counts, row counts, and result
  finalization. Supported in-range `ASCII()` / `ORD()` evaluations append no
  warnings.
- Lexer/parser/AST: admits exact one-argument `ASCII()` and `ORD()` expression
  nodes. `ORD()` also has wrong-arity AST nodes so the runtime can return
  MySQL's native-function parameter-count diagnostic. Invalid `ASCII()` arity is
  left to the existing syntax-error path because that matches MySQL 8.4.9.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: row-backed projection lowers to generated SQLite SQL that
  calls private MyLite scalar helpers over quoted descriptor expressions and
  bound parameters. The helpers use SQLite's public scalar-function API; no
  SQLite fork patch is required.
- Result builder: uses existing row result conventions for labels and aliases.
- Storage/VFS/file format: row reads only. The `.mylite` preamble and shifted
  SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT codepoint_item[, codepoint_item ...]
SELECT codepoint_item[, codepoint_item ...] FROM DUAL
```

`DO` form:

```sql
DO codepoint_expr[, codepoint_expr ...]
```

Single-table row-backed forms, with at least one select item containing
`ASCII()` or `ORD()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
codepoint_expr:
    ASCII ( codepoint_value )
  | ORD ( codepoint_value )

codepoint_value:
    string_literal
  | hex_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( codepoint_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for this phase are:

- integer-family columns stored in the current signed 64-bit physical range;
- exact `DECIMAL`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `BINARY`, `VARBINARY`, and baseline `BLOB` family;
- `BIT`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`.

Approximate numeric, `ENUM`, `SET`, JSON, and spatial row-backed values are
deferred for this slice.

The following remain outside this phase:

- `WHERE ASCII(column) ...`, `HAVING ORD(...) ...`, expression `ORDER BY`,
  grouping, aggregate arguments, and distinct expression rows;
- DML assignment values such as `UPDATE t SET c = ASCII(v)`;
- nested row functions such as `ASCII(CONCAT(v, 'x'))`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- complete character-set/collation metadata and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= ASCII(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ASCII_FUNCTION, B, R);
}
expression(A) ::= ORD(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ORD_FUNCTION, B, R);
}
expression(A) ::= ORD(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ORD(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ORD_ARGUMENT_COUNT_ERROR, C, R);
}
```

`ASCII` and `ORD` are nonreserved function keywords and remain available as
identifiers where MyLite's current identifier productions admit function
keywords. Whitespace between the function name and `(` is accepted.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the argument to input bytes:
   - ordinary string literal: decoded bytes, including embedded NUL bytes;
   - binary hex literal: decoded bytes and binary semantics;
   - integer literal and `TRUE` / `FALSE`: decimal text;
   - `NULL`: SQL `NULL`;
   - supported scalar/session/system values: their existing visible bytes or
     SQL `NULL`.
3. Return SQL `NULL` if the argument is `NULL`.
4. Return integer text `0` if the byte sequence is empty.
5. `ASCII()` returns the first byte value.
6. `ORD()` returns the first byte value for binary inputs and for single-byte
   nonbinary inputs. For nonbinary UTF-8 text whose first character spans
   multiple bytes, it folds the first UTF-8 character bytes in left-to-right
   order as `(((b0 * 256) + b1) * 256) + ...`. Invalid UTF-8 lead or
   continuation bytes fall back to first-byte behavior for this baseline rather
   than guessing character-set repair semantics.

Row-backed evaluation lowers the expression to `_mylite_ascii(value)` or
`_mylite_ord(value)`. The private SQLite helpers:

- return `NULL` for SQLite `NULL`;
- use `sqlite3_value_blob()` / `sqlite3_value_bytes()` for binary values and
  first-byte `ORD()` semantics;
- use `sqlite3_value_text()` / `sqlite3_value_bytes()` for nonbinary values and
  the UTF-8 byte-packing rule;
- return an integer result, allowing SQLite to continue filtering, sorting, and
  limiting rows in the existing row-scalar SELECT pipeline.

## Diagnostics

Supported diagnostics include:

- invalid `ASCII()` arity and unsupported grammar: existing syntax error
  `1064 / 42000`;
- invalid `ORD()` arity: native-function parameter-count error
  `1582 / 42000`;
- missing default schema, unknown schema, unknown table, reserved MyLite
  schema/table names, and unsupported object kind through existing planners;
- unknown row-backed argument column: existing `1054 / 42S22`;
- unsupported argument literals or expression shapes: MyLite unsupported-feature
  diagnostics;
- unsupported row-backed descriptor family: MyLite unsupported-feature
  diagnostics;
- physical SQLite failures, allocation failures, and public API misuse through
  existing public API and execution-layer diagnostics.

Supported successful evaluations produce `warning_count == 0`.

## Compatibility Notes

`COMPATIBILITY.md` and `docs/compatibility/functions-string.md` mark
`ASCII()` and `ORD()` as limited support. The compatibility text must avoid
claiming predicate, DML assignment, ordering/grouping expression, subquery,
parameter, complete character-set metadata, or general expression support.
