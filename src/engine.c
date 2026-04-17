/*
 * engine.c - calculator engine abstraction implementation.
 */

#include "engine.h"

#include <string.h>
#include "engine_qalc_bridge.h"

struct talc_engine {
	char *last_error;
};

static gboolean talc_identifier_is_constant_name (const char *ident)
{
	static const char *allowed[] = {
		"pi", "e", "i",
		NULL
	};
	int i;

	if (!ident || ident[0] == '\0') return FALSE;
	for (i = 0; allowed[i] != NULL; i++) {
		if (g_ascii_strcasecmp (ident, allowed[i]) == 0) return TRUE;
	}
	return FALSE;
}

static gboolean talc_identifier_is_function_name (const char *ident)
{
	static const char *allowed[] = {
		"sin", "cos", "tan",
		"asin", "acos", "atan",
		"sinh", "cosh", "tanh",
		"asinh", "acosh", "atanh",
		"log", "log10", "ln", "sqrt", "abs",
		NULL
	};
	int i;

	if (!ident || ident[0] == '\0') return FALSE;
	for (i = 0; allowed[i] != NULL; i++) {
		if (g_ascii_strcasecmp (ident, allowed[i]) == 0) return TRUE;
	}
	return FALSE;
}

static gboolean talc_identifier_is_binary_keyword (const char *ident)
{
	if (!ident || ident[0] == '\0') return FALSE;
	return (g_ascii_strcasecmp (ident, "mod") == 0) ||
		(g_ascii_strcasecmp (ident, "and") == 0) ||
		(g_ascii_strcasecmp (ident, "or") == 0) ||
		(g_ascii_strcasecmp (ident, "xor") == 0);
}

static gboolean talc_identifier_is_unary_keyword (const char *ident)
{
	if (!ident || ident[0] == '\0') return FALSE;
	return (g_ascii_strcasecmp (ident, "not") == 0);
}

static gboolean talc_identifier_is_hex_literal (const talc_engine_context *ctx, const char *ident)
{
	int i;

	if (!ctx || !ident || ident[0] == '\0') return FALSE;
	if (ctx->base != TALC_ENGINE_BASE_HEX) return FALSE;
	for (i = 0; ident[i] != '\0'; i++) {
		if (!g_ascii_isxdigit ((guchar) ident[i])) return FALSE;
	}
	return TRUE;
}

static gboolean talc_is_value_tail_char (char c)
{
	return g_ascii_isalnum ((guchar) c) || c == '_' || c == '.' || c == ')' || c == '!' || c == '%';
}

static int talc_prev_non_space (const char *text, int start)
{
	int i;

	if (!text) return -1;
	for (i = start; i >= 0; i--) {
		if (!g_ascii_isspace (text[i])) return i;
	}
	return -1;
}

static int talc_next_non_space (const char *text, int start)
{
	int i;

	if (!text) return -1;
	for (i = start; text[i] != '\0'; i++) {
		if (!g_ascii_isspace (text[i])) return i;
	}
	return -1;
}

static gboolean talc_is_operand_start_char (char c)
{
	return g_ascii_isalnum ((guchar) c) || c == '_' || c == '.' ||
		c == '(' || c == '+' || c == '-';
}

static char *talc_engine_validate_identifiers (const talc_engine_context *ctx, const char *expression)
{
	int i = 0;

	if (!expression) return NULL;
	while (expression[i] != '\0') {
		int start = i;
		int end;
		int prev_idx;
		int next_idx;
		char *ident;

		if (!(g_ascii_isalpha ((guchar) expression[i]) || expression[i] == '_')) {
			i++;
			continue;
		}
		i++;
		while (g_ascii_isalnum ((guchar) expression[i]) || expression[i] == '_') i++;
		end = i - 1;
		prev_idx = talc_prev_non_space (expression, start - 1);
		next_idx = talc_next_non_space (expression, end + 1);
		ident = g_strndup (&expression[start], (gsize) (i - start));
		if (!ident) return g_strdup ("Out of memory");
		if (talc_identifier_is_hex_literal (ctx, ident)) {
			g_free (ident);
			continue;
		}
		if (talc_identifier_is_constant_name (ident)) {
			g_free (ident);
			continue;
		}
		if (talc_identifier_is_function_name (ident)) {
			if (ctx && ctx->rpn_notation) {
				g_free (ident);
				continue;
			}
			if (next_idx < 0 || expression[next_idx] != '(') {
				char *err = g_strdup_printf ("Function requires parentheses: %s", ident);
				g_free (ident);
				return err;
			}
			g_free (ident);
			continue;
		}
		if (talc_identifier_is_binary_keyword (ident)) {
			if (ctx && ctx->rpn_notation) {
				g_free (ident);
				continue;
			}
			if ((prev_idx < 0) || !talc_is_value_tail_char (expression[prev_idx]) ||
				(next_idx < 0) || !talc_is_operand_start_char (expression[next_idx])) {
				char *err = g_strdup_printf ("Invalid operator position: %s", ident);
				g_free (ident);
				return err;
			}
			g_free (ident);
			continue;
		}
		if (talc_identifier_is_unary_keyword (ident)) {
			if (ctx && ctx->rpn_notation) {
				g_free (ident);
				continue;
			}
			if ((next_idx < 0) || !talc_is_operand_start_char (expression[next_idx])) {
				char *err = g_strdup_printf ("Invalid operator position: %s", ident);
				g_free (ident);
				return err;
			}
			g_free (ident);
			continue;
		}
		{
			char *err = g_strdup_printf ("Unknown identifier: %s", ident);
			g_free (ident);
			return err;
		}
	}
	return NULL;
}

