# Baseline Row Base-Conversion Functions

## Scope

This slice extends the existing MySQL-compatible `BIN()`, `OCT()`, and `CONV()`
baseline from no-source scalar statements to single-row and table-backed
row-scalar expression contexts.

Admitted forms:

```sql
SELECT BIN(expr), OCT(expr), CONV(expr, expr, expr) FROM table_name;
SELECT CONCAT('0b', BIN(expr)) FROM table_name;
SELECT CASE WHEN predicate THEN CONV(expr, 10, 16) END FROM table_name;
SELECT id FROM table_name WHERE BIN(expr) = '1010' ORDER BY OCT(expr);
```

The row-backed operand domain is intentionally integer-only in this phase:

- integer descriptor columns;
- integer, boolean, and `NULL` literals;
- supported signed-64 row integer arithmetic;
- supported integer-producing row functions such as `BIT_COUNT()`;
- integer-like row control-flow arguments where the existing row planner admits
  them.

`BIN()` and `OCT()` format non-`NULL` inputs as unsigned 64-bit values in base 2
and base 8 respectively, with no leading zeroes. `CONV()` converts the integer
input through decimal text using the absolute `from_base` and `to_base`; base
arguments outside `2..36` return `NULL`. Positive `to_base` formats the unsigned
64-bit result, while negative `to_base` formats the signed result. Invalid
leading digits in the input text yield `0` and append MySQL warning 1292 with
SQLSTATE `22007`; later invalid digits stop parsing at the valid prefix, and
overflow while parsing also emits warning 1292.

## MyLite Design

The row path lowers admitted calls to private SQLite functions:

```sql
_mylite_bin(value)
_mylite_oct(value)
_mylite_conv(value, from_base, to_base)
```

These callbacks are registered during SQLite bootstrap and attach warnings to
the owning MyLite connection. This keeps row evaluation inside SQLite's normal
scan/expression machinery and avoids loading tables into MyLite memory.

## MyLite Lemon Syntax

No new grammar is required. This slice uses the existing independently authored
forms from the scalar baseline:

```lemon
expression(A) ::= BIN(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= OCT(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= CONV(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R).
```

Wrong-arity nodes keep returning MySQL error 1582.

## Deliberate Gaps

This slice does not claim the full MySQL coercion surface. Deferred behavior:

- string, binary-string, hex, bit, decimal, and floating row operands;
- unsigned integer row literals above signed-64 range in the row-backed path;
- descriptor string/decimal/float column coercion;
- expression metadata beyond the current generic row-scalar text result shape;
- non-ASCII collation parity for the text returned by base-conversion calls.

Those gaps remain tracked in the numeric compatibility rows.

## Verification

The test suite must cover:

- table-backed `BIN(id)` and `OCT(id)` for `NULL`, zero, positive, and negative
  integer rows;
- table-backed `CONV(id, 10, 2)`, `CONV(id, 10, 16)`, signed output, and invalid
  base `NULL` behavior;
- row warning behavior for invalid `CONV()` input digits;
- row-scalar comparison predicates and `ORDER BY` expression keys;
- nested row-scalar use under supported functions/control-flow;
- unchanged scalar no-source behavior and wrong-arity errors.
