/*
 *  display.c - code for this nifty display.
 *	part of talculator
 *  	(c) 2002-2014 Simon Flöry (simon.floery@rechenraum.com)
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
#include <string.h>
#include <math.h>

#include "talculator.h"
#include "display.h"
#include "general_functions.h"
#include "config_file.h"
#include "ui.h"

#include <gtk/gtk.h>
#include <glib.h>

#define view (active_tab->tab_display_view)
#define buffer (active_tab->tab_display_buffer)
#define display_result_counter (active_tab->tab_display_result_counter)
#define display_result_line (active_tab->tab_display_result_line)

static char	*number_mod_labels[5] = {" DEC ", " HEX ", " OCT ", " BIN ", NULL}, 
		*angle_mod_labels[4] = {" DEG ", " RAD ", " GRAD ", NULL},
		*notation_mod_labels[3] = {" ALG ", " RPN ", NULL};

/* no one outside display.c should need to call these functions */
static void display_set_line (char *string, int line, char *tag);
static char *display_get_line (int line_nr);
static void display_engine_context_for_base (talc_engine_context *ctx, int number_base_status);
static char *display_convert_base_string (const char *value, int from_base, int to_base);
static void display_widget_css_set (GtkWidget *widget, const gchar *css, const gchar *data_key);
static int display_bracket_forward_count (int brackets);

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

static int display_bracket_forward_count (int brackets)
{
	int digits = 1;
	int value = brackets;

	if (value < 0) value = 0;
	while (value >= 10) {
		value /= 10;
		digits++;
	}
	return 5 + digits;
}

static gboolean display_bracket_module_should_render (void)
{
	GtkWidget *formula_hbox;

	if ((prefs.vis_bracket == FALSE) || (active_tab->tab_mode != SCIENTIFIC_MODE))
		return FALSE;

	formula_hbox = GTK_WIDGET(gtk_builder_get_object (view_xml, "formula_entry_hbox"));
	if (formula_hbox && gtk_widget_get_visible (formula_hbox))
		return FALSE;

	return TRUE;
}

