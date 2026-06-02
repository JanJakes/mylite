static int handle_update_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = MYLITE_OK;

    if (executable_plan->assignment_value.is_null) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool exists = false;
        bool found = false;

        if (!candidate->index.is_unique ||
            !loaded_index_contains_column_id(candidate, plan->assignment_column.column_id)) {
            continue;
        }
        if (conflicting_index == NULL) {
            conflicting_index = candidate;
        }
        if (candidate->part_count == 1U && !candidate->parts[0].index_column.has_prefix_length) {
            rc = unique_key_single_value_exists(
                database,
                plan->table.physical_name,
                candidate,
                &executable_plan->assignment_value,
                &exists
            );
            if (rc == MYLITE_OK && exists) {
                conflicting_index = candidate;
                rc = format_key_value(
                    &executable_plan->assignment_value,
                    value_text,
                    sizeof(value_text)
                );
                break;
            }
        } else {
            rc = find_update_unique_key_conflict_tuple(
                database,
                executable_plan,
                plan,
                candidate,
                value_text,
                sizeof(value_text),
                &found
            );
            if (rc == MYLITE_OK && found) {
                conflicting_index = candidate;
                break;
            }
        }
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (value_text[0] == '\0') {
        rc = format_key_value(&executable_plan->assignment_value, value_text, sizeof(value_text));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    set_duplicate_key_error(
        database,
        plan->target.table_name,
        conflicting_index->index.name,
        value_text
    );
    return MYLITE_ERROR;
}
