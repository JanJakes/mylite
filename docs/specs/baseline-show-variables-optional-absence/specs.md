# Baseline SHOW VARIABLES Optional Absence

## Summary

MyLite targets the pinned MySQL 8.4.9 runtime, not every optional plugin,
Enterprise, debug, or NDB Cluster build. Some documented system variables are
absent from that runtime. This slice records the absence contract for those
target-build-absent variables:

- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` return no rows when filtered to the absent names.
- Scalar reads such as `SELECT @@audit_log_buffer_size` fail with
  `1193 / HY000` unknown-system-variable diagnostics.
- MyLite intentionally keeps these variables out of its runtime registry.

This is a compatibility decision, not a placeholder-variable implementation.
Variables that are present in the MySQL 8.4.9 target runtime remain separate
compatibility work and must not be covered by this absence slice.

## Compatibility Authority

The supported surface is based on MySQL 8.4.9 runtime probes recorded in
`packages/libmylite/tests/mysql_baseline_show_variables_optional_absence_expectations.sh`.
The probe compares the red system-variable inventory against
`performance_schema.variables_info` in the target runtime, verifies all absent
names return zero `SHOW VARIABLES` rows for default/session/local/global
scopes, and verifies representative scalar reads return unknown-variable
diagnostics.

## Syntax

No grammar expansion is required. Existing MyLite grammar already admits the
statement shapes used by this slice:

```lemon
cmd ::= SHOW show_variable_scope_opt VARIABLES show_like_or_where_opt.
expr ::= SYSTEM_VARIABLE.
```

The existing `SHOW VARIABLES WHERE` predicate subset continues to apply. This
slice does not add arbitrary `WHERE` expressions or new scalar expression
contexts.

## Semantics

### SHOW VARIABLES

For the target-build-absent variable names, MyLite must produce an empty
result set with the normal `SHOW VARIABLES` column shape:

| Column | Type |
| --- | --- |
| `Variable_name` | text |
| `Value` | text |

The absence applies equally to these forms:

- `SHOW VARIABLES WHERE Variable_name IN (...)`
- `SHOW SESSION VARIABLES WHERE Variable_name IN (...)`
- `SHOW LOCAL VARIABLES WHERE Variable_name IN (...)`
- `SHOW GLOBAL VARIABLES WHERE Variable_name IN (...)`

The result has zero rows, warning count `0`, and affected rows `0`.

### Scalar Reads

Representative scalar reads of absent variables return:

- error number `1193`;
- SQLSTATE `HY000`;
- message containing `Unknown system variable '<name>'`.

This matches MySQL's behavior for absent plugin/component variables, including
dotted names such as `validate_password.length`.

## MyLite-Specific Decisions

MyLite does not add registry descriptors for target-build-absent variables.
Doing so would make `SHOW VARIABLES` diverge from the pinned target runtime and
would imply false support for plugin state that is not present in the target
server.

If a future target runtime enables one of these optional components, that name
must move out of this slice and receive an explicit value, scope, mutability,
and diagnostics specification.

## Out of Scope

- Value, scope, mutability, privilege, startup, or persisted-variable behavior
  for variables that are present in the target runtime.
- Installing optional MySQL plugins, components, Enterprise features, debug
  builds, NDB Cluster, thread pool, telemetry, keyring, semisync replication,
  query rewrite, or validate-password components.
- Performance Schema variable table emulation.
- `SET` behavior for absent variables beyond the existing unknown-variable
  diagnostics for scalar/assignment resolution paths.

## Documentation Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/runtime-system-variables.md`
- `docs/compatibility/sql-show-statements.md`
