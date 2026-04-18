/*
 *  calc_basic.c - RPN stack runtime for talculator.
 */

#include <glib.h>

#include "calc_basic.h"
#include "talculator.h"
#include "config_file.h"
#include "engine.h"

static GPtrArray *rpn_stack;
static int rpn_stack_size;
static int rpn_debug;

static void rpn_ensure_init (void)
{
	if (!rpn_stack) {
		rpn_stack = g_ptr_array_new_with_free_func (g_free);
	}
}

static gsize custom_constant_count (void)
{
	gsize n = 0;
	while (constant && constant[n].name) n++;
	return n;
}

static gsize custom_function_count (void)
{
	gsize n = 0;
	while (user_function && user_function[n].name) n++;
	return n;
}

static void engine_context_from_ui_state (talc_engine_context *ctx)
{
	if (!ctx) return;
	ctx->mode = (talc_engine_mode) active_tab->tab_mode;
	ctx->base = (talc_engine_base) current_status.number;
	ctx->angle = (talc_engine_angle) current_status.angle;
	ctx->rpn_notation = (current_status.notation == CS_RPN);
	ctx->display_precision = 24;
	ctx->decimal_point = DEFAULT_DEC_POINT;
	ctx->base_bits = 0;
	ctx->base_signed = FALSE;
	ctx->custom_constants = (const talc_engine_custom_constant *) constant;
	ctx->custom_constants_len = custom_constant_count ();
	ctx->custom_functions = (const talc_engine_custom_function *) user_function;
	ctx->custom_functions_len = custom_function_count ();
	switch (current_status.number) {
	case CS_HEX:
		ctx->base_bits = prefs.hex_bits;
		ctx->base_signed = prefs.hex_signed;
		break;
	case CS_OCT:
		ctx->base_bits = prefs.oct_bits;
		ctx->base_signed = prefs.oct_signed;
		break;
	case CS_BIN:
		ctx->base_bits = prefs.bin_bits;
		ctx->base_signed = prefs.bin_signed;
		break;
	case CS_DEC:
	default:
		break;
	}
}

static char *rpn_eval_binary_expression (const char *left_hand,
	char operation,
	const char *right_hand)
{
	talc_engine_context engine_ctx;
	char *expr = NULL;
	char *result = NULL;
	const char *op_text;

	if (!calc_engine) return NULL;
	if (!left_hand || !right_hand) return NULL;

	switch (operation) {
	case '<':
		op_text = "<<";
		break;
	case '>':
		op_text = ">>";
		break;
	case 'x':
		op_text = " xor ";
		break;
	case 'm':
		op_text = " mod ";
		break;
	default:
		op_text = NULL;
		break;
	}

	if (operation == '%') {
		expr = g_strdup_printf ("((%s)*(%s))/100", left_hand, right_hand);
	} else if (op_text) {
		expr = g_strdup_printf ("(%s)%s(%s)", left_hand, op_text, right_hand);
	} else {
		expr = g_strdup_printf ("(%s)%c(%s)", left_hand, operation, right_hand);
	}
	if (!expr) return NULL;

	engine_context_from_ui_state (&engine_ctx);
	result = talc_engine_eval_expression (calc_engine, &engine_ctx, expr);
	g_free (expr);
	return result;
}

void rpn_init (int size, int debug_level)
{
	rpn_free ();
	rpn_stack = g_ptr_array_new_with_free_func (g_free);
	rpn_stack_size = size;
	rpn_debug = debug_level;
	(void) rpn_debug;
}

void rpn_stack_set_array (char **values, int length)
{
	int i;

	rpn_ensure_init ();
	g_ptr_array_set_size (rpn_stack, 0);
	if (!values || length <= 0) return;
	for (i = 0; i < length; i++) {
		g_ptr_array_add (rpn_stack, g_strdup (values[i] ? values[i] : CLEARED_DISPLAY));
	}
	if ((rpn_stack_size > 0) && ((int) rpn_stack->len > rpn_stack_size)) {
		g_ptr_array_set_size (rpn_stack, rpn_stack_size);
	}
}

int rpn_stack_length (void)
{
	if (!rpn_stack) return 0;
	return (int) rpn_stack->len;
}

