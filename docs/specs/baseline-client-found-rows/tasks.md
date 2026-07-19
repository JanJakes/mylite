# Baseline CLIENT_FOUND_ROWS Tasks

## Checklist

- [x] Verify default and `MYSQLI_CLIENT_FOUND_ROWS` no-op UPDATE affected rows
      against MySQL 8.4.9 for direct and prepared execution.
- [x] Specify the connection policy, core affected-row metadata, ownership,
      diagnostics, ABI, storage, and non-goals.
- [x] Add a core connection policy API and session state.
- [x] Publish policy-selected affected rows from successful UPDATE execution.
- [x] Accept only `MYSQLI_CLIENT_FOUND_ROWS` in the mysqli connection flags.
- [x] Apply found-row reporting to direct and native prepared mysqli UPDATEs.
- [x] Preserve default changed rows and align SQL `ROW_COUNT()` with the flag.
- [x] Cover native, PHP, unsupported-flag, and application behavior.
- [x] Update compatibility and PHP extension documentation.
- [x] Run focused native/PHP tests and the MediaWiki application baseline.
- [ ] Run the complete release qualification after integration.
