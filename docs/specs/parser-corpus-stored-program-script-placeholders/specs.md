# Parser Corpus Stored Program Script Placeholders

This slice reduces parser-corpus failures where valid MySQL 8.4.9
stored-program setup scripts contain stored routine, trigger, event, `SIGNAL`,
or `RESIGNAL` syntax that MyLite does not execute yet.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/stored-objects.html
- https://dev.mysql.com/doc/refman/8.4/en/create-procedure.html
- https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html
- https://dev.mysql.com/doc/refman/8.4/en/create-event.html
- https://dev.mysql.com/doc/refman/8.4/en/signal.html
- https://dev.mysql.com/doc/refman/8.4/en/resignal.html

## MySQL 8.4.9 Observations

Runtime probes show that MySQL accepts compound stored-program definitions
containing semicolon-delimited body statements when the client sends the
definition as one statement. Representative accepted surface:

```sql
SET sql_mode = default;
CREATE PROCEDURE p(IN x INT)
BEGIN
  DECLARE y INT DEFAULT 1;
  SELECT x + y;
END
```

MySQL also accepts a top-level `SIGNAL SQLSTATE '01000'`, producing warning
`1642` and warning count `1`. A top-level `RESIGNAL` is syntactically valid but
fails at runtime with `1645 / 0K000` when no handler is active.

## Scope

In scope:

- recognize parser fallback inputs that contain stored-program DDL after
  earlier setup statements such as `SET`, `DROP PROCEDURE`, or `PREPARE`;
- route those inputs to the existing
  `unsupported_stored_program_statement` AST node;
- recognize top-level `SIGNAL` and `RESIGNAL` syntax as unsupported stored
  program/condition-handling placeholders;
- preserve the existing limited no-argument single-`SELECT` procedure bridge
  when the normal grammar can parse it;
- preserve ordinary multi-statement rejection for admin no-op and utility
  placeholders that are not stored-program scripts.

Out of scope:

- executing stored procedures with parameters, declarations, handlers, cursors,
  loops, condition handling, triggers, events, or stored functions;
- persisting stored routine, trigger, or event descriptors beyond the existing
  limited session-local procedure bridge;
- implementing `SIGNAL` warning/error diagnostics or `RESIGNAL` handler-state
  semantics;
- accepting arbitrary non-stored-program fragments that only happen to contain
  tokens such as `END`, `DECLARE`, or `IF`.

## MyLite Parser Direction

The normal Lemon grammar remains responsible for supported MyLite SQL. The
existing post-parse placeholder classifier runs only after a syntax error. This
slice broadens that classifier for stored-program scripts:

```text
unsupported_stored_program_script:
    ... CREATE [DEFINER ...] PROCEDURE ...
  | ... CREATE [DEFINER ...] FUNCTION ...
  | ... CREATE [DEFINER ...] TRIGGER ...
  | ... CREATE [DEFINER ...] EVENT ...
  | ... ALTER [DEFINER ...] EVENT ...
  | ... ALTER PROCEDURE ...
  | ... ALTER FUNCTION ...
  | ... DROP PROCEDURE ...
  | ... DROP FUNCTION ...
  | ... DROP TRIGGER ...
  | ... DROP EVENT ...
  | SIGNAL ...
  | RESIGNAL ...
```

The classifier must require a stored-program statement keyword sequence, not
just body-only tokens. Standalone fragments such as `END IF` remain syntax
errors.

## Runtime Behavior

Runtime behavior remains the existing unsupported stored-program diagnostic:

- return error `1064`;
- SQLSTATE `42000`;
- message includes `not supported`;
- no catalog, data, or transaction side effects.

## Tests

Tests cover:

- parser placeholder acceptance for multi-statement scripts that contain
  `CREATE PROCEDURE`, `DROP PROCEDURE` followed by `CREATE PROCEDURE`, and
  `ALTER DEFINER ... EVENT`;
- parser placeholder acceptance for top-level `SIGNAL` and `RESIGNAL`;
- parser rejection of body-only fragments such as `END IF`;
- runtime unsupported diagnostics for representative script, `SIGNAL`, and
  `RESIGNAL` placeholders;
- MySQL 8.4.9 expectation probes for compound procedure creation, top-level
  `SIGNAL`, and top-level `RESIGNAL`;
- parser-corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This is parser admission only. Stored-program execution remains unsupported
except for the existing limited no-argument single-`SELECT` procedure bridge.
`SIGNAL` and `RESIGNAL` move from raw parser rejection to parse-and-error
placeholders.

The detailed body-construct compatibility rows are tracked by
`docs/specs/baseline-stored-program-body-placeholders/specs.md`, which keeps the
same unsupported stored-program placeholder boundary while documenting
representative `BEGIN`, declaration, control-flow, cursor, handler, and
`RETURN` syntax.
