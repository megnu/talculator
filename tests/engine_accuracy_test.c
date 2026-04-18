#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "engine.h"

typedef struct {
	int failures;
	talc_engine *engine;
} test_state;

static talc_engine_context ctx_default (void)
{
	talc_engine_context ctx;
	memset (&ctx, 0, sizeof (ctx));
	ctx.mode = TALC_ENGINE_MODE_SCIENTIFIC;
	ctx.base = TALC_ENGINE_BASE_DEC;
	ctx.angle = TALC_ENGINE_ANGLE_DEG;
	ctx.rpn_notation = FALSE;
	ctx.display_precision = 16;
	ctx.decimal_point = '.';
	return ctx;
}

static void report_failure (test_state *state, const char *name, const char *msg)
{
	state->failures++;
	fprintf (stderr, "FAIL: %s: %s\n", name, msg);
}

static void expect_exact (test_state *state, const char *name,
	const talc_engine_context *ctx, const char *expr, const char *expected)
{
	char *actual = talc_engine_eval_expression (state->engine, ctx, expr);
	if (!actual) {
		char *msg = g_strdup_printf ("expected \"%s\", got error: %s",
			expected, talc_engine_last_error (state->engine));
		report_failure (state, name, msg);
		g_free (msg);
		return;
	}
	if (g_strcmp0 (actual, expected) != 0) {
		char *msg = g_strdup_printf ("expected \"%s\", got \"%s\"", expected, actual);
		report_failure (state, name, msg);
		g_free (msg);
	}
	g_free (actual);
}

static void expect_exact_with_contexts (test_state *state, const char *name,
	const talc_engine_context *parse_ctx, const talc_engine_context *print_ctx,
	const char *expr, const char *expected)
{
	char *actual = talc_engine_eval_expression_with_contexts (state->engine,
		parse_ctx, print_ctx, expr);
	if (!actual) {
		char *msg = g_strdup_printf ("expected \"%s\", got error: %s",
			expected, talc_engine_last_error (state->engine));
		report_failure (state, name, msg);
		g_free (msg);
		return;
	}
	if (g_strcmp0 (actual, expected) != 0) {
		char *msg = g_strdup_printf ("expected \"%s\", got \"%s\"", expected, actual);
		report_failure (state, name, msg);
		g_free (msg);
	}
	g_free (actual);
}

static void expect_contains (test_state *state, const char *name,
	const talc_engine_context *ctx, const char *expr, const char *needle)
{
	char *actual = talc_engine_eval_expression (state->engine, ctx, expr);
	if (!actual) {
		char *msg = g_strdup_printf ("expected output containing \"%s\", got error: %s",
			needle, talc_engine_last_error (state->engine));
		report_failure (state, name, msg);
		g_free (msg);
		return;
	}
	if (!g_strstr_len (actual, -1, needle)) {
		char *msg = g_strdup_printf ("expected output containing \"%s\", got \"%s\"",
			needle, actual);
		report_failure (state, name, msg);
		g_free (msg);
	}
	g_free (actual);
}

static void expect_not_contains (test_state *state, const char *name,
	const talc_engine_context *ctx, const char *expr, const char *needle)
{
	char *actual = talc_engine_eval_expression (state->engine, ctx, expr);
	if (!actual) {
		char *msg = g_strdup_printf ("expected output not containing \"%s\", got error: %s",
			needle, talc_engine_last_error (state->engine));
		report_failure (state, name, msg);
		g_free (msg);
		return;
	}
	if (g_strstr_len (actual, -1, needle)) {
		char *msg = g_strdup_printf ("expected output not containing \"%s\", got \"%s\"",
			needle, actual);
		report_failure (state, name, msg);
		g_free (msg);
	}
	g_free (actual);
}

