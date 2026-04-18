/* engine.h - calculator engine abstraction for talculator. */

#ifndef TALCULATOR_ENGINE_H
#define TALCULATOR_ENGINE_H

#include <glib.h>

typedef enum {
	TALC_ENGINE_MODE_BASIC = 0,
	TALC_ENGINE_MODE_SCIENTIFIC = 1,
	TALC_ENGINE_MODE_PAPER = 2
} talc_engine_mode;

typedef enum {
	TALC_ENGINE_BASE_DEC = 0,
	TALC_ENGINE_BASE_HEX = 1,
	TALC_ENGINE_BASE_OCT = 2,
	TALC_ENGINE_BASE_BIN = 3
} talc_engine_base;

typedef enum {
	TALC_ENGINE_ANGLE_DEG = 0,
	TALC_ENGINE_ANGLE_RAD = 1,
	TALC_ENGINE_ANGLE_GRAD = 2
} talc_engine_angle;

typedef struct {
	const char *description;
	const char *name;
	const char *value;
} talc_engine_custom_constant;

typedef struct {
	const char *name;
	const char *variable;
	const char *expression;
} talc_engine_custom_function;

typedef struct {
	talc_engine_mode mode;
	talc_engine_base base;
	talc_engine_angle angle;
	gboolean rpn_notation;
	int display_precision;
	char decimal_point;
	int base_bits;
	gboolean base_signed;
	const talc_engine_custom_constant *custom_constants;
	gsize custom_constants_len;
	const talc_engine_custom_function *custom_functions;
	gsize custom_functions_len;
} talc_engine_context;

typedef struct talc_engine talc_engine;

talc_engine *talc_engine_new (void);
void talc_engine_free (talc_engine *engine);
gboolean talc_engine_available (void);

/*
 * Evaluate a full expression and return a newly allocated canonical result
 * string (caller owns it) or NULL on failure.
 */
char *talc_engine_eval_expression (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression);

/*
 * Evaluate expression with parse semantics from parse_ctx and formatting from
 * print_ctx. Returns a newly allocated result string on success, NULL on
 * backend failure. Evaluation errors are reported via talc_engine_last_error.
 */
char *talc_engine_eval_expression_with_contexts (talc_engine *engine,
	const talc_engine_context *parse_ctx,
	const talc_engine_context *print_ctx,
	const char *expression);

/*
 * Returns a pointer to an internal, human-readable error string.
 * The pointer remains valid until the next engine call.
 */
const char *talc_engine_last_error (const talc_engine *engine);

#endif /* TALCULATOR_ENGINE_H */
