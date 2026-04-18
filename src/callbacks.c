/*
 *  callbacks.c - functions to handle GUI events.
 *    part of talculator
 *      (c) 2002-2014 Simon Flöry (simon.floery@rechenraum.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc_basic.h"
#include "talculator.h"
#include "general_functions.h"
#include "display.h"
#include "config_file.h"
#include "callbacks.h"
#include "ui.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

static gboolean session_state_captured = FALSE;

static void capture_session_state_if_needed (const char *source)
{
	s_session_state session_state;
	(void) source;

	if (session_state_captured) return;
	session_state_captured = TRUE;
	if (!prefs.rem_display) {
		config_file_set_session_state (NULL);
		return;
	}

	memset (&session_state, 0, sizeof(session_state));
	ui_collect_session_state (&session_state);
	config_file_set_session_state (&session_state);
	config_file_session_state_clear (&session_state);
}

static void request_main_window_quit ()
{
    GtkWidget *main_window;
    capture_session_state_if_needed ("request_quit");
    if (!main_window_xml) {
        gtk_main_quit();
        return;
    }
    main_window = GTK_WIDGET(gtk_builder_get_object (main_window_xml, "main_window"));
    if (main_window) gtk_widget_destroy (main_window);
    else gtk_main_quit();
}

static void popup_menu_for_button (GtkWidget *menu, GtkWidget *button)
{
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu_popup_at_widget (GTK_MENU(menu), button,
        GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else
    gtk_menu_popup (GTK_MENU(menu), NULL, NULL,
        (GtkMenuPositionFunc) position_menu, button, 0, gtk_get_current_event_time());
#endif
}

static void popup_menu_for_event (GtkWidget *menu, GdkEventButton *event)
{
#if GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu_popup_at_pointer (GTK_MENU(menu), (const GdkEvent *) event);
#else
    gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL,
        event ? event->button : 0,
        event ? event->time : gtk_get_current_event_time());
#endif
}

static gboolean cycle_tab_from_key (GdkEventKey *key_event)
{
    GtkNotebook *notebook;
    gint page_count, current_page, target_page;
    gboolean backward;

    if ((key_event->state & GDK_CONTROL_MASK) == 0) return FALSE;
    if ((key_event->keyval != GDK_KEY_Tab) && (key_event->keyval != GDK_KEY_ISO_Left_Tab)) return FALSE;

    notebook = ui_tabs_get_notebook ();
    if (!notebook) return FALSE;
    page_count = gtk_notebook_get_n_pages (notebook);
    if (page_count <= 1) return TRUE;

    current_page = gtk_notebook_get_current_page (notebook);
    backward = ((key_event->state & GDK_SHIFT_MASK) != 0) || (key_event->keyval == GDK_KEY_ISO_Left_Tab);
    if (backward) target_page = (current_page - 1 + page_count) % page_count;
    else target_page = (current_page + 1) % page_count;
    ui_tab_select (target_page);
    return TRUE;
}

static gboolean tab_keyword_digit (char c)
{
	return (c >= '1') && (c <= '6');
}

static gboolean tab_keyword_is_reserved_name (const char *name)
{
	if (!name) return FALSE;
	return (g_ascii_strncasecmp (name, "tab", 3) == 0) &&
		(name[3] != '\0') &&
		tab_keyword_digit (name[3]) &&
		(name[4] == '\0');
}

static gboolean custom_symbol_name_has_valid_syntax (const char *name)
{
	int i;

	if (!name || name[0] == '\0') return FALSE;
	if (!(g_ascii_isalpha ((guchar) name[0]) || name[0] == '_')) return FALSE;
	for (i = 1; name[i] != '\0'; i++) {
		if (!(g_ascii_isalnum ((guchar) name[i]) || name[i] == '_')) return FALSE;
	}
	return TRUE;
}

static gboolean custom_symbol_name_is_reserved (const char *name)
{
	static const char *reserved[] = {
		"pi", "e", "i",
		"sin", "cos", "tan",
		"asin", "acos", "atan",
		"sinh", "cosh", "tanh",
		"asinh", "acosh", "atanh",
		"log", "log10", "ln", "sqrt", "abs",
		"mod", "and", "or", "xor", "not",
		NULL
	};
	int i;

	if (!name) return FALSE;
	if (tab_keyword_is_reserved_name (name)) return TRUE;
	for (i = 0; reserved[i] != NULL; i++) {
		if (g_ascii_strcasecmp (name, reserved[i]) == 0) return TRUE;
	}
	return FALSE;
}

static gboolean custom_symbol_name_is_accepted (const char *name, const char *kind)
{
	if (!custom_symbol_name_has_valid_syntax (name)) {
		error_message (_("%s name must match [A-Za-z_][A-Za-z0-9_]*."), kind);
		return FALSE;
	}
	if (custom_symbol_name_is_reserved (name)) {
		error_message (_("%s name is reserved."), kind);
		return FALSE;
	}
	return TRUE;
}

static char *tab_keyword_display_value_by_index (int tab_index_1based)
{
	GtkNotebook *notebook;
	GtkWidget *page;
	s_tab_context *ctx;
	GtkTextIter start, end;
	char *value;

	if (tab_index_1based < 1 || tab_index_1based > 6) return g_strdup ("0");
	notebook = ui_tabs_get_notebook ();
	if (!notebook) return g_strdup ("0");
	if (gtk_notebook_get_n_pages (notebook) < tab_index_1based) return g_strdup ("0");
	page = gtk_notebook_get_nth_page (notebook, tab_index_1based - 1);
	if (!page) return g_strdup ("0");

	ctx = g_object_get_data (G_OBJECT(page), "tab-context");
	if (!ctx || !ctx->tab_display_buffer) return g_strdup ("0");
	if (gtk_text_buffer_get_line_count (ctx->tab_display_buffer) <= ctx->tab_display_result_line)
		return g_strdup ("0");

	gtk_text_buffer_get_iter_at_line (ctx->tab_display_buffer, &start, ctx->tab_display_result_line);
	end = start;
	gtk_text_iter_forward_to_line_end (&end);
	value = gtk_text_buffer_get_text (ctx->tab_display_buffer, &start, &end, FALSE);
	if (!value || value[0] == '\0') {
		if (value) g_free (value);
		return g_strdup ("0");
	}

	return value;
}

static char *expression_resolve_tab_keywords (const char *expression)
{
	GString *out;
	size_t i = 0;

	if (!expression) return NULL;
	out = g_string_new ("");
	while (expression[i] != '\0') {
		size_t start = i;
		char *ident;

		if (!(g_ascii_isalpha ((guchar) expression[i]) || expression[i] == '_')) {
			g_string_append_c (out, expression[i]);
			i++;
			continue;
		}

		i++;
		while (g_ascii_isalnum ((guchar) expression[i]) || expression[i] == '_') i++;
		ident = g_strndup (&expression[start], i - start);
		if (!ident) {
			g_string_free (out, TRUE);
			return NULL;
		}

		if (tab_keyword_is_reserved_name (ident)) {
			int tab_index = ident[3] - '0';
			char *tab_value = tab_keyword_display_value_by_index (tab_index);
			g_string_append_printf (out, "(%s)", tab_value ? tab_value : "0");
			if (tab_value) g_free (tab_value);
		} else {
			g_string_append (out, ident);
		}
		g_free (ident);
	}

	return g_string_free (out, FALSE);
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
	ctx->display_precision = get_display_number_length (current_status.number);
	ctx->decimal_point = dec_point[0];
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

static gboolean engine_eval_expression (const char *expression,
	char **out_display)
{
	talc_engine_context engine_ctx;
	char *display_string = NULL;
	char *resolved_expression = NULL;

	if (!expression || !calc_engine) return FALSE;
	if (out_display) *out_display = NULL;

	engine_context_from_ui_state (&engine_ctx);
	resolved_expression = expression_resolve_tab_keywords (expression);
	if (!resolved_expression) return FALSE;
	display_string = talc_engine_eval_expression (calc_engine, &engine_ctx, resolved_expression);
	g_free (resolved_expression);
	if (!display_string || display_string[0] == '\0') {
		if (display_string) g_free (display_string);
		return FALSE;
	}
	if (out_display) *out_display = display_string;
	else g_free (display_string);
	return TRUE;
}

typedef struct {
	GPtrArray *undo_stack;
	GPtrArray *redo_stack;
	char *last_text;
	gboolean in_replay;
} talc_entry_history;

static void talc_entry_history_free (gpointer data)
{
	talc_entry_history *history = (talc_entry_history *) data;
	if (!history) return;
	if (history->undo_stack) g_ptr_array_free (history->undo_stack, TRUE);
	if (history->redo_stack) g_ptr_array_free (history->redo_stack, TRUE);
	if (history->last_text) g_free (history->last_text);
	g_free (history);
}

static talc_entry_history *talc_entry_history_get (GtkEditable *editable, gboolean create)
{
	talc_entry_history *history;

	if (!editable) return NULL;
	history = (talc_entry_history *) g_object_get_data (G_OBJECT (editable), "talc-entry-history");
	if (history || !create) return history;

	history = g_new0 (talc_entry_history, 1);
	history->undo_stack = g_ptr_array_new_with_free_func (g_free);
	history->redo_stack = g_ptr_array_new_with_free_func (g_free);
	history->last_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (editable)));
	g_object_set_data_full (G_OBJECT (editable), "talc-entry-history",
		history, talc_entry_history_free);
	return history;
}

static void talc_entry_history_push (GPtrArray *stack, const char *text)
{
	if (!stack) return;
	g_ptr_array_add (stack, g_strdup (text ? text : ""));
	if (stack->len > 256) g_ptr_array_remove_index (stack, 0);
}

static char *talc_entry_history_pop (GPtrArray *stack)
{
	char *text;
	char *copy;

	if (!stack || stack->len == 0) return NULL;
	text = (char *) g_ptr_array_index (stack, stack->len - 1);
	copy = g_strdup (text ? text : "");
	g_ptr_array_remove_index (stack, stack->len - 1);
	return copy;
}

static GtkWidget *focused_input_entry (void)
{
	GtkWidget *main_window;
	GtkWidget *focus;
	const char *name;

	if (!main_window_xml) return NULL;
	main_window = GTK_WIDGET (gtk_builder_get_object (main_window_xml, "main_window"));
	if (!main_window || !GTK_IS_WINDOW (main_window)) return NULL;

	focus = gtk_window_get_focus (GTK_WINDOW (main_window));
	if (!focus || !GTK_IS_ENTRY (focus)) return NULL;
	name = gtk_buildable_get_name (GTK_BUILDABLE (focus));
	if ((g_strcmp0 (name, "formula_entry") == 0) ||
		(g_strcmp0 (name, "paper_entry") == 0)) {
		return focus;
	}
	return NULL;
}

static GtkWidget *active_tab_input_entry (void)
{
	GtkWidget *entry;

	if (!active_tab || !view_xml) return NULL;
	if (active_tab->tab_mode == PAPER_MODE)
		entry = GTK_WIDGET (gtk_builder_get_object (view_xml, "paper_entry"));
	else
		entry = GTK_WIDGET (gtk_builder_get_object (view_xml, "formula_entry"));
	if (!entry || !GTK_IS_EDITABLE (entry)) return NULL;
	return entry;
}

static void mirror_display_value_into_formula_entry (const char *value)
{
	GtkWidget *entry;

	if (!active_tab || active_tab->tab_mode == PAPER_MODE) return;
	if (current_status.notation == CS_RPN) return;
	entry = GTK_WIDGET (gtk_builder_get_object (view_xml, "formula_entry"));
	if (!entry || !GTK_IS_ENTRY (entry)) return;
	if (!value || strcmp (value, CLEARED_DISPLAY) == 0)
		gtk_entry_set_text (GTK_ENTRY (entry), "");
	else
		gtk_entry_set_text (GTK_ENTRY (entry), value);
}

static void talc_entry_history_on_changed (GtkEditable *editable)
{
	talc_entry_history *history;
	const char *current_text;

	if (!editable || !GTK_IS_ENTRY (editable)) return;
	history = talc_entry_history_get (editable, TRUE);
	if (!history) return;

	current_text = gtk_entry_get_text (GTK_ENTRY (editable));
	if (history->in_replay) {
		g_free (history->last_text);
		history->last_text = g_strdup (current_text);
		return;
	}

	if (g_strcmp0 (history->last_text, current_text) == 0) return;

	talc_entry_history_push (history->undo_stack, history->last_text ? history->last_text : "");
	g_ptr_array_set_size (history->redo_stack, 0);
	g_free (history->last_text);
	history->last_text = g_strdup (current_text);
}

static gboolean talc_entry_history_undo (GtkEditable *editable)
{
	talc_entry_history *history;
	char *previous_text;
	const char *current_text;

	if (!editable || !GTK_IS_ENTRY (editable)) return FALSE;
	history = talc_entry_history_get (editable, TRUE);
	if (!history || history->undo_stack->len == 0) return FALSE;

	current_text = gtk_entry_get_text (GTK_ENTRY (editable));
	talc_entry_history_push (history->redo_stack, current_text);

	previous_text = talc_entry_history_pop (history->undo_stack);
	if (!previous_text) return FALSE;
	history->in_replay = TRUE;
	gtk_entry_set_text (GTK_ENTRY (editable), previous_text);
	gtk_editable_set_position (editable, -1);
	history->in_replay = FALSE;
	g_free (history->last_text);
	history->last_text = previous_text;
	return TRUE;
}

static gboolean talc_entry_history_redo (GtkEditable *editable)
{
	talc_entry_history *history;
	char *next_text;
	const char *current_text;

	if (!editable || !GTK_IS_ENTRY (editable)) return FALSE;
	history = talc_entry_history_get (editable, TRUE);
	if (!history || history->redo_stack->len == 0) return FALSE;

	current_text = gtk_entry_get_text (GTK_ENTRY (editable));
	talc_entry_history_push (history->undo_stack, current_text);

	next_text = talc_entry_history_pop (history->redo_stack);
	if (!next_text) return FALSE;
	history->in_replay = TRUE;
	gtk_entry_set_text (GTK_ENTRY (editable), next_text);
	gtk_editable_set_position (editable, -1);
	history->in_replay = FALSE;
	g_free (history->last_text);
	history->last_text = next_text;
	return TRUE;
}

static gboolean handle_entry_clipboard_shortcut (GtkWidget *widget, GdkEventKey *event)
{
	gboolean ctrl_only;
	gboolean shift;

	if (!widget || !GTK_IS_EDITABLE (widget) || !event) return FALSE;

	ctrl_only = ((event->state & GDK_CONTROL_MASK) != 0) &&
		((event->state & (GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK)) == 0);
	shift = ((event->state & GDK_SHIFT_MASK) != 0);
	if (!ctrl_only) return FALSE;

	if (((event->keyval == GDK_KEY_c) || (event->keyval == GDK_KEY_C)) && !shift) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
		return TRUE;
	}
	if (((event->keyval == GDK_KEY_x) || (event->keyval == GDK_KEY_X)) && !shift) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
		return TRUE;
	}
	if (((event->keyval == GDK_KEY_v) || (event->keyval == GDK_KEY_V)) && !shift) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
		return TRUE;
	}
	if (((event->keyval == GDK_KEY_z) || (event->keyval == GDK_KEY_Z)) && !shift) {
		return talc_entry_history_undo (GTK_EDITABLE (widget));
	}
	if ((event->keyval == GDK_KEY_y) || (event->keyval == GDK_KEY_Y) ||
		(((event->keyval == GDK_KEY_z) || (event->keyval == GDK_KEY_Z)) && shift)) {
		return talc_entry_history_redo (GTK_EDITABLE (widget));
	}

	return FALSE;
}

static gboolean talc_widget_is_same_or_ancestor (GtkWidget *widget, GtkWidget *possible_descendant)
{
	GtkWidget *cursor;

	if (!widget || !possible_descendant) return FALSE;
	for (cursor = possible_descendant; cursor != NULL; cursor = gtk_widget_get_parent (cursor)) {
		if (cursor == widget) return TRUE;
	}
	return FALSE;
}

static char *build_unary_function_expression (const char *fn_text, const char *operand)
{
	if (!fn_text || !operand) return NULL;
	if (g_str_has_suffix (fn_text, "(")) {
		return g_strdup_printf ("%s%s)", fn_text, operand);
	}
	if (strcmp (fn_text, "^2") == 0) return g_strdup_printf ("(%s)^2", operand);
	if (strcmp (fn_text, "10^") == 0) return g_strdup_printf ("10^(%s)", operand);
	if (strcmp (fn_text, "e^") == 0) return g_strdup_printf ("e^(%s)", operand);
	if (strcmp (fn_text, "!") == 0) return g_strdup_printf ("(%s)!", operand);
	if (strcmp (fn_text, "~") == 0) return g_strdup_printf ("~(%s)", operand);
	return NULL;
}

static char *substitute_user_variable (const char *expression,
	const char *variable,
	const char *value)
{
	GString *result;
	size_t var_len;
	size_t i;
	gboolean left_ok, right_ok;
	char prev, next;

	if (!expression || !variable || !value) return NULL;
	var_len = strlen (variable);
	if (var_len == 0) return g_strdup (expression);

	result = g_string_new ("");
	for (i = 0; expression[i] != '\0';) {
		if (strncmp (&expression[i], variable, var_len) != 0) {
			g_string_append_c (result, expression[i]);
			i++;
			continue;
		}
		prev = (i == 0) ? '\0' : expression[i - 1];
		next = expression[i + var_len];
		left_ok = (i == 0) || !(g_ascii_isalnum ((guchar) prev) || prev == '_');
		right_ok = (next == '\0') || !(g_ascii_isalnum ((guchar) next) || next == '_');
		if (left_ok && right_ok) {
			g_string_append_printf (result, "(%s)", value);
			i += var_len;
		} else {
			g_string_append_len (result, &expression[i], (gssize) var_len);
			i += var_len;
		}
	}
	return g_string_free (result, FALSE);
}

static const char *operation_expr_text (char operation)
{
	switch (operation) {
	case '<':
		return "<<";
	case '>':
		return ">>";
	case 'x':
		return " xor ";
	case 'm':
		return " mod ";
	default:
		return NULL;
	}
}

/* File */

