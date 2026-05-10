#include <stdio.h>
#include <string.h>

#include "calc_basic.h"
#include "config_file.h"
#include "engine.h"
#include "talculator.h"

s_preferences prefs;
s_constant *constant = NULL;
s_user_function *user_function = NULL;
talc_engine *calc_engine = NULL;
s_tab_context *active_tab = NULL;

static s_tab_context test_tab;

typedef struct {
	int failures;
} test_state;

static void report_failure (test_state *state, const char *name, const char *msg)
{
	state->failures++;
	fprintf (stderr, "FAIL: %s: %s\n", name, msg);
}

static void expect_string (test_state *state, const char *name,
	const char *actual, const char *expected)
{
	if (g_strcmp0 (actual, expected) != 0) {
		char *msg = g_strdup_printf ("expected \"%s\", got \"%s\"",
			expected, actual ? actual : "(null)");
		report_failure (state, name, msg);
		g_free (msg);
	}
}

static void expect_int (test_state *state, const char *name,
	int actual, int expected)
{
	if (actual != expected) {
		char *msg = g_strdup_printf ("expected %d, got %d", expected, actual);
		report_failure (state, name, msg);
		g_free (msg);
	}
}

static void expect_stack3 (test_state *state, const char *name,
	const char *expected_x, const char *expected_y, const char *expected_z)
{
	char **stack = rpn_stack_get (RPN_FINITE_STACK);

	if (!stack) {
		report_failure (state, name, "stack unavailable");
		return;
	}
	expect_string (state, name, stack[0], expected_x);
	expect_string (state, name, stack[1], expected_y);
	expect_string (state, name, stack[2], expected_z);
	g_free (stack[0]);
	g_free (stack[1]);
	g_free (stack[2]);
	g_free (stack);
}

static void reset_rpn (int stack_size)
{
	rpn_init (stack_size, 0);
	memset (&test_tab, 0, sizeof (test_tab));
	active_tab = &test_tab;
	test_tab.tab_mode = SCIENTIFIC_MODE;
	current_status.number = CS_DEC;
	current_status.angle = CS_RAD;
	current_status.notation = CS_RPN;
	prefs.stack_size = stack_size;
	prefs.hex_bits = 16;
	prefs.hex_signed = TRUE;
	prefs.oct_bits = 16;
	prefs.bin_bits = 16;
	prefs.bin_signed = TRUE;
}

static void test_binary_order (test_state *state)
{
	char *result;

	reset_rpn (RPN_FINITE_STACK);
	rpn_stack_push ("5");
	result = rpn_stack_operation ('-', "2");
	expect_string (state, "rpn_subtract_order", result, "3");
	expect_int (state, "rpn_subtract_consumes_y", rpn_stack_length (), 0);
	g_free (result);

	reset_rpn (RPN_FINITE_STACK);
	rpn_stack_push ("10");
	result = rpn_stack_operation ('/', "2");
	expect_string (state, "rpn_divide_order", result, "5");
	g_free (result);
}

static void test_finite_t_duplication (test_state *state)
{
	char *values[] = { "3", "2", "1" };
	char *result;

	reset_rpn (RPN_FINITE_STACK);
	rpn_stack_set_array (values, 3);
	result = rpn_stack_operation ('+', "4");
	expect_string (state, "rpn_finite_t_dup_result", result, "7");
	expect_stack3 (state, "rpn_finite_t_dup_stack", "2", "1", "1");
	g_free (result);
}

static void test_percent_preserves_y (test_state *state)
{
	char *percent;
	char *sum;

	reset_rpn (RPN_FINITE_STACK);
	rpn_stack_push ("200");
	percent = rpn_stack_operation ('%', "5");
	expect_string (state, "rpn_percent_result", percent, "10");
	expect_stack3 (state, "rpn_percent_preserves_y", "200", "0", "0");

	sum = rpn_stack_operation ('+', percent);
	expect_string (state, "rpn_percent_then_add", sum, "210");
	expect_int (state, "rpn_percent_add_consumes_y", rpn_stack_length (), 0);
	g_free (percent);
	g_free (sum);
}

static void test_swap_and_roll (test_state *state)
{
	char *values[] = { "2", "3", "4" };
	char *result;

	reset_rpn (RPN_FINITE_STACK);
	rpn_stack_set_array (values, 3);
	result = rpn_stack_swapxy ("1");
	expect_string (state, "rpn_swap_result", result, "2");
	expect_stack3 (state, "rpn_swap_stack", "1", "3", "4");
	g_free (result);

	result = rpn_stack_rolldown ("2");
	expect_string (state, "rpn_roll_result", result, "1");
	expect_stack3 (state, "rpn_roll_stack", "3", "4", "2");
	g_free (result);
}

static void test_stack_size_truncates (test_state *state)
{
	char *values[] = { "1", "2", "3", "4" };

	reset_rpn (RPN_INFINITE_STACK);
	rpn_stack_set_array (values, 4);
	expect_int (state, "rpn_infinite_stack_len", rpn_stack_length (), 4);
	rpn_stack_set_size (RPN_FINITE_STACK);
	expect_int (state, "rpn_finite_stack_len", rpn_stack_length (), 3);
	expect_stack3 (state, "rpn_finite_stack_values", "1", "2", "3");
}

int main (void)
{
	test_state state;

	memset (&state, 0, sizeof (state));
	calc_engine = talc_engine_new ();
	if (!calc_engine) {
		fprintf (stderr, "failed to allocate engine\n");
		return 1;
	}

	test_binary_order (&state);
	test_finite_t_duplication (&state);
	test_percent_preserves_y (&state);
	test_swap_and_roll (&state);
	test_stack_size_truncates (&state);

	rpn_free ();
	talc_engine_free (calc_engine);
	if (state.failures > 0) {
		fprintf (stderr, "%d test(s) failed\n", state.failures);
		return 1;
	}
	printf ("all RPN stack tests passed\n");
	return 0;
}