static void expect_approx (test_state *state, const char *name,
	const talc_engine_context *ctx, const char *expr, double expected, double tol)
{
	char *actual = talc_engine_eval_expression (state->engine, ctx, expr);
	double value, lower, upper;
	char *end = NULL;

	if (!actual) {
		char *msg = g_strdup_printf ("expected %.12g, got error: %s",
			expected, talc_engine_last_error (state->engine));
		report_failure (state, name, msg);
		g_free (msg);
		return;
	}

	if (g_str_has_prefix (actual, "interval(")) {
		if (sscanf (actual, "interval(%lf; %lf)", &lower, &upper) != 2) {
			char *msg = g_strdup_printf ("unparseable interval output \"%s\"", actual);
			report_failure (state, name, msg);
			g_free (msg);
			g_free (actual);
			return;
		}
		if ((expected + tol) < lower || (expected - tol) > upper) {
			char *msg = g_strdup_printf ("expected %.12g +/- %.3g, got interval [%.12g, %.12g]",
				expected, tol, lower, upper);
			report_failure (state, name, msg);
			g_free (msg);
		}
		g_free (actual);
		return;
	}

	value = g_ascii_strtod (actual, &end);
	while (end && g_ascii_isspace ((guchar) *end)) end++;
	if (!end || *end != '\0') {
		char *msg = g_strdup_printf ("non-numeric output \"%s\"", actual);
		report_failure (state, name, msg);
		g_free (msg);
		g_free (actual);
		return;
	}
	if (fabs (value - expected) > tol) {
		char *msg = g_strdup_printf ("expected %.12g +/- %.3g, got %.12g",
			expected, tol, value);
		report_failure (state, name, msg);
		g_free (msg);
	}
	g_free (actual);
}

static void expect_error (test_state *state, const char *name,
	const talc_engine_context *ctx, const char *expr)
{
	char *actual = talc_engine_eval_expression (state->engine, ctx, expr);
	if (actual) {
		char *msg = g_strdup_printf ("expected failure, got \"%s\"", actual);
		report_failure (state, name, msg);
		g_free (msg);
		g_free (actual);
	}
}

