# Parser Corpus Nested IF Stack

This slice raises MyLite's generated parser stack budget so valid nested
`IF()` expressions from the MySQL server-test query corpus parse without
reporting `MYLITE_SQL_PARSE_STACK_OVERFLOW`.

The immediate corpus shape is a `SELECT` projection containing roughly thirty
nested `IF((ROUND(t1.a,2)=1), 2, ...)` calls plus a `FROM t1` clause. MyLite's
grammar already admits the expression shape; the failure is Lemon's default
100-entry parser stack, not an unsupported syntax rule.

## Compatibility Authority

MySQL 8.4 accepts nested scalar expressions, including nested flow-control
functions, subject to normal server resource limits. MyLite should not reject a
moderately nested expression as a parser stack overflow when the grammar and
later runtime diagnostics can otherwise handle the statement.

References:

- <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>

## Parser Decision

MyLite uses SQLite's bundled Lemon generator for its independently authored
MySQL-facing grammar. Lemon's default parser stack size is 100 entries. This
slice sets MyLite's grammar stack size explicitly:

```lemon
%stack_size 512
```

The stack remains fixed-size and parser-local. The change does not introduce
dynamic parser allocation, SQLite fork behavior, runtime evaluation changes, or
new syntax acceptance beyond statements already described by the grammar.

## Runtime Behavior

Runtime behavior is unchanged. Successfully parsed but unsupported table-backed
or broader row-expression forms continue to flow into the existing analyzer and
runtime diagnostics. Supported scalar `IF()` behavior remains covered by the
baseline `IF()` feature.

## Tests

Coverage includes:

- parser acceptance for a corpus-shaped nested `IF()` projection using a table
  column condition and `FROM t1`;
- corpus benchmark verification showing the nested `IF()` row no longer
  contributes a `stack_overflow` parse failure;
- existing parser, runtime, and tidy release checks.
