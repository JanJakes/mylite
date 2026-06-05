static int handle_update_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan)) {
        return handle_update_multiple_unique_key_conflict(
            database,
            executable_plan,
            plan,
            sqlite_rc
        );
    }
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

static int handle_update_multiple_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool exists = false;
        bool found = false;

        if (!candidate->index.is_unique ||
            !update_unique_key_index_has_assignment(plan, candidate)) {
            continue;
        }
        if (conflicting_index == NULL) {
            conflicting_index = candidate;
        }
        if (candidate->part_count == 1U && !candidate->parts[0].index_column.has_prefix_length) {
            const struct planned_value *value = update_unique_key_assignment_value_for_part(
                executable_plan,
                plan,
                &candidate->parts[0]
            );

            if (value == NULL || value->is_null) {
                continue;
            }
            rc = unique_key_single_value_exists(
                database,
                plan->table.physical_name,
                candidate,
                value,
                &exists
            );
            if (rc == MYLITE_OK && exists) {
                conflicting_index = candidate;
                rc = format_key_value(value, value_text, sizeof(value_text));
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
    if (conflicting_index == NULL || value_text[0] == '\0') {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    set_duplicate_key_error(
        database,
        plan->target.table_name,
        conflicting_index->index.name,
        value_text
    );
    return MYLITE_ERROR;
}

static bool update_unique_key_index_has_assignment(
    const struct planned_update *plan,
    const struct loaded_index_info *index
) {
    if (plan == NULL || index == NULL) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];

        if (planned_update_has_multiple_assignments(plan)) {
            if (planned_update_assignment_for_column(plan, part->index_column.column_id) != NULL) {
                return true;
            }
        } else if (part->index_column.column_id == plan->assignment_column.column_id) {
            return true;
        }
    }

    return false;
}

static const struct planned_value *update_unique_key_assignment_value_for_part(
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_part *part
) {
    if (executable_plan == NULL || plan == NULL || part == NULL) {
        return NULL;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        const struct planned_update_assignment *assignment = planned_update_assignment_for_column(
            executable_plan,
            part->index_column.column_id
        );

        return assignment == NULL ? NULL : &assignment->value;
    }
    if (part->index_column.column_id == plan->assignment_column.column_id) {
        return &executable_plan->assignment_value;
    }

    return NULL;
}