void
on_main_window_destroy               (GtkWidget*         widget,
                                        gpointer         user_data)
{
    (void) widget;
    (void) user_data;
    GtkNotebook *notebook;
    GtkWidget *page;
    s_tab_context *ctx;

    notebook = ui_tabs_get_notebook();
    if (notebook) {
        page = gtk_notebook_get_nth_page (notebook, gtk_notebook_get_current_page (notebook));
        if (page) {
            ctx = g_object_get_data (G_OBJECT(page), "tab-context");
            if (ctx) active_tab = ctx;
        }
    }
    
    capture_session_state_if_needed ("destroy");

    g_object_unref(main_window_xml);
    main_window_xml = NULL;
    if (calc_engine) {
        talc_engine_free (calc_engine);
        calc_engine = NULL;
    }

    gtk_main_quit();
}

gboolean
on_main_window_delete_event          (GtkWidget *widget,
                                        GdkEvent *event,
                                        gpointer user_data)
{
    (void) widget;
    (void) event;
    (void) user_data;
    capture_session_state_if_needed ("delete_event");
    return FALSE;
}

void
on_quit_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    request_main_window_quit ();
}

/* Help */

void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) menuitem;
    (void) user_data;
    GtkWidget *about_dialog = ui_about_dialog_create();
    gtk_dialog_run (GTK_DIALOG(about_dialog));
}

void
on_new_tab_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    ui_tab_create (NULL);
}

void
on_close_tab_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!ui_tab_close_current ()) request_main_window_quit ();
}

/* this callback is called if a button for entering a number is clicked. There are two
 * cases: either starting a new number or appending a digit to the existing number.
 * The decimal point leads to some specialities.
 */

void
on_number_button_clicked               (GtkToggleButton  *button,
                                        gpointer         user_data)
{    
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    if (current_status.notation == CS_ALG) {
        ui_formula_entry_insert (gtk_button_get_label ((GtkButton *)button));
    } else {
        rpn_stack_lift();
        display_result_add_digit (*(gtk_button_get_label ((GtkButton *)button)), current_status.number);
    }
    return;
}

