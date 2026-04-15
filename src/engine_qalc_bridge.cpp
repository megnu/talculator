/*
 * engine_qalc_bridge.cpp - C++ bridge for libqalculate backend.
 *
 * This pass only introduces the translation unit and ABI boundary.
 * Actual libqalculate initialization/evaluation wiring is added later.
 */

#include "engine_qalc_bridge.h"

gboolean talc_qalc_bridge_available (void)
{
	return FALSE;
}

gboolean talc_qalc_bridge_eval_numeric (const char *expression,
	talc_engine_eval_result *out_result,
	const char **out_error)
{
	if (out_error) *out_error = "libqalculate backend not wired yet";
	if (!out_result) return FALSE;
	if (!expression || expression[0] == '\0') {
		out_result->error = TRUE;
		out_result->value = 0;
		return TRUE;
	}
	out_result->error = TRUE;
	out_result->value = 0;
	return FALSE;
}
