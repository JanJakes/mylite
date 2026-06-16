# Baseline String Bitmask Functions

## Summary

This phase adds a narrow MySQL string-function slice:

```sql
EXPORT_SET(bits, on, off[, separator[, number_of_bits]])
MAKE_SET(bits, value[, value ...])
```

The supported surface covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection contexts. It deliberately
does not add predicates over these functions, expression ordering, grouping,
DML assignment expressions, generated columns, defaults, parameters, subqueries,
or arbitrary nested expression planning.

Core supported behavior:

- `EXPORT_SET()` maps bit positions to `on` and `off` strings from least
  significant bit upward;
- `MAKE_SET()` joins the selected value arguments with commas;
- `EXPORT_SET()` accepts three, four, or five arguments, using `,` and `64` for
  omitted separator and bit-count arguments;
- `MAKE_SET()` requires at least two arguments;
- `EXPORT_SET()` returns `NULL` when any admitted argument is `NULL`;
- `MAKE_SET()` returns `NULL` when the bitmask is `NULL`, skips selected
  `NULL` values, and returns the empty string when no selected non-`NULL` value
  remains;
- negative bitmasks use MySQL's visible two's-complement bit behavior for the
  admitted signed-64 input range;
- `EXPORT_SET()` clamps negative or greater-than-64 `number_of_bits` to `64`;
- `EXPORT_SET(..., 0)` returns the empty string;
- successful supported calls produce no warnings.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-scalar-expression-projection/specs.md`
  - `docs/specs/baseline-row-scalar-expressions/specs.md`
  - `docs/specs/baseline-field-function/specs.md`
  - `docs/specs/baseline-elt-function/specs.md`
  - `docs/specs/baseline-string-padding-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_string_bitmask_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `EXPORT_SET()` with zero, one, two, or more than five arguments reports
  `1582 / 42000`;
- `MAKE_SET()` with zero or one argument reports `1582 / 42000`;
- function-name whitespace such as `EXPORT_SET (...)` and `MAKE_SET (...)` is
  accepted in default SQL mode;
- `EXPORT_SET(5,'Y','N',',',4) = 'Y,N,Y,N'`;
- `EXPORT_SET(5,'Y','N')` uses comma and 64 bit positions;
- `EXPORT_SET(5,'Y','N',':')` uses the supplied separator and 64 bit
  positions;
- `EXPORT_SET(0,'Y','N',',',4) = 'N,N,N,N'`;
- `EXPORT_SET(1,'Y','N',',',0)` returns the empty string;
- `EXPORT_SET(1,'Y','N',',',-1)` and `EXPORT_SET(1,'Y','N',',',65)` each
  return 64 comma-separated positions;
- any `NULL` `EXPORT_SET()` argument returns `NULL`, including `bits`, `on`,
  `off`, `separator`, and `number_of_bits`;
- boolean `bits` and `number_of_bits` values are admitted as `1` and `0`;
- numeric `on` and `off` arguments are converted to visible string form;
- `MAKE_SET(1,'a','b','c') = 'a'`;
- `MAKE_SET(3,'a','b','c') = 'a,b'`;
- `MAKE_SET(0,'a','b')` returns the empty string;
- `MAKE_SET(NULL,'a')` returns `NULL`;
- `MAKE_SET(1,NULL,'b')` returns the empty string;
- `MAKE_SET(2,NULL,'b') = 'b'`;
- `MAKE_SET(7,'a',NULL,'c') = 'a,c'`;
- `MAKE_SET(-1,'a','b','c') = 'a,b,c'`;
- `MAKE_SET(TRUE,'a','b') = 'a'`;
- `MAKE_SET(FALSE,'a','b')` returns the empty string;
- numeric and boolean value arguments are converted to visible string form;
- a preceding `DO EXPORT_SET(...), MAKE_SET(...)` followed by `ROW_COUNT()`
  reports `0`, and `@@warning_count` remains `0`.

MySQL also accepts deferred behavior such as string and decimal bitmask/count
conversion with warnings, unsigned integer literals outside the signed-64
range, non-ASCII collation-sensitive result typing, binary string metadata,
predicates over these functions, arbitrary nested functions, and DML assignment
expressions. Those forms remain outside this baseline.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits exact `EXPORT_SET()` and `MAKE_SET()` expressions
  while preserving source spans for labels and diagnostics. Wrong-arity nodes
  are used where MySQL reports native-function parameter-count diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to generated SQLite
  expressions over stable physical table names and quoted physical column
  names. MyLite registers private scalar helpers through SQLite's public
  function API for row-backed `EXPORT_SET()` and `MAKE_SET()` semantics. No
  SQLite fork patch is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT string_bitmask_item[, string_bitmask_item ...]