/* this callback is called if a button for doing one of the arithmetic operations plus, minus, 
 * multiply, divide or power is clicked. it is mainly an interface to the calc_basic code.
 */

void
on_operation_button_clicked(GtkToggleButton *button, gpointer user_data)
{
    (void) user_data;
    char              current_operation;
    char             *current_number = NULL;
    char            **stack = NULL;
    char             *rpn_result = NULL;
    GtkWidget        *tbutton;
    
    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    
    current_operation = GPOINTER_TO_INT(g_object_get_data(G_OBJECT (button), "operation"));
    /* do inverse left shift is a right shift */
    if ((current_operation == '<') && (BIT (current_status.fmod, CS_FMOD_FLAG_INV) == 1)) {
		tbutton = GTK_WIDGET(gtk_builder_get_object (button_box_xml, "button_inv"));
		gtk_toggle_button_set_active ((GtkToggleButton *) tbutton, FALSE);
        current_operation = '>';
    }
    
    if (current_status.notation == CS_ALG) {
        if (strcmp (gtk_buildable_get_name(GTK_BUILDABLE(button)), "button_enter") == 0)
            ui_formula_entry_activate();
        /* as long as we don't support string operation ids, we take
         * operation char. take this later on:
         * else ui_formula_entry_insert (
         *    g_object_get_data (G_OBJECT (button), "display_string"));
         */
        else {
			const char *op_text = operation_expr_text (current_operation);
			if (op_text)
				ui_formula_entry_insert(op_text);
			else {
				char text[2];
				text[0] = current_operation;
				text[1] = '\0';
				ui_formula_entry_insert(text);
			}
        }
        return;
    }
    
    /* current number, get it from the display! */
    current_number = display_result_get ();
    
    /* notation specific interface code */
    
    if (current_status.notation == CS_RPN) {
        switch (current_operation) {
        case '=':
            rpn_stack_push (current_number);
            stack = rpn_stack_get (RPN_FINITE_STACK);
            display_stack_set_yzt (stack);
            g_free (stack[0]);
            g_free (stack[1]);
            g_free (stack[2]);
            g_free (stack);
            /* ENT is a stack lift disabling button */
            current_status.rpn_stack_lift_enabled = FALSE;
            /* display line isn't cleared! */
            break;
        default:
            rpn_result = rpn_stack_operation (current_operation, current_number);
            display_result_set (rpn_result, TRUE);
            g_free (rpn_result);
            stack = rpn_stack_get (RPN_FINITE_STACK);
            display_stack_set_yzt (stack);
            g_free (stack[0]);
            g_free (stack[1]);
            g_free (stack[2]);
            g_free (stack);
            /* all other operations are stack lift enabling */
            current_status.rpn_stack_lift_enabled = TRUE;
        }
    } else error_message ("on_operation_button_clicked: unknown status");

    current_status.calc_entry_start_new = TRUE;
    if (current_number) g_free (current_number);
    return;
}

/* this callback is called if a button for a function manipulating the current 
 * entry directly is clicked. the array function_list knows the relation between 
 * button label and function to call.
 */

void
on_function_button_clicked             (GtkToggleButton    *button,
                                        gpointer user_data)
{
    (void) user_data;
    char         **display_name;
    const char    *fn_text;
    char          *operand = NULL;
    char          *expression = NULL;
    char          *display_result = NULL;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    display_name = (char **) g_object_get_data (G_OBJECT (button), "display_names");
    if (!display_name) {
        error_message ("This button has no function associated with");
        return;
    }
    if (current_status.notation == CS_ALG) {
        ui_formula_entry_insert (display_name[current_status.fmod]);
        if (current_status.fmod != 0) ui_relax_fmod_buttons();
        return;
    }

    fn_text = display_name[current_status.fmod];
    operand = display_result_get ();
    expression = build_unary_function_expression (fn_text, operand);
    if (!expression || !engine_eval_expression (expression, &display_result)) {
        error_message ("Engine failed to evaluate function expression");
    } else {
        display_result_set (display_result, TRUE);
        g_free (display_result);
    }
    if (operand) g_free (operand);
    if (expression) g_free (expression);

    current_status.calc_entry_start_new = TRUE;    
    if (current_status.notation == CS_RPN) 
        current_status.rpn_stack_lift_enabled = TRUE;
    if (current_status.fmod != 0) ui_relax_fmod_buttons();
}

/* tbutton_fmod - these are function modifiers such as INV (inverse) 
 *    and HYP (hyperbolic).
 */

void
on_tbutton_fmod_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (strcmp (gtk_button_get_label (button), "inv") == 0)
        current_status.fmod ^= 1 << CS_FMOD_FLAG_INV;
    else if (strcmp (gtk_button_get_label (button), "hyp") == 0)
        current_status.fmod ^= 1 << CS_FMOD_FLAG_HYP;
    else error_message ("unknown function modifier (INV/HYP)");
}

void
on_gfunc_button_clicked                (GtkToggleButton       *button,
                                        gpointer         user_data)
{
    (void) user_data;
    void    (*func)(GtkToggleButton *button);
    char     *display_string;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    if (current_status.notation == CS_ALG) {
        display_string = g_object_get_data (G_OBJECT (button), "display_string");
        if (display_string != NULL) {
            ui_formula_entry_insert (display_string);
            return;
        }
    }
    if (strcmp(gtk_buildable_get_name(GTK_BUILDABLE(button)), "button_ee") == 0) 
        rpn_stack_lift();
    func = g_object_get_data (G_OBJECT (button), "func");
    if (func != NULL) func(button);
    else error_message ("This button has no general function associated with");
}

/*
 * MENU
 */

void
on_dec_toggled                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_DEC, DISPLAY_OPT_NUMBER);
}


void
on_hex_toggled (GtkMenuItem     *menuitem, gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_HEX, DISPLAY_OPT_NUMBER);
}


void
on_oct_toggled                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_OCT, DISPLAY_OPT_NUMBER);
}


void
on_bin_toggled                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_BIN, DISPLAY_OPT_NUMBER);
}


void
on_deg_toggled                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_DEG, DISPLAY_OPT_ANGLE);
}


void
on_rad_toggled                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_RAD, DISPLAY_OPT_ANGLE);
}


void
on_grad_toggled                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (!gtk_check_menu_item_get_active((GtkCheckMenuItem *)menuitem)) return;
    change_option (CS_GRAD, DISPLAY_OPT_ANGLE);
}

void
on_ordinary_toggled                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) == FALSE) return;
    change_option (CS_ALG, DISPLAY_OPT_NOTATION);
    set_widget_visibility (view_xml, "formula_entry_hbox", TRUE);
    rpn_free();
    all_clear();
    ui_button_set_pan();
    display_stack_remove();
    update_dispctrl();
    /* pixel above/below display result line */
    display_update_tags ();
}

void
on_toggle_notation_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (active_tab->tab_mode == PAPER_MODE) return;
    if (current_status.notation == CS_RPN) activate_menu_item ("alg");
    else activate_menu_item ("rpn");
}

void
on_cycle_view_mode_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    switch (active_tab->tab_mode) {
    case BASIC_MODE:
        activate_menu_item ("scientific_mode");
        break;
    case SCIENTIFIC_MODE:
        activate_menu_item ("paper_mode");
        break;
    case PAPER_MODE:
    default:
        activate_menu_item ("basic_mode");
        break;
    }
}

void
on_rpn_toggled                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) == FALSE) return;
    change_option (CS_RPN, DISPLAY_OPT_NOTATION);
    
    set_widget_visibility (view_xml, "formula_entry_hbox", FALSE);
    all_clear();
    ui_button_set_rpn();
    /* stack is created by all_clear */
    update_dispctrl();
    /* pixel above/below display result line */
    display_update_tags ();
}

void
on_display_control_toggled (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (active_tab->tab_mode == PAPER_MODE) return;
    active_tab->tab_vis_dispctrl =
        gtk_check_menu_item_get_active((GtkCheckMenuItem *) menuitem);
    prefs.vis_dispctrl = active_tab->tab_vis_dispctrl;
    set_widget_visibility (dispctrl_xml, "table_dispctrl", 
        active_tab->tab_vis_dispctrl);
}

void 
on_logical_toggled (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (active_tab->tab_mode == BASIC_MODE) return;
    if (active_tab->tab_mode == PAPER_MODE) return;

	active_tab->tab_vis_logic =
        gtk_check_menu_item_get_active((GtkCheckMenuItem *) menuitem);
    prefs.vis_logic = active_tab->tab_vis_logic;
    set_widget_visibility (button_box_xml, "table_bin_buttons",
        active_tab->tab_vis_logic);
}

void
on_functions_toggled (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (active_tab->tab_mode == BASIC_MODE) return;
    if (active_tab->tab_mode == PAPER_MODE) return;
    active_tab->tab_vis_funcs =
        gtk_check_menu_item_get_active((GtkCheckMenuItem *) menuitem);
    prefs.vis_funcs = active_tab->tab_vis_funcs;
    set_widget_visibility (button_box_xml, "table_func_buttons",
        active_tab->tab_vis_funcs);
}

void
on_standard_toggled (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (active_tab->tab_mode == BASIC_MODE) return;
    if (active_tab->tab_mode == PAPER_MODE) return;
    active_tab->tab_vis_standard =
        gtk_check_menu_item_get_active((GtkCheckMenuItem *) menuitem);
    prefs.vis_standard = active_tab->tab_vis_standard;
    set_widget_visibility (button_box_xml, "table_standard_buttons",
        active_tab->tab_vis_standard);
}