static void display_engine_context_for_base (talc_engine_context *ctx, int number_base_status)
{
	if (!ctx) return;
	ctx->mode = (talc_engine_mode) active_tab->tab_mode;
	ctx->base = (talc_engine_base) number_base_status;
	ctx->angle = (talc_engine_angle) current_status.angle;
	ctx->rpn_notation = (current_status.notation == CS_RPN);
	ctx->display_precision = get_display_number_length (number_base_status);
	ctx->decimal_point = dec_point[0];
	ctx->base_bits = 0;
	ctx->base_signed = FALSE;
	ctx->custom_constants = (const talc_engine_custom_constant *) constant;
	ctx->custom_constants_len = custom_constant_count ();
	ctx->custom_functions = (const talc_engine_custom_function *) user_function;
	ctx->custom_functions_len = custom_function_count ();
	switch (number_base_status) {
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

static char *display_convert_base_string (const char *value, int from_base, int to_base)
{
	talc_engine_context parse_ctx;
	talc_engine_context print_ctx;
	char *converted;

	if (!value || value[0] == '\0') return g_strdup (CLEARED_DISPLAY);
	if (!calc_engine) return g_strdup (value);
	if (from_base == to_base) return g_strdup (value);

	display_engine_context_for_base (&parse_ctx, from_base);
	display_engine_context_for_base (&print_ctx, to_base);
	converted = talc_engine_eval_expression_with_contexts (calc_engine,
		&parse_ctx, &print_ctx, value);
	if (!converted || converted[0] == '\0') {
		if (converted) g_free (converted);
		return g_strdup (value);
	}
	return converted;
}

static void display_widget_css_set (GtkWidget *widget, const gchar *css, const gchar *data_key)
{
	GtkStyleContext	*style_context;
	GtkCssProvider	*provider;

	if (!widget || !css || !data_key) return;
	style_context = gtk_widget_get_style_context (widget);
	if (!style_context) return;
	provider = g_object_get_data (G_OBJECT(widget), data_key);
	if (!provider) {
		provider = gtk_css_provider_new ();
		gtk_style_context_add_provider (style_context,
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
		g_object_set_data_full (G_OBJECT(widget), data_key, provider, g_object_unref);
	}
	gtk_css_provider_load_from_data (provider, css, -1, NULL);
}

/*
 * display.c mainly consists of two parts: first display setup code
 * and second display manipulation code.
 */

/* this code is taken from the GTK 2.0 tutorial: 
 *		http://www.gtk.org/tutorial
 */
gboolean on_textview_button_press_event (GtkWidget *widget,
						GdkEventButton *event,
						gpointer user_data)
{
    (void) user_data;
	ui_bind_active_tab_from_widget (widget);
	static 			GdkAtom targets_atom = GDK_NONE;
	int			x, y;
	GtkTextIter		start, end;
	char 			*selected_text;

	if (event->button == 1)	{
        // GTK_CHECK_VERSION returns TRUE if version is at least ...
#if GTK_CHECK_VERSION(3, 4, 0)
        GdkModifierType mask;
        gdk_window_get_device_position(gtk_widget_get_window(widget), gtk_get_current_event_device(), &x, &y, &mask);
#else
        gtk_widget_get_pointer(widget, &x, &y);
#endif
		gtk_text_view_get_iter_at_location (view, &start, x, y);
		/* we return if we are in the first line */
		if (gtk_text_iter_get_line (&start) != display_result_line+1) return FALSE;
		/* we return if its the end iterator */
		if (gtk_text_iter_is_end (&start) == TRUE) return FALSE;
		end = start;
		if (!gtk_text_iter_starts_word(&start)) gtk_text_iter_backward_word_start (&start);
		if (!gtk_text_iter_ends_word(&end)) gtk_text_iter_forward_word_end (&end);
		selected_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		/* in a rare case, we get two options as selected_text */
		if (strchr (selected_text, ' ') != NULL) return FALSE;
		/* rather a hack: last_arith is ignored as one char only gets selected as
			a word with spaces (because of iter_[back|for]ward_..). So we have to
			ignore the open brackets. */
		if (strlen (selected_text) <= 2) return FALSE;
	
		activate_menu_item (selected_text);
	}	
	else if (event->button == 2) {
		/* it's pasting selection time ...*/
		/* Get the atom corresponding to the string "STRING" */
		if (targets_atom == GDK_NONE) \
			targets_atom = gdk_atom_intern ("STRING", FALSE);
	
		/* And request the "STRING" target for the primary selection */
		gtk_selection_convert (widget, GDK_SELECTION_PRIMARY, targets_atom, \
			GDK_CURRENT_TIME);
	}		
	return FALSE;
}

/* this code is taken from the GTK 2.0 tutorial: 
 *		http://www.gtk.org/tutorial
 */
void on_textview_selection_received (GtkWidget *widget,
					GtkSelectionData *data,
					guint time,
					gpointer user_data)
{
    (void) time;
    (void) user_data;
	ui_bind_active_tab_from_widget (widget);
	/* **** IMPORTANT **** Check to see if retrieval succeeded  */
	/* occurs if we just press the middle button with no active selection */
	if (gtk_selection_data_get_length(data) < 0) return;
	
	/* Make sure we got the data in the expected form */
	if (gtk_selection_data_get_data_type(data) != GDK_SELECTION_TYPE_STRING) return;

	/* ok, we tried to avoid this in display.* but here we can't avoid using the global var. */
	display_result_feed ((char *)gtk_selection_data_get_data(data), current_status.number);

	return;
}

/*
 * display_create_text_tags - creates a tag for the result and (in-)active modules
 */

void display_create_text_tags ()
{
	int	pixels=0;
	
	/* note: wrap MUST NOT be set to none in order to justify the text! */
	if (current_status.notation == CS_ALG) pixels = 5;

	gtk_text_buffer_create_tag (buffer, "result",
		"justification", GTK_JUSTIFY_RIGHT,
		"font", prefs.result_font,
		"foreground", prefs.result_color,
		"pixels-above-lines", pixels,
		"pixels-below-lines", pixels,
		NULL);
	
	gtk_text_buffer_create_tag (buffer, "active_module",
 		"font", prefs.mod_font,
		"foreground", prefs.act_mod_color,
		"wrap-mode", GTK_WRAP_NONE,
		NULL);
	
	gtk_text_buffer_create_tag (buffer, "inactive_module",
		"font", prefs.mod_font,
		"foreground", prefs.inact_mod_color,
		"wrap-mode", GTK_WRAP_NONE,
		NULL);
	
	gtk_text_buffer_create_tag (buffer, "stack", 
		"font", prefs.stack_font,
		"foreground", prefs.stack_color,
		"justification", GTK_JUSTIFY_LEFT, 
		NULL);
}

/*
 * display_init. Call this function before any other function of this file.
 */

void display_init ()
{
    int                     char_width;
    PangoContext            *pango_context;
    PangoFontMetrics        *font_metrics;
    GtkTextTag              *tag;
    PangoTabArray           *tab_array;
    GtkTextTagTable         *tag_table;
    PangoLanguage           *language;
    PangoFontDescription    *font_desc;
    char                    *lang_name;

	current_status.calc_entry_start_new = FALSE;
	active_tab->tab_display_last_arith = ' ';
	active_tab->tab_display_brackets = 0;
	view = (GtkTextView *) gtk_builder_get_object (view_xml, "textview");
    display_set_bkg_color(prefs.bkg_color);

	buffer = gtk_text_view_get_buffer (view);
	display_create_text_tags ();
	/* compute the approx char/digit width and create a tab stops */
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, "active_module");
	/* get the approx char width */
	pango_context = gtk_widget_get_pango_context ((GtkWidget *)view);

    g_object_get(tag, "font-desc", &font_desc, "language", &lang_name, NULL);
    language = pango_language_from_string(lang_name);
    g_free(lang_name);
	font_metrics = pango_context_get_metrics(pango_context,
		font_desc,
		language);
    pango_font_description_free(font_desc);
    g_boxed_free(PANGO_TYPE_LANGUAGE, language);

	char_width = MAX (pango_font_metrics_get_approximate_char_width (font_metrics),
		pango_font_metrics_get_approximate_digit_width (font_metrics));
	tab_array = pango_tab_array_new_with_positions (1, FALSE, PANGO_TAB_LEFT, 3*char_width);
	gtk_text_view_set_tabs (view, tab_array);
	pango_tab_array_free (tab_array);
	
	/* if we display a stack, this will be updated later */
	display_result_line = 0;		
	display_set_line (CLEARED_DISPLAY, display_result_line, "result");
	
	display_update_modules ();

	/* number, angle and notation are now set in src/callbacks.c::
	 * on_scientific_mode_activate resp on_basic_mode_activate.
	 */
	
	/* was in general_functions::apply_preferences before, now go here*/	
	display_update_tags ();
	display_set_bkg_color (prefs.bkg_color);
}

/*
 * display_module_arith_label_update - code for the "display last arithmetic  
 * operation" display module. put operation in static current_char to survive
 * an apply f preferences.
 */

void display_module_arith_label_update (char operation)
{
	GtkTextMark	*this_mark;
	GtkTextIter	start, end;
	
	if ((prefs.vis_arith == FALSE) || (active_tab->tab_mode != SCIENTIFIC_MODE))
		return;
	if (strchr ("()", operation) != NULL) return;
	
	if ((this_mark = gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_ARITH)) == NULL) \
		return;
	
	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	end = start;
	gtk_text_iter_forward_chars (&end, 3);
	gtk_text_buffer_delete (buffer, &start, &end);
	
	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	if (operation != NOP) active_tab->tab_display_last_arith = operation;

	gtk_text_buffer_insert_with_tags_by_name (buffer, &start, \
		g_strdup_printf ("\t%c\t", active_tab->tab_display_last_arith), -1, "active_module", NULL);
}

/*
 * display_module_bracket_label_update - code for the "display number of open
 * brackets" display module
 */

int display_module_bracket_label_update (int option)
{
	GtkTextMark	*this_mark;
	GtkTextIter	start, end;
	int		forward_count=2;
	char 		*string;
	
	switch (option) {
		case ONE_MORE:
			active_tab->tab_display_brackets++;
			if (active_tab->tab_display_brackets > 0)
				forward_count = display_bracket_forward_count (active_tab->tab_display_brackets);
			break;
		case ONE_LESS:
			if (active_tab->tab_display_brackets > 0)
				forward_count = display_bracket_forward_count (active_tab->tab_display_brackets);
			active_tab->tab_display_brackets--;
			break;
		case RESET:
			if (active_tab->tab_display_brackets > 0)
				forward_count = display_bracket_forward_count (active_tab->tab_display_brackets);
			active_tab->tab_display_brackets = 0;
			break;
		case GET:
			/* doing this here to not touch the display */
			return active_tab->tab_display_brackets;
		case NOP:
			if (active_tab->tab_display_brackets > 0)
				forward_count = display_bracket_forward_count (active_tab->tab_display_brackets);
			break;
	}
	if (!display_bracket_module_should_render ())
		return active_tab->tab_display_brackets;
	
	if ((this_mark = gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_BRACKET)) == NULL) \
		return active_tab->tab_display_brackets;
	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	end = start;
	gtk_text_iter_forward_chars (&end, forward_count);
	gtk_text_buffer_delete (buffer, &start, &end);

	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);

	if (active_tab->tab_display_brackets > 0) {
		string = g_strdup_printf ("\t(%i\t", active_tab->tab_display_brackets);
		gtk_text_buffer_insert_with_tags_by_name (buffer, &start, string, \
			-1, "active_module", NULL);
		g_free (string);	
	}
	else {
		active_tab->tab_display_brackets = 0;
		gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\t\t", \
			-1, "active_module", NULL);
	}
	return active_tab->tab_display_brackets;
}

