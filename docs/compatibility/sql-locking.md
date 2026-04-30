# SQL locking

Instance and table locking statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `LOCK INSTANCE FOR BACKUP` | ❌ | Backup lock syntax and embedded behavior |
| `UNLOCK INSTANCE` | ❌ | Backup lock release syntax |
| `LOCK TABLES` | ❌ | Lock modes and implicit commit |
| `UNLOCK TABLES` | ❌ | Table lock release and transaction interaction |

[Back to compatibility overview](../../COMPATIBILITY.md)
