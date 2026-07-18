#include "mylite_version_comment.h"

enum {
    mylite_mysql_version_comment_gate = 80409,
    version_comment_decimal_radix = 10,
    version_comment_min_length = 5,
};

static bool ascii_byte_is_digit(char byte);

bool mylite_sql_version_comment_parse(
    const char *text,
    size_t length,
    struct mylite_sql_version_comment_payload *out_payload
) {
    const char *payload_start = NULL;
    const char *payload_end = NULL;
    const char *cursor = NULL;
    unsigned int version = 0U;
    bool has_version = false;

    if (out_payload == NULL) {
        return false;
    }
    *out_payload = (struct mylite_sql_version_comment_payload){0};
    if (text == NULL || length < (size_t)version_comment_min_length || text[0] != '/' ||
        text[1] != '*' || text[2] != '!' || text[length - 2U] != '*' || text[length - 1U] != '/') {
        return false;
    }

    payload_start = text + 3U;
    payload_end = text + length - 2U;
    cursor = payload_start;
    while (cursor < payload_end && ascii_byte_is_digit(*cursor)) {
        has_version = true;
        if (version <= (unsigned int)mylite_mysql_version_comment_gate) {
            version = (version * (unsigned int)version_comment_decimal_radix) +
                      (unsigned int)(*cursor - '0');
        }
        ++cursor;
    }

    out_payload->text = cursor;
    out_payload->length = (size_t)(payload_end - cursor);
    out_payload->active =
        !has_version || version <= (unsigned int)mylite_mysql_version_comment_gate;
    return true;
}

static bool ascii_byte_is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}
