# Parser Corpus SHOW GRANTS Surfaces

This slice expands `SHOW GRANTS` parser compatibility for the MySQL
server-test corpus while preserving MyLite's embedded privilege model.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/show-grants.html
- https://dev.mysql.com/doc/refman/8.4/en/account-names.html
- https://dev.mysql.com/doc/refman/8.4/en/role-names.html

Runtime probes are verified against MySQL 8.4.9 before this slice is marked
complete.

## Scope

MySQL accepts:

```sql
SHOW GRANTS
SHOW GRANTS FOR user_or_role
SHOW GRANTS FOR user_or_role USING role [, role] ...
```

User and role names use account-name syntax. The user or role name may omit the
host part, in which case MySQL uses `%`; user and host parts may be quoted
separately with string or identifier quotes. `CURRENT_USER` and
`CURRENT_USER()` are accepted target spellings for the current account.

MyLite has no account store, role graph, grant descriptors, authentication, or
privilege enforcement. This slice therefore admits broader syntax but keeps the
existing embedded behavior:

- `SHOW GRANTS`, current-user forms, and root account spellings that resolve to
  `root@%` return the existing two synthetic grant rows.
- unknown named accounts return MySQL-shaped `1141 / 42000` no-such-grant
  diagnostics, using `%` for an omitted host and the literal empty string for a
  trailing bare `@`.
- `USING` role clauses are parsed. If the target resolves to embedded
  `root@%`, MyLite returns MySQL-shaped `3530 / HY000` for the first requested
  role because MyLite has no granted roles. If the target is not embedded
  `root@%`, the target no-such-grant diagnostic is returned first.

No role-derived grant expansion, account creation side effects, privilege
filtering, or grant-table storage is added.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
show_grants_statement ::= SHOW GRANTS.
show_grants_statement ::= SHOW GRANTS FOR show_grants_target show_grants_using_opt.

show_grants_target ::= CURRENT_USER.
show_grants_target ::= CURRENT_USER LPAREN RPAREN.
show_grants_target ::= show_grants_account_name.

show_grants_account_name ::= show_grants_account_user.
show_grants_account_name ::= show_grants_account_user user_variable_host.

show_grants_account_user ::= identifier.
show_grants_account_user ::= STRING.

show_grants_using_opt ::= .
show_grants_using_opt ::= USING show_grants_role_list.

show_grants_role_list ::= show_grants_role_name.
show_grants_role_list ::= show_grants_role_list COMMA show_grants_role_name.

show_grants_role_name ::= show_grants_account_user.
show_grants_role_name ::= show_grants_account_user user_variable_host.
```

The implementation stores a target node and an optional role-list node under
the `SHOW GRANTS` statement, so omitted hosts and current-user role clauses are
unambiguous.

## Runtime Behavior

No SQLite fork hook is needed. The runtime remains a constant-result or
constant-diagnostic path:

- embedded root with no `USING` returns the current synthetic grant result;
- embedded root with `USING` returns role-not-granted diagnostics;
- non-root named targets return no-such-grant diagnostics;
- no catalog, descriptor, file-format, or SQLite schema mutation occurs;
- successful results keep warning count `0`, affected rows `0`, and following
  `ROW_COUNT() = -1`.

## Tests

MySQL 8.4.9 expectations cover unquoted and quoted account names, omitted and
empty hosts, `CURRENT_USER USING role`, target-missing diagnostic precedence,
and role-not-granted diagnostics. The bare `@` lexer change needed for empty
account hosts also preserves MySQL's user-variable split: empty-name reads
return `NULL`, while empty-name assignments remain `3061 / 42000`. MyLite
parser tests cover the accepted AST shapes. Runtime tests cover root result
preservation and the new diagnostics.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

Observed after implementation:

```text
parse.csv.mysql_server_tests: queries=69595 ok=66406 errors=3189
parse_status: lexer_error=21 syntax_error=3167 stack_overflow=1
```

## Compatibility Status

This slice improves parser compatibility for `SHOW GRANTS` account and role
syntax. It does not mark roles, account storage, grant descriptors, privilege
checks, or role-expanded `SHOW GRANTS` output as supported.