/*
 * display_module_base_create - start_mark is the mark where this module starts.
 * Therefore, the single labels are inserted starting with the last (as the mark
 * is at the beginning!). A simple gtk_text_buffer_insert_with_tags_by_name
 * doesn't work as we insert in an already tagged region an tag priority rules out.
 * solving this matter with setting priorities is not possible, so we have to remove
 * all tags and apply the tag we want.
 */

void display_module_base_create (char **module_label, char *mark_name, int active_index)
{
	int		label_counter=0, counter;
	GtkTextIter 	start, end;
	GtkTextMark	*this_mark;

	if ((this_mark = gtk_text_buffer_get_mark (buffer, mark_name)) == NULL) return;
	while (module_label[label_counter] != NULL) label_counter++;
	for (counter = (label_counter-1); counter >= 0; counter--) {
		gtk_text_buffer_get_iter_at_mark (buffer, &end, this_mark);
		gtk_text_buffer_insert (buffer, &end, module_label[counter], -1);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
		gtk_text_buffer_remove_all_tags (buffer, &start, &end);
		if (counter == active_index) {
			gtk_text_buffer_apply_tag_by_name (buffer, "active_module", &start, &end);
		} else {
			gtk_text_buffer_apply_tag_by_name (buffer, "inactive_module", &start, &end);	
		}
	}
}

