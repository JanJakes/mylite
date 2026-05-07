# SQL users, roles, and privileges

Account, role, password, privilege grant, and active-role statement surface.

| Feature | Status | Notes |
| --- | --- | --- |
| Embedded current identity | 🟡 | Limited scalar `USER()`, `SESSION_USER()`, `SYSTEM_USER()`, `CURRENT_USER()`, and bare `CURRENT_USER` expose `root@%`; no account storage, authentication, roles, privileges, definer behavior, `IGNORE_SPACE`, or stored-function resolution |
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
