# SQL Prepared Statements

## Scope

This slice implements SQL-level prepared statements:

- `PREPARE stmt_name FROM 'sql text'`
- `PREPARE stmt_name FROM @sql_text`
- `EXECUTE stmt_name [USING @var [, @var] ...]`
- `DEALLOCATE PREPARE stmt_name`
- `DROP PREPARE stmt_name`
- `?` parameter markers inside prepared SQL text
- result metadata, affected rows, last insert id, and diagnostics delegated from
  the executed inner statement

Binary protocol prepared statements, public C bind APIs, automatic
repreparation, prepared-statement status counters, and stored-program scope
interactions remain deferred.

## References

- MySQL 8.4 Reference Manual, PREPARE Statement:
  https://dev.mysql.com/doc/refman/8.4/en/prepare.html
- MySQL 8.4 Reference Manual, EXECUTE Statement:
  https://dev.mysql.com/doc/refman/8.4/en/execute.html
- MySQL 8.4 Reference Manual, DEALLOCATE PREPARE Statement:
  https://dev.mysql.com/doc/refman/8.4/en/deallocate-prepare.html
- MySQL 8.4.9 runtime probes run against the local `mysql:8.4.9` comparison
  container.

## MySQL Behavior

Statement names are case-insensitive session-local names. A prepared statement
created in one session is not available in another and is discarded when the
session ends.

`PREPARE` accepts SQL text from a string literal or from a user variable. The
text must be exactly one statement. `?` markers represent data values only; they
do not stand in for SQL identifiers or keywords. If a statement with the same
name already exists, MySQL implicitly deallocates the old statement before
preparing the new text. If the new text fails to prepare, no statement with that
name remains.

`EXECUTE` runs the named statement. If the prepared text contains markers, the
`USING` clause must provide exactly the same number of user variables. Literal
values and expressions are syntax errors in `USING`. Reading an unset user
variable as a parameter supplies `NULL`.

`DEALLOCATE PREPARE` and `DROP PREPARE` are synonyms. Executing or deallocating
an unknown prepared statement returns error 1243. Supplying the wrong marker
count to `EXECUTE` returns error 1210. Preparing nested `PREPARE`, `EXECUTE`, or
`DEALLOCATE PREPARE` statements returns error 1295.

Observed MySQL 8.4.9 behavior:

```sql
PREPARE p FROM 'SELECT ? + 1 AS plus_one';
EXECUTE p;
-- ERROR 1210 Incorrect arguments to EXECUTE
SET @x = 41;
EXECUTE p USING @x;
-- 42
DEALLOCATE PREPARE p;
EXECUTE p;
-- ERROR 1243 Unknown prepared statement handler (p) given to EXECUTE
```

## MyLite Design

Each `mylite_db` owns a prepared-statement registry keyed by normalized
ASCII-lowercase statement names. Registry entries store the original SQL text
and the number of parameter markers found by MyLite's lexer outside strings and
comments.

`PREPARE` execution:

1. Resolve the statement name.
2. Remove any existing registry entry for the same name.
3. Resolve the SQL source from a literal or user variable. Non-text and `NULL`
   user-variable values are converted to their textual SQL value before parsing,
   matching MySQL's parse-error behavior for `NULL` or `123`.
4. Count parameter markers.
5. Validate the SQL by replacing markers with a neutral numeric literal and
   preparing the resulting single statement through the ordinary MyLite prepare
   pipeline. This preserves expression and `LIMIT ?` parse acceptance while
   still rejecting identifier/table-name marker placement.
6. Reject nested SQL prepared-statement commands with MySQL error 1295.
7. Store the original text and marker count on success.

`EXECUTE` execution substitutes each marker with a SQL literal serialized from
the corresponding user-variable value, prepares that concrete SQL through the
ordinary pipeline, and then delegates stepping, result metadata, values,
affected rows, found rows, and last insert id to the inner statement. This keeps
SQL-level prepared statements independent from the public C prepare API while
using the existing runtime semantics for the executed statement.

This substitution design is a first slice. It is correct for data-value markers
in currently supported SQL and preserves marker boundaries by using the lexer,
not ad hoc string scanning. Native typed binds and parameter-derived metadata
can replace the substitution layer later without changing the SQL statement
registry contract.

## Grammar

MyLite Lemon-level intent:

```lemon
statement ::= prepare_statement.
statement ::= execute_statement.
statement ::= deallocate_prepare_statement.

prepare_statement ::= PREPARE identifier FROM prepare_source.
prepare_source ::= STRING.
prepare_source ::= USER_VARIABLE.

execute_statement ::= EXECUTE identifier opt_execute_using_list.
opt_execute_using_list ::= .
opt_execute_using_list ::= USING execute_using_list.
execute_using_list ::= USER_VARIABLE.
execute_using_list ::= execute_using_list COMMA USER_VARIABLE.

deallocate_prepare_statement ::= DEALLOCATE PREPARE identifier.
deallocate_prepare_statement ::= DROP PREPARE identifier.
```

Parameter markers are recognized by the lexer inside prepared SQL text. Direct
top-level `SELECT ?` remains a syntax error until MyLite has a public prepared
statement bind API that intentionally accepts marker syntax.

## Diagnostics

- Unknown handler for `EXECUTE`: error 1243,
  `Unknown prepared statement handler (<name>) given to EXECUTE`
- Unknown handler for `DEALLOCATE PREPARE` / `DROP PREPARE`: error 1243,
  `Unknown prepared statement handler (<name>) given to DEALLOCATE PREPARE`
- Wrong `USING` count: error 1210, `Incorrect arguments to EXECUTE`
- Nested prepared-statement commands: error 1295,
  `This command is not supported in the prepared statement protocol yet`
- Invalid prepared SQL text: preserve the ordinary MyLite parse/prepare
  diagnostic and error code where possible

## Tests

Runtime tests cover:

- prepare from string literal and user variable
- case-insensitive statement names
- `EXECUTE ... USING` with values and `NULL`
- result metadata delegation
- affected rows and last insert id delegation for prepared DML
- marker count mismatch
- implicit deallocation on failed re-prepare
- `DROP PREPARE` synonym
- unknown handler diagnostics
- nested prepared statement diagnostics