void
on_basic_mode_toggled (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    char *display_value = NULL;
    
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) == FALSE) return;

    display_value = display_result_get();
    if (active_tab->tab_mode == SCIENTIFIC_MODE) {
        /* remember number and angle. notation is active in basic mode */
        active_tab->tab_def_number = current_status.number;
        active_tab->tab_def_angle = current_status.angle;
    }
    active_tab->tab_mode = BASIC_MODE;
    prefs.mode = active_tab->tab_mode;
    
    ui_paper_view_destroy ();
    ui_classic_view_create ();
    ui_main_window_buttons_destroy ();
    ui_main_window_buttons_create (active_tab->tab_mode);
    update_dispctrl();
    
    display_update_modules();
    
    /* In basic mode:
     *    - number base is always decimal.
     *    - ignore angle, as there are no angle operations in basic mode.
     *    - notation is fully functional.
     */    
    display_module_number_activate (CS_DEC);
    display_module_notation_activate (current_status.notation);

    update_active_buttons (current_status.number, current_status.notation);
    ui_sync_main_menu_for_active_tab ();
    
    set_window_size_minimal();
    
    if (display_value != NULL) {
        display_result_set(display_value, TRUE);
        mirror_display_value_into_formula_entry (display_value);
        g_free(display_value);
    }
}

void
on_scientific_mode_toggled (GtkMenuItem *menuitem,
                gpointer user_data)
{
    (void) user_data;
    char *display_value = NULL;
    
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) == FALSE) return;

    display_value = display_result_get();
			
    active_tab->tab_mode = SCIENTIFIC_MODE;
    prefs.mode = active_tab->tab_mode;

    ui_paper_view_destroy ();
    ui_classic_view_create ();
    ui_main_window_buttons_destroy ();
    ui_main_window_buttons_create (active_tab->tab_mode);
    
    display_update_modules();
    display_module_number_activate (active_tab->tab_def_number);
    display_module_angle_activate (active_tab->tab_def_angle);
    display_module_notation_activate (current_status.notation);

    update_active_buttons (current_status.number, current_status.notation);
    update_dispctrl();
    
    ui_sync_main_menu_for_active_tab ();
    
    set_window_size_minimal();
    
    if (display_value != NULL) {
        display_result_set(display_value, TRUE);
        mirror_display_value_into_formula_entry (display_value);
        g_free(display_value);
    }
}

void
on_paper_mode_toggled (GtkMenuItem *menuitem,
                gpointer user_data)
{
    (void) user_data;
    char *display_value = NULL;

    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) == FALSE) return;
    display_value = display_result_get();

    active_tab->tab_mode = PAPER_MODE;
    prefs.mode = active_tab->tab_mode;

    ui_classic_view_destroy();
    ui_paper_view_create ();
    
    /* this is to get the radio menu items right. display* naming is 
     * misleading though ...
     */
    display_module_number_activate (active_tab->tab_def_number);
    display_module_angle_activate (active_tab->tab_def_angle);
    ui_sync_main_menu_for_active_tab ();
    
    set_window_size_minimal();
    if (display_value != NULL) {
        display_result_set(display_value, TRUE);
        if (strcmp (display_value, CLEARED_DISPLAY) == 0) {
            GtkWidget *paper_entry = GTK_WIDGET (gtk_builder_get_object (view_xml, "paper_entry"));
            if (paper_entry && GTK_IS_ENTRY (paper_entry))
                gtk_entry_set_text (GTK_ENTRY (paper_entry), "");
        }
        g_free(display_value);
    }
}    

void
on_cut_activate (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    GtkWidget *entry;

    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    entry = focused_input_entry ();
    if (entry) {
        gtk_editable_cut_clipboard (GTK_EDITABLE (entry));
        return;
    }
    if (active_tab->tab_mode == PAPER_MODE) return;
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), 
        display_result_get(), -1);
    clear ();
}

void
on_paste_activate (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    GtkWidget    *entry;
    GtkEditable *editable;
    char        *cp_text;
    gint        position;
    
    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    entry = focused_input_entry ();
    if (entry) {
        gtk_editable_paste_clipboard (GTK_EDITABLE (entry));
        return;
    }
    entry = active_tab_input_entry ();
    if (!entry) return;
    editable = GTK_EDITABLE (entry);
    gtk_widget_grab_focus (entry);
    cp_text = gtk_clipboard_wait_for_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
    if (cp_text) {
        gtk_editable_set_position (editable, -1);
        position = gtk_editable_get_position (editable);
        gtk_editable_insert_text (editable, cp_text, -1, &position);
        gtk_editable_set_position (editable, position);
        g_free (cp_text);
    }
}

void
on_copy_activate (GtkMenuItem     *menuitem,
            gpointer         user_data)
{
    (void) user_data;
    GtkWidget *entry;

    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    entry = focused_input_entry ();
    if (entry) {
        gtk_editable_copy_clipboard (GTK_EDITABLE (entry));
        return;
    }
    if (active_tab->tab_mode == PAPER_MODE) return;
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), 
        display_result_get(), -1);
}

void
on_undo_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    (void) user_data;
    GtkWidget *entry;

    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    entry = focused_input_entry ();
    if (entry) talc_entry_history_undo (GTK_EDITABLE (entry));
}

void
on_redo_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    (void) user_data;
    GtkWidget *entry;

    ui_bind_active_tab_from_widget (GTK_WIDGET(menuitem));
    entry = focused_input_entry ();
    if (entry) talc_entry_history_redo (GTK_EDITABLE (entry));
}

/*
 * Preferences
 */

void user_function_list_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
    (void) data;
    GtkTreeModel     *model;
    char         *string;
    GtkWidget    *entry;
    GtkTreeIter     current_list_iter;
    
        if (gtk_tree_selection_get_selected (selection, &model, &current_list_iter))
        {
                gtk_tree_model_get (model, &current_list_iter, UFUNC_NAME_COLUMN, &string, -1);
        entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufname_entry"));
        gtk_entry_set_text ((GtkEntry *) entry, string);
                g_free (string);
        gtk_tree_model_get (model, &current_list_iter, UFUNC_VARIABLE_COLUMN, &string, -1);
        entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufvar_entry"));
        gtk_entry_set_text ((GtkEntry *) entry, string);
                g_free (string);
        gtk_tree_model_get (model, &current_list_iter, UFUNC_EXPRESSION_COLUMN, &string, -1);
        entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufexpr_entry"));
        gtk_entry_set_text ((GtkEntry *) entry, string);
                g_free (string);
        }
}

void const_list_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
    (void) data;
    GtkTreeModel    *model;
    char            *string;
    GtkWidget       *entry;
    GtkTreeIter     current_list_iter;
    
        if (gtk_tree_selection_get_selected (selection, &model, &current_list_iter))
        {
                gtk_tree_model_get (model, &current_list_iter, CONST_NAME_COLUMN, &string, -1);
        entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cname_entry"));
        gtk_entry_set_text ((GtkEntry *) entry, string);
                g_free (string);
        gtk_tree_model_get (model, &current_list_iter, CONST_VALUE_COLUMN, &string, -1);
        entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cvalue_entry"));
        gtk_entry_set_text ((GtkEntry *) entry, string);
                g_free (string);
        gtk_tree_model_get (model, &current_list_iter, CONST_DESC_COLUMN, &string, -1);
        entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cdesc_entry"));
        gtk_entry_set_text ((GtkEntry *) entry, string);
                g_free (string);
        }
}

void
on_preferences1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    (void) menuitem;
    (void) user_data;
    ui_pref_dialog_create();
}

void
on_prefs_result_font_set(GtkFontButton *button, gpointer user_data)
{
    (void) user_data;
    char *font_name;

#if GTK_CHECK_VERSION(3, 2, 0)
    font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER(button));
#else
    font_name = g_strdup (gtk_font_button_get_font_name(button));
#endif
    if (prefs.result_font != NULL) g_free (prefs.result_font);
    prefs.result_font = font_name ? font_name : g_strdup ("");
    display_update_tags();
}

void
on_prefs_result_color_set(GtkColorButton *button, gpointer user_data)
{
    (void) user_data;
    if (prefs.result_color != NULL) g_free (prefs.result_color);

    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    prefs.result_color = gdk_rgba_to_string(&color);

    display_update_tags();
}

void
on_prefs_stack_font_set(GtkFontButton *button, gpointer user_data)
{
    (void) user_data;
    char *font_name;

#if GTK_CHECK_VERSION(3, 2, 0)
    font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER(button));
#else
    font_name = g_strdup (gtk_font_button_get_font_name(button));
#endif
    if (prefs.stack_font != NULL) g_free (prefs.stack_font);
    prefs.stack_font = font_name ? font_name : g_strdup ("");
    display_update_tags();
}

void
on_prefs_stack_color_set(GtkColorButton *button, gpointer user_data)
{
    (void) user_data;
	if (prefs.stack_color != NULL) g_free (prefs.stack_color);
	
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    prefs.stack_color = gdk_rgba_to_string(&color);
    
    display_update_tags();
}

void
on_prefs_mod_font_set(GtkFontButton *button, gpointer user_data)
{
    (void) user_data;
    char *font_name;

#if GTK_CHECK_VERSION(3, 2, 0)
    font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER(button));
#else
    font_name = g_strdup (gtk_font_button_get_font_name(button));
#endif
    if (prefs.mod_font != NULL) g_free (prefs.mod_font);
    prefs.mod_font = font_name ? font_name : g_strdup ("");
    display_update_tags();
}

void
on_prefs_act_mod_color_set(GtkColorButton *button, gpointer user_data)
{
    (void) user_data;
    if (prefs.act_mod_color != NULL) g_free (prefs.act_mod_color);
    
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    prefs.act_mod_color = gdk_rgba_to_string(&color);
    
    display_update_tags();
}

void
on_prefs_inact_mod_color_set(GtkColorButton *button, gpointer user_data)
{
    (void) user_data;
	if (prefs.inact_mod_color != NULL) g_free (prefs.inact_mod_color);
    
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    prefs.inact_mod_color = gdk_rgba_to_string(&color);

    display_update_tags();
}

void
on_prefs_bkg_color_set(GtkColorButton *button, gpointer user_data)
{
    (void) user_data;
	if (prefs.bkg_color != NULL) g_free (prefs.bkg_color);
	
    GdkRGBA color;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);
    prefs.bkg_color = gdk_rgba_to_string(&color);

    display_set_bkg_color (prefs.bkg_color);
}

