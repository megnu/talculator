/*
 * engine_qalc_bridge.cpp - C++ bridge for libqalculate backend.
 */

#include "engine_qalc_bridge.h"

#include <string>
#include <libqalculate/qalculate.h>

static bool s_init_attempted = false;
static bool s_init_ok = false;
static std::string s_last_error;

static AngleUnit talc_qalc_angle_unit (const talc_engine_context *ctx)
{
	if (!ctx) return ANGLE_UNIT_NONE;
	switch (ctx->angle) {
	case TALC_ENGINE_ANGLE_RAD:
		return ANGLE_UNIT_RADIANS;
	case TALC_ENGINE_ANGLE_DEG:
		return ANGLE_UNIT_DEGREES;
	case TALC_ENGINE_ANGLE_GRAD:
		return ANGLE_UNIT_GRADIANS;
	default:
		return ANGLE_UNIT_NONE;
	}
}

static int talc_qalc_number_base (const talc_engine_context *ctx)
{
	if (!ctx) return BASE_DECIMAL;
	switch (ctx->base) {
	case TALC_ENGINE_BASE_HEX:
		return BASE_HEXADECIMAL;
	case TALC_ENGINE_BASE_OCT:
		return BASE_OCTAL;
	case TALC_ENGINE_BASE_BIN:
		return BASE_BINARY;
	case TALC_ENGINE_BASE_DEC:
	default:
		return BASE_DECIMAL;
	}
}

static unsigned int talc_qalc_binary_bits (const talc_engine_context *ctx)
{
	if (!ctx || ctx->base_bits <= 0) return 0U;
	return (unsigned int) ctx->base_bits;
}

static void talc_qalc_apply_base_semantics (const talc_engine_context *ctx,
	ParseOptions *parse_opts,
	PrintOptions *print_opts)
{
	const unsigned int bits = talc_qalc_binary_bits (ctx);

	if (parse_opts) {
		parse_opts->twos_complement = false;
		parse_opts->hexadecimal_twos_complement = false;
		parse_opts->binary_bits = bits;
		if (ctx) {
			if (ctx->base == TALC_ENGINE_BASE_BIN) {
				parse_opts->twos_complement = (ctx->base_signed != FALSE);
			} else if (ctx->base == TALC_ENGINE_BASE_HEX) {
				parse_opts->hexadecimal_twos_complement = (ctx->base_signed != FALSE);
			}
		}
	}
	if (print_opts) {
		print_opts->twos_complement = false;
		print_opts->hexadecimal_twos_complement = false;
		print_opts->binary_bits = bits;
		if (ctx) {
			if (ctx->base == TALC_ENGINE_BASE_BIN) {
				print_opts->twos_complement = (ctx->base_signed != FALSE);
			} else if (ctx->base == TALC_ENGINE_BASE_HEX) {
				print_opts->hexadecimal_twos_complement = (ctx->base_signed != FALSE);
			}
		}
	}
}

static void talc_qalc_configure_locale (const talc_engine_context *ctx)
{
	if (!CALCULATOR) return;
	if (ctx && ctx->decimal_point == ',') CALCULATOR->useDecimalComma ();
	else CALCULATOR->useDecimalPoint (true);
}

static void talc_qalc_fill_print_options (const talc_engine_context *ctx,
	PrintOptions *po)
{
	if (!po) return;
	*po = default_print_options;
	po->base = talc_qalc_number_base (ctx);
	po->base_display = BASE_DISPLAY_NONE;
	po->number_fraction_format = FRACTION_DECIMAL;
	po->show_ending_zeroes = false;
	po->use_unicode_signs = UNICODE_SIGNS_OFF;
	po->exp_display = EXP_LOWERCASE_E;
	po->digit_grouping = DIGIT_GROUPING_NONE;
	po->spacious = false;
	po->indicate_infinite_series = REPEATING_DECIMALS_OFF;
	po->interval_display = INTERVAL_DISPLAY_MIDPOINT;
	talc_qalc_apply_base_semantics (ctx, NULL, po);
	if (ctx && ctx->display_precision > 0 && CALCULATOR) {
		CALCULATOR->setPrecision (ctx->display_precision);
	}
	if (ctx && ctx->decimal_point != '\0') {
		po->decimalpoint_sign = std::string (1, ctx->decimal_point);
	}
}

static bool talc_qalc_init_once (void)
{
	if (s_init_attempted) return s_init_ok;
	s_init_attempted = true;
	s_last_error.clear ();

	try {
		new Calculator ();
		if (!CALCULATOR) {
			s_last_error = "Failed to allocate libqalculate calculator";
			return false;
		}

		if (!CALCULATOR->loadGlobalDefinitions ()) {
			s_last_error = "libqalculate global definitions failed to load";
			return false;
		}
		CALCULATOR->loadLocalDefinitions ();
		s_init_ok = true;
		return true;
	} catch (...) {
		s_last_error = "libqalculate initialization threw an exception";
		return false;
	}
}

