# Parser Corpus SET And DML Scalar Value Surfaces

This slice extends parser-corpus expression-value compatibility for MySQL
8.4.9 scalar function values in `SET` user-variable assignments and DML value
positions.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- https://dev.mysql.com/doc/refman/8.4/en/user-variables.html
- https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/control-flow-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/information-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

MySQL accepts expressions in `SET @user_variable = value`, including scalar
function calls. MyLite currently supports literal and variable assignments plus
a narrow expression subset, but corpus queries commonly use function-valued
assignments such as `CONCAT(...)`, `REPEAT(...)`, `IF(...)`,
`CONNECTION_ID()`, `LAST_INSERT_ID()`, `UUID()`, and statement-current temporal
values.

This slice admits and executes source-free scalar function values that MyLite
already evaluates in no-source scalar contexts:

- `SET @v = CONCAT(...)`, `CONCAT_WS(...)`, `REPEAT(...)`, `REPLACE(...)`,
  and `REGEXP_REPLACE(...)` over admitted scalar operands;
- `SET @v = IF(...)`, `IFNULL(...)`, `NULLIF(...)`, `COALESCE(...)`,
  `GREATEST(...)`, and `LEAST(...)` over the current signed-integer,
  boolean, and `NULL` scalar operand subset;
- `SET @v = CURRENT_TIMESTAMP`, `CURRENT_TIMESTAMP()`, `NOW()`, `LOCALTIME`,
  `LOCALTIME()`, `LOCALTIMESTAMP`, `LOCALTIMESTAMP()`, `CURRENT_DATE`,
  `CURRENT_TIME`, UTC date/time/timestamp, and `SYSDATE()` values within the
  existing zero-fractional MyLite temporal envelope;
- `SET @v = CONNECTION_ID()`, `LAST_INSERT_ID()`,
  `LAST_INSERT_ID(expr)`, `ROW_COUNT()`, `FOUND_ROWS()`, and `UUID()`;
- temporal literal introducers such as `DATE '2001-01-02'` in user-variable
  assignment values.

The DML value grammar also gains the missing keyword-function forms for
`REPLACE(...)`, `REGEXP_REPLACE(...)`, `CONNECTION_ID()`, `LAST_INSERT_ID()`,
`LAST_INSERT_ID(expr)`, `ROW_COUNT()`, `FOUND_ROWS()`, and `UUID()`, matching
the existing DML constant-scalar value pattern.

This is not a broad `SET value ::= expression` implementation. Table-backed
column references, arbitrary operators, predicates, subqueries beyond the
existing scalar-subquery assignment subset, and arbitrary function calls remain
outside this slice.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
set_function_value ::= dml_function_call.
set_function_value ::= current_timestamp_value.
set_function_value ::= current_date_value.
set_function_value ::= current_time_value.
set_function_value ::= utc_date_value.
set_function_value ::= utc_time_value.
set_function_value ::= utc_timestamp_value.
set_function_value ::= sysdate_value.
set_function_value ::= row_count_function.
set_function_value ::= found_rows_function.
set_function_value ::= connection_id_function.
set_function_value ::= last_insert_id_function.
set_function_value ::= uuid_function.
set_function_value ::= CONCAT LPAREN function_argument_list RPAREN.
set_function_value ::= CONCAT_WS LPAREN function_argument_list RPAREN.
set_function_value ::= REPEAT LPAREN expression COMMA expression RPAREN.
set_function_value ::= REPLACE LPAREN expression COMMA expression
                       COMMA expression RPAREN.
set_function_value ::= REGEXP_REPLACE LPAREN expression COMMA expression
                       COMMA expression RPAREN.
set_function_value ::= IF LPAREN expression COMMA expression
                       COMMA expression RPAREN.
set_function_value ::= IFNULL LPAREN expression COMMA expression RPAREN.
set_function_value ::= NULLIF LPAREN expression COMMA expression RPAREN.
set_function_value ::= COALESCE LPAREN function_argument_list RPAREN.
set_function_value ::= GREATEST LPAREN function_argument_list RPAREN.
set_function_value ::= LEAST LPAREN function_argument_list RPAREN.
set_function_value ::= cast_convert_expression.