void
on_prefs_button_font_set(GtkFontButton *button, gpointer user_data)
{
    (void) user_data;
    char        *font_name;
    char        *button_font;

#if GTK_CHECK_VERSION(3, 2, 0)
    font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER(button));
#else
    font_name = g_strdup (gtk_font_button_get_font_name(button));
#endif
    if (prefs.button_font != NULL) g_free (prefs.button_font);
    prefs.button_font = font_name ? font_name : g_strdup ("");
    if (prefs.custom_button_font == TRUE) button_font = g_strdup (prefs.button_font);
    else button_font = g_strdup ("");
    set_all_buttons_font (button_font);    
    g_free (button_font);
}

void
on_prefs_close_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
    (void) user_data;
    gtk_widget_destroy (gtk_widget_get_toplevel((GtkWidget *)button));
}

void
on_show_menubar1_toggled              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkWidget    *menu_item;
    prefs.show_menu = gtk_check_menu_item_get_active ((GtkCheckMenuItem *) menuitem);
#ifdef WITH_HILDON
    set_widget_visibility (main_window_xml, "main_menu", prefs.show_menu);
#else
	set_widget_visibility (main_window_xml, "menubar", prefs.show_menu);
#endif

    /* in case this cb is called by the right button mouse click menu */
    menu_item = GTK_WIDGET(gtk_builder_get_object (main_window_xml, "show_menubar1"));
    g_signal_handlers_block_by_func(menuitem, on_show_menubar1_toggled, user_data);
    gtk_check_menu_item_set_active ((GtkCheckMenuItem *) menu_item, prefs.show_menu);
    g_signal_handlers_unblock_by_func(menuitem, on_show_menubar1_toggled, user_data);
}

void
on_prefs_custom_button_font_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    (void) user_data;
    GtkWidget    *w;
    char        *button_font;
    
    prefs.custom_button_font = gtk_toggle_button_get_active (togglebutton);
    
    w = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_button_font_label"));
    gtk_widget_set_sensitive (w, prefs.custom_button_font);
    
    w = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_button_font"));
    gtk_widget_set_sensitive (w, prefs.custom_button_font);
    
    if (prefs.custom_button_font == TRUE) button_font = g_strdup (prefs.button_font);
    else button_font = g_strdup ("");
    set_all_buttons_font (button_font);
    g_free (button_font);
}

void on_prefs_vis_number_toggled (GtkToggleButton *togglebutton, 
                gpointer user_data)
{
    (void) user_data;
    prefs.vis_number = gtk_toggle_button_get_active (togglebutton);
    active_tab->tab_vis_number = prefs.vis_number;
    display_update_modules ();
}
                                        
void on_prefs_vis_angle_toggled (GtkToggleButton *togglebutton, 
                gpointer user_data)
{
    (void) user_data;
    prefs.vis_angle = gtk_toggle_button_get_active (togglebutton);
    active_tab->tab_vis_angle = prefs.vis_angle;
    display_update_modules ();
}

void on_prefs_vis_notation_toggled (GtkToggleButton *togglebutton, 
                gpointer user_data)
{
    (void) user_data;
    prefs.vis_notation = gtk_toggle_button_get_active (togglebutton);
    active_tab->tab_vis_notation = prefs.vis_notation;
    display_update_modules ();
}

void on_prefs_vis_arith_toggled (GtkToggleButton *togglebutton, 
                gpointer user_data)
{
    (void) user_data;
    prefs.vis_arith = gtk_toggle_button_get_active (togglebutton);
    display_update_modules ();
}
    
void on_prefs_vis_bracket_toggled (GtkToggleButton *togglebutton, 
                gpointer user_data)
{
    (void) user_data;
    prefs.vis_bracket = gtk_toggle_button_get_active (togglebutton);
    display_update_modules ();
}

void on_prefs_show_menu_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    GtkCheckMenuItem        *show_menubar_item;
    
    prefs.show_menu = gtk_toggle_button_get_active (togglebutton);
#ifdef WITH_HILDON
    set_widget_visibility (main_window_xml, "main_menu", prefs.show_menu);
#else
     set_widget_visibility (main_window_xml, "menubar", prefs.show_menu);
#endif
    show_menubar_item = (GtkCheckMenuItem *) gtk_builder_get_object (main_window_xml, "show_menubar1");
    gtk_check_menu_item_set_active (show_menubar_item, prefs.show_menu);
}

void on_prefs_rem_display_toggled (GtkToggleButton *togglebutton, 
                gpointer user_data)
{
    (void) user_data;
    prefs.rem_display = gtk_toggle_button_get_active (togglebutton);
    /* only is important when leaving talculator */
}

void on_prefs_button_width_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    int new_button_width = (int) gtk_spin_button_get_value (spinbutton);
    
    if (new_button_width != prefs.button_width) {
        prefs.button_width = new_button_width;
        set_all_buttons_size (prefs.button_width, prefs.button_height);
    }
}

void on_prefs_button_height_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{    
    (void) arg1;
    (void) user_data;
    int new_button_height = (int) gtk_spin_button_get_value (spinbutton);
    if (new_button_height != prefs.button_height) {
        prefs.button_height = new_button_height;
        set_all_buttons_size (prefs.button_width, prefs.button_height);
    }
}

/*
 * USER FUNCTIONS
 */

void user_functions_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    int             index;
    char           *display_value = NULL;
    char           *substituted_expr = NULL;
    char           *current_value = NULL;
    
    ui_bind_active_tab_from_menu_item (menuitem);
    index = GPOINTER_TO_INT(user_data);
    current_value = display_result_get ();
    substituted_expr = substitute_user_variable (user_function[index].expression,
        user_function[index].variable, current_value ? current_value : "0");
    if (substituted_expr &&
        engine_eval_expression (substituted_expr, &display_value)) {
        display_result_set (display_value, TRUE);
        g_free (display_value);
        current_status.calc_entry_start_new = TRUE;    
        if (current_status.notation == CS_RPN) 
            current_status.rpn_stack_lift_enabled = TRUE;
    } else fprintf (stderr, "[%s] User function %s(%s)=%s returned with an error.\
Please check the expression string.\n", PROG_NAME, user_function[index].name, 
user_function[index].variable, user_function[index].expression); 
    if (current_value) g_free (current_value);
    if (substituted_expr) g_free (substituted_expr);
}

void
on_user_function_button_clicked (GtkToggleButton       *button,
                                        gpointer         user_data)
{
    (void) user_data;
    GtkWidget    *menu;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_user_functions_menu_create(user_function, (GCallback)user_functions_menu_handler);
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));
}

/*
 * CONSTANTS
 */

void constants_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    char        *const_value;
    
    ui_bind_active_tab_from_menu_item (menuitem);
    /* push current display value */
    current_status.rpn_stack_lift_enabled = TRUE;
    rpn_stack_lift();
	const_value = user_data;
    display_result_set (const_value, TRUE);
    current_status.rpn_stack_lift_enabled = TRUE;
    current_status.calc_entry_start_new = TRUE;
}


void
on_constant_button_clicked (GtkToggleButton       *button,
                                        gpointer         user_data)
{
    (void) user_data;
    GtkWidget        *menu;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_constants_menu_create(constant, (GCallback)constants_menu_handler);
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));
}

/*
 * MEMORY
 */

void ms_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget    *button;
    int        index;
    char      *current_value;
    
    ui_bind_active_tab_from_menu_item (menuitem);
    index = GPOINTER_TO_INT(user_data);
	if (index >= memory.len) {
	        index = memory.len;
	        memory.data = (char **) g_realloc (memory.data, ((gsize) (index + 1)) * sizeof(char *));
	        memory.len++;
	        memory.data[index] = NULL;
	    }
    current_value = display_result_get ();
    if (memory.data[index]) g_free (memory.data[index]);
    memory.data[index] = current_value;
    
    /* at startup, mr and m+ button are disabled as there is nothing
     * to show. now, as there is sth, enable them. see also 
     * ui.c::ui_main_window_buttons_create
     */
    button = GTK_WIDGET(gtk_builder_get_object (button_box_xml, "button_MR"));
    gtk_widget_set_sensitive (button, TRUE);
    button = GTK_WIDGET(gtk_builder_get_object (button_box_xml, "button_Mplus"));
    gtk_widget_set_sensitive (button, TRUE);
    
    current_status.calc_entry_start_new = TRUE;
}

void on_ms_button_clicked (GtkToggleButton *button, gpointer user_data)
{
    (void) user_data;
    GtkWidget    *menu;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_memory_menu_create (memory, (GCallback)ms_menu_handler, _("save here"));
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));
}

void mr_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    int        index;

    ui_bind_active_tab_from_menu_item (menuitem);
    /* current display value on stack */
    current_status.rpn_stack_lift_enabled = TRUE;
    rpn_stack_lift();
    index = GPOINTER_TO_INT(user_data);
    if ((index < 0) || (index >= memory.len) || (memory.data == NULL) || (memory.data[index] == NULL)) {
        return;
    }
    display_result_set(memory.data[index], TRUE);
    current_status.rpn_stack_lift_enabled = TRUE;
    current_status.calc_entry_start_new = TRUE;
}

void on_mr_button_clicked (GtkToggleButton *button, gpointer user_data)
{
    (void) user_data;
    GtkWidget    *menu;;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_memory_menu_create(memory, (GCallback)mr_menu_handler, NULL);
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));

}

void mplus_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    int        index;
    char      *current_value = NULL;
    char      *expression = NULL;
    char      *sum_value = NULL;
    
    ui_bind_active_tab_from_menu_item (menuitem);
    index = GPOINTER_TO_INT(user_data);
    if ((index < 0) || (index >= memory.len) || (memory.data == NULL)) {
        return;
    }
    current_value = display_result_get ();
    expression = g_strdup_printf ("(%s)+(%s)",
        memory.data[index] ? memory.data[index] : CLEARED_DISPLAY,
        current_value ? current_value : CLEARED_DISPLAY);
    if (expression && engine_eval_expression (expression, &sum_value)) {
        if (memory.data[index]) g_free (memory.data[index]);
        memory.data[index] = sum_value;
    } else if (sum_value) {
        g_free (sum_value);
    }
    if (current_value) g_free (current_value);
    if (expression) g_free (expression);
}