void rpn_stack_push (const char *number)
{
	int i;
	char *copy;

	rpn_ensure_init ();
	copy = g_strdup ((number && number[0] != '\0') ? number : CLEARED_DISPLAY);
	g_ptr_array_add (rpn_stack, NULL);
	for (i = (int) rpn_stack->len - 1; i > 0; i--) {
		rpn_stack->pdata[i] = rpn_stack->pdata[i - 1];
	}
	rpn_stack->pdata[0] = copy;

	if (((int) rpn_stack->len > rpn_stack_size) && (rpn_stack_size > 0)) {
		g_free (rpn_stack->pdata[rpn_stack_size]);
		g_ptr_array_set_size (rpn_stack, rpn_stack_size);
	}
}

char *rpn_stack_operation (char operation, const char *number)
{
	char *left_hand;
	char *last_on_stack = NULL;
	char *result;

	rpn_ensure_init ();

	if (rpn_stack->len < 1) {
		if (operation == '%') left_hand = g_strdup ("1");
		else left_hand = g_strdup (CLEARED_DISPLAY);
	} else {
		left_hand = g_strdup ((char *) rpn_stack->pdata[0]);
		last_on_stack = g_strdup ((char *) rpn_stack->pdata[rpn_stack->len - 1]);
		g_free (rpn_stack->pdata[0]);
		g_ptr_array_remove_index (rpn_stack, 0);
		if (((int) rpn_stack->len == rpn_stack_size - 1) && (rpn_stack_size > 0)) {
			g_ptr_array_add (rpn_stack, last_on_stack);
			last_on_stack = NULL;
		}
	}

	result = rpn_eval_binary_expression (left_hand, operation,
		(number && number[0] != '\0') ? number : CLEARED_DISPLAY);
	g_free (left_hand);
	if (last_on_stack) g_free (last_on_stack);
	if (!result) return g_strdup (CLEARED_DISPLAY);
	return result;
}

char *rpn_stack_swapxy (const char *x)
{
	char *ret_val;

	rpn_ensure_init ();
	if ((int) rpn_stack->len < 1) {
		g_ptr_array_add (rpn_stack, g_strdup ((x && x[0] != '\0') ? x : CLEARED_DISPLAY));
		return g_strdup (CLEARED_DISPLAY);
	}

	ret_val = g_strdup ((char *) rpn_stack->pdata[0]);
	g_free (rpn_stack->pdata[0]);
	rpn_stack->pdata[0] = g_strdup ((x && x[0] != '\0') ? x : CLEARED_DISPLAY);
	return ret_val;
}

char *rpn_stack_rolldown (const char *x)
{
	char *ret_val;
	int i;

	rpn_ensure_init ();
	if (rpn_stack_size <= 0) return g_strdup ((x && x[0] != '\0') ? x : CLEARED_DISPLAY);

	while ((int) rpn_stack->len < rpn_stack_size) {
		g_ptr_array_add (rpn_stack, g_strdup (CLEARED_DISPLAY));
	}

	ret_val = g_strdup ((char *) rpn_stack->pdata[0]);
	for (i = 0; i < (int) rpn_stack->len - 1; i++) {
		g_free (rpn_stack->pdata[i]);
		rpn_stack->pdata[i] = g_strdup ((char *) rpn_stack->pdata[i + 1]);
	}
	g_free (rpn_stack->pdata[rpn_stack->len - 1]);
	rpn_stack->pdata[rpn_stack->len - 1] = g_strdup ((x && x[0] != '\0') ? x : CLEARED_DISPLAY);
	return ret_val;
}

char **rpn_stack_get (int length)
{
	char **return_array;
	int i;
	int used_len;

	used_len = length;
	if (used_len <= 0) used_len = rpn_stack ? (int) rpn_stack->len : 0;
	if (used_len <= 0) return NULL;

	return_array = g_new0 (char *, used_len);
	for (i = 0; i < used_len; i++) {
		if (rpn_stack && i < (int) rpn_stack->len)
			return_array[i] = g_strdup ((char *) rpn_stack->pdata[i]);
		else
			return_array[i] = g_strdup (CLEARED_DISPLAY);
	}
	return return_array;
}

void rpn_stack_set_size (int size)
{
	int i;

	rpn_ensure_init ();
	if ((size > 0) && ((size < rpn_stack_size) || (rpn_stack_size == -1))) {
		for (i = (int) rpn_stack->len - 1; i >= size; i--) {
			g_free (rpn_stack->pdata[i]);
			g_ptr_array_remove_index (rpn_stack, i);
		}
	}
	rpn_stack_size = size;
}

void rpn_free (void)
{
	if (rpn_stack) {
		g_ptr_array_free (rpn_stack, TRUE);
		rpn_stack = NULL;
	}
}
