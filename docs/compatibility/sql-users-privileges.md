# SQL users, roles, and privileges

Account, role, password, privilege grant, and active-role statement surface.

| Feature | Status | Notes |
| --- | --- | --- |
| Embedded current identity | 🟡 | Limited scalar `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_USER()`, and bare `CURRENT_USER` expose `root@%`; limited `INFORMATION_SCHEMA.USER_PRIVILEGES`, `INFORMATION_SCHEMA.USER_ATTRIBUTES`, and current-user plus simple named-root `SHOW GRANTS` expose synthetic global privilege rows, a `NULL` root attribute row, and grant text for that embedded identity; limited `SHOW PRIVILEGES` exposes the static MySQL 8.4.9 privilege-name catalog; account/role/privilege management statements are accepted as embedded no-ops with a warning; no account storage, authentication, roles, grant descriptors, privilege enforcement, definer behavior, `IGNORE_SPACE`, or stored-function resolution |
| Privilege metadata views | 🟡 | Limited synthetic `INFORMATION_SCHEMA.USER_PRIVILEGES` global rows for `root@%` plus empty `SCHEMA_PRIVILEGES`, `TABLE_PRIVILEGES`, and `COLUMN_PRIVILEGES` system views; no grant storage, grant/revoke DDL, roles, lower-level privilege rows, privilege filtering, or enforcement |
| `SHOW GRANTS` | 🟡 | Limited current-user forms `SHOW GRANTS`, `SHOW GRANTS FOR CURRENT_USER`, `SHOW GRANTS FOR CURRENT_USER()`, and simple named `SHOW GRANTS FOR 'root'@'%'` return fixed MySQL 8.4.9-shaped global grant text for `root@%`; other simple named accounts return `1141 / 42000`; no roles, `USING`, account storage, grant descriptors, privilege enforcement, or filters |
| `SHOW PRIVILEGES` | 🟡 | Limited static MySQL 8.4.9-shaped privilege-name catalog with columns `Privilege`, `Context`, and `Comment` and the observed 73 target-runtime rows; no filters, account storage, grant descriptors, privilege enforcement, mutable dynamic privileges, ordering, or limits |
| Current active role | 🟡 | Limited scalar `CURRENT_ROLE()` returns `NONE`; `SET ROLE` is accepted as an embedded no-op warning; no role catalog, grants, default roles, active-role state, privileges, or bare `CURRENT_ROLE` |
| `ALTER USER` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no auth, TLS, resources, roles, metadata, or persistence |
| `CREATE USER` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no account creation, auth factors, TLS, resources, attributes, roles, metadata, or persistence |
| `CREATE ROLE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no role creation, role graph, metadata, or persistence |
| `DROP USER` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no user deletion, privilege cleanup, metadata, or persistence |
| `DROP ROLE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no role deletion, grant cleanup, metadata, or persistence |
| `GRANT` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no privilege or role grants, metadata, enforcement, or persistence |
| `RENAME USER` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no account rename, privilege metadata, or persistence |
| `REVOKE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no privilege or role revocation, metadata, enforcement, or persistence |
| `SET DEFAULT ROLE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no default-role metadata or active-role effect |
| `SET PASSWORD` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no password storage or authentication effect |
| `SET ROLE` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no active-role state change |

[Back to compatibility overview](../../COMPATIBILITY.md)
