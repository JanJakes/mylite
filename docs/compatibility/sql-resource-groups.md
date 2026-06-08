# SQL resource groups

Resource group DDL and thread assignment statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `CREATE RESOURCE GROUP` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no thread resource-group metadata or scheduler side effects |
| `ALTER RESOURCE GROUP` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no thread resource-group metadata or scheduler side effects |
| `DROP RESOURCE GROUP` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no thread resource-group metadata or scheduler side effects |
| `SET RESOURCE GROUP` | ⚪ | Parsed and accepted as an embedded no-op with warning `1105`; no thread assignment side effects |

[Back to compatibility overview](../../COMPATIBILITY.md)
