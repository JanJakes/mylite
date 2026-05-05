# `DEFAULT()`

## Scope

MyLite implements `DEFAULT(col_name)` as a scalar expression over base-table
columns. The first implementation covers table-backed `SELECT` expressions and
single-table `UPDATE`/`DELETE` expression contexts.

## Behavior

- `DEFAULT(col_name)` returns the named column's default value.
- The argument must be an unqualified or qualified column identifier. Literal,
  empty, and multi-argument forms are syntax errors.
- Column resolution follows the surrounding expression scope. Unknown and
  ambiguous columns use the normal MySQL-style column diagnostics for that
  scope.
- Literal defaults are converted using the column's declared type conversion
  rules.
- Nullable columns without an explicit default return `NULL`, matching MySQL's
  implicit `DEFAULT NULL` rule.
- `NOT NULL` columns without an explicit default raise error 1364.
- Defaults represented by expressions raise error 3773. MyLite stores
  `CURRENT_TIMESTAMP` defaults with generated-default metadata, but
  `DEFAULT(timestamp_column)` returns `NULL` to match MySQL's observed runtime
  behavior for this special timestamp default form.
- The result descriptor is the descriptor of the referenced column. Projection
  labels follow the usual expression or alias label rules.

## Grammar

`DEFAULT()` is intentionally not admitted through the generic function grammar,
because MySQL accepts only a column identifier argument:

```lemon
scalar_function_call ::= DEFAULT LPAREN qualified_identifier RPAREN.
```

## Verified Expectations

Verified against MySQL 8.4.9:

| Statement | Result |
| --- | --- |
| `SELECT DEFAULT(a) FROM t` where `a INT DEFAULT 7` | `7` |
| `SELECT DEFAULT(b) FROM t` where `b VARCHAR(10) DEFAULT 'x'` | `x` |
| `SELECT DEFAULT(c) FROM t` where `c INT NULL` | `NULL` |
| `SELECT DEFAULT(d) FROM t` where `d INT NOT NULL` | error 1364 |
| `SELECT DEFAULT(id) FROM t` where `id INT PRIMARY KEY` | error 1364 |
| `SELECT DEFAULT(f) FROM t` where `f INT DEFAULT (1 + 2)` | error 3773 |
| `SELECT DEFAULT(e) FROM t` where `e TIMESTAMP DEFAULT CURRENT_TIMESTAMP` | `NULL` |
| `SELECT DEFAULT(t.a) FROM t` | `7` |
| `SELECT DEFAULT(db.t.a) FROM db.t` | `7` |
| `SELECT DEFAULT(a)` | error 1054 |
| `SELECT DEFAULT(1) FROM t` | syntax error |
| `SELECT DEFAULT() FROM t` | syntax error |
| `SELECT DEFAULT(a, b) FROM t` | syntax error |

## References

- MySQL 8.4 Reference Manual, Built-In Function and Operator Reference:
  https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html
- MySQL 8.4 Reference Manual, Data Type Default Values:
  https://dev.mysql.com/doc/mysql/en/data-type-defaults.html
