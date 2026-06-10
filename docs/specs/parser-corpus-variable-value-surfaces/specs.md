# Parser Corpus Variable Value Surfaces

This slice reduces MySQL server-test parser-corpus failures where user and
system variables appear as data values in `SET`, DML value lists, and predicate
value syntax. The goal is to execute the source-free value forms MyLite already
has scalar runtime support for, while keeping table-dependent expression
planning and identifier substitution outside this slice.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/user-variables.html
- https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- https://dev.mysql.com/doc/refman/8.4/en/expressions.html

## MySQL 8.4.9 Observations

The focused MySQL probe used a `mysql:8.4.9` runtime:

```sql
CREATE TABLE t (id INT PRIMARY KEY, i INT, s VARCHAR(32));
SET @id = 1, @i = 7, @s = 'abc', @step = 3, @now = 1000;
INSERT INTO t VALUES (@id, @i, @s),
    (@id + 1, @step * 3, CONCAT(@s, '-x'));
SELECT GROUP_CONCAT(CONCAT(id, ':', i, ':', s) ORDER BY id SEPARATOR '|')
    FROM t;

SET @step3 = @step * 3;
SET @unix_time = @now + 7 * @step;
SET @mod = @unix_time - @unix_time % @step3;
SELECT @step3, @unix_time, @mod;

SELECT id FROM t WHERE i IN (@i, @step3) ORDER BY id;
SELECT id FROM t WHERE s LIKE @s ORDER BY id;
INSERT INTO t VALUES (3, @@warning_count, @@time_zone);
SELECT id, i, s FROM t WHERE id = 3;
SET TIMESTAMP = @@TIMESTAMP + 1;
SELECT @@TIMESTAMP IS NOT NULL;
```

Observed behavior:

- User variables are accepted as ordinary data values in `INSERT ... VALUES`.
- Supported scalar expressions over user variables are accepted in `SET`
  user-variable assignments and DML values.
- System-variable reads are accepted as DML values.
- User variables are accepted in comparison, `IN`, and `LIKE` predicate value
  positions. MyLite admits these predicate forms in this slice but only
  executes the already-supported predicate value runtime subset.
- Within one `SET` statement, reads of a variable assigned earlier in the same
  statement should not be assumed to observe the new value. MyLite tests use
  separate statements for deterministic arithmetic readback.

## Scope

### SET user-variable values

MyLite already evaluates source-free scalar arithmetic and function values for
user-variable assignment. This slice admits variable-rooted arithmetic and
zero-argument `UNIX_TIMESTAMP()` in `SET @name = value` and
`SET @name := value` positions, then relies on the existing runtime guard to
reject table-dependent identifiers and unsupported expression nodes with an
explicit diagnostic.

Supported examples include:

- `SET @step3 = @step * 3`
- `SET @unix_time = @unix_time - @unix_time % @step6`
- `SET @now = UNIX_TIMESTAMP()`

### DML variable values

`INSERT` / `REPLACE` `VALUES` and `SET` forms admit user variables, system
variables, and source-free scalar expressions over variables where the current
scalar evaluator can produce a text/`NULL` cell. Supported `UPDATE`
assignments admit direct user-variable and system-variable values. The value is
then converted through the target descriptor's existing DML conversion path.

This slice covers direct variable values and `INSERT` / `REPLACE`
arithmetic/function expressions that do not reference table columns or subquery
rows. It does not add table-dependent expression planning, `UPDATE` variable
arithmetic execution, generated-expression storage, stored routine evaluation,
or variable substitution for identifiers.

### Predicate variable values

MySQL accepts variables in ordinary predicate value positions such as
`column = @v`, `column IN (@a, @b)`, and `column LIKE @pattern`. This slice
admits those forms in the parser to avoid raw syntax failures and to classify
the syntax correctly. Runtime execution remains limited to currently supported
predicate value conversion; unsupported variable predicates return deterministic
runtime diagnostics rather than being planned through SQLite with accidental
coercion.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
set_system_variable_value ::= variable_value_expression.
user_variable_set_value ::= variable_value_expression.
set_function_value ::= UNIX_TIMESTAMP LPAREN RPAREN.

insert_value ::= system_variable.
insert_value ::= user_variable.
insert_value ::= variable_value_expression.
update_value ::= system_variable.
update_value ::= user_variable.

predicate_comparison_value ::= system_variable.
predicate_comparison_value ::= user_variable.
predicate_in_value ::= system_variable.
predicate_in_value ::= user_variable.
predicate_range_value ::= system_variable.
predicate_range_value ::= user_variable.
predicate_like_pattern ::= system_variable.
predicate_like_pattern ::= user_variable.

variable_value_expression ::= variable_value_head PLUS expression.
variable_value_expression ::= variable_value_head MINUS expression.
variable_value_expression ::= variable_value_head STAR expression.
variable_value_expression ::= variable_value_head DIV expression.
variable_value_expression ::= variable_value_head PERCENT expression.
variable_value_expression ::= variable_value_head MOD expression.
variable_value_head ::= system_variable.
variable_value_head ::= user_variable.
```

The variable-rooted expression admission keeps parser conflicts low while
covering the corpus forms that read session variables as values. The runtime
still rejects descriptors, subqueries, and unsupported expression families
outside the current DML scalar evaluator.

## Runtime Behavior

This slice stays in MyLite parser/runtime code and does not need a SQLite fork
hook.

- `SET @name = expression` evaluates through `session_scalar_value()` and
  stores the resulting session user-variable cell.
- DML variable values are accepted by the existing source-free scalar DML value
  conversion path and then converted for the target descriptor. Variable-rooted
  arithmetic currently executes for `INSERT` / `REPLACE` values; `UPDATE`
  variable arithmetic is parsed but remains outside the executed assignment
  subset.
- `SET TIMESTAMP = @@TIMESTAMP + integer` evaluates in the timestamp setter by
  adding the integer delta to the current effective timestamp. This does not
  make system variables general scalar arithmetic operands.
- Unsupported table-dependent DML expressions continue to fail before catalog
  mutation or row writes.
- Predicate variable syntax is parser-admitted; execution support remains
  bounded by existing predicate planners.

## Tests

Focused tests cover:

- parser acceptance for variable SET arithmetic, INSERT/REPLACE/UPDATE value
  expressions, and comparison/`IN`/`LIKE` predicate variable values;
- runtime SET user-variable arithmetic and direct variable DML conversion;
- runtime rejection/no-mutation for unsupported table-dependent DML
  expressions that become syntactically admitted by this slice;
- MySQL 8.4.9 expectation script output for representative accepted forms.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice implements executable source-free variable value expressions for
`SET` user variables, `SET TIMESTAMP` system-variable arithmetic, `INSERT` /
`REPLACE` value conversion, and direct variable `UPDATE` assignment values.
Predicate variable syntax is admitted but not marked fully executable until the
descriptor predicate conversion path handles session scalar cells with
MySQL-compatible comparison warnings and coercions.
