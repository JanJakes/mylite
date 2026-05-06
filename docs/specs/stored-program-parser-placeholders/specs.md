# Stored Program Parser Placeholders

## Status

This feature adds parser recognition and runtime placeholder handling for the
stored-program-adjacent statements that commonly appear in application install
or migration SQL:

- `CALL`
- `CREATE PROCEDURE`
- `CREATE FUNCTION` for stored functions
- `CREATE FUNCTION` for loadable functions
- `CREATE TRIGGER`
- `CREATE EVENT`
- `DROP PROCEDURE`
- `DROP FUNCTION`
- `DROP TRIGGER`
- `DROP EVENT`
- `SIGNAL`

The feature does not implement stored routine, trigger, event, condition, or
loadable-function functionality. Accepted statements prepare successfully,
execute as no-ops, affect zero rows, return no result set, and append a warning
with MySQL error code `1235` (`ER_NOT_SUPPORTED_YET`) explaining that the
statement is accepted only as a parser placeholder.

## Sources

- MySQL 8.4 Reference Manual, `CALL` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/call.html
- MySQL 8.4 Reference Manual, `CREATE PROCEDURE` and stored
  `CREATE FUNCTION` Statements:
  https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html
- MySQL 8.4 Reference Manual, `CREATE FUNCTION` Statement for Loadable
  Functions:
  https://dev.mysql.com/doc/refman/8.4/en/create-function-loadable.html
- MySQL 8.4 Reference Manual, `CREATE TRIGGER` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html
- MySQL 8.4 Reference Manual, `CREATE EVENT` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-event.html
- MySQL 8.4 Reference Manual, `DROP PROCEDURE` and `DROP FUNCTION`
  Statements:
  https://dev.mysql.com/doc/refman/8.4/en/drop-procedure.html
- MySQL 8.4 Reference Manual, `DROP TRIGGER` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/drop-trigger.html
- MySQL 8.4 Reference Manual, `DROP EVENT` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/drop-event.html
- MySQL 8.4 Reference Manual, `SIGNAL` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/signal.html

Observed behavior was verified against MySQL 8.4.9 in Docker container
`mylite-mysql-849` with `mysql:8.4.9`, using focused probes through:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --table --force --show-warnings -vvv
```

The verified server reported version `8.4.9` and default session SQL mode
`ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar, documentation
prose, or implementation sources.

## Scope

The first parser placeholder slice must recognize representative MySQL 8.4
syntax for the listed statements:

- `CALL` with a qualified routine name, optional empty parentheses, or an
  expression argument list.
- `CREATE PROCEDURE` with optional `DEFINER`, `IF NOT EXISTS`, parameters,
  routine characteristics, and a simple or `BEGIN ... END` body.
- Stored `CREATE FUNCTION` with optional `DEFINER`, `IF NOT EXISTS`,
  parameters, a `RETURNS` type, routine characteristics, and a simple or
  compound body.
- Loadable `CREATE FUNCTION` with optional `AGGREGATE`, `IF NOT EXISTS`,
  supported return class, and `SONAME`.
- `CREATE TRIGGER` with optional `DEFINER`, `IF NOT EXISTS`, timing, event,
  target table, `FOR EACH ROW`, optional ordering, and body.
- `CREATE EVENT` with optional `DEFINER`, `IF NOT EXISTS`, one-time or
  repeating schedule, optional completion/status/comment clauses, and body.
- Drop statements with `IF EXISTS` where MySQL allows it.
- `SIGNAL` with direct `SQLSTATE [VALUE]`, named conditions, and condition
  item assignments.

Routine, trigger, and event bodies are parsed only far enough to accept simple
statements already known by the MyLite parser plus `CALL`, `SIGNAL`, `RETURN`,
and `BEGIN ... END` statement lists. Full stored-program control flow,
declarations, handlers, cursors, labels, and local variables are deferred.

## Non-Goals

This feature must not:

- persist routine, trigger, event, or loadable-function metadata;
- invoke routines, emit routine result sets, or bind `OUT`/`INOUT` parameters;
- execute trigger or event bodies;
- enforce routine privileges, definer security, determinism declarations, SQL
  data-access characteristics, scheduler state, or implicit commits;
