/*
 * engine.c - calculator engine abstraction implementation.
 */

#include "engine.h"

#include <string.h>
#include "engine_qalc_bridge.h"

struct talc_engine {
	char *last_error;
};

static void talc_engine_set_error (talc_engine *engine, const char *message)
{
	if (!engine) return;
	if (engine->last_error) g_free (engine->last_error);
	engine->last_error = g_strdup (message ? message : "");
}

talc_engine *talc_engine_new (void)
{
	talc_engine *engine = g_new0 (talc_engine, 1);
	return engine;
}

void talc_engine_free (talc_engine *engine)
{
	if (!engine) return;
	if (engine->last_error) g_free (engine->last_error);
	g_free (engine);
}

gboolean talc_engine_available (void)
{
	return talc_qalc_bridge_available ();
}

char *talc_engine_eval_expression (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression)
{
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

	if (talc_qalc_bridge_eval_formatted (ctx, expression, &formatted, &bridge_error)) {
		talc_engine_set_error (engine, bridge_error ? bridge_error : "");
		return formatted;
	}
	talc_engine_set_error (engine, bridge_error ? bridge_error : "libqalculate backend failed");
	return NULL;
}

char *talc_engine_eval_expression_with_contexts (talc_engine *engine,
	const talc_engine_context *parse_ctx,
	const talc_engine_context *print_ctx,
	const char *expression)
{
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

	if (talc_qalc_bridge_eval_formatted_with_contexts (parse_ctx, print_ctx,
		expression, &formatted, &bridge_error)) {
		talc_engine_set_error (engine, bridge_error ? bridge_error : "");
		return formatted;
	}
	talc_engine_set_error (engine, bridge_error ? bridge_error : "libqalculate backend failed");
	return NULL;
}

const char *talc_engine_last_error (const talc_engine *engine)
{
	if (!engine || !engine->last_error) return "";
	return engine->last_error;
}