static gboolean talc_is_operand_char (char c)
{
	return g_ascii_isalnum ((guchar) c) || c == '_' || c == '.';
}

static gboolean talc_is_identifier_tail_char (char c)
{
	return g_ascii_isalnum ((guchar) c) || c == '_';
}

static gboolean talc_is_binary_left_value (char c)
{
	return talc_is_value_tail_char (c);
}

static char *talc_expression_rewrite_word_math (const char *expression)
{
	GString *out;
	int i = 0;

	if (!expression) return NULL;
	out = g_string_new ("");
	while (expression[i] != '\0') {
		int start = i;
		char *ident;
		int next_idx;

		if (!(g_ascii_isalpha ((guchar) expression[i]) || expression[i] == '_')) {
			g_string_append_c (out, expression[i]);
			i++;
			continue;
		}
		i++;
		while (g_ascii_isalnum ((guchar) expression[i]) || expression[i] == '_') i++;
		ident = g_strndup (&expression[start], (gsize) (i - start));
		if (!ident) {
			g_string_free (out, TRUE);
			return NULL;
		}
		next_idx = talc_next_non_space (expression, i);
		if ((g_ascii_strcasecmp (ident, "log") == 0) &&
			(next_idx >= 0) && (expression[next_idx] == '(')) {
			g_string_append (out, "log10");
		} else if (g_ascii_strcasecmp (ident, "and") == 0) {
			g_string_append_c (out, '&');
		} else if (g_ascii_strcasecmp (ident, "or") == 0) {
			g_string_append_c (out, '|');
		} else if (g_ascii_strcasecmp (ident, "not") == 0) {
			g_string_append_c (out, '~');
		} else {
			g_string_append (out, ident);
		}
		g_free (ident);
	}
	return g_string_free (out, FALSE);
}

static gboolean talc_prev_token_is_numeric_literal (const char *text, int token_end)
{
	int start;
	int len;

	if (!text || token_end < 0) return FALSE;
	if (!talc_is_operand_char (text[token_end])) return FALSE;
	start = token_end;
	while (start > 0 && talc_is_operand_char (text[start - 1])) start--;
	len = token_end - start + 1;
	if (len <= 0) return FALSE;
	if (g_ascii_isdigit ((guchar) text[start]) || text[start] == '.') return TRUE;
	if ((len > 2) && (text[start] == '0') &&
		((text[start + 1] == 'x') || (text[start + 1] == 'X') ||
		 (text[start + 1] == 'b') || (text[start + 1] == 'B') ||
		 (text[start + 1] == 'o') || (text[start + 1] == 'O'))) {
		return TRUE;
	}
	return FALSE;
}

static gboolean talc_is_unary_sign_position (const char *text, int sign_idx)
{
	int prev;

	if (!text || sign_idx < 0) return FALSE;
	prev = talc_prev_non_space (text, sign_idx - 1);
	if (prev < 0) return TRUE;
	switch (text[prev]) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '^':
	case '<':
	case '>':
	case '&':
	case '|':
	case 'x':
	case 'm':
	case '(':
		return TRUE;
	default:
		return FALSE;
	}
}