int main (void)
{
	test_state state;
	static const talc_engine_custom_constant custom_constants[] = {
		{ "golden ratio", "phi", "(1+sqrt(5))/2" },
		{ "custom c", "c", "7" }
	};
	static const talc_engine_custom_function custom_functions[] = {
		{ "f", "x", "1-x" },
		{ "g", "x", "1/f(x)" }
	};
	talc_engine_context dec;
	talc_engine_context rad;
	talc_engine_context rpn;
	talc_engine_context rpn_rad;
	talc_engine_context rpn_bin;
	talc_engine_context hex;
	talc_engine_context dec_parse;
	talc_engine_context hex_parse;
	talc_engine_context hex_print;
	talc_engine_context bin_print_u8;
	talc_engine_context bin_print_s8;
	talc_engine_context bin_unsigned;
	talc_engine_context bin_signed;

	memset (&state, 0, sizeof (state));
	if (!talc_engine_available ()) {
		fprintf (stderr, "engine unavailable\n");
		return 1;
	}

	state.engine = talc_engine_new ();
	if (!state.engine) {
		fprintf (stderr, "failed to allocate engine\n");
		return 1;
	}

	dec = ctx_default ();
	rad = dec;
	rad.angle = TALC_ENGINE_ANGLE_RAD;
	rpn = dec;
	rpn.rpn_notation = TRUE;
	rpn_rad = rpn;
	rpn_rad.angle = TALC_ENGINE_ANGLE_RAD;
	rpn_bin = rpn;
	rpn_bin.base = TALC_ENGINE_BASE_BIN;
	rpn_bin.base_bits = 8;
	rpn_bin.base_signed = FALSE;
	hex = dec;
	hex.base = TALC_ENGINE_BASE_HEX;
	hex.base_bits = 8;
	hex.base_signed = FALSE;
	dec_parse = dec;
	hex_parse = dec;
	hex_parse.base = TALC_ENGINE_BASE_HEX;
	hex_parse.base_bits = 8;
	hex_parse.base_signed = FALSE;
	hex_print = hex_parse;
	bin_print_u8 = dec;
	bin_print_u8.base = TALC_ENGINE_BASE_BIN;
	bin_print_u8.base_bits = 8;
	bin_print_u8.base_signed = FALSE;
	bin_print_s8 = bin_print_u8;
	bin_print_s8.base_signed = TRUE;
	bin_unsigned = dec;
	bin_unsigned.base = TALC_ENGINE_BASE_BIN;
	bin_unsigned.base_bits = 8;
	bin_unsigned.base_signed = FALSE;
	bin_signed = bin_unsigned;
	bin_signed.base_signed = TRUE;
	dec.custom_constants = custom_constants;
	dec.custom_constants_len = G_N_ELEMENTS (custom_constants);
	dec.custom_functions = custom_functions;
	dec.custom_functions_len = G_N_ELEMENTS (custom_functions);
	rad.custom_constants = custom_constants;
	rad.custom_constants_len = G_N_ELEMENTS (custom_constants);
	rad.custom_functions = custom_functions;
	rad.custom_functions_len = G_N_ELEMENTS (custom_functions);
	rpn.custom_constants = custom_constants;
	rpn.custom_constants_len = G_N_ELEMENTS (custom_constants);
	rpn.custom_functions = custom_functions;
	rpn.custom_functions_len = G_N_ELEMENTS (custom_functions);
	rpn_rad.custom_constants = custom_constants;
	rpn_rad.custom_constants_len = G_N_ELEMENTS (custom_constants);
	rpn_rad.custom_functions = custom_functions;
	rpn_rad.custom_functions_len = G_N_ELEMENTS (custom_functions);
	rpn_bin.custom_constants = custom_constants;
	rpn_bin.custom_constants_len = G_N_ELEMENTS (custom_constants);
	rpn_bin.custom_functions = custom_functions;
	rpn_bin.custom_functions_len = G_N_ELEMENTS (custom_functions);
	hex.custom_constants = custom_constants;
	hex.custom_constants_len = G_N_ELEMENTS (custom_constants);
	hex.custom_functions = custom_functions;
	hex.custom_functions_len = G_N_ELEMENTS (custom_functions);
	dec_parse.custom_constants = custom_constants;
	dec_parse.custom_constants_len = G_N_ELEMENTS (custom_constants);
	dec_parse.custom_functions = custom_functions;
	dec_parse.custom_functions_len = G_N_ELEMENTS (custom_functions);
	hex_parse.custom_constants = custom_constants;
	hex_parse.custom_constants_len = G_N_ELEMENTS (custom_constants);
	hex_parse.custom_functions = custom_functions;
	hex_parse.custom_functions_len = G_N_ELEMENTS (custom_functions);
	hex_print.custom_constants = custom_constants;
	hex_print.custom_constants_len = G_N_ELEMENTS (custom_constants);
	hex_print.custom_functions = custom_functions;
	hex_print.custom_functions_len = G_N_ELEMENTS (custom_functions);
	bin_print_u8.custom_constants = custom_constants;
	bin_print_u8.custom_constants_len = G_N_ELEMENTS (custom_constants);
	bin_print_u8.custom_functions = custom_functions;
	bin_print_u8.custom_functions_len = G_N_ELEMENTS (custom_functions);
	bin_print_s8.custom_constants = custom_constants;
	bin_print_s8.custom_constants_len = G_N_ELEMENTS (custom_constants);
	bin_print_s8.custom_functions = custom_functions;
	bin_print_s8.custom_functions_len = G_N_ELEMENTS (custom_functions);
	bin_unsigned.custom_constants = custom_constants;
	bin_unsigned.custom_constants_len = G_N_ELEMENTS (custom_constants);
	bin_unsigned.custom_functions = custom_functions;
	bin_unsigned.custom_functions_len = G_N_ELEMENTS (custom_functions);
	bin_signed.custom_constants = custom_constants;
	bin_signed.custom_constants_len = G_N_ELEMENTS (custom_constants);
	bin_signed.custom_functions = custom_functions;
	bin_signed.custom_functions_len = G_N_ELEMENTS (custom_functions);

	expect_exact (&state, "add", &dec, "1+2", "3");
	expect_exact (&state, "add_whitespace", &dec, "  1 +   2  ", "3");
	expect_exact (&state, "precedence", &dec, "2+3*4", "14");
	expect_exact (&state, "parentheses", &dec, "(2+3)*4", "20");
	expect_exact (&state, "double_unary_neg", &dec, "-(-5)", "5");
	expect_exact (&state, "explicit_add_paren", &dec, "88+(3+2)", "93");
	expect_exact (&state, "explicit_sub_paren", &dec, "88-(3+2)", "83");
	expect_exact (&state, "explicit_mul_paren", &dec, "88*(3+2)", "440");
	expect_exact (&state, "explicit_div_paren", &dec, "88/(3+2)", "17.6");
	expect_exact (&state, "implicit_mul_num_paren", &dec, "2(3+4)", "14");
	expect_exact (&state, "implicit_mul_paren_paren", &dec, "(1+2)(3+4)", "21");
	expect_exact (&state, "implicit_mul_factorial_paren", &dec, "(3!)(2)", "12");

	expect_exact (&state, "percent_add", &dec, "200+10%", "220");
	expect_exact (&state, "percent_sub", &dec, "200-10%", "180");
	expect_exact (&state, "percent_mul", &dec, "200*10%", "20");
	expect_exact (&state, "percent_div", &dec, "200/10%", "2000");
	expect_exact (&state, "percent_mul_rhs_paren", &dec, "88*(3+2)%", "4.4");
	expect_exact (&state, "percent_div_rhs_paren", &dec, "88/(3+2)%", "1760");
	expect_exact (&state, "percent_standalone", &dec, "10%", "0.1");
	expect_error (&state, "percent_chain_invalid", &dec, "10%%");

	expect_approx (&state, "sin_deg_30", &dec, "sin(30)", 0.5, 1e-12);
	expect_approx (&state, "cos_deg_60", &dec, "cos(60)", 0.5, 1e-12);
	expect_approx (&state, "tan_deg_45", &dec, "tan(45)", 1.0, 1e-12);
	expect_approx (&state, "sin_rad_pi_2", &rad, "sin(pi/2)", 1.0, 1e-12);
	expect_exact (&state, "log_base10_100", &dec, "log(100)", "2");
	expect_exact (&state, "ln_natural_e", &dec, "ln(e)", "1");
	expect_approx (&state, "sqrt2", &dec, "sqrt(2)", 1.4142135623730951, 1e-12);
	expect_not_contains (&state, "sqrt3_not_interval", &dec, "sqrt(3)", "interval(");
	expect_not_contains (&state, "sqrt5_not_interval", &dec, "sqrt(5)", "interval(");

	expect_exact (&state, "hex_A_plus_1", &hex, "A+1", "B");
	expect_error (&state, "hex_G_plus_1_invalid", &hex, "G+1");
	expect_exact (&state, "bin_and", &bin_unsigned, "11110000 & 10101010", "10100000");
	expect_exact (&state, "bin_or", &bin_unsigned, "11110000 | 10101010", "11111010");
	expect_exact (&state, "bin_shift_left", &bin_unsigned, "1 << 7", "10000000");
	expect_exact (&state, "bin_neg1_signed", &bin_signed, "-1", "11111111");

	/* parse/print context split coverage */
	expect_exact_with_contexts (&state, "ctx_parse_hex_print_dec", &hex_parse, &dec_parse, "A+1", "11");
	expect_exact_with_contexts (&state, "ctx_parse_dec_print_hex", &dec_parse, &hex_print, "10+5", "F");
	expect_exact_with_contexts (&state, "ctx_parse_dec_print_hex_prefixed", &dec_parse, &hex_print, "255", "FF");
	expect_exact_with_contexts (&state, "ctx_parse_dec_print_bin_signed", &dec_parse, &bin_print_s8, "-1", "11111111");

	/* base-width and shift/bitwise semantic boundaries */
	expect_exact_with_contexts (&state, "bin_u8_255_plus_1", &dec_parse, &bin_print_u8, "255+1", "100000000");
	expect_exact_with_contexts (&state, "bin_s8_neg128_minus_1", &dec_parse, &bin_print_s8, "-128-1", "1111111101111111");
	expect_exact (&state, "shift_precedence_default", &dec, "1<<2+1", "8");
	expect_exact (&state, "shift_precedence_parenthesized", &dec, "(1<<2)+1", "5");

	/* percent edge chains and nested forms */
	expect_exact (&state, "percent_nested_add", &dec, "(200+10%)", "220");
	expect_exact (&state, "percent_then_multiply", &dec, "50%*8", "4");
	expect_exact (&state, "percent_sub_expr", &dec, "200-(5+5)%", "180");
	expect_exact (&state, "percent_add_decimal_rhs", &dec, "15+2.5%", "15.375");
	expect_error (&state, "percent_chain_triple_invalid", &dec, "10%%%");

	/* domain/error behavior */
	expect_exact (&state, "domain_sqrt_neg_one", &dec, "sqrt(-1)", "i");
	expect_contains (&state, "domain_log_neg_one_imag", &dec, "log(-1)", "i");
	expect_exact (&state, "domain_div_zero", &dec, "1/0", "1/0");
	expect_exact (&state, "domain_zero_div_zero", &dec, "0/0", "0/0");
	expect_error (&state, "domain_invalid_percent_error", &dec, "%%");

	/* function/constant parsing robustness */
	expect_error (&state, "parse_sin30_implicit_invalid", &dec, "sin30");
	expect_error (&state, "parse_pi2_implicit_mul_invalid", &dec, "pi2");
	expect_exact (&state, "parse_parenthesized_unary", &dec, "(-2)^2", "4");
	expect_exact (&state, "custom_constant_phi", &dec, "phi*2", "3.23606797749979");
	expect_exact (&state, "custom_constant_c", &dec, "c+1", "8");
	expect_exact (&state, "custom_function_f", &dec, "f(3)", "-2");
	expect_exact (&state, "custom_function_nested_g", &dec, "g(3)", "-0.5");
	expect_approx (&state, "custom_function_composition", &dec, "f(phi)", -0.618033988749895, 1e-15);
	expect_exact (&state, "custom_function_nested_arg", &dec, "f(1+2)", "-2");
	expect_exact (&state, "factorial_power_precedence", &dec, "2^3!", "64");
	expect_exact (&state, "factorial_power_parenthesized", &dec, "(2^3)!", "40320");
	expect_error (&state, "function_name_without_parentheses", &dec, "A+sin");
	expect_error (&state, "binary_keyword_invalid_position", &dec, "sin(mod)");
	expect_exact (&state, "unary_not_infix_bitwise", &dec, "not 1", "-2");
	expect_error (&state, "unknown_identifier_single_word", &dec, "hello");
	expect_error (&state, "unknown_identifier_expression", &dec, "hello+1");
	expect_error (&state, "unknown_identifier_like_variable", &dec, "abc");

	/* RPN context coverage */
	expect_exact (&state, "rpn_add", &rpn, "3 4 +", "7");
	expect_exact (&state, "rpn_add_whitespace", &rpn, "  3   4   + ", "7");
	expect_exact (&state, "rpn_div", &rpn, "10 2 /", "5");
	expect_exact (&state, "rpn_nested", &rpn, "2 3 4 + *", "14");
	expect_exact (&state, "rpn_complex_chain", &rpn, "5 1 2 + 4 * + 3 -", "14");
	expect_error (&state, "rpn_invalid_percent_chain", &rpn, "10%%");
	expect_exact (&state, "rpn_order_sub", &rpn, "5 2 -", "3");
	expect_exact (&state, "rpn_order_sub_infix_control", &dec, "5-2", "3");
	expect_approx (&state, "rpn_sin_30_deg", &rpn, "30 sin", 0.5, 1e-12);
	expect_approx (&state, "rpn_sin_pi_over_2_rad", &rpn_rad, "pi 2 / sin", 1.0, 1e-12);
	expect_approx (&state, "rpn_sqrt2", &rpn, "2 sqrt", 1.4142135623730951, 1e-12);
	expect_exact (&state, "rpn_bin_and", &rpn_bin, "1111 0011 &", "11");
	expect_exact (&state, "rpn_word_mod", &rpn, "10 3 mod", "1");
	expect_exact (&state, "rpn_word_and", &rpn, "10 3 and", "2");
	expect_exact (&state, "rpn_word_or", &rpn, "10 3 or", "11");
	expect_exact (&state, "rpn_word_xor", &rpn, "10 3 xor", "9");
	expect_exact (&state, "rpn_word_not", &rpn, "1 not", "-2");
	expect_exact (&state, "rpn_residual_stack_returns_top", &rpn, "3 4", "3");
	expect_exact (&state, "rpn_residual_stack_with_operation", &rpn, "3 4 5 +", "12");

	talc_engine_free (state.engine);
	if (state.failures > 0) {
		fprintf (stderr, "%d test(s) failed\n", state.failures);
		return 1;
	}
	printf ("all engine accuracy tests passed\n");
	return 0;
}
