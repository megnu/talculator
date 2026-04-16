/*
 * engine.h - calculator engine abstraction for talculator.
 *
 * Initial goal: define a stable seam between GTK/UI code and evaluation code
 * so we can swap the legacy G_REAL core for a libqalculate backend
 * incrementally.
 */

#ifndef TALCULATOR_ENGINE_H
#define TALCULATOR_ENGINE_H

#include <glib.h>
#include "g_real.h"

typedef enum {
	TALC_ENGINE_BACKEND_LEGACY = 0,
	TALC_ENGINE_BACKEND_LIBQALCULATE = 1
} talc_engine_backend;

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
	talc_engine_mode mode;
	talc_engine_base base;
	talc_engine_angle angle;
	gboolean rpn_notation;
	gboolean formula_notation;
	int display_precision;
	char decimal_point;
	int base_bits;
	gboolean base_signed;
} talc_engine_context;

typedef struct talc_engine talc_engine;
typedef struct {
	gboolean error;
	G_REAL value;
} talc_engine_eval_result;

talc_engine *talc_engine_new (talc_engine_backend backend);
void talc_engine_free (talc_engine *engine);

talc_engine_backend talc_engine_backend_get (const talc_engine *engine);
gboolean talc_engine_backend_available (talc_engine_backend backend);

/*
 * Evaluate a full expression and return a newly allocated canonical result
 * string (caller owns it) or NULL on failure.
 */
char *talc_engine_eval_expression (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression);

/*
 * Returns a pointer to an internal, human-readable error string.
 * The pointer remains valid until the next engine call.
 */
const char *talc_engine_last_error (const talc_engine *engine);

/*
 * Evaluate expression and return numerical result.
 * Returns TRUE when evaluation was performed (with error flag in out_result),
 * FALSE when backend is unavailable or invocation is invalid.
 */
gboolean talc_engine_eval_expression_numeric (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression,
	talc_engine_eval_result *out_result);

#endif /* TALCULATOR_ENGINE_H */