dml_constant_scalar_value ::= REPLACE LPAREN expression COMMA expression
                              COMMA expression RPAREN.
dml_constant_scalar_value ::= REGEXP_REPLACE LPAREN expression COMMA expression
                              COMMA expression RPAREN.
dml_constant_scalar_value ::= row_count_function.
dml_constant_scalar_value ::= found_rows_function.
dml_constant_scalar_value ::= connection_id_function.
dml_constant_scalar_value ::= last_insert_id_function.
dml_constant_scalar_value ::= uuid_function.
```

The helper nonterminals above are MyLite-local wrappers that construct the same
AST nodes already used by scalar projection. `SET` uses explicit function rules
instead of reusing the whole DML constant-scalar rule, keeping grammar coverage
targeted and avoiding parser conflicts or accidental acceptance of
table-dependent expressions in assignment values.

## Runtime Behavior

No SQLite fork hook is needed. User-variable assignment uses MyLite's existing
session scalar evaluator for the newly admitted source-free scalar function
nodes. The assigned user variable stores the evaluated scalar text/NULL value
with the same value-kind inference used by current literal and variable
assignments.

`LAST_INSERT_ID(expr)` keeps the existing session-state side effect from the
scalar function implementation. `CONNECTION_ID()` returns the MyLite
connection-local id. `UUID()` uses MyLite's existing UUID generator. Statement
current temporal values follow the existing session `timestamp` and `time_zone`
behavior and the existing fractional-precision limitations.

System-variable `SET` assignments remain narrower. Admitting these functions
for user-variable assignments must not make unsupported system-variable
expression writes silently succeed.

## MySQL 8.4.9 Probe

Representative MySQL behavior:

```sql
CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v VARCHAR(128));
INSERT INTO t(v) VALUES ('seed');
SET @concat = CONCAT('a', REPEAT('b', 2));
SELECT @concat; -- abb
SET @flow = IF(0, 2, 3), @nil = IFNULL(NULL, 4),
    @co = COALESCE(NULL, 5);
SELECT @flow, @nil, @co; -- 3, 4, 5
SET @cid = CONNECTION_ID(), @lid = LAST_INSERT_ID(), @uuid = UUID();
SELECT @cid REGEXP '^[0-9]+$', @lid, CHAR_LENGTH(@uuid),
    @uuid REGEXP '^[0-9a-f-]{36}$'; -- 1, 1, 36, 1
SET @now = NOW(), @ts = CURRENT_TIMESTAMP();
SELECT @now IS NOT NULL, @ts IS NOT NULL; -- 1, 1
INSERT INTO t(v) VALUES
    (REPLACE('abc', 'b', 'B')),
    (REGEXP_REPLACE('abc', 'b', 'B'));
UPDATE t SET v = CONCAT(v, '-x') WHERE id = 1;
SELECT GROUP_CONCAT(v ORDER BY id SEPARATOR '|') FROM t;
-- seed-x|aBc|aBc
```

## Tests

Parser tests cover accepted `SET` user-variable scalar values, temporal literal
introducers, and DML values using the newly admitted keyword functions. Runtime
tests verify user-variable assignment side effects/readback and DML
insert/update behavior for the supported source-free subset. The MySQL
expectation script records the MySQL 8.4.9 behavior above.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

Final corpus measurement for this slice:

```text
parse.csv.mysql_server_tests: queries=69595 ok=66259 errors=3336
parse_status: lexer_error=21 syntax_error=3314 stack_overflow=1
```

The previous committed slice accepted 66,199 of the same 69,595 queries, so
this slice admits 60 additional corpus queries without changing lexer or
stack-overflow counts.

## Compatibility Status

This slice improves executable compatibility for `SET @user_variable` scalar
function assignments and parser/runtime compatibility for selected scalar
function DML values. It does not provide general expression assignment, table
column expressions in user-variable `SET`, stored-program variable semantics,
or broader system-variable expression support.