/*
 * display_module_leading_spaces - the distance between two modules is done with 
 * spaces. when inserting this spaces, we have to pay attention that it really 
 * is between two modules (e.g. we want no spaces after the last module) and 
 * that we don't lose the text mark pointing to the start of a module.
 */

void display_module_leading_spaces (char *mark_name, gboolean leading_spaces)
{	
	GtkTextIter	iter;
	GtkTextMark	*start_mark;
	
	if (leading_spaces == FALSE) return;
	
	if ((start_mark = gtk_text_buffer_get_mark (buffer, mark_name)) == NULL) return;
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, start_mark);
	/* insert spaces */
	gtk_text_buffer_insert (buffer, &iter, DISPLAY_MODULES_DELIM, -1);
	/* update text mark */
	gtk_text_buffer_delete_mark (buffer, start_mark);
	gtk_text_buffer_create_mark (buffer, mark_name, &iter, TRUE);
}

/* display_module_number_activate.
 */

void display_module_number_activate (int number_base)
{
	activate_menu_item (number_mod_labels[number_base]);
}

/* display_module_angle_activate.
 */

void display_module_angle_activate (int angle_unit)
{
	activate_menu_item (angle_mod_labels[angle_unit]);
}

/* display_module_notation_activate.
 */

void display_module_notation_activate (int mode)
{
	activate_menu_item (notation_mod_labels[mode]);
}

void display_get_line_end_iter (GtkTextBuffer *b, int line_index, GtkTextIter *end)
{
	gtk_text_buffer_get_iter_at_line (b, end, line_index);
	gtk_text_iter_forward_to_line_end (end);
}

void display_get_line_iters (GtkTextBuffer *b, int line_index, GtkTextIter *start, GtkTextIter *end)
{	
	gtk_text_buffer_get_iter_at_line (b, start, line_index);
	*end = *start;
	gtk_text_iter_forward_to_line_end (end);
}

/*
 * display_delete_line - deletes given line. start points to the place where we
 *	deleted.
 */

void display_delete_line (GtkTextBuffer *b, int line_index, GtkTextIter *iter)
{
	GtkTextIter start, end;
	
	if (gtk_text_buffer_get_line_count (b) <= line_index) {
		fprintf (stderr, _("[%s] Line_index exceeds valid range in function \"display_delete_line\". %s\n"), PROG_NAME, BUG_REPORT);
		return;
	}
	display_get_line_iters (b, line_index, &start, &end);
	gtk_text_buffer_delete (buffer, &start, &end);
	*iter = start;
}

/*
 * display_create_modules - the display modules compose the second line of then
 *	display. available modules:
 *		- change number base
 *		- change angle base
 *		- change notation
 */