SELECT string_bitmask_item[, string_bitmask_item ...] FROM DUAL
```

`DO` form:

```sql
DO string_bitmask_expr[, string_bitmask_expr ...]
```

Single-table row-backed forms, with at least one select item containing a
string-bitmask function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
string_bitmask_expr:
    EXPORT_SET ( bitmask_value , string_value , string_value )
  | EXPORT_SET ( bitmask_value , string_value , string_value , string_value )
  | EXPORT_SET ( bitmask_value , string_value , string_value , string_value ,
                 bit_count_value )
  | MAKE_SET ( bitmask_value , string_value , string_value_list )

bitmask_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | supported_integer_arithmetic_expression
  | supported_integer_scalar_function
  | descriptor_integer_column_reference        -- table-backed SELECT only
  | supported_integer_row_scalar_expression    -- table-backed SELECT only
  | ( bitmask_value )

bit_count_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | supported_integer_arithmetic_expression
  | supported_integer_scalar_function
  | descriptor_integer_column_reference        -- table-backed SELECT only
  | supported_integer_row_scalar_expression    -- table-backed SELECT only
  | ( bit_count_value )

string_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( string_value )
```

`descriptor_integer_column_reference` follows the existing single-source table
alias policy and may explicitly name invisible descriptor columns. Integer
descriptor columns used as `bits` or `number_of_bits` must use the current
integer-family physical storage.

Supported descriptor column families for `string_value` arguments are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The bitmask and `number_of_bits` arguments admit signed-64 integer, boolean,
and `NULL` scalar values, direct supported integer arithmetic, supported
integer-domain numeric and string-length scalar functions, descriptor integer
columns, and the existing supported table-backed integer row-scalar expression
subset. That row-scalar subset includes integer arithmetic over admitted
operands, supported integer-domain numeric functions, string-length functions,
`UNIX_TIMESTAMP()`, and numeric temporal extractors. Warning-producing
string/decimal/float bitmask conversion, string/decimal/float count conversion,
unsigned magnitudes beyond the signed-64 parser envelope, binary string values,
`BIT`, approximate numeric values, `ENUM`, `SET`, `JSON`, spatial values,
parameters, and user variables are deferred.

The following remain outside this phase:

- `WHERE EXPORT_SET(...) ...`, `HAVING MAKE_SET(...) ...`, expression
  `ORDER BY`, grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = MAKE_SET(bits, 'a', 'b')`;
- arbitrary nested row functions outside the supported row-scalar value and
  integer argument subsets;
- scalar subqueries, correlated subqueries, CTEs, table joins beyond the
  already supported row-scalar source envelope, parameters, user variables, and
  stored functions;
- string introducers, national strings, arbitrary binary literals as scalar
  arguments, binary casts as arguments, and full expression metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= EXPORT_SET(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_EXPORT_SET_FUNCTION, B, R);
}

expression(A) ::= MAKE_SET(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_list_argument_function(
        state, T, MYLITE_SQL_AST_MAKE_SET_FUNCTION, B, R);
}

expression(A) ::= EXPORT_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR, NULL, R);
}

expression(A) ::= MAKE_SET(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR, NULL, R);
}

identifier(A) ::= EXPORT_SET(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

identifier(A) ::= MAKE_SET(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

Arity ranges beyond the empty-argument forms are checked by the analyzer and
runtime so diagnostics match the current MyLite native-function count policy.

## Semantics

### `EXPORT_SET()`

`bits` is converted to the admitted signed-64 integer domain, then interpreted
as an unsigned 64-bit mask for bit testing. Bit position zero is the least
significant bit. For each visible position, MyLite appends the `on` string when
the bit is set and the `off` string otherwise. The separator is inserted
between positions, not before the first or after the last.

If the separator is omitted, it is `,`. If `number_of_bits` is omitted, it is
`64`. A `number_of_bits` value less than zero or greater than 64 is treated as
64. A value of zero returns the empty string. Any supported `NULL` input returns
SQL `NULL`.

### `MAKE_SET()`

`bits` is converted to the admitted signed-64 integer domain, then interpreted
as an unsigned 64-bit mask for bit testing. Value argument one corresponds to
bit position zero. Selected non-`NULL` values are converted to text and joined
with `,`. Selected `NULL` values are skipped. If `bits` is `NULL`, the result is
SQL `NULL`. If no selected non-`NULL` values remain, the result is the empty
string.

## Diagnostics

This phase uses MySQL-compatible native-function arity diagnostics where the
supported grammar can identify wrong parameter counts:

- `EXPORT_SET()` accepts exactly three, four, or five arguments;
- `MAKE_SET()` accepts at least two arguments.

Unsupported argument shapes are rejected before SQLite SQL generation with
deterministic MyLite syntax diagnostics. Allocation failures use the existing
`MYLITE_NOMEM` path. SQLite helper failures are surfaced as runtime errors.

## Generated SQLite Shape

Row-backed projection uses generated SQLite expressions:

```sql
_mylite_export_set(arg0, arg1, arg2[, arg3[, arg4]])
_mylite_make_set(arg0, arg1[, argN ...])
```

Every generated identifier is quoted by the existing row-scalar SQL emitter,
supported integer/value expressions are emitted through the row-scalar SQL
emitters, and scalar literal arguments are parameter-bound by the existing
row-scalar parameter binder. MyLite registers the `_mylite_*` helpers on
connection bootstrap through SQLite's public scalar-function API.

## Result Behavior

Successful scalar `SELECT` returns one row through the existing row-result API
with labels from aliases or source spans. Successful `DO` returns no row set,
`affected_rows == 0`, and `warning_count == 0`. Successful supported calls do
not change the catalog, table descriptors, file preamble, or SQLite payload
except for normal read-only SQLite statement state.