static int talc_find_operand_start (const char *text, int end_idx)
{
	int i;

	if (!text) return -1;
	i = talc_prev_non_space (text, end_idx);
	if (i < 0) return -1;

	while (i >= 0 && text[i] == '!') {
		i = talc_prev_non_space (text, i - 1);
	}
	if (i < 0) return -1;

	if (text[i] == ')') {
		int depth = 1;
		i--;
		while (i >= 0) {
			if (text[i] == ')') depth++;
			else if (text[i] == '(') {
				depth--;
				if (depth == 0) {
					int start = i;
					int f = talc_prev_non_space (text, start - 1);
					while (f >= 0 && talc_is_identifier_tail_char (text[f])) {
						start = f;
						f--;
					}
					if ((start > 0) &&
						((text[start - 1] == '+') || (text[start - 1] == '-')) &&
						talc_is_unary_sign_position (text, start - 1)) {
						start--;
					}
					return start;
				}
			}
			i--;
		}
		return -1;
	}

	if (!talc_is_operand_char (text[i])) return -1;
	while (i > 0 && talc_is_operand_char (text[i - 1])) i--;
	if ((i > 0) && ((text[i - 1] == '+') || (text[i - 1] == '-')) &&
		talc_is_unary_sign_position (text, i - 1)) {
		i--;
	}
	return i;
}

static int talc_find_segment_start_for_addsub (const char *text, int operator_idx)
{
	int i;
	int depth = 0;
	int start = 0;

	if (!text || operator_idx <= 0) return 0;
	for (i = operator_idx - 1; i >= 0; i--) {
		if (text[i] == ')') depth++;
		else if (text[i] == '(') {
			if (depth == 0) {
				start = i + 1;
				break;
			}
			depth--;
		}
	}
	return talc_next_non_space (text, start);
}

static gboolean talc_is_binary_operator_idx (const char *text, int idx, char op)
{
	int prev;

	if (!text || idx < 0) return FALSE;
	if (text[idx] != op) return FALSE;
	prev = talc_prev_non_space (text, idx - 1);
	if (prev < 0) return FALSE;
	return talc_is_binary_left_value (text[prev]);
}

static void talc_gstring_replace_range (GString *str, int begin, int end_inclusive, const char *replacement)
{
	gsize remove_len;

	if (!str || !replacement || begin < 0 || end_inclusive < begin) return;
	remove_len = (gsize) (end_inclusive - begin + 1);
	g_string_erase (str, begin, remove_len);
	g_string_insert (str, begin, replacement);
}

static char *talc_expression_add_implicit_multiply (const char *expression)
{
	GString *result;
	int i;

	if (!expression) return NULL;
	result = g_string_new ("");
	for (i = 0; expression[i] != '\0'; i++) {
		char c = expression[i];
		int prev_idx;
		char prev = '\0';

		prev_idx = talc_prev_non_space (result->str, (int) result->len - 1);
		if (prev_idx >= 0) prev = result->str[prev_idx];

		if (c == '(') {
			if ((prev == ')') || (prev == '%') || (prev == '!') ||
				talc_prev_token_is_numeric_literal (result->str, prev_idx)) {
				g_string_append_c (result, '*');
			}
		} else if (talc_is_operand_char (c)) {
			if (prev == ')') g_string_append_c (result, '*');
		}
		g_string_append_c (result, c);
	}
	return g_string_free (result, FALSE);
}

static char *talc_expression_rewrite_percent (const char *expression)
{
	GString *expr;
	int i;

	if (!expression) return NULL;
	expr = g_string_new (expression);
	for (i = 0; i < (int) expr->len; i++) {
		int y_end;
		int y_start;
		int prev_idx;
		int next_idx;
		int op_idx;
		char op = '\0';

		if (expr->str[i] != '%') continue;

		prev_idx = talc_prev_non_space (expr->str, i - 1);
		next_idx = talc_next_non_space (expr->str, i + 1);
		if ((prev_idx >= 0 && expr->str[prev_idx] == '%') ||
			(next_idx >= 0 && expr->str[next_idx] == '%')) {
			g_string_free (expr, TRUE);
			return NULL;
		}

		y_end = talc_prev_non_space (expr->str, i - 1);
		if (y_end < 0) {
			g_string_free (expr, TRUE);
			return NULL;
		}
		y_start = talc_find_operand_start (expr->str, y_end);
		if (y_start < 0) {
			g_string_free (expr, TRUE);
			return NULL;
		}

		op_idx = talc_prev_non_space (expr->str, y_start - 1);
		if ((op_idx >= 0) &&
			((expr->str[op_idx] == '+') || (expr->str[op_idx] == '-') ||
			 (expr->str[op_idx] == '*') || (expr->str[op_idx] == '/')) &&
			talc_is_binary_operator_idx (expr->str, op_idx, expr->str[op_idx])) {
			int lhs_start;
			int lhs_end;
			char *lhs;
			char *y;
			char *replacement;

			op = expr->str[op_idx];
			lhs_end = talc_prev_non_space (expr->str, op_idx - 1);
			if ((op == '+') || (op == '-')) lhs_start = talc_find_segment_start_for_addsub (expr->str, op_idx);
			else lhs_start = talc_find_operand_start (expr->str, lhs_end);
			if ((lhs_end < 0) || (lhs_start < 0) || (lhs_start > lhs_end)) {
				g_string_free (expr, TRUE);
				return NULL;
			}

			lhs = g_strndup (&expr->str[lhs_start], (gsize) (lhs_end - lhs_start + 1));
			y = g_strndup (&expr->str[y_start], (gsize) (y_end - y_start + 1));
			if (op == '+')
				replacement = g_strdup_printf ("((%s)+((%s)*(%s)/100))", lhs, lhs, y);
			else if (op == '-')
				replacement = g_strdup_printf ("((%s)-((%s)*(%s)/100))", lhs, lhs, y);
			else if (op == '*')
				replacement = g_strdup_printf ("((%s)*((%s)/100))", lhs, y);
			else
				replacement = g_strdup_printf ("((%s)/((%s)/100))", lhs, y);

			talc_gstring_replace_range (expr, lhs_start, i, replacement);
			i = lhs_start + (int) strlen (replacement) - 1;
			g_free (lhs);
			g_free (y);
			g_free (replacement);
		} else {
			char *y;
			char *replacement;

			y = g_strndup (&expr->str[y_start], (gsize) (y_end - y_start + 1));
			replacement = g_strdup_printf ("((%s)/100)", y);
			talc_gstring_replace_range (expr, y_start, i, replacement);
			i = y_start + (int) strlen (replacement) - 1;
			g_free (y);
			g_free (replacement);
		}
	}
	return g_string_free (expr, FALSE);
}