void on_mplus_button_clicked (GtkToggleButton *button, gpointer user_data)
{
    (void) user_data;
    GtkWidget    *menu;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_memory_menu_create(memory, (GCallback)mplus_menu_handler, NULL);
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));

}

void mc_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    int        index, counter;
    
    ui_bind_active_tab_from_menu_item (menuitem);
    index = GPOINTER_TO_INT(user_data);
    if (index >= memory.len) {
        for (counter = 0; counter < memory.len; counter++) {
            if (memory.data[counter]) g_free (memory.data[counter]);
        }
        if (memory.len > 0) g_free (memory.data);
        memory.data = NULL;
        memory.len = 0;
    } else {
        if (memory.data[index]) g_free (memory.data[index]);
        for (counter = index; counter < (memory.len - 1); counter++) 
            memory.data[counter] = memory.data[counter + 1];
	        memory.len--;
	        if (memory.len > 0)
	            memory.data = (char **) g_realloc (memory.data, ((gsize) memory.len) * sizeof(char *));
	        else {
            g_free (memory.data);
            memory.data = NULL;
        }
    }
}

void
on_mc_button_clicked             (GtkToggleButton       *button,
                gpointer         user_data)
{
    (void) user_data;
    GtkWidget    *menu;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_memory_menu_create(memory, (GCallback)mc_menu_handler, "clear all");
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));

}

void mx_menu_handler (GtkMenuItem *menuitem, gpointer user_data)
{
    int        index;
    char      *temp;
    char      *current_value;
    
    ui_bind_active_tab_from_menu_item (menuitem);
    index = GPOINTER_TO_INT(user_data);
    if ((index < 0) || (index >= memory.len) || (memory.data == NULL)) {
        return;
    }
    temp = memory.data[index] ? g_strdup (memory.data[index]) : g_strdup (CLEARED_DISPLAY);
    current_value = display_result_get ();
    if (memory.data[index]) g_free (memory.data[index]);
    memory.data[index] = current_value;
    display_result_set (temp, TRUE);
    g_free (temp);
}

void
on_mx_button_clicked             (GtkToggleButton       *button,
                gpointer         user_data)
{
    (void) user_data;
    GtkWidget    *menu;

    ui_bind_active_tab_from_widget (GTK_WIDGET(button));
    if (gtk_toggle_button_get_active(button) == FALSE) return;
    button_activation (button);
    menu = ui_memory_menu_create(memory, (GCallback)mx_menu_handler, NULL);
    g_object_set_data (G_OBJECT(menu), "tab-context", active_tab);
    popup_menu_for_button (menu, GTK_WIDGET(button));

}

void on_prefs_ufclear_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkWidget    *entry;
    
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufname_entry"));
    gtk_entry_set_text ((GtkEntry *) entry, "");
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufvar_entry"));
    gtk_entry_set_text ((GtkEntry *) entry, "");
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufexpr_entry"));
    gtk_entry_set_text ((GtkEntry *) entry, "");
}

void on_prefs_ufadd_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkWidget        *entry;
    GtkTreeIter           iter;
    int            nr_user_functions;
    char             *name, *value, *desc;
    
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufname_entry"));
    name = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufvar_entry"));
    value = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufexpr_entry"));
    desc = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    
    if ((strlen(name) == 0) || (strlen(value) == 0) || (strlen(desc) == 0)) {
        g_free (name);
        g_free (value);
        g_free (desc);
        return;
    }
	if (!custom_symbol_name_is_accepted (name, _("Function"))) {
		g_free (name);
		g_free (value);
		g_free (desc);
		return;
	}
        
    nr_user_functions = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(prefs_user_function_store), NULL);
	    user_function = (s_user_function *) g_realloc (user_function, ((gsize) (nr_user_functions + 2)) * sizeof(s_user_function));
    user_function[nr_user_functions + 1].name = NULL;
    
    user_function[nr_user_functions].name = name;
    user_function[nr_user_functions].variable = value;
    user_function[nr_user_functions].expression = desc;
    
    gtk_list_store_append (prefs_user_function_store, &iter);    
    gtk_list_store_set (prefs_user_function_store, &iter, 
        UFUNC_NAME_COLUMN, user_function[nr_user_functions].name, 
        UFUNC_VARIABLE_COLUMN, user_function[nr_user_functions].variable, 
        UFUNC_EXPRESSION_COLUMN, user_function[nr_user_functions].expression, 
        -1);
}

void on_prefs_ufdelete_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkTreePath        *path;
    int            index, counter, nr_user_functions;
    GtkTreeIter        current_list_iter;
        
    if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection(
        (GtkTreeView *)gtk_builder_get_object (prefs_xml, "user_function_treeview")),
        NULL, &current_list_iter)) return;
    
    nr_user_functions = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(prefs_user_function_store), NULL);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (prefs_user_function_store), 
        &current_list_iter);
    index = *(gtk_tree_path_get_indices (path));
	gtk_tree_path_free (path);

    gtk_list_store_remove (prefs_user_function_store, &current_list_iter);
    on_prefs_ufclear_clicked (NULL, NULL);

	if (user_function[index].name) g_free (user_function[index].name);
	if (user_function[index].variable) g_free (user_function[index].variable);
	if (user_function[index].expression) g_free (user_function[index].expression);

    for (counter = index; counter < (nr_user_functions - 1); counter++) {
		memmove (&user_function[counter], &user_function[counter + 1], sizeof(s_user_function));
	}
    
    nr_user_functions--;
	    user_function = (s_user_function *) g_realloc (user_function, ((gsize) (nr_user_functions + 1)) * sizeof(s_user_function));
    
	user_function[nr_user_functions].variable = NULL;
	user_function[nr_user_functions].expression = NULL;
    user_function[nr_user_functions].name = NULL;
}

void on_prefs_ufupdate_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkWidget    *entry;
    GtkTreePath    *path;
    int        index;
    GtkTreeIter     current_list_iter;
    
    if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection(
        (GtkTreeView *)gtk_builder_get_object (prefs_xml, "user_function_treeview")),
        NULL, &current_list_iter)) return;
    
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (prefs_user_function_store), 
        &current_list_iter);
    index = *(gtk_tree_path_get_indices (path));
	gtk_tree_path_free (path);
    
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufname_entry"));
	{
		const char *new_name = gtk_entry_get_text ((GtkEntry *) entry);
		if (!custom_symbol_name_is_accepted (new_name, _("Function"))) {
			return;
		}
		if (user_function[index].name) g_free (user_function[index].name);
		user_function[index].name = g_strdup (new_name);
	}
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufvar_entry"));
	if (user_function[index].variable) g_free (user_function[index].variable);
    user_function[index].variable = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_ufexpr_entry"));
	if (user_function[index].expression) g_free (user_function[index].expression);
    user_function[index].expression = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    
    gtk_list_store_set (prefs_user_function_store, &current_list_iter, 
        UFUNC_NAME_COLUMN, user_function[index].name, 
        UFUNC_VARIABLE_COLUMN, user_function[index].variable, 
        UFUNC_EXPRESSION_COLUMN, user_function[index].expression, 
        -1);
}

void on_prefs_cclear_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkWidget    *entry;
    
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cname_entry"));
    gtk_entry_set_text ((GtkEntry *) entry, "");
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cvalue_entry"));
    gtk_entry_set_text ((GtkEntry *) entry, "");
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cdesc_entry"));
    gtk_entry_set_text ((GtkEntry *) entry, "");
}

void on_prefs_cadd_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkWidget        *entry;
    GtkTreeIter           iter;
    int            nr_consts;
    char             *name, *value, *desc;
    
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cname_entry"));
    name = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cvalue_entry"));
    value = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cdesc_entry"));
    desc = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    
    if ((strlen(name) == 0) || (strlen(value) == 0) || (strlen(desc) == 0)) {
        g_free (name);
        g_free (value);
        g_free (desc);
        return;
    }
	if (!custom_symbol_name_is_accepted (name, _("Constant"))) {
		g_free (name);
		g_free (value);
		g_free (desc);
		return;
	}
        
    nr_consts = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(prefs_constant_store), NULL);
	    constant = (s_constant *) g_realloc (constant, ((gsize) (nr_consts + 2)) * sizeof(s_constant));
    constant[nr_consts + 1].name = NULL;
    
    constant[nr_consts].name = name;
    constant[nr_consts].value = value;
    constant[nr_consts].desc = desc;
    
    gtk_list_store_append (prefs_constant_store, &iter);    
    gtk_list_store_set (prefs_constant_store, &iter, 
        CONST_NAME_COLUMN, constant[nr_consts].name, 
        CONST_VALUE_COLUMN, constant[nr_consts].value, 
        CONST_DESC_COLUMN, constant[nr_consts].desc, 
        -1);
}

void on_prefs_cdelete_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkTreePath        *path;
    int            index, counter, nr_consts;
    GtkTreeIter        current_list_iter;
        
    if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection(
        (GtkTreeView *)gtk_builder_get_object (prefs_xml, "constant_treeview")),
        NULL, &current_list_iter)) return;
    
    nr_consts = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(prefs_constant_store), NULL);
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (prefs_constant_store), &current_list_iter);
    index = *(gtk_tree_path_get_indices (path));
	gtk_tree_path_free (path);

    gtk_list_store_remove (prefs_constant_store, &current_list_iter);
    on_prefs_cclear_clicked (NULL, NULL);

	if (constant[index].name) g_free (constant[index].name);
	if (constant[index].value) g_free (constant[index].value);
	if (constant[index].desc) g_free (constant[index].desc);

    for (counter = index; counter < (nr_consts - 1); counter++) {
		memmove (&constant[counter], &constant[counter + 1], sizeof(s_constant));
	}
    
    nr_consts--;
	    constant = (s_constant *) g_realloc (constant, ((gsize) (nr_consts + 1)) * sizeof(s_constant));
    
	constant[nr_consts].value = NULL;
	constant[nr_consts].desc = NULL;
    constant[nr_consts].name = NULL;
}

