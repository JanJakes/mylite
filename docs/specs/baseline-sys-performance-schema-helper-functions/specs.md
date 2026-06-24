# Baseline sys Performance Schema Helper Functions

## Scope

This slice adds executable support for the remaining MySQL 8.4.9 `sys`
Performance Schema helper functions:

- `sys.ps_is_account_enabled(host, user)`
- `sys.ps_is_consumer_enabled(consumer)`
- `sys.ps_is_instrument_default_enabled(instrument)`
- `sys.ps_is_instrument_default_timed(instrument)`
- `sys.ps_is_thread_instrumented(connection_id)`
- `sys.ps_thread_account(thread_id)`
- `sys.ps_thread_id(connection_id)`
- `sys.ps_thread_stack(thread_id, debug)`
- `sys.ps_thread_trx_info(thread_id)`

Unqualified helper names are accepted in the same expression contexts as the
existing sys helper functions, and schema-qualified `sys.<name>(...)` calls are
rewritten by the existing sys-helper normalization allow-list.

Out of scope:

- Live Performance Schema setup table mutability.
- Background thread inventory.
- Statement, stage, wait, transaction, or stack event collection.
- The deprecated `sys.ps_thread_id()` name-conflict warning emitted by MySQL
  8.4.9 because a native Performance Schema function with the same name exists.
- Stored sys procedures.

## MySQL Authority

Compatibility is based on the MySQL 8.4 Reference Manual sys stored-function
pages and MySQL 8.4.9 runtime probes against `mylite-mysql-849`.

Relevant documentation:

- `ps_is_account_enabled()` returns `YES` or `NO` for account
  instrumentation.
- `ps_is_consumer_enabled()` returns `YES`, `NO`, or `NULL`; invalid consumer
  names raise error `3047 / HY000`.
- `ps_is_instrument_default_enabled()` and
  `ps_is_instrument_default_timed()` return `YES` or `NO` for default
  instrument setup.
- `ps_is_thread_instrumented()` returns `YES`, `NO`, `UNKNOWN`, or `NULL`.
- `ps_thread_account()` maps a Performance Schema thread id to an account
  string.
- `ps_thread_id()` maps a connection id to a Performance Schema thread id, or
  uses the current connection when the argument is `NULL`.
- `ps_thread_stack()` returns JSON text for a thread stack.
- `ps_thread_trx_info()` returns JSON text for transaction history.

Observed MySQL 8.4.9 target behavior:

- Default consumer values include `thread_instrumentation = YES`,
  `events_statements_history = NO`, `events_waits_current = NO`, and
  `events_transactions_history_long = NO`.
- Invalid consumer names raise `ERROR 3047 (HY000)`.
- Default instrument helpers return `NO` for disabled internal mutex
  instruments, `YES` for `statement/sql/select`, `NO` for timed memory
  instruments, and `YES` for `NULL` input.
- The current connection is instrumented; `NULL` connection ids return `NULL`;
  unknown connection ids return `UNKNOWN`.
- `ps_thread_id(CONNECTION_ID())` and `ps_thread_id(NULL)` return non-`NULL`
  thread ids in MySQL; unknown connection ids return `NULL`.
- `ps_thread_account(current_thread_id)` returns a non-`NULL` account string;
  unknown thread ids return `NULL`.
- `ps_thread_stack(NULL, 0)` returns valid JSON.
- `ps_thread_trx_info(NULL)` and unknown thread ids return `NULL`.
- Invalid unsigned integer input such as `'abc'` raises `1366 / HY000`; negative
  input raises `1264 / 22003`.

## Runtime Semantics

The functions are implemented in MyLite's sys helper module and registered as
SQLite scalar UDFs through the public SQLite extension API. No SQLite fork hook
is needed.

MyLite treats its session `connection_id` as the synthetic Performance Schema
`thread_id`, matching existing `sys.processlist`, `sys.session`,
`sys.x$session`, and `sys.session_ssl_status` synthetic rows. This keeps thread
ids consistent across the current embedded metadata surface without adding a
second state store.

### Function Details

- `ps_is_account_enabled(host, user)` returns `YES` for the current embedded
  default account setup. MyLite does not expose configurable Performance Schema
  account instrumentation yet.
- `ps_is_consumer_enabled(consumer)` returns MySQL 8.4.9 default values for the
  target `performance_schema.setup_consumers` names and `NULL` for `NULL`.
  Unknown names raise `3047 / HY000`.
- `ps_is_instrument_default_enabled(instrument)` returns selected MySQL 8.4.9
  default setup values: disabled internal mutex patterns return `NO`; all other
  values, including `NULL`, return `YES`.
- `ps_is_instrument_default_timed(instrument)` returns `NO` for disabled
  internal mutex patterns and memory instruments, and `YES` otherwise.
- `ps_is_thread_instrumented(connection_id)` returns `NULL` for `NULL`, `YES`
  for known MyLite processlist sessions, and `UNKNOWN` for missing ids.
- `ps_thread_id(connection_id)` returns the current MyLite connection id for
  `NULL`, returns the same id for known MyLite processlist sessions, and
  returns `NULL` for missing ids.
- `ps_thread_account(thread_id)` returns the matching MyLite session's
  `client_user_identity` or `NULL`.
- `ps_thread_stack(thread_id, debug)` validates the unsigned thread-id argument
  and returns a MySQL-shaped empty JSON stack placeholder. The signed boolean
  `debug` flag is accepted but has no effect because MyLite does not collect
  live Performance Schema statement, stage, wait, or stack events.
- `ps_thread_trx_info(thread_id)` returns `NULL` for `NULL` or missing ids and
  `[]` for known MyLite session thread ids. MyLite does not collect transaction
  statement history yet.

## Syntax

The existing generic-function path handles the calls:

```lemon
expr ::= generic_function_call.
generic_function_call ::= ident LP opt_expr_list RP.
generic_function_call ::= ident DOT ident LP opt_expr_list RP.
```

MyLite's normalization layer rewrites schema-qualified calls from the `sys`
allow-list to unqualified helper names before parsing. Argument-count validation
is performed before runtime evaluation.

## Diagnostics

- `ps_is_consumer_enabled('not_a_consumer')` raises:
  - code: `3047`
  - SQLSTATE: `HY000`
  - message: `Invalid argument error: not_a_consumer in function sys.ps_is_consumer_enabled.`
- Invalid unsigned thread/connection arguments raise:
  - `1366 / HY000` for nonnumeric text.
  - `1264 / 22003` for negative or out-of-range input.

## Tests

The focused runtime test covers:

- schema-qualified direct and `DUAL` calls;
- unqualified row-backed calls lowered through SQLite UDFs;
- current-session thread-id, account, and instrumentation mapping;
- unknown-id `NULL` and `UNKNOWN` behavior;
- consumer and default-instrument values;
- empty stack and transaction placeholders;
- invalid consumer, invalid integer, and out-of-range diagnostics.

The MySQL expectation script records the MySQL 8.4.9 behavior used to shape the
supported and intentionally placeholder portions of the slice.
