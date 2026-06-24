# Baseline SHOW BINLOG EVENTS Options

This slice extends MyLite's binary-log placeholder surface for
`SHOW BINLOG EVENTS` to accept the MySQL 8.4.9 option grammar used by
applications and parser corpora:

- `SHOW BINLOG EVENTS`
- `SHOW BINLOG EVENTS IN 'log_name'`
- `SHOW BINLOG EVENTS FROM unsigned_integer`
- `SHOW BINLOG EVENTS LIMIT unsigned_integer`
- `SHOW BINLOG EVENTS LIMIT unsigned_integer, unsigned_integer`
- `SHOW BINLOG EVENTS LIMIT unsigned_integer OFFSET unsigned_integer`

MyLite still has no physical binary log, event stream, GTID recovery, log
rotation, privilege model, or mutable replication state. The implemented
behavior filters the existing single synthetic `Format_desc` event row in a
deterministic MySQL-shaped result set.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/show-binlog-events.html
- https://dev.mysql.com/doc/refman/8.4/en/select.html

## MySQL 8.4.9 Observations

Runtime probes were executed against the `mysql:8.4.9` container
`mylite-mysql-849`.

```sql
SHOW BINLOG EVENTS LIMIT 1;
SHOW BINLOG EVENTS IN 'binlog.000001' FROM 4 LIMIT 0, 1;
SHOW BINLOG EVENTS LIMIT 0 OFFSET 1;
SHOW BINLOG EVENTS IN 'missing' LIMIT 1;
SHOW BINLOG EVENTS FROM '4';
SHOW BINLOG EVENTS LIMIT '1';
SHOW BINLOG EVENTS WHERE Log_name IS NOT NULL;
SHOW BINLOG EVENTS FOR CHANNEL '';
SHOW BINLOG EVENTS FROM 4 IN 'binlog.000001' LIMIT 1;
```

Observed behavior:

- `SHOW BINLOG EVENTS LIMIT 1` returns the first event in `binlog.000001` with
  columns `Log_name`, `Pos`, `Event_type`, `Server_id`, `End_log_pos`, and
  `Info`. On the controlled MySQL 8.4.9 container, the first row is
  `binlog.000001`, position `4`, `Format_desc`, server id `1`, end position
  `127`, and info `Server ver: 8.4.9, Binlog ver: 4`.
- `IN 'binlog.000001' FROM 4 LIMIT 0, 1` returns that same first event.
- `LIMIT 0 OFFSET 1` returns no rows.
- `LIMIT 0` and `LIMIT 0,0` are accepted by MySQL and behave like an unbounded
  request for this statement. MyLite records this behavior but does not use
  unbounded MySQL probes in the automated expectation script.
- A missing log name fails with `1220 / HY000` and a message containing
  `Could not find target log`.
- String `FROM` / `LIMIT` values, `WHERE`, `FOR CHANNEL`, and option orders
  other than `IN`, then `FROM`, then `LIMIT` are syntax errors.

## Scope

MyLite exposes one synthetic binary-log event:

| Column | Placeholder |
| --- | --- |
| `Log_name` | `binlog.000001` |
| `Pos` | `4` |
| `Event_type` | `Format_desc` |
| `Server_id` | `1` |
| `End_log_pos` | `127` |
| `Info` | `Server ver: 8.4.9, Binlog ver: 4` |

Options are interpreted over that one-row placeholder stream:

- omitted `IN` uses `binlog.000001`;
- `IN 'binlog.000001'` is accepted;
- any other decoded log name returns `1220 / HY000` with the MySQL-style
  target-log diagnostic;
- omitted `FROM` starts at the first placeholder event;
- `FROM N` includes the row when `N <= 4` and returns an empty result when
  `N > 4`;
- omitted `LIMIT` includes the row;
- `LIMIT row_count` includes the row for every unsigned row-count value,
  including `0`, matching the observed MySQL special case for
  `SHOW BINLOG EVENTS LIMIT 0`;
- `LIMIT offset, row_count` and `LIMIT row_count OFFSET offset` include the row
  only when `offset == 0`; otherwise they return an empty result.

The result metadata, warning count, affected rows, and following `ROW_COUNT()`
behavior are unchanged from the baseline binary-log metadata placeholder.
Successful statements return warning count `0`, affected rows `0`, and make a
following `ROW_COUNT()` return `-1`.

## Out Of Scope

This slice does not implement:

- physical binary log files;
- live event streams beyond the single synthetic first event;
- GTIDs, binary-log rotation, purge, encryption, or recovery state;
- privilege checks;
- mutable server variables caused by replication state;
- prepared-mode restrictions;
- `BINLOG`, `PURGE BINARY LOGS`, or source-control statements.

## MyLite Grammar Snippets

These Lemon-shape snippets are independently authored from MySQL documentation
and runtime behavior:

```lemon
show_binlog_events_statement ::=
    SHOW BINLOG EVENTS show_binlog_events_in_opt
    show_binlog_events_from_opt show_binlog_events_limit_opt.

show_binlog_events_in_opt ::= .
show_binlog_events_in_opt ::= IN STRING.

show_binlog_events_from_opt ::= .
show_binlog_events_from_opt ::= FROM INTEGER.

show_binlog_events_limit_opt ::= .
show_binlog_events_limit_opt ::= LIMIT INTEGER.
show_binlog_events_limit_opt ::= LIMIT INTEGER COMMA INTEGER.
show_binlog_events_limit_opt ::= LIMIT INTEGER OFFSET INTEGER.
```

The grammar intentionally does not admit `WHERE`, `LIKE`, `FOR CHANNEL`, string
numeric operands, negative numeric operands, or reordered options.

## Runtime Architecture

No SQLite public extension API or SQLite fork hook is needed. The statement is
metadata-only and is implemented in the MyLite runtime by validating option AST
children and conditionally appending the synthetic row to a MyLite-owned result
object. It does not read or mutate SQLite user data, the MyLite catalog, the
file preamble, session transactions, or session replication state.

## Tests

Focused coverage includes:

- parser acceptance for the option grammar and AST option children;
- parser rejection for unsupported filters, channel clauses, string numeric
  operands, and wrong option order;
- MySQL 8.4.9 expectation probes for accepted low-volume forms, metadata,
  diagnostics, missing-log errors, and syntax errors;
- MyLite runtime result metadata, row filtering, row-count state, warning state,
  missing-log diagnostics, file-preamble stability, and independent handles.

## Compatibility Status

This slice turns the `SHOW BINLOG EVENTS` option placeholder baseline green.
The broader binary-log SHOW family remains limited because MyLite intentionally
does not have physical binary logs or real replication state yet.