void on_prefs_cupdate_clicked (GtkButton *button, gpointer user_data)
{
    (void) user_data;
    (void) button;
    GtkWidget    *entry;
    GtkTreePath    *path;
    int        index;
    GtkTreeIter     current_list_iter;
    
    if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection(
        (GtkTreeView *)gtk_builder_get_object (prefs_xml, "constant_treeview")),
        NULL, &current_list_iter)) return;
    
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (prefs_constant_store), 
        &current_list_iter);
    index = *(gtk_tree_path_get_indices (path));
	gtk_tree_path_free (path);
    
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cname_entry"));
	{
		const char *new_name = gtk_entry_get_text ((GtkEntry *) entry);
		if (!custom_symbol_name_is_accepted (new_name, _("Constant"))) {
			return;
		}
		if (constant[index].name) g_free (constant[index].name);
		constant[index].name = g_strdup (new_name);
	}
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cvalue_entry"));
	if (constant[index].value) g_free (constant[index].value);
    constant[index].value = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    entry = GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_cdesc_entry"));
	if (constant[index].desc) g_free (constant[index].desc);
    constant[index].desc = g_strdup (gtk_entry_get_text ((GtkEntry *) entry));
    
    gtk_list_store_set (prefs_constant_store, &current_list_iter, 
        CONST_NAME_COLUMN, constant[index].name, 
        CONST_VALUE_COLUMN, constant[index].value, 
        CONST_DESC_COLUMN, constant[index].desc, 
        -1);
}

void on_prefs_hex_bits_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{    
    (void) arg1;
    (void) user_data;
    prefs.hex_bits = (int) gtk_spin_button_get_value (spinbutton);
}

void on_prefs_hex_signed_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    prefs.hex_signed = gtk_toggle_button_get_active (togglebutton);
}

void on_prefs_oct_bits_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    prefs.oct_bits = (int) gtk_spin_button_get_value (spinbutton);
}

void on_prefs_bin_bits_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    prefs.bin_bits = (int) gtk_spin_button_get_value (spinbutton);
}


void on_prefs_bin_signed_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    prefs.bin_signed = gtk_toggle_button_get_active (togglebutton);
}

void on_prefs_number_combo_changed(GtkComboBox *widget, gpointer user_data)
{
    (void) user_data;
	GtkTreeIter iter;
	if (!gtk_combo_box_get_active_iter(widget, &iter)) {
		fprintf (stderr, _("[%s] on_prefs_number_combo_changed failed to retrieve iter. %s\n"), PROG_NAME, BUG_REPORT);
		return;
	}
	
	int selID;
	gtk_tree_model_get(gtk_combo_box_get_model(widget), &iter, 1, &selID, -1);

    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_dec")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_hex")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_oct")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_bin")));
    
    if (selID == 0) gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_dec")));
    else if (selID == 1) gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_hex")));
    else if (selID == 2) gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_oct")));
    else if (selID == 3) gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_bin")));
}

void on_prefs_menu_dec_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    (void) user_data;
    (void) menuitem;
    gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_dec")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_hex")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_oct")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_bin")));
}
            
void on_prefs_menu_hex_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    (void) user_data;
    (void) menuitem;
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_dec")));
    gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_hex")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_oct")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_bin")));
}
            
void on_prefs_menu_oct_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    (void) user_data;
    (void) menuitem;
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_dec")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_hex")));
    gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_oct")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_bin")));
}
            
void on_prefs_menu_bin_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    (void) user_data;
    (void) menuitem;
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_dec")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_hex")));
    gtk_widget_hide (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_oct")));
    gtk_widget_show (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_vbox_bin")));
}

void on_prefs_dec_sep_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    prefs.dec_sep = gtk_toggle_button_get_active (togglebutton);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_dec_sep_char_label")), prefs.dec_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_dec_sep_char")), prefs.dec_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_dec_sep_length_label")), prefs.dec_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_dec_sep_length")), prefs.dec_sep);
    display_result_getset();
}

void on_prefs_hex_sep_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    prefs.hex_sep = gtk_toggle_button_get_active (togglebutton);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_hex_sep_char_label")), prefs.hex_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_hex_sep_char")), prefs.hex_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_hex_sep_length_label")), prefs.hex_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_hex_sep_length")), prefs.hex_sep);
    display_result_getset();
}

void on_prefs_oct_sep_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    prefs.oct_sep = gtk_toggle_button_get_active (togglebutton);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_oct_sep_char_label")), prefs.oct_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_oct_sep_char")), prefs.oct_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_oct_sep_length_label")), prefs.oct_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_oct_sep_length")), prefs.oct_sep);
    display_result_getset();
}

void on_prefs_bin_sep_toggled (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    prefs.bin_sep = gtk_toggle_button_get_active (togglebutton);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_bin_sep_char_label")), prefs.bin_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_bin_sep_char")), prefs.bin_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_bin_sep_length_label")), prefs.bin_sep);
    gtk_widget_set_sensitive (GTK_WIDGET(gtk_builder_get_object (prefs_xml, "prefs_bin_sep_length")), prefs.bin_sep);
    display_result_getset();
}

void on_prefs_dec_sep_length_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    prefs.dec_sep_length = (int) gtk_spin_button_get_value (spinbutton);
    display_result_getset();
}

void on_prefs_hex_sep_length_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    prefs.hex_sep_length = (int) gtk_spin_button_get_value (spinbutton);
    display_result_getset();
}

void on_prefs_oct_sep_length_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    prefs.oct_sep_length = (int) gtk_spin_button_get_value (spinbutton);
    display_result_getset();
}

void on_prefs_bin_sep_length_value_changed (GtkSpinButton *spinbutton,
                    GtkScrollType arg1,
                    gpointer user_data)
{
    (void) arg1;
    (void) user_data;
    prefs.bin_sep_length = (int) gtk_spin_button_get_value (spinbutton);
    display_result_getset();
}

void on_prefs_dec_sep_char_changed (GtkEditable *editable,
                                    gpointer user_data)
{
    (void) user_data;
    prefs_sep_char_changed (editable, &prefs.dec_sep_char, CS_DEC);
}

void on_prefs_hex_sep_char_changed (GtkEditable *editable,
                                    gpointer user_data)
{
    (void) user_data;
    prefs_sep_char_changed (editable, &prefs.hex_sep_char, CS_HEX);
}

void on_prefs_oct_sep_char_changed (GtkEditable *editable,
                                    gpointer user_data)
{
    (void) user_data;
    prefs_sep_char_changed (editable, &prefs.oct_sep_char, CS_OCT);
}

void on_prefs_bin_sep_char_changed (GtkEditable *editable,
                                    gpointer user_data)
{
    (void) user_data;
    prefs_sep_char_changed (editable, &prefs.bin_sep_char, CS_BIN);
}

void on_togglebutton_released (GtkToggleButton *togglebutton, 
                    gpointer user_data)
{
    (void) user_data;
    gtk_toggle_button_set_active (togglebutton, FALSE);
}

/* this function was the signal handler for 'check-resize' of main_window.
 * I see no reason to have it further. so i unlinked it. the code is here
 * if we need it some later time.
 */
void on_main_window_check_resize (GtkContainer *container,
                                            gpointer user_data)
{
    (void) user_data;
    fprintf (stderr, _("[%s] on_main_window_check_resize should not get called. %s\n"), PROG_NAME, BUG_REPORT);
    
    static gboolean        itsme=FALSE;
    
    /* only in classic views this function may take effect */
    if (active_tab->tab_mode == PAPER_MODE) return;
    /* is there a nicer way to to this? */
    if (itsme) {
        itsme = FALSE;
        return;
    }
    gtk_window_resize ((GtkWindow *)gtk_widget_get_toplevel((GtkWidget *)container), 1, 1);
    itsme = TRUE;
}

void on_finite_stack_size_clicked (GtkRadioButton *rb, gpointer user_data)
{
    (void) rb;
    (void) user_data;
    prefs.stack_size = RPN_FINITE_STACK;
    rpn_stack_set_size (prefs.stack_size);
}

void on_infinite_stack_size_clicked (GtkRadioButton *rb, gpointer user_data)
{
    (void) rb;
    (void) user_data;
    prefs.stack_size = RPN_INFINITE_STACK;
    rpn_stack_set_size (prefs.stack_size);
}

gboolean on_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    (void) user_data;
    GtkWidget    *menu;
    
    ui_bind_active_tab_from_widget (widget);
    if (event->button != 3) return FALSE;
    menu = ui_right_mouse_menu_create ();
    popup_menu_for_event (menu, event);
    return FALSE;
}

void on_formula_entry_activate (GtkEntry *entry, gpointer user_data)
{
    (void) user_data;
    char                    *formatted_result = NULL;
    
    ui_bind_active_tab_from_widget (GTK_WIDGET(entry));
    engine_eval_expression (gtk_entry_get_text(entry), &formatted_result);
    ui_formula_entry_state (formatted_result == NULL);
    if (formatted_result) {
        display_result_set (formatted_result, TRUE);
        g_free (formatted_result);
    }
}

void on_formula_entry_changed (GtkEditable *editable, gpointer user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(editable));
    talc_entry_history_on_changed (editable);
    ui_formula_entry_state(FALSE);
}

gboolean on_formula_entry_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void) user_data;
    GtkWidget *toplevel;
    GtkDirectionType direction;

    ui_bind_active_tab_from_widget (widget);

    if (cycle_tab_from_key (event)) return TRUE;

    if ((event->state & GDK_CONTROL_MASK) &&
        !(event->state & GDK_SUPER_MASK) &&
        !(event->state & GDK_HYPER_MASK) &&
        !(event->state & GDK_META_MASK) &&
        ((event->keyval == GDK_KEY_a) || (event->keyval == GDK_KEY_A))) {
        gtk_editable_select_region (GTK_EDITABLE(widget), 0, -1);
        return TRUE;
    }

    if ((event->keyval == GDK_KEY_Tab) || (event->keyval == GDK_KEY_ISO_Left_Tab)) {
        toplevel = gtk_widget_get_toplevel (widget);
        if (!GTK_IS_WINDOW (toplevel)) return FALSE;
        direction = (event->keyval == GDK_KEY_ISO_Left_Tab) ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD;
        return gtk_widget_child_focus (toplevel, direction);
    }

    if (handle_entry_clipboard_shortcut (widget, event)) return TRUE;

    return FALSE;
}

