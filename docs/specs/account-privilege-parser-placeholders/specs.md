# Account And Privilege Parser Placeholders

## Scope

This slice recognizes MySQL account, role, privilege, and grant-management
statement families as parser placeholders. It does not implement grant tables,
authentication plugins, password state, TLS requirements, active/default roles,
privilege checks, privilege metadata result sets, binary logging, or security
side effects.

The covered statement families are:

- `ALTER USER`
- `CREATE USER`
- `CREATE ROLE`
- `DROP USER`
- `DROP ROLE`
- `GRANT`
- `RENAME USER`
- `REVOKE`
- `SET DEFAULT ROLE`
- `SET PASSWORD`
- `SET ROLE`
- `SHOW GRANTS`
- `SHOW PRIVILEGES`

## Sources

- MySQL 8.4 Reference Manual, `CREATE USER` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-user.html
- MySQL 8.4 Reference Manual, `ALTER USER` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/alter-user.html
- MySQL 8.4 Reference Manual, `GRANT` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/grant.html
- MySQL 8.4 Reference Manual, `REVOKE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/revoke.html
- MySQL 8.4 Reference Manual, `CREATE ROLE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-role.html
- MySQL 8.4 Reference Manual, `SET DEFAULT ROLE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-default-role.html
- MySQL 8.4 Reference Manual, `SET PASSWORD` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-password.html
- MySQL 8.4 Reference Manual, `SET ROLE` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-role.html
- MySQL 8.4 Reference Manual, `SHOW GRANTS` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-grants.html
- MySQL 8.4 Reference Manual, `SHOW PRIVILEGES` Statement:
  https://dev.mysql.com/doc/refman/8.4/en/show-privileges.html
- Runtime probes against local `mysql:8.4.9` container
  `mylite-mysql-849`.

## MySQL Runtime Observations

Representative MySQL 8.4.9 probes accepted:

- `CREATE USER IF NOT EXISTS 'mylite_probe_u'@'localhost' IDENTIFIED BY 'x' ACCOUNT LOCK`
- `ALTER USER IF EXISTS 'mylite_probe_u'@'localhost' IDENTIFIED BY 'y' PASSWORD EXPIRE`
- `CREATE ROLE IF NOT EXISTS 'mylite_probe_role'`
- `GRANT SELECT ON mylite_placeholder_probe.* TO 'mylite_probe_u'@'localhost'`
- `GRANT 'mylite_probe_role' TO 'mylite_probe_u'@'localhost' WITH ADMIN OPTION`
- `SHOW GRANTS FOR 'mylite_probe_u'@'localhost'`
- `REVOKE SELECT ON mylite_placeholder_probe.* FROM 'mylite_probe_u'@'localhost'`
- `SET DEFAULT ROLE 'mylite_probe_role' TO 'mylite_probe_u'@'localhost'`
- `SET ROLE DEFAULT`
- `SET PASSWORD FOR 'mylite_probe_u'@'localhost' = 'z'`
- `DROP ROLE IF EXISTS 'mylite_probe_role'`
- `DROP USER IF EXISTS 'mylite_probe_u'@'localhost'`

The local server runs with `--skip-name-resolve`, so some account-management
statements emit MySQL warning `1285`; this is an environment warning and not
part of MyLite's placeholder behavior.

## Syntax

The grammar uses exact statement-family prefixes and a shared parser-placeholder
tail for the option-heavy MySQL account grammar. This intentionally recognizes
the real MySQL surface without constructing account-specific ASTs while the
runtime is still a no-op placeholder.

```lemon
account_statement ::= ALTER USER parser_placeholder_tail.
account_statement ::= ALTER USER IF EXISTS parser_placeholder_tail.
account_statement ::= CREATE USER parser_placeholder_tail.
account_statement ::= CREATE USER IF NOT EXISTS parser_placeholder_tail.
account_statement ::= CREATE ROLE parser_placeholder_tail.
account_statement ::= CREATE ROLE IF NOT EXISTS parser_placeholder_tail.
account_statement ::= DROP USER parser_placeholder_tail.
account_statement ::= DROP USER IF EXISTS parser_placeholder_tail.
account_statement ::= DROP ROLE parser_placeholder_tail.
account_statement ::= DROP ROLE IF EXISTS parser_placeholder_tail.
account_statement ::= RENAME USER parser_placeholder_tail.
account_statement ::= GRANT parser_placeholder_tail.
account_statement ::= REVOKE parser_placeholder_tail.
account_statement ::= SET DEFAULT ROLE parser_placeholder_tail.
account_statement ::= SET ROLE parser_placeholder_tail.
account_statement ::= SET PASSWORD parser_placeholder_tail.
account_statement ::= SHOW GRANTS.
account_statement ::= SHOW GRANTS parser_placeholder_tail.
account_statement ::= SHOW PRIVILEGES.
```

The placeholder tail accepts identifiers, quoted identifiers, string and numeric
literals, user/system variables, punctuation, and the reserved or mapped keyword
tokens that appear in these statement families.

## Runtime Semantics

Each statement family has a dedicated placeholder kind so diagnostics are
specific. Preparing maps it to a custom statement. Executing appends one warning
with code `1235`, returns `MYLITE_DONE`, reports zero affected rows, and returns
no result columns.

No account, role, password, privilege, or default-role state is created or
changed. `CURRENT_ROLE()` keeps returning the existing embedded `NONE`
placeholder result.

## Tests

Parser coverage must include representative syntax for every covered statement
family, including account names with `user@host` spelling, role lists, grant and
revoke privilege forms, `SHOW GRANTS FOR ... USING ...`, and `SHOW PRIVILEGES`.

Runtime coverage must assert warning `1235` and no result columns for
representative account, role, grant, revoke, `SET ROLE`, and `SHOW` forms.
