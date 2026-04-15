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

gboolean talc_qalc_bridge_eval_numeric (const char *expression,
	talc_engine_eval_result *out_result,
	const char **out_error);

#ifdef __cplusplus
}
#endif

#endif /* TALCULATOR_ENGINE_QALC_BRIDGE_H */