void display_update_modules ()
{
	GtkTextIter	start, end;
	gboolean	first_module = TRUE;
	gint		line_count;
	
	if (active_tab && active_tab->tab_mode == PAPER_MODE) return;
	
	line_count = gtk_text_buffer_get_line_count (buffer);
	if (line_count <= 0) return;
	if ((display_result_line + 1) < line_count)
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line + 1);
	else
		gtk_text_buffer_get_end_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);
	gtk_text_buffer_delete (buffer, &start, &end);
	
	if (active_tab->tab_mode == BASIC_MODE) return;
	
	/* change number base */
	if (active_tab->tab_vis_number == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_NUMBER) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_NUMBER);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_NUMBER, &start, TRUE);
		display_module_base_create (number_mod_labels, DISPLAY_MARK_NUMBER, current_status.number);
		display_module_leading_spaces (DISPLAY_MARK_NUMBER, !first_module);
		first_module = FALSE;
	}
	
	/* angle */
	if (active_tab->tab_vis_angle == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_ANGLE) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_ANGLE);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_ANGLE, &start, TRUE);
		display_module_base_create (angle_mod_labels, DISPLAY_MARK_ANGLE, current_status.angle);
		display_module_leading_spaces (DISPLAY_MARK_ANGLE, !first_module);
		first_module = FALSE;
	}
	
	/* notation */
	if (active_tab->tab_vis_notation == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_NOTATION) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_NOTATION);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_NOTATION, &start, TRUE);
		display_module_base_create (notation_mod_labels, DISPLAY_MARK_NOTATION, current_status.notation);
		display_module_leading_spaces (DISPLAY_MARK_NOTATION, !first_module);
		first_module = FALSE;
	}	
	
	/* last arithmetic operation */
	if (prefs.vis_arith == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_ARITH) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_ARITH);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_ARITH, &start, TRUE);
		display_module_arith_label_update (NOP);
		display_module_leading_spaces (DISPLAY_MARK_ARITH, !first_module);
		first_module = FALSE;
	}
	
	/* number of open brackets */
	if (display_bracket_module_should_render ()) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_BRACKET) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_BRACKET);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_BRACKET, &start, TRUE);
		display_module_bracket_label_update (NOP);
		display_module_leading_spaces (DISPLAY_MARK_BRACKET, !first_module);
		first_module = FALSE;
	}
}

/*
 * display_module_base_delete - delete the given module at given text mark.
 * used for display_change_option
 */

void display_module_base_delete (char *mark_name, char **text)
{
	int		counter=0;
	gsize		length=0;
	GtkTextIter	start, end;
	GtkTextMark	*this_mark;
	gint		forward_chars;
	
	if ((this_mark = gtk_text_buffer_get_mark (buffer, mark_name)) == NULL) return;
		
	while (text[counter] != NULL) {
		length += strlen(text[counter]);
		counter++;
	}
	forward_chars = (length > (gsize) G_MAXINT) ? G_MAXINT : (gint) length;

	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	end = start;
	gtk_text_iter_forward_chars (&end, forward_chars);
	gtk_text_buffer_delete (buffer, &start, &end);
}


/*
 * display_change_option - updates the display in case of number/angle/notation
 * changes. we need old_status to get display value in correct base.
 * The last function in the signal handling cascade of changing base etc.
 */

void display_change_option (int old_status, int new_status, int opt_group)
{
	char	*current_display_value = NULL;
	char	**stack = NULL;
	int 	counter;
	char	*converted = NULL;
	
	switch (opt_group) {
		case DISPLAY_OPT_NUMBER:
			update_active_buttons (new_status, current_status.notation);
			current_display_value = display_result_get ();
			stack = display_stack_get_yzt ();
			converted = display_convert_base_string (current_display_value, old_status, new_status);
			display_result_set (converted, TRUE);
			g_free (converted);
			for (counter = 0; counter < display_result_line; counter++) {
				char *stack_converted = display_convert_base_string (stack[counter], old_status, new_status);
				g_free (stack[counter]);
				stack[counter] = stack_converted;
			}
			display_stack_set_yzt (stack);
			for (counter = 0; counter < display_result_line; counter++) g_free (stack[counter]);
			g_free (stack);
			g_free (current_display_value);
			if ((active_tab->tab_vis_number) && (active_tab->tab_mode == SCIENTIFIC_MODE)) {
				display_module_base_delete (DISPLAY_MARK_NUMBER, number_mod_labels);
				display_module_base_create (number_mod_labels, DISPLAY_MARK_NUMBER, new_status);
			}
			break;
		case DISPLAY_OPT_ANGLE:
			if ((active_tab->tab_vis_angle) && (active_tab->tab_mode == SCIENTIFIC_MODE)){
				display_module_base_delete (DISPLAY_MARK_ANGLE, angle_mod_labels);
				display_module_base_create (angle_mod_labels, DISPLAY_MARK_ANGLE, new_status);
			}
			break;
		case DISPLAY_OPT_NOTATION:
			update_active_buttons (current_status.number, new_status);
			if ((active_tab->tab_vis_notation) && (active_tab->tab_mode == SCIENTIFIC_MODE)){
				display_module_base_delete (DISPLAY_MARK_NOTATION, notation_mod_labels);
				display_module_base_create (notation_mod_labels, DISPLAY_MARK_NOTATION, new_status);
			}
			break;
		default:
			error_message (_("[%s] unknown display option in function \"display_change_option\""));
	}
}

