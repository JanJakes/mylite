# Generated Default Expressions

## Scope

This slice evaluates parenthesized generated default expressions that MyLite
already accepts in `CREATE TABLE`, limited to constant expressions supported by
the existing scalar expression evaluator:

- arithmetic and comparison expressions over literals
- parenthesized expressions
- string and numeric literal coercion through the existing write path
- `CURRENT_TIMESTAMP` / `NOW()` generated defaults through the existing temporal
  default path

Generic function-call defaults such as `DEFAULT (UPPER('x'))` remain outside
this slice because the column-default parser still rejects non-`NOW` function
calls.

## Behavior

When `INSERT ... VALUES`, `INSERT ... SET`, `INSERT ... SELECT`, ODKU, or an
`UPDATE ... SET col = DEFAULT` path needs a generated default expression, MyLite
re-parses the cataloged default text as a scalar expression, evaluates it with
the existing expression evaluator, and then runs the result through the same
column coercion logic used by explicit insert expressions.
`SHOW CREATE TABLE` renders cataloged generated defaults as expression defaults
with the same outer parentheses MySQL displays, rather than quoting the stored
expression text as a string literal.

For example:

```sql
CREATE TABLE t (a INT DEFAULT (1 + 2));
INSERT INTO t VALUES (DEFAULT);
```

stores `3`.

Unsupported generated default expressions fail deterministically through the
same expression/default diagnostics used by the covered write path. Full MySQL
default-expression semantic validation, column references, subqueries, stored
functions, loadable functions, variables, parameters, and broader function
defaults remain deferred.

## Tests

Runtime tests cover an `INSERT ... VALUES` defaulted row for
`INT DEFAULT (1 + 2)`, the existing parenthesized current-timestamp generated
default, and `SHOW CREATE TABLE` formatting for generated arithmetic defaults.
