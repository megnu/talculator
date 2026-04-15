/*
 * engine.c - calculator engine abstraction implementation.
 *
 * This is an initial scaffold. It does not change runtime behavior yet.
 * Existing UI code still calls the legacy core directly.
 */

#include "engine.h"

#include <string.h>
#include "flex_parser.h"
#include "engine_qalc_bridge.h"

struct talc_engine {
	talc_engine_backend backend;
	char *last_error;
};

static void talc_engine_set_error (talc_engine *engine, const char *message)
{
	if (!engine) return;
	if (engine->last_error) g_free (engine->last_error);
	engine->last_error = g_strdup (message ? message : "");
}

talc_engine *talc_engine_new (talc_engine_backend backend)
{
	talc_engine *engine = g_new0 (talc_engine, 1);
	engine->backend = backend;
	return engine;
}

void talc_engine_free (talc_engine *engine)
{
	if (!engine) return;
	if (engine->last_error) g_free (engine->last_error);
	g_free (engine);
}

talc_engine_backend talc_engine_backend_get (const talc_engine *engine)
{
	if (!engine) return TALC_ENGINE_BACKEND_LEGACY;
	return engine->backend;
}

gboolean talc_engine_backend_available (talc_engine_backend backend)
{
	if (backend == TALC_ENGINE_BACKEND_LEGACY) return TRUE;
	if (backend == TALC_ENGINE_BACKEND_LIBQALCULATE)
		return talc_qalc_bridge_available ();
	return FALSE;
}

char *talc_engine_eval_expression (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression)
{
	s_flex_parser_result parsed;
	char *formatted = NULL;
	const char *bridge_error = NULL;

	if (!engine) return NULL;
	if (!expression) {
		talc_engine_set_error (engine, "NULL expression");
		return NULL;
	}
	if (expression[0] == '\0') {
		talc_engine_set_error (engine, "Empty expression");
		return NULL;
	}

	switch (engine->backend) {
	case TALC_ENGINE_BACKEND_LEGACY:
		parsed = flex_parser (expression);
		if (parsed.error) {
			talc_engine_set_error (engine, "Parse/evaluation error");
			return NULL;
		}
		formatted = g_strdup_printf ("%.12g", (double) parsed.value);
		talc_engine_set_error (engine, "");
		return formatted;
	case TALC_ENGINE_BACKEND_LIBQALCULATE:
		if (talc_qalc_bridge_eval_formatted (ctx, expression, &formatted, &bridge_error)) {
			talc_engine_set_error (engine, bridge_error ? bridge_error : "");
			return formatted;
		}
		talc_engine_set_error (engine, bridge_error ? bridge_error : "libqalculate backend failed");
		return NULL;
	default:
		talc_engine_set_error (engine, "Unknown backend");
		return NULL;
	}
}

const char *talc_engine_last_error (const talc_engine *engine)
{
	if (!engine || !engine->last_error) return "";
	return engine->last_error;
}

gboolean talc_engine_eval_expression_numeric (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression,
	talc_engine_eval_result *out_result)
{
	s_flex_parser_result parsed;
	const char *bridge_error = NULL;

	(void) ctx;
	if (!engine || !out_result) return FALSE;
	if (!expression) {
		talc_engine_set_error (engine, "NULL expression");
		return FALSE;
	}

	switch (engine->backend) {
	case TALC_ENGINE_BACKEND_LEGACY:
		parsed = flex_parser (expression);
		out_result->error = parsed.error;
		out_result->value = parsed.value;
		talc_engine_set_error (engine, parsed.error ? "Parse/evaluation error" : "");
		return TRUE;
	case TALC_ENGINE_BACKEND_LIBQALCULATE:
		if (talc_qalc_bridge_eval_numeric (ctx, expression, out_result, &bridge_error)) {
			talc_engine_set_error (engine, (out_result->error && bridge_error) ? bridge_error : "");
			return TRUE;
		}
		talc_engine_set_error (engine, bridge_error ? bridge_error : "libqalculate backend failed");
		return FALSE;
	default:
		talc_engine_set_error (engine, "Unknown backend");
		return FALSE;
	}
}
