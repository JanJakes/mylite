# Wire protocol

## Connection, authentication, and packets

| Feature | Status | Notes |
| --- | --- | --- |
| Protocol 10 initial handshake | ❌ | Greeting, auth data, status, charset, capabilities |
| Capability flags | ❌ | Capability negotiation and rejection |
| SSLRequest and TLS upgrade | ❌ | TLS negotiation and failures |
| Handshake Response 41 | ❌ | User, auth, database, attributes, charset |
| `caching_sha2_password` | ❌ | Auth packet flow |
| `sha256_password` | ❌ | Password exchange packet flow and diagnostics |
| `mysql_native_password` | ❌ | Deprecated plugin compatibility and behavior |
| Auth switch request/response | ❌ | Plugin switching packet flow |
| Auth more data | ❌ | Multi-step authentication packet flow |
| OK packet | ❌ | Rows, insert id, status, warnings, session state |
| ERR packet | ❌ | Code, SQLSTATE, message, fatality |
| EOF packet compatibility | ❌ | Legacy EOF without CLIENT_DEPRECATE_EOF |
| Result set metadata | ❌ | Column metadata and EOF/OK termination; limited public `mylite_result` metadata accessors are separate from the wire protocol surface |
| Text result rows | ❌ | Text row encoding, NULLs, charsets |
| Binary result rows | ❌ | Binary row encoding and null bitmap |
| LOCAL INFILE request | ❌ | LOCAL INFILE flow and security |
| Compression protocol | ❌ | Compression framing and negotiation |
| Zstandard compression | ❌ | zstd negotiation and packets |
| Connection attributes | ❌ | Connection attributes metadata |
| Session state tracking | ❌ | Schema, variables, GTIDs, transaction notices |

## Command packets

| Command | Status | Notes |
| --- | --- | --- |
| `COM_SLEEP` | ❌ | Packet parsing, responses, status, errors |
| `COM_QUIT` | ❌ | Packet parsing, responses, status, errors |
| `COM_INIT_DB` | ❌ | Packet parsing, responses, status, errors |
| `COM_QUERY` | ❌ | Packet parsing, responses, status, errors |
| `COM_FIELD_LIST` | ❌ | Packet parsing, responses, status, errors |
| `COM_CREATE_DB` | ❌ | Packet parsing, responses, status, errors |
| `COM_DROP_DB` | ❌ | Packet parsing, responses, status, errors |
| `COM_UNUSED_2` | ❌ | Removed command slot diagnostics |
| `COM_UNUSED_1` | ❌ | Removed command slot diagnostics |
| `COM_STATISTICS` | ❌ | Packet parsing, responses, status, errors |
| `COM_UNUSED_4` | ❌ | Removed command slot diagnostics |
| `COM_CONNECT` | ❌ | Packet parsing, responses, status, errors |
| `COM_UNUSED_5` | ❌ | Removed command slot diagnostics |
| `COM_DEBUG` | ❌ | Packet parsing, responses, status, errors |
| `COM_PING` | ❌ | Packet parsing, responses, status, errors |
| `COM_TIME` | ❌ | Packet parsing, responses, status, errors |
| `COM_DELAYED_INSERT` | ❌ | Packet parsing, responses, status, errors |
| `COM_CHANGE_USER` | ❌ | Packet parsing, responses, status, errors |
| `COM_BINLOG_DUMP` | ❌ | Packet parsing, responses, status, errors |
| `COM_TABLE_DUMP` | ❌ | Packet parsing, responses, status, errors |
| `COM_CONNECT_OUT` | ❌ | Packet parsing, responses, status, errors |
| `COM_REGISTER_SLAVE` | ❌ | Packet parsing, responses, status, errors |
| `COM_STMT_PREPARE` | ❌ | Packet parsing, responses, status, errors |
| `COM_STMT_EXECUTE` | ❌ | Packet parsing, responses, status, errors |
| `COM_STMT_SEND_LONG_DATA` | ❌ | Packet parsing, responses, status, errors |
| `COM_STMT_CLOSE` | ❌ | Packet parsing, responses, status, errors |
| `COM_STMT_RESET` | ❌ | Packet parsing, responses, status, errors |
| `COM_SET_OPTION` | ❌ | Packet parsing, responses, status, errors |
| `COM_STMT_FETCH` | ❌ | Packet parsing, responses, status, errors |
| `COM_DAEMON` | ❌ | Packet parsing, responses, status, errors |
| `COM_BINLOG_DUMP_GTID` | ❌ | Packet parsing, responses, status, errors |
| `COM_RESET_CONNECTION` | ❌ | Packet parsing, responses, status, errors |
| `COM_CLONE` | ❌ | Clone command diagnostics |
| `COM_SUBSCRIBE_GROUP_REPLICATION_STREAM` | ❌ | Group Replication stream command |

[Back to compatibility overview](../../COMPATIBILITY.md)