static const char *talc_qalc_get_message_or_default (const char *fallback)
{
	CalculatorMessage *msg = NULL;

	if (!CALCULATOR) return fallback;
	msg = CALCULATOR->message ();
	if (!msg) return fallback;
	s_last_error = msg->message ();
	CALCULATOR->clearMessages ();
	return s_last_error.c_str ();
}

gboolean talc_qalc_bridge_available (void)
{
	return talc_qalc_init_once () ? TRUE : FALSE;
}

gboolean talc_qalc_bridge_eval_formatted (const talc_engine_context *ctx,
	const char *expression,
	char **out_result,
	const char **out_error)
{
	MathStructure value;
	EvaluationOptions eval_opts;
	PrintOptions print_opts;
	std::string result;

	if (out_error) *out_error = NULL;
	if (!out_result) return FALSE;
	*out_result = NULL;

	if (!expression || expression[0] == '\0') {
		if (out_error) *out_error = "Empty expression";
		return TRUE;
	}
	if (!talc_qalc_init_once ()) {
		if (out_error) *out_error = s_last_error.c_str ();
		return FALSE;
	}

	talc_qalc_configure_locale (ctx);
	talc_qalc_fill_print_options (ctx, &print_opts);
	eval_opts = default_user_evaluation_options;
	eval_opts.parse_options.base = talc_qalc_number_base (ctx);
	eval_opts.parse_options.angle_unit = talc_qalc_angle_unit (ctx);
	talc_qalc_apply_base_semantics (ctx, &eval_opts.parse_options, NULL);
	if (ctx && ctx->rpn_notation) {
		eval_opts.parse_options.parsing_mode = PARSING_MODE_RPN;
	}

	try {
		CALCULATOR->clearMessages ();
		value = CALCULATOR->calculate (std::string (expression), eval_opts);
	} catch (...) {
		if (out_error) *out_error = "libqalculate evaluation threw an exception";
		return FALSE;
	}

	if (value.isUndefined ()) {
		if (out_error) *out_error = talc_qalc_get_message_or_default ("Undefined result");
		return TRUE;
	}

	try {
		value.format (print_opts);
		result = value.print (print_opts);
	} catch (...) {
		if (out_error) *out_error = "libqalculate formatting threw an exception";
		return FALSE;
	}

	*out_result = g_strdup (result.c_str ());
	if (!*out_result) {
		if (out_error) *out_error = "Out of memory";
		return FALSE;
	}
	if (out_error) *out_error = "";
	return TRUE;
}

gboolean talc_qalc_bridge_eval_formatted_with_contexts (
	const talc_engine_context *parse_ctx,
	const talc_engine_context *print_ctx,
	const char *expression,
	char **out_result,
	const char **out_error)
{
	MathStructure value;
	EvaluationOptions eval_opts;
	PrintOptions print_opts;
	std::string result;

	if (out_error) *out_error = NULL;
	if (!out_result) return FALSE;
	*out_result = NULL;

	if (!expression || expression[0] == '\0') {
		if (out_error) *out_error = "Empty expression";
		return TRUE;
	}
	if (!talc_qalc_init_once ()) {
		if (out_error) *out_error = s_last_error.c_str ();
		return FALSE;
	}

	talc_qalc_configure_locale (print_ctx ? print_ctx : parse_ctx);
	talc_qalc_fill_print_options (print_ctx ? print_ctx : parse_ctx, &print_opts);
	eval_opts = default_user_evaluation_options;
	eval_opts.parse_options.base = talc_qalc_number_base (parse_ctx);
	eval_opts.parse_options.angle_unit = talc_qalc_angle_unit (parse_ctx);
	talc_qalc_apply_base_semantics (parse_ctx, &eval_opts.parse_options, NULL);
	if (parse_ctx && parse_ctx->rpn_notation) {
		eval_opts.parse_options.parsing_mode = PARSING_MODE_RPN;
	}

	try {
		CALCULATOR->clearMessages ();
		value = CALCULATOR->calculate (std::string (expression), eval_opts);
	} catch (...) {
		if (out_error) *out_error = "libqalculate evaluation threw an exception";
		return FALSE;
	}

	if (value.isUndefined ()) {
		if (out_error) *out_error = talc_qalc_get_message_or_default ("Undefined result");
		return TRUE;
	}

	try {
		value.format (print_opts);
		result = value.print (print_opts);
	} catch (...) {
		if (out_error) *out_error = "libqalculate formatting threw an exception";
		return FALSE;
	}

	*out_result = g_strdup (result.c_str ());
	if (!*out_result) {
		if (out_error) *out_error = "Out of memory";
		return FALSE;
	}
	if (out_error) *out_error = "";
	return TRUE;
}
