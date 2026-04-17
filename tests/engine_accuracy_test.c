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
	ctx.formula_notation = TRUE;
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
	talc_engine_context dec;
	talc_engine_context rad;
	talc_engine_context rpn;
	talc_engine_context rpn_rad;
	talc_engine_context rpn_bin;
	talc_engine_context hex;
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
	rpn.formula_notation = FALSE;
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
	bin_unsigned = dec;
	bin_unsigned.base = TALC_ENGINE_BASE_BIN;
	bin_unsigned.base_bits = 8;
	bin_unsigned.base_signed = FALSE;
	bin_signed = bin_unsigned;
	bin_signed.base_signed = TRUE;

	expect_exact (&state, "add", &dec, "1+2", "3");
	expect_exact (&state, "precedence", &dec, "2+3*4", "14");
	expect_exact (&state, "parentheses", &dec, "(2+3)*4", "20");
	expect_exact (&state, "implicit_mul_num_paren", &dec, "2(3+4)", "14");
	expect_exact (&state, "implicit_mul_paren_paren", &dec, "(1+2)(3+4)", "21");

	expect_exact (&state, "percent_add", &dec, "200+10%", "220");
	expect_exact (&state, "percent_sub", &dec, "200-10%", "180");
	expect_exact (&state, "percent_mul", &dec, "200*10%", "20");
	expect_exact (&state, "percent_div", &dec, "200/10%", "2000");
	expect_exact (&state, "percent_standalone", &dec, "10%", "0.1");
	expect_error (&state, "percent_chain_invalid", &dec, "10%%");

	expect_approx (&state, "sin_deg_30", &dec, "sin(30)", 0.5, 1e-12);
	expect_approx (&state, "sin_rad_pi_2", &rad, "sin(pi/2)", 1.0, 1e-12);
	expect_approx (&state, "sqrt2", &dec, "sqrt(2)", 1.4142135623730951, 1e-12);

	expect_exact (&state, "hex_A_plus_1", &hex, "A+1", "B");
	expect_exact (&state, "bin_and", &bin_unsigned, "11110000 & 10101010", "10100000");
	expect_exact (&state, "bin_neg1_signed", &bin_signed, "-1", "11111111");

	/* RPN context coverage */
	expect_exact (&state, "rpn_add", &rpn, "3 4 +", "7");
	expect_exact (&state, "rpn_div", &rpn, "10 2 /", "5");
	expect_exact (&state, "rpn_nested", &rpn, "2 3 4 + *", "14");
	expect_error (&state, "rpn_invalid_percent_chain", &rpn, "10%%");
	expect_exact (&state, "rpn_order_sub", &rpn, "5 2 -", "3");
	expect_exact (&state, "rpn_order_sub_infix_control", &dec, "5-2", "3");
	expect_approx (&state, "rpn_sin_30_deg", &rpn, "30 sin", 0.5, 1e-12);
	expect_approx (&state, "rpn_sin_pi_over_2_rad", &rpn_rad, "pi 2 / sin", 1.0, 1e-12);
	expect_approx (&state, "rpn_sqrt2", &rpn, "2 sqrt", 1.4142135623730951, 1e-12);
	expect_exact (&state, "rpn_bin_and", &rpn_bin, "1111 0011 &", "11");

	talc_engine_free (state.engine);
	if (state.failures > 0) {
		fprintf (stderr, "%d test(s) failed\n", state.failures);
		return 1;
	}
	printf ("all engine accuracy tests passed\n");
	return 0;
}
