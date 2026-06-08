# SQL XA transactions

XA transaction branch lifecycle and recovery statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `XA START` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no XA transaction branch state |
| `XA END` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no XA transaction branch state |
| `XA PREPARE` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no prepare phase |
| `XA COMMIT` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no one-phase or two-phase commit state |
| `XA ROLLBACK` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no XA rollback state |
| `XA RECOVER` | ⚪ | Parsed and rejected with unsupported utility diagnostics; no XA recovery rows or transaction registry |

[Back to compatibility overview](../../COMPATIBILITY.md)