static char *talc_engine_normalize_expression (const char *expression, const char **out_error)
{
	char *word_normalized;
	char *with_mul;
	char *with_percent;

	if (out_error) *out_error = NULL;
	word_normalized = talc_expression_rewrite_word_math (expression);
	if (!word_normalized) {
		if (out_error) *out_error = "Failed to rewrite expression keywords";
		return NULL;
	}
	with_mul = talc_expression_add_implicit_multiply (word_normalized);
	g_free (word_normalized);
	if (!with_mul) {
		if (out_error) *out_error = "Failed to normalize expression";
		return NULL;
	}
	with_percent = talc_expression_rewrite_percent (with_mul);
	g_free (with_mul);
	if (!with_percent) {
		if (out_error) *out_error = "Invalid percent expression";
		return NULL;
	}
	return with_percent;
}

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
	char *normalized = NULL;
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
	normalized = talc_engine_normalize_expression (expression, &bridge_error);
	if (!normalized) {
		talc_engine_set_error (engine, bridge_error ? bridge_error : "Expression normalization failed");
		return NULL;
	}
	{
		char *validation_error = talc_engine_validate_identifiers (ctx, normalized);
		if (validation_error) {
			talc_engine_set_error (engine, validation_error);
			g_free (validation_error);
			g_free (normalized);
			return NULL;
		}
	}

	if (talc_qalc_bridge_eval_formatted (ctx, normalized, &formatted, &bridge_error)) {
		g_free (normalized);
		talc_engine_set_error (engine, bridge_error ? bridge_error : "");
		return formatted;
	}
	g_free (normalized);
	talc_engine_set_error (engine, bridge_error ? bridge_error : "libqalculate backend failed");
	return NULL;
}

char *talc_engine_eval_expression_with_contexts (talc_engine *engine,
	const talc_engine_context *parse_ctx,
	const talc_engine_context *print_ctx,
	const char *expression)
{
	char *formatted = NULL;
	char *normalized = NULL;
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
	normalized = talc_engine_normalize_expression (expression, &bridge_error);
	if (!normalized) {
		talc_engine_set_error (engine, bridge_error ? bridge_error : "Expression normalization failed");
		return NULL;
	}
	{
		char *validation_error = talc_engine_validate_identifiers (parse_ctx, normalized);
		if (validation_error) {
			talc_engine_set_error (engine, validation_error);
			g_free (validation_error);
			g_free (normalized);
			return NULL;
		}
	}

	if (talc_qalc_bridge_eval_formatted_with_contexts (parse_ctx, print_ctx,
		normalized, &formatted, &bridge_error)) {
		g_free (normalized);
		talc_engine_set_error (engine, bridge_error ? bridge_error : "");
		return formatted;
	}
	g_free (normalized);
	talc_engine_set_error (engine, bridge_error ? bridge_error : "libqalculate backend failed");
	return NULL;
}

const char *talc_engine_last_error (const talc_engine *engine)
{
	if (!engine || !engine->last_error) return "";
	return engine->last_error;
}
