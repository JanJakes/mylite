# AUTO_INCREMENT Concurrency Integrity Tasks

- [x] Specify the serialized writer and durable high-water contract.
- [x] Add a cache-bypassing catalog counter read.
- [x] Rebase generated and explicit INSERT values under the writer boundary.
- [x] Initialize LOAD DATA and INSERT SELECT counters under the writer boundary.
- [x] Compare UPDATE counter advancement with the locked current value.
- [x] Add deterministic two-handle coverage for every duplicate policy.
- [x] Add close, reopen, rollback, savepoint, and process-death coverage.
- [x] Run focused Release, development, and sanitizer verification.
- [x] Update the remediation ledger and compatibility documentation.