- implement MySQL `SIGNAL` error, warning, handler, diagnostics-area, or
  SQLSTATE semantics;
- expose `INFORMATION_SCHEMA`, `SHOW CREATE`, or `SHOW ... STATUS` metadata for
  stored programs;
- mark the covered statements as fully supported in the compatibility matrix.

## Placeholder Runtime Semantics

At prepare time, each covered AST statement maps to a dedicated custom
statement kind. Prepare must not mutate the database or create metadata rows.

At execution time:

- return `MYLITE_DONE`;
- set affected rows to `0`;
- leave result metadata empty;
- append exactly one warning with code `1235`;
- keep warnings visible through existing `SHOW WARNINGS` support until the next
  normal diagnostic-clearing statement;
- keep the statements out of write-statement classification because this slice
  intentionally performs no metadata or data mutation.

The warning message should name the statement family and state that it was
accepted as a MyLite parser placeholder and not executed.

## Parser Shape

The intended MyLite grammar shape is:

```lemon
call_statement ::= CALL routine_name.
call_statement ::= CALL routine_name LPAREN RPAREN.
call_statement ::= CALL routine_name LPAREN expression_list RPAREN.

create_procedure_statement ::=
    CREATE opt_definer PROCEDURE opt_if_not_exists routine_name
    LPAREN opt_proc_parameter_list RPAREN routine_characteristic_list routine_body.

create_function_statement ::=
    CREATE opt_definer FUNCTION opt_if_not_exists routine_name
    LPAREN opt_func_parameter_list RPAREN RETURNS column_type
    routine_characteristic_list routine_body.

create_function_statement ::=
    CREATE opt_aggregate FUNCTION opt_if_not_exists routine_name
    RETURNS loadable_function_return_type SONAME STRING.

create_trigger_statement ::=
    CREATE opt_definer TRIGGER opt_if_not_exists trigger_name
    trigger_time trigger_event ON table_name FOR EACH ROW opt_trigger_order
    routine_body.

create_event_statement ::=
    CREATE opt_definer EVENT opt_if_not_exists event_name ON SCHEDULE
    event_schedule opt_event_completion opt_event_status opt_event_comment
    DO routine_body.

drop_statement ::= DROP PROCEDURE opt_if_exists routine_name.
drop_statement ::= DROP FUNCTION opt_if_exists routine_name.
drop_statement ::= DROP TRIGGER opt_if_exists trigger_name.
drop_statement ::= DROP EVENT opt_if_exists event_name.

signal_statement ::=
    SIGNAL signal_condition_value opt_signal_information_items.
```

The implementation may use compact placeholder AST nodes instead of preserving
all parsed clauses until runtime support needs the clause data.

## MySQL 8.4.9 Runtime Probes

The following representative statements were accepted by MySQL 8.4.9:

```sql
CALL no_such_proc;
CALL no_such_proc();
CREATE PROCEDURE p0 () SELECT 1;
CREATE FUNCTION f1(value BIGINT) RETURNS BIGINT DETERMINISTIC RETURN value;
CREATE TRIGGER tr_bi BEFORE INSERT ON t FOR EACH ROW SET @seen = NEW.id;
CREATE EVENT ev1 ON SCHEDULE EVERY 1 DAY DO DELETE FROM t WHERE id < 0;
SIGNAL SQLSTATE '01000' SET MESSAGE_TEXT = 'probe warning', MYSQL_ERRNO = 1000;
CREATE PROCEDURE p1(IN id BIGINT, OUT note VARCHAR(20), INOUT counter INT)
BEGIN
  SET @probe_id = id;
  SIGNAL SQLSTATE '01000' SET MESSAGE_TEXT = 'probe warning', MYSQL_ERRNO = 1000;
END;
```

The `CALL` statements failed at execution with unknown-procedure errors, which
confirms parser acceptance separately from functionality. The `SIGNAL` warning
probe returned a warning row with code `1000`; an exception-class `SIGNAL`
returned error `1644`.

## Compatibility

The compatibility matrix should mark the covered rows as `⚪`: accepted by the
parser/API and intentionally handled as a no-op warning placeholder. Their full
metadata, execution, diagnostics, scheduler, trigger, and routine semantics
remain future work.
