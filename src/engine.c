/*
 * engine.c - calculator engine abstraction implementation.
 *
 * This is an initial scaffold. It does not change runtime behavior yet.
 * Existing UI code still calls the legacy core directly.
 */

#include "engine.h"

#include <string.h>

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
	/* libqalculate backend wiring will be added in a later step. */
	if (backend == TALC_ENGINE_BACKEND_LIBQALCULATE) return FALSE;
	return FALSE;
}

char *talc_engine_eval_expression (talc_engine *engine,
	const talc_engine_context *ctx,
	const char *expression)
{
	(void) ctx;

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
		talc_engine_set_error (engine, "Legacy backend bridge not wired yet");
		return NULL;
	case TALC_ENGINE_BACKEND_LIBQALCULATE:
		talc_engine_set_error (engine, "libqalculate backend not wired yet");
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