/*
 * display_set_bkg_color - change the background color of the text view
 */

void display_set_bkg_color (char *color_string)
{
	gchar *css;
	if (active_tab->tab_mode == PAPER_MODE) return;
	if (view) {
		css = g_strdup_printf ("textview, textview text { background-color: %s; }",
			color_string ? color_string : "#ffffff");
		display_widget_css_set (GTK_WIDGET(view), css, "talculator-display-css-provider");
		g_free (css);
	}
}

/*
 * if we change any settings for result/module fonts this function updates the
 * all the tags.
 */

void display_update_tags ()
{
	GtkTextIter		start, end;
	GtkTextTagTable		*tag_table;
	GtkTextTag 		*tag;
	
	if (active_tab->tab_mode == PAPER_MODE) return;
	/* remove all tags from tag_table, so we can define the new tags */
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, "result");
	gtk_text_tag_table_remove (tag_table, tag);
	tag = gtk_text_tag_table_lookup (tag_table, "inactive_module");
	gtk_text_tag_table_remove (tag_table, tag);
	tag = gtk_text_tag_table_lookup (tag_table, "active_module");
	gtk_text_tag_table_remove (tag_table, tag);
	tag = gtk_text_tag_table_lookup (tag_table, "stack");
	gtk_text_tag_table_remove (tag_table, tag);
	
	/* create the tags again, up2date */
	display_create_text_tags ();
	
	/* apply to stack */
	if (display_result_line > 0) {
		gtk_text_buffer_get_iter_at_line (buffer, &start, 0);
		display_get_line_end_iter (buffer, display_result_line - 1, &end);
		gtk_text_buffer_apply_tag_by_name (buffer, "stack", &start, &end);
	}

	/* apply to result */
	display_get_line_iters (buffer, display_result_line, &start, &end);
	gtk_text_buffer_apply_tag_by_name (buffer, "result", &start, &end);
	
	/* apply to modules */
	display_update_modules ();
}

/******************
 *** END of display CONFIGuration code.
 *** from here on display manipulation code.
 ******************/

/* these is the most basic routine. all functions here finally result in a call
 * to this.
 */
static void display_set_line (char *string, int line, char *tag)
{
	char 		*separator_string;
	GtkTextIter	start;

	/* at first clear the result field */
	display_delete_line (buffer, line, &start);
	
	separator_string = string_add_separator(string, get_sep(current_status.number), 
		get_sep_length(current_status.number), get_sep_char(current_status.number), dec_point[0]);
	/* DISPLAY RESULT MODIFIED */
	gtk_text_buffer_insert_with_tags_by_name (buffer, &start, separator_string, -1, tag, NULL);
	g_free (separator_string);
	if (line == display_result_line) {
		gsize display_len = strlen (string);
		display_result_counter = (display_len > (gsize) G_MAXINT) ? G_MAXINT : (gint) display_len;
		/* this is some cosmetics. try to keep counter up2date */
		if (strchr (string, 'e') != NULL) {
				gsize prefix_len = (gsize) (strchr(string, 'e') - string + 1);
				gint prefix_chars = (prefix_len > (gsize) G_MAXINT) ? G_MAXINT : (gint) prefix_len;
				display_result_counter -= prefix_chars;
			display_result_counter += get_display_number_length(current_status.number) - DISPLAY_RESULT_E_LENGTH - 1;
		}
		else if (strchr (string, dec_point[0]) != NULL) display_result_counter--;
	}
}

/*
 * display_result_add_digit. appends the given digit to the current entry, 
 * handles zeros, decimal points. call e.g. with *(gtk_button_get_label (button))
 */
void display_result_add_digit (char digit, int number_base_status)
{
    (void) number_base_status;
	char			*display_string=NULL, *new_display_string=NULL;

	/* fool the following code */
	if (current_status.calc_entry_start_new == TRUE) {
		display_result_set (CLEARED_DISPLAY, TRUE);
		current_status.calc_entry_start_new = FALSE;
		display_result_counter = 1;
	}
	
	display_string = display_result_get();
	if (!display_string) return;
	
	if (digit == dec_point[0]) {
		/* don't manipulate display_result_counter here! */
		if (strlen (display_string) == 0)
			new_display_string = g_strdup_printf ("0%c", digit);
		else if ((strchr (display_string, dec_point[0]) == NULL) && \
					(strchr (display_string, 'e') == NULL))
			new_display_string = g_strdup_printf ("%s%c", display_string, digit);
	} else {
		/* replace "0" on display with new digit */
		if (strcmp (display_string, CLEARED_DISPLAY) == 0) new_display_string = g_strdup_printf ("%c", digit);
		else if (display_result_counter < get_display_number_length(current_status.number)) {
			new_display_string = g_strdup_printf ("%s%c", display_string, digit);
			/* increment counter only in this if directive as above the counter remains 1! */
			display_result_counter++;
		}
	}
	if (new_display_string) display_result_set (new_display_string, TRUE);
	if (display_string) g_free (display_string);
	if (new_display_string) g_free (new_display_string);
}

