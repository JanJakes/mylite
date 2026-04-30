#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite/parser.h"

typedef struct input_buffer {
	char *data;
	size_t length;
	size_t capacity;
} input_buffer;

static int run_cli(int argc, char **argv);
static int parse_single_sql(const char *sql, size_t length, int quiet);
static int parse_nul_batch(const char *data, size_t length, int quiet);
static int read_stdin(input_buffer *buffer);
static int append_bytes(input_buffer *buffer, const char *data, size_t length);
static void free_buffer(input_buffer *buffer);
static void print_result(const mylite_parse_result *result);
static void print_usage(const char *program);

int main(int argc, char **argv)
{
	return run_cli(argc, argv);
}

static int run_cli(int argc, char **argv)
{
	input_buffer buffer = { 0 };
	int quiet = 0;
	int nul_batch = 0;
	int argi;
	int rc;

	for (argi = 1; argi < argc; argi++) {
		if (strcmp(argv[argi], "--quiet") == 0) {
			quiet = 1;
		} else if (strcmp(argv[argi], "--nul") == 0) {
			nul_batch = 1;
		} else if (strcmp(argv[argi], "--help") == 0) {
			print_usage(argv[0]);
			return 0;
		} else if (argv[argi][0] == '-') {
			fprintf(stderr, "unknown option: %s\n", argv[argi]);
			print_usage(argv[0]);
			return 2;
		} else {
			if (buffer.length > 0 && !append_bytes(&buffer, " ", 1)) {
				free_buffer(&buffer);
				return 2;
			}
			if (!append_bytes(&buffer, argv[argi], strlen(argv[argi]))) {
				free_buffer(&buffer);
				return 2;
			}
		}
	}

	if (buffer.length == 0 && !read_stdin(&buffer)) {
		free_buffer(&buffer);
		return 2;
	}

	if (nul_batch) {
		rc = parse_nul_batch(buffer.data, buffer.length, quiet);
	} else {
		rc = parse_single_sql(buffer.data, buffer.length, quiet);
	}
	free_buffer(&buffer);
	return rc;
}

static int parse_single_sql(const char *sql, size_t length, int quiet)
{
	mylite_parse_result result;
	int ok = mylite_parse_sql(sql, length, &result);

	if (!quiet) {
		if (ok) {
			print_result(&result);
		} else {
			fprintf(stderr, "error at %u:%u: %s\n",
			        result.error_line, result.error_column, result.error);
		}
	}
	mylite_parse_result_free(&result);
	return ok ? 0 : 1;
}

static int parse_nul_batch(const char *data, size_t length, int quiet)
{
	size_t start = 0;
	size_t index = 0;
	size_t failures = 0;
	size_t i;

	for (i = 0; i < length; i++) {
		if (data[i] != '\0') {
			continue;
		}
		if (i > start) {
			mylite_parse_result result;
			int ok = mylite_parse_sql(data + start, i - start, &result);
			if (!ok) {
				failures++;
				if (!quiet) {
					fprintf(stderr, "%zu\t%u:%u\t%s\n",
					        index + 1, result.error_line, result.error_column, result.error);
				}
			}
			mylite_parse_result_free(&result);
		}
		index++;
		start = i + 1;
	}
	if (length > start) {
		mylite_parse_result result;
		int ok = mylite_parse_sql(data + start, length - start, &result);
		if (!ok) {
			failures++;
			if (!quiet) {
				fprintf(stderr, "%zu\t%u:%u\t%s\n",
				        index + 1, result.error_line, result.error_column, result.error);
			}
		}
		mylite_parse_result_free(&result);
		index++;
	}

	if (!quiet) {
		printf("records=%zu failures=%zu\n", index, failures);
	}
	return failures == 0 ? 0 : 1;
}

static int read_stdin(input_buffer *buffer)
{
	char chunk[8192];
	size_t nread;

	while ((nread = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
		if (!append_bytes(buffer, chunk, nread)) {
			return 0;
		}
	}
	if (ferror(stdin)) {
		fprintf(stderr, "failed to read stdin: %s\n", strerror(errno));
		return 0;
	}
	return 1;
}

static int append_bytes(input_buffer *buffer, const char *data, size_t length)
{
	char *next;
	size_t next_capacity;

	if (length == 0) {
		return 1;
	}
	if (buffer->length + length > buffer->capacity) {
		next_capacity = buffer->capacity == 0 ? 8192 : buffer->capacity;
		while (next_capacity < buffer->length + length) {
			next_capacity *= 2;
		}
		next = (char *)realloc(buffer->data, next_capacity);
		if (next == NULL) {
			fprintf(stderr, "out of memory\n");
			return 0;
		}
		buffer->data = next;
		buffer->capacity = next_capacity;
	}
	memcpy(buffer->data + buffer->length, data, length);
	buffer->length += length;
	return 1;
}

static void free_buffer(input_buffer *buffer)
{
	free(buffer->data);
	buffer->data = NULL;
	buffer->length = 0;
	buffer->capacity = 0;
}

static void print_result(const mylite_parse_result *result)
{
	size_t i;

	printf("ok statements=%zu", result->statement_count);
	for (i = 0; i < result->statement_count; i++) {
		printf("%s%s", i == 0 ? " kinds=" : ",",
		       mylite_statement_kind_name(result->statements[i].kind));
	}
	printf("\n");
}

static void print_usage(const char *program)
{
	fprintf(stderr, "usage: %s [--quiet] [--nul] [sql...]\n", program);
}