void on_paper_entry_changed (GtkEditable *editable, gpointer user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (GTK_WIDGET(editable));
    talc_entry_history_on_changed (editable);
}

gboolean on_paper_entry_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void) user_data;
    ui_bind_active_tab_from_widget (widget);

    if (cycle_tab_from_key (event)) return TRUE;
    if (handle_entry_clipboard_shortcut (widget, event)) return TRUE;

    return FALSE;
}

void on_paper_entry_activate (GtkWidget *activated_widget, gpointer user_data)
{
    (void) user_data;
    GtkEntry                *entry;
    GtkTreeView                *tree_view;
    GtkListStore            *paper_store;
    GtkTreeIter               iter;
    char                    *escaped_input_string, *result_string, *markup_result_string;
    GtkTreePath*             last_row_path;
    
    ui_bind_active_tab_from_widget (activated_widget);
    if (!GTK_IS_ENTRY(activated_widget))
        entry = GTK_ENTRY(gtk_builder_get_object (view_xml, "paper_entry"));
    else
        entry = GTK_ENTRY(activated_widget);
    
    if (strcmp(gtk_entry_get_text(entry), "") == 0) return;
    result_string = NULL;
	engine_eval_expression (gtk_entry_get_text(entry), &result_string);
    
    /* add to tree view */
    tree_view = GTK_TREE_VIEW(gtk_builder_get_object (view_xml, "paper_treeview"));
    paper_store = GTK_LIST_STORE(gtk_tree_view_get_model(tree_view));
    gtk_list_store_append (paper_store, &iter);
    escaped_input_string = g_markup_escape_text(gtk_entry_get_text(entry), -1);
    gtk_list_store_set (paper_store, &iter, 0, escaped_input_string, 1, 0.0, 2, NULL, -1);
    g_free(escaped_input_string);
    
    gtk_list_store_append (paper_store, &iter);
    if (!result_string)
        gtk_list_store_set (paper_store, &iter, 0, "Syntax Error", 1, 1.0, 2, "red", -1);
    else {
        /* Keep paper mode aligned with session persistence by updating
         * the tab's current display result on successful evaluation.
         */
        display_result_set (result_string, TRUE);
        markup_result_string = g_markup_printf_escaped ("<b>%s</b>", result_string);
        gtk_list_store_set (paper_store, &iter, 0, markup_result_string, 1, 1.0, 2, NULL, -1);
        g_free (result_string);
        g_free (markup_result_string);
    }
    
    /* scroll to last row */
    last_row_path = gtk_tree_model_get_path (gtk_tree_view_get_model(tree_view), &iter);
    gtk_tree_view_scroll_to_cell(tree_view, last_row_path, NULL, FALSE, 0., 0.);
    
    /* clear entry */
    gtk_entry_set_text (entry, "");
}

gboolean paper_tree_view_selection_changed_cb (GtkWidget *widget,
                                            GdkEventButton *event,
                                            gpointer user_data)
{
    (void) user_data;
    GtkTreeModel         *model;
    char             *string, *stripped_string;
    GtkWidget        *entry;
    GtkTreeIter         current_list_iter;
    int            position;
    GtkTreeSelection    *select;
    
    ui_bind_active_tab_from_widget (widget);
    if ((event->type == GDK_2BUTTON_PRESS) && (event->button == 1)) {
        select = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
        if (gtk_tree_selection_get_selected (select, &model, &current_list_iter)) {
            gtk_tree_model_get (model, &current_list_iter, 0, &string, -1);
            stripped_string = g_strdup(string);
            pango_parse_markup (string, -1, 0, NULL, &stripped_string, NULL, NULL);
            g_free (string);
            entry = GTK_WIDGET(gtk_builder_get_object (view_xml, "paper_entry"));
            position = gtk_editable_get_position (GTK_EDITABLE(entry));
	            {
	                gsize len = strlen (stripped_string);
	                gint insert_len = (len > (gsize) G_MAXINT) ? G_MAXINT : (gint) len;
	                gtk_editable_insert_text (GTK_EDITABLE (entry), stripped_string, insert_len, &position);
	            }
            /* set position after currently inserted text */
            gtk_editable_set_position (GTK_EDITABLE(entry), position);
            g_free (stripped_string);
        }
    }
    /* return FALSE to let other process this signal, too */
    return FALSE;
}

gboolean on_button_can_activate_accel (GtkWidget *widget, guint signal_id, gpointer user_data)
{
    (void) user_data;
    if (!gtk_widget_is_sensitive(widget)) return FALSE;
    if (strcmp("clicked", g_signal_name(signal_id)) == 0) return TRUE;
    return FALSE;
}

gboolean on_menuitem_can_activate_accel (GtkWidget *widget, guint signal_id, gpointer user_data)
{
    (void) user_data;
    if (!gtk_widget_is_sensitive(widget)) return FALSE;
    if (strcmp("activate", g_signal_name(signal_id)) == 0) return TRUE;
    return FALSE;
}

/* This callback is connected to the "Event" event of the main window. Every
 * event in gtk triggers three events: this general "Event", the more specific
 * event and the general "Event-after". If we return TRUE, processing of the
 * event stops.
 */
gboolean on_button_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void) user_data;
    if (event->type == GDK_KEY_PRESS) {
        GdkEventKey *key_event = (GdkEventKey *) event;
        GtkWidget *focus = NULL;

        if (GTK_IS_WINDOW(widget)) {
            focus = gtk_window_get_focus (GTK_WINDOW(widget));
            if (focus) ui_bind_active_tab_from_widget (focus);
        }
        if (cycle_tab_from_key (key_event)) return TRUE;

        if ((key_event->state & GDK_CONTROL_MASK) &&
            !(key_event->state & GDK_SUPER_MASK) &&
            !(key_event->state & GDK_HYPER_MASK) &&
            !(key_event->state & GDK_META_MASK) &&
            ((key_event->keyval == GDK_KEY_l) || (key_event->keyval == GDK_KEY_L))) {
            GtkWidget *target = NULL;
            if (active_tab->tab_mode == PAPER_MODE)
                target = GTK_WIDGET(gtk_builder_get_object (view_xml, "paper_entry"));
            else
                target = GTK_WIDGET(gtk_builder_get_object (view_xml, "formula_entry"));
            if (target && gtk_widget_get_visible (target) && gtk_widget_get_sensitive (target)) {
                gtk_widget_grab_focus (target);
                if (GTK_IS_EDITABLE (target)) {
                    gint pos = gtk_editable_get_position (GTK_EDITABLE (target));
                    gtk_editable_select_region (GTK_EDITABLE (target), pos, pos);
                }
                return TRUE;
            }
        }

        if (key_event->keyval == GDK_KEY_Escape && active_tab->tab_mode != PAPER_MODE) {
            GtkWidget *formula_entry_widget;
            GtkWidget *main_window;
            gboolean in_formula_focus = FALSE;
            GtkWidget *allclr_button;

            formula_entry_widget = GTK_WIDGET(gtk_builder_get_object (view_xml, "formula_entry"));
            main_window = GTK_WIDGET(gtk_builder_get_object (main_window_xml, "main_window"));
            if (focus && formula_entry_widget)
                in_formula_focus = talc_widget_is_same_or_ancestor (formula_entry_widget, focus);

            if (in_formula_focus) {
                if (GTK_IS_EDITABLE (focus)) {
                    gint pos = gtk_editable_get_position (GTK_EDITABLE (focus));
                    gtk_editable_select_region (GTK_EDITABLE (focus), pos, pos);
                }
                if (active_tab->tab_display_view)
                    gtk_widget_grab_focus (GTK_WIDGET (active_tab->tab_display_view));
                else if (main_window && GTK_IS_WINDOW (main_window))
                    gtk_window_set_focus (GTK_WINDOW (main_window), NULL);
                return TRUE;
            }
            allclr_button = GTK_WIDGET(gtk_builder_get_object (dispctrl_xml, "button_allclr"));
            if (allclr_button && gtk_widget_get_sensitive (allclr_button))
                gtk_button_clicked (GTK_BUTTON (allclr_button));
            else
                all_clear ();
            return TRUE;
        }
    }
    /* do all cheap checks first before calling expensive formula_entry_is_active */
    if ((current_status.notation == CS_ALG) &&
        (event->type == GDK_KEY_PRESS)) {
        GdkEventKey *key_event = (GdkEventKey *) event;
        GtkWidget *formula_entry_widget;

        formula_entry_widget = GTK_WIDGET(gtk_builder_get_object (view_xml, "formula_entry"));
        if ((key_event->state & GDK_CONTROL_MASK) &&
            !(key_event->state & GDK_SUPER_MASK) &&
            !(key_event->state & GDK_HYPER_MASK) &&
            !(key_event->state & GDK_META_MASK) &&
            ((key_event->keyval == GDK_KEY_a) || (key_event->keyval == GDK_KEY_A))) {
            if (formula_entry_widget &&
                gtk_widget_get_visible (formula_entry_widget) &&
                gtk_widget_get_sensitive (formula_entry_widget)) {
                gtk_widget_grab_focus (formula_entry_widget);
                gtk_editable_select_region (GTK_EDITABLE (formula_entry_widget), 0, -1);
                return TRUE;
            }
        }
        if ((key_event->keyval == GDK_KEY_Escape) ||
            (key_event->keyval == GDK_KEY_Tab) ||
            (key_event->keyval == GDK_KEY_ISO_Left_Tab))
            return FALSE;
        /* try to rule out some obvious key presses */
        if (key_event->state & GDK_SUPER_MASK ||
             key_event->state & GDK_HYPER_MASK ||
             key_event->state & GDK_META_MASK ||
             key_event->state & GDK_MOD1_MASK ||
             key_event->state & GDK_CONTROL_MASK) return FALSE;
        GtkWidget *formula_entry = formula_entry_is_active(widget);
        if (formula_entry) {
            gtk_widget_event (formula_entry, event);
            return TRUE;
        }
    }
    return FALSE;
}

/* END */