void display_result_set (char *string_value, int update_result_counter)
{
	const char *value = string_value ? string_value : CLEARED_DISPLAY;

	if (active_tab) {
		g_free (active_tab->tab_display_value);
		active_tab->tab_display_value = g_strdup (value);
	}
	if (active_tab->tab_mode == PAPER_MODE) {
		GtkWidget *paper_entry = GTK_WIDGET(gtk_builder_get_object (view_xml, "paper_entry"));
		if (paper_entry && GTK_IS_ENTRY(paper_entry))
			gtk_entry_set_text (GTK_ENTRY(paper_entry), value);
		return;
	}

	current_status.allow_arith_op = TRUE;
	display_module_arith_label_update (' ');
	display_set_line ((char *)value, display_result_line, "result");
	if (!update_result_counter) return;
}

void display_result_feed (char *string, int number_base_status)
{
	gsize	counter;
	gsize	string_len;

	string_len = strlen (string);
    gboolean toggleSign = current_status.calc_entry_start_new && (string_len > 0) && (*string == '-');

	    for (counter = 0; counter < string_len; counter++) {
	    	char digit_char = string[counter];
	        switch (string[counter]) {
	        case '-': {
	            /* this only applies if we just had "EE" before */
	            char *dline = display_result_get();
	            if (!dline) break;
	            if (strlen(dline) < 3) {
				g_free (dline);
				break;
			}
	            if (strncmp(&dline[strlen(dline)-2], "e+", 2) == 0) display_result_toggle_sign(NULL);
			g_free (dline);
	            break;
	        }
        case 'e':
        case 'E':
			/* In decimal mode, call display_append_e, otherwise, run into 
			 * default handler. simon20130214
			 */
			if (current_status.number == CS_DEC) {
				display_append_e(NULL);
				break;
			}
			/* fall through */
	        default:
	            if ((digit_char >= 'a') && (digit_char <= 'z'))
	            	digit_char = (char) (digit_char - ('a' - 'A'));
	            if (is_valid_number(current_status.number, digit_char))
	                display_result_add_digit(digit_char, number_base_status);
	        }
        if (toggleSign && counter==1) display_result_toggle_sign(NULL);
	}
//
}

/*
 * display_result_get_line. The only function calling a gtk_text_buffer_get_*
 *	function directly.
 */
static char *display_get_line (int line_nr)
{
	GtkTextIter 	start, end;
	
	if (!active_tab) return g_strdup (CLEARED_DISPLAY);
	if (active_tab->tab_mode == PAPER_MODE) {
		return g_strdup (active_tab->tab_display_value ?
			active_tab->tab_display_value : CLEARED_DISPLAY);
	}
	
	display_get_line_iters (buffer, line_nr, &start, &end);
	/* DISPLAY RESULT GET */
	return string_del_separator(gtk_text_buffer_get_text (buffer, &start, &end, TRUE), get_sep_char(current_status.number));
}

/* display_result_get. a simplfied call to display_result_get_line
 */
char *display_result_get ()
{
	return display_get_line (display_result_line);
}

void display_append_e (GtkToggleButton *button)
{
    (void) button;
	char *display_line;

	/* we have kind of a shortcut. we don't set to 0e+ but 1e+ */
	if (current_status.number != CS_DEC) return;
	display_line = display_result_get();
	if (!display_line) return;
	if ((current_status.calc_entry_start_new == FALSE) &&
		(strcmp (display_line, "0") != 0)) {
		if (strstr (display_line, "e") == NULL) {
			/* DISPLAY RESULT MODIFIED
			display_get_line_end_iter (buffer, display_result_line, &end);
			gtk_text_buffer_insert_with_tags_by_name (buffer, &end, "e+", -1, "result", NULL);
			*/
			char *display_string = g_strdup_printf ("%se+", display_line);
				display_result_set (display_string, FALSE);
				g_free (display_string);
			}
		} else {
			display_result_set ("1e+", TRUE);
			current_status.calc_entry_start_new = FALSE;
		}
	g_free (display_line);
	display_result_counter = get_display_number_length(current_status.number) - DISPLAY_RESULT_E_LENGTH;
}

