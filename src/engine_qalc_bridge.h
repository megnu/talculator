/*
 * engine_qalc_bridge.h - C-callable bridge to C++ libqalculate backend.
 */

#ifndef TALCULATOR_ENGINE_QALC_BRIDGE_H
#define TALCULATOR_ENGINE_QALC_BRIDGE_H

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

gboolean talc_qalc_bridge_available (void);

gboolean talc_qalc_bridge_eval_formatted (const talc_engine_context *ctx,
	const char *expression,
	char **out_result,
	const char **out_error);

gboolean talc_qalc_bridge_eval_formatted_with_contexts (
	const talc_engine_context *parse_ctx,
	const talc_engine_context *print_ctx,
	const char *expression,
	char **out_result,
	const char **out_error);

#ifdef __cplusplus
}
#endif

#endif /* TALCULATOR_ENGINE_QALC_BRIDGE_H */
