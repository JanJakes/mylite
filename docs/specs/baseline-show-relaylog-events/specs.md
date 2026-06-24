# Baseline SHOW RELAYLOG EVENTS

## Purpose

This slice adds a narrow MySQL 8.4.9-compatible placeholder for
`SHOW RELAYLOG EVENTS`. MyLite has no replica relay logs, replication channels,
or relay-log privileges, so the embedded behavior is intentionally metadata
only: accepted no-replication forms return the MySQL column shape and zero rows.

## Compatibility Sources

- MySQL 8.4 Reference Manual, `SHOW RELAYLOG EVENTS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-relaylog-events.html
- MySQL 8.4 Reference Manual, `SHOW MASTER STATUS` no longer supported:
  https://dev.mysql.com/doc/refman/8.4/en/show-master-status.html
- MySQL 8.4 Reference Manual, `SHOW PARSE_TREE` debug-only statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-parse-tree.html
- MySQL 8.4.9 runtime probes against the local `mylite-mysql-849` container.

## MySQL 8.4.9 Observations

These statements succeed with zero rows and no diagnostics in a default
non-replica MySQL 8.4.9 container:

```sql
SHOW RELAYLOG EVENTS;
SHOW RELAYLOG EVENTS IN 'x';
SHOW RELAYLOG EVENTS FROM 4;
SHOW RELAYLOG EVENTS LIMIT 1;
SHOW RELAYLOG EVENTS LIMIT 1, 2;
SHOW RELAYLOG EVENTS LIMIT 1 OFFSET 2;
SHOW RELAYLOG EVENTS IN 'x' FROM 4 LIMIT 1;
SHOW RELAYLOG EVENTS IN 'x' FROM 4 LIMIT 1, 2;
SHOW RELAYLOG EVENTS IN 'x' FROM 4 LIMIT 1 OFFSET 2;
SHOW RELAYLOG EVENTS FOR CHANNEL '';
```

The result columns are, in order:

```text
Log_name
Pos
Event_type
Server_id
End_log_pos
Info
```

Successful statements leave the following statement diagnostics:

```sql
SELECT ROW_COUNT(), @@warning_count, @@error_count;
-- -1, 0, 0
```

These forms are rejected:

```sql
SHOW RELAYLOG EVENTS WHERE Log_name IS NOT NULL;
SHOW FULL RELAYLOG EVENTS;
SHOW RELAYLOG EVENTS FROM '4';
SHOW RELAYLOG EVENTS LIMIT '1';
SHOW RELAYLOG EVENTS FOR CHANNEL 'default';
```

The named channel form returns `ERROR 3074 (HY000): Replica channel 'default'
does not exist` in the no-channel runtime.

`SHOW MASTER STATUS` is no longer supported in MySQL 8.4 and remains a syntax
error. `SHOW PARSE_TREE` is documented as debug/development-only and is a syntax
error in the local production MySQL 8.4.9 runtime.

## MyLite Behavior

- Parse and execute the accepted `SHOW RELAYLOG EVENTS` forms listed above.
- Return the six MySQL column labels with zero rows.
- Ignore log name, position, and limit values because there is no relay log.
- Accept `FOR CHANNEL ''` as the default no-channel form.
- Return MySQL error 3074 / `HY000` for non-empty channel names.
- Reject `WHERE`, `FULL`, string `FROM`, string `LIMIT`, and other unlisted
  forms as syntax errors.
- Keep `SHOW MASTER STATUS` rejected as removed MySQL 8.4 syntax.
- Keep `SHOW PARSE_TREE` rejected for production-compatible builds.

## Grammar

The intended MyLite Lemon grammar surface is independently authored as:

```lemon
show_relaylog_events_statement ::=
    SHOW RELAYLOG EVENTS
    show_relaylog_events_in_opt
    show_relaylog_events_from_opt
    show_relaylog_events_limit_opt
    show_relaylog_events_channel_opt.

show_relaylog_events_in_opt ::= .
show_relaylog_events_in_opt ::= IN STRING.

show_relaylog_events_from_opt ::= .
show_relaylog_events_from_opt ::= FROM INTEGER.

show_relaylog_events_limit_opt ::= .
show_relaylog_events_limit_opt ::= LIMIT INTEGER.
show_relaylog_events_limit_opt ::= LIMIT INTEGER COMMA INTEGER.
show_relaylog_events_limit_opt ::= LIMIT INTEGER OFFSET INTEGER.

show_relaylog_events_channel_opt ::= .
show_relaylog_events_channel_opt ::= FOR CHANNEL STRING.
```

## Storage And Runtime

No SQLite schema, MyLite catalog, or `.mylite` file-format changes are needed.
This is a MyLite runtime result wrapper. It does not require SQLite extension
APIs or SQLite fork hooks.

## Tests

- Parser tests cover accepted base, option, and channel forms plus rejected
  forms.
- Runtime tests cover result columns, zero rows, diagnostics, file reopen, no
  catalog-generation changes, independent handles, channel error diagnostics,
  and removed/debug-only SHOW rejections.
- MySQL expectation script records the MySQL 8.4.9 result shape, diagnostics,
  accepted forms, and errors.

## Out Of Scope

- Live relay-log storage or event streaming.
- Replica channel registry, relay-log file names, or relay position state.
- Replication privileges.
- Full `SHOW PARSE_TREE` JSON output.
- Compatibility with removed pre-8.4 `SHOW MASTER STATUS` syntax.