void display_result_toggle_sign (GtkToggleButton *button)
{
    (void) button;
	char			*result_field, *e_pointer;

	if (current_status.number != CS_DEC) return;
	result_field = display_result_get();
	if (!result_field) return;
	/* if there is no e? we toggle the leading sign, otherwise the sign after e */
	if ((e_pointer = strchr (result_field, 'e')) == NULL || current_status.calc_entry_start_new) {
		if (*result_field == '-') 
		{
			/* FIXED on 20140108 by simon. g_stpcpy calls stpcpy, if available,
			* and for stpcpy, the memory must not overlap.
			*/
			// g_stpcpy (result_field, result_field + sizeof(char));
			memmove(result_field, result_field + sizeof(char), strlen(result_field));
		}
		else if (strcmp (result_field, "0") != 0)
		{
			result_field = g_strdup_printf ("-%s", result_field);
		}
		} else {
			if (*(++e_pointer) == '-') *e_pointer = '+';
			else *e_pointer = '-';
		}
		display_result_set (result_field, FALSE);
		g_free (result_field);
}

/* display_result_backspace - deletes the tail of the display.
 *		sets the display with display_result_set => no additional manipulation of 
 *		display_result_counter
 *		necessary
 */

void display_result_backspace (int number_base_status)
{															
    (void) number_base_status;
	char	*current_entry;

		if (current_status.calc_entry_start_new == TRUE) {
			current_status.calc_entry_start_new = FALSE;
			display_result_set (CLEARED_DISPLAY, TRUE);
		} else {
			current_entry = display_result_get();
			if (!current_entry) return;
			/* to avoid an empty/senseless result field */
			if (strlen(current_entry) == 0) current_entry[0] = '0';
			else if (strlen(current_entry) == 1) current_entry[0] = '0';
			else if ((strlen(current_entry) == 2) && (*current_entry == '-')) current_entry[0] = '0';
			else if (current_entry[strlen(current_entry) - 2] == 'e') current_entry[strlen(current_entry) - 2] = '\0';
			else current_entry[strlen(current_entry) - 1] = '\0';
			display_result_set (current_entry, TRUE);
			g_free (current_entry);
		}
}

/* display_result_getset. kind of display result redraw. use this to get and
 * set the display, it frees the return string of display_result_get()
 */

void display_result_getset ()
{
	char	*result;
	char	**stack;
	int	i;
	
	if (active_tab->tab_mode == PAPER_MODE) return;
	
	result = display_result_get();
	stack = display_stack_get_yzt();
	display_result_set(result, FALSE);
	display_stack_set_yzt(stack);
	g_free(result);
	for (i = 0; i < 3; i++) g_free (stack[i]);
	g_free(stack);
}

/*
 * STACK functions manipulating the display
 */
void display_stack_create ()
{
	int 		counter;
	GtkTextIter	start;
	
	if (display_result_line > 0) return;
	display_result_line = 3;
	for (counter = 0; counter < display_result_line; counter++) {
		gtk_text_buffer_get_iter_at_line (buffer, &start, 0);
		gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "0\n",
			-1, "stack", NULL);
	}
}

void display_stack_remove ()
{
	GtkTextIter	start, end;
	
	gtk_text_buffer_get_iter_at_line (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_line (buffer, &end, display_result_line);
	gtk_text_buffer_delete (buffer, &start, &end);
	display_result_line = 0;
}

/*
 * display_stack_set_yzt - stack in calc_basic IS NOT modified.
 */

void display_stack_set_yzt (char **stack)
{
	int		counter;
	
	for (counter = 0; counter < display_result_line; counter++) {
		display_set_line (stack[display_result_line - counter - 1], 
			counter, "stack");
	}
}

/*
 * display_stack_get_yzt - returns a three-sized char array with 1:1 copies
 *	of the current displayed stack.
 */

char **display_stack_get_yzt ()
{
	char 		**stack;
	int 		counter;
	
	stack = (char **) g_malloc (3* sizeof (char *));
	if (display_result_line <= 0) {
		for (counter = 0; counter < 3; counter++) stack[counter] = g_strdup (CLEARED_DISPLAY);
		return stack;
	}
	for (counter = 0; counter < 3; counter++) {
		/*  DISPLAY RESULT GET 
		display_get_line_iters (buffer, display_result_line - counter - 1, &start, &end);
		stack[counter] = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
		*/
		stack[counter] = display_get_line(display_result_line - counter - 1);
	}
	return stack;
}
