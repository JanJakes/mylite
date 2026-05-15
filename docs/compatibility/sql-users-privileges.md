# SQL users, roles, and privileges

Account, role, password, privilege grant, and active-role statement surface.

| Feature | Status | Notes |
| --- | --- | --- |
| Embedded current identity | 🟡 | Limited scalar `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_USER()`, and bare `CURRENT_USER` expose `root@%`; limited `INFORMATION_SCHEMA.USER_PRIVILEGES` and current-user `SHOW GRANTS` expose synthetic global privilege rows/text for that embedded identity; no account storage, authentication, roles, grant descriptors, privilege enforcement, definer behavior, `IGNORE_SPACE`, or stored-function resolution |
| Privilege metadata views | 🟡 | Limited synthetic `INFORMATION_SCHEMA.USER_PRIVILEGES` global rows for `root@%` plus empty `SCHEMA_PRIVILEGES`, `TABLE_PRIVILEGES`, and `COLUMN_PRIVILEGES` system views; no grant storage, grant/revoke DDL, roles, lower-level privilege rows, privilege filtering, or enforcement |
| `SHOW GRANTS` | 🟡 | Limited current-user forms `SHOW GRANTS`, `SHOW GRANTS FOR CURRENT_USER`, and `SHOW GRANTS FOR CURRENT_USER()` return fixed MySQL 8.4.9-shaped global grant text for `root@%`; no named accounts, role `USING`, account storage, grant descriptors, privilege enforcement, or filters |
| Current active role | 🟡 | Limited scalar `CURRENT_ROLE()` returns `NONE`; no role catalog, grants, default roles, active-role state, `SET ROLE`, privileges, or bare `CURRENT_ROLE` |
| `ALTER USER` | ❌ | Auth, TLS, resources, roles |
| `CREATE USER` | ❌ | Auth factors, TLS, resources |
| `CREATE ROLE` | ❌ | Role creation syntax and metadata |
| `DROP USER` | ❌ | User deletion syntax and privilege cleanup |
| `DROP ROLE` | ❌ | Role deletion syntax and grant cleanup |
| `GRANT` | ❌ | Privilege and role grants |
| `RENAME USER` | ❌ | User rename syntax and privilege metadata |
| `REVOKE` | ❌ | Privilege and role revocation semantics |
| `SET DEFAULT ROLE` | ❌ | Default role assignment |
| `SET PASSWORD` | ❌ | Password assignment semantics |
| `SET ROLE` | ❌ | Active-role selection |

[Back to compatibility overview](../../COMPATIBILITY.md)
