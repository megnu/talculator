/*
 *  config_file.c - manages config file access.
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
 
/* to add a new preference, add it to 
	- s_preferences in config_file.h
	- #define DEFAULTS in config_file.h
	- to config_file_get_default_prefs
	- to struct prefs_list (increase array size!)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* i18n */

#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#include "config_file.h"
#include "general_functions.h"

extern s_preferences prefs;
static s_prefs_entry prefs_list[53] = {
	{"display_bkg_color", &(prefs.bkg_color), STRING, "prefs_bkg_color_button", set_button_color},
	{"display_result_font", &(prefs.result_font), STRING, "prefs_result_font", set_button_font},
	{"display_result_color", &(prefs.result_color), STRING, "prefs_result_color_button", set_button_color},
	{"display_stack_font", &(prefs.stack_font), STRING, "prefs_stack_font", set_button_font},
	{"display_stack_color", &(prefs.stack_color), STRING, "prefs_stack_color_button", set_button_color},
	{"display_module_font", &(prefs.mod_font), STRING, "prefs_mod_font", set_button_font},
	{"display_module_active_color", &(prefs.act_mod_color), STRING, "prefs_act_mod_color_button", set_button_color},
	{"display_module_inactive_color", &(prefs.inact_mod_color), STRING, "prefs_inact_mod_color_button", set_button_color},
	{"display_module_number", &(prefs.vis_number), BOOLEAN, "prefs_vis_number", set_checkbutton},
/*10*/	{"display_module_angle", &(prefs.vis_angle), BOOLEAN, "prefs_vis_angle", set_checkbutton},
	{"display_module_notation", &(prefs.vis_notation), BOOLEAN, "prefs_vis_notation", set_checkbutton},
	{"display_module_arith", &(prefs.vis_arith), BOOLEAN, "prefs_vis_arith", set_checkbutton},
	{"display_module_open", &(prefs.vis_bracket), BOOLEAN, "prefs_vis_bracket", set_checkbutton},
	{"custom_button_font", &(prefs.custom_button_font), BOOLEAN, "prefs_custom_button_font", set_checkbutton},
	{"button_font", &(prefs.button_font), STRING, "prefs_button_font", set_button_font},
	{"button_width", &(prefs.button_width), INTEGER, "prefs_button_width", set_spinbutton},
	{"button_height", &(prefs.button_height), INTEGER, "prefs_button_height", set_spinbutton},
	{"function_button_group", &(prefs.vis_funcs), BOOLEAN, NULL, NULL},
	{"dispctrl_button_group", &(prefs.vis_dispctrl), BOOLEAN, NULL, NULL},
/*20*/	{"logic_button_group", &(prefs.vis_logic), BOOLEAN, NULL, NULL},
	{"standard_button_group", &(prefs.vis_standard), BOOLEAN, NULL, NULL},
	{"mode", &(prefs.mode), INTEGER, NULL, NULL},
	{"dec_sep", &(prefs.dec_sep), BOOLEAN, "prefs_dec_sep", set_checkbutton},
	{"dec_sep_length", &(prefs.dec_sep_length), INTEGER, "prefs_dec_sep_length", set_spinbutton},
	{"dec_sep_char", &(prefs.dec_sep_char), STRING, "prefs_dec_sep_char", set_entry},
	{"hex_bits", &(prefs.hex_bits), INTEGER, "prefs_hex_bits", set_spinbutton},
	{"hex_signed", &(prefs.hex_signed), BOOLEAN, "prefs_hex_signed", set_checkbutton},
	{"hex_sep", &(prefs.hex_sep), BOOLEAN, "prefs_hex_sep", set_checkbutton},
	{"hex_sep_length", &(prefs.hex_sep_length), INTEGER, "prefs_hex_sep_length", set_spinbutton},
/*30*/	{"hex_sep_char", &(prefs.hex_sep_char), STRING, "prefs_hex_sep_char", set_entry},
	{"oct_bits", &(prefs.oct_bits), INTEGER, "prefs_oct_bits", set_spinbutton},
	{"oct_sep", &(prefs.oct_sep), BOOLEAN, "prefs_oct_sep", set_checkbutton},
	{"oct_sep_length", &(prefs.oct_sep_length), INTEGER, "prefs_oct_sep_length", set_spinbutton},
	{"oct_sep_char", &(prefs.oct_sep_char), STRING, "prefs_oct_sep_char", set_entry},
	{"bin_bits", &(prefs.bin_bits), INTEGER, "prefs_bin_bits", set_spinbutton},
	{"bin_signed", &(prefs.bin_signed), BOOLEAN, "prefs_bin_signed", set_checkbutton},
/*40*/	{"bin_sep", &(prefs.bin_sep), BOOLEAN, "prefs_bin_sep", set_checkbutton},
	{"bin_sep_length", &(prefs.bin_sep_length), INTEGER, "prefs_bin_sep_length", set_spinbutton},
	{"bin_sep_char", &(prefs.bin_sep_char), STRING, "prefs_bin_sep_char", set_entry},
	{"default_number_base", &(prefs.def_number), INTEGER, NULL, NULL},
	{"default_angle_base", &(prefs.def_angle), INTEGER, NULL, NULL},
	{"default_notation_mode", &(prefs.def_notation), INTEGER, NULL, NULL},
	{"stack_size", &(prefs.stack_size), INTEGER, NULL, set_stacksize},
	{"remember_session", &(prefs.rem_display), BOOLEAN, "prefs_rem_display", set_checkbutton},
/*50*/
	{"show_menu_bar", &(prefs.show_menu), BOOLEAN, "prefs_show_menu", set_checkbutton},
	{NULL, NULL, 0, NULL, NULL}
};
static s_constant *cf_constant=NULL;
static s_user_function *cf_user_function=NULL;
static s_session_state cf_session_state = {0};
#define TALC_SESSION_MAX_ITEMS 256

static void session_tab_clear (s_session_tab_state *tab)
{
	int i;

	if (!tab) return;
	if (tab->display_value) g_free (tab->display_value);
	tab->display_value = NULL;
	if (tab->rpn_stack) {
		for (i = 0; i < tab->rpn_stack_len; i++) {
			if (tab->rpn_stack[i]) g_free (tab->rpn_stack[i]);
		}
		g_free (tab->rpn_stack);
	}
	tab->rpn_stack = NULL;
	tab->rpn_stack_len = 0;
	if (tab->mem_values) {
		for (i = 0; i < tab->memory_len; i++) {
			if (tab->mem_values[i]) g_free (tab->mem_values[i]);
		}
		g_free (tab->mem_values);
	}
	tab->mem_values = NULL;
	tab->memory_len = 0;
	tab->mode = BASIC_MODE;
	tab->number = CS_DEC;
	tab->angle = CS_RAD;
	tab->notation = CS_ALG;
}

void config_file_session_state_clear (s_session_state *state)
{
	int i;

	if (!state) return;
	for (i = 0; i < TALC_MAX_SESSION_TABS; i++) session_tab_clear (&state->tabs[i]);
	state->tab_count = 0;
	state->active_tab = 0;
}

static void config_file_session_state_copy (s_session_state *dst, const s_session_state *src)
{
	int i, j;

	if (!dst) return;
	config_file_session_state_clear (dst);
	if (!src) return;
	dst->tab_count = src->tab_count;
	dst->active_tab = src->active_tab;
	if (dst->tab_count < 0) dst->tab_count = 0;
	if (dst->tab_count > TALC_MAX_SESSION_TABS) dst->tab_count = TALC_MAX_SESSION_TABS;
	if (dst->active_tab < 0) dst->active_tab = 0;
	if (dst->active_tab >= dst->tab_count && dst->tab_count > 0) dst->active_tab = dst->tab_count - 1;
	for (i = 0; i < dst->tab_count; i++) {
		dst->tabs[i].mode = src->tabs[i].mode;
		dst->tabs[i].number = src->tabs[i].number;
		dst->tabs[i].angle = src->tabs[i].angle;
		dst->tabs[i].notation = src->tabs[i].notation;
		dst->tabs[i].display_value = g_strdup (src->tabs[i].display_value ? src->tabs[i].display_value : CLEARED_DISPLAY);
		dst->tabs[i].rpn_stack_len = src->tabs[i].rpn_stack_len;
		if (dst->tabs[i].rpn_stack_len < 0) dst->tabs[i].rpn_stack_len = 0;
		if (dst->tabs[i].rpn_stack_len > 0) {
			dst->tabs[i].rpn_stack = g_new0 (char *, (gsize) dst->tabs[i].rpn_stack_len);
			for (j = 0; j < dst->tabs[i].rpn_stack_len; j++) {
				dst->tabs[i].rpn_stack[j] = g_strdup (src->tabs[i].rpn_stack && src->tabs[i].rpn_stack[j] ? src->tabs[i].rpn_stack[j] : CLEARED_DISPLAY);
			}
		}
		dst->tabs[i].memory_len = src->tabs[i].memory_len;
		if (dst->tabs[i].memory_len < 0) dst->tabs[i].memory_len = 0;
		if (dst->tabs[i].memory_len > 0) {
			dst->tabs[i].mem_values = g_new0 (char *, (gsize) dst->tabs[i].memory_len);
			for (j = 0; j < dst->tabs[i].memory_len; j++) {
				dst->tabs[i].mem_values[j] = g_strdup (src->tabs[i].mem_values && src->tabs[i].mem_values[j] ? src->tabs[i].mem_values[j] : CLEARED_DISPLAY);
			}
		}
	}
}

const s_session_state *config_file_get_session_state (void)
{
	return &cf_session_state;
}

void config_file_set_session_state (const s_session_state *state)
{
	config_file_session_state_copy (&cf_session_state, state);
}

static char *config_string_unquote_dup (const char *value)
{
	gsize len;

	if (!value) return g_strdup ("");
	len = strlen (value);
	if (len >= 2 && value[0] == '\"' && value[len - 1] == '\"') {
		return g_strndup (value + 1, len - 2);
	}
	return g_strdup (value);
}

static gboolean config_file_set_session_entry (const char *key, const char *value)
{
	int tab_idx = -1;
	int item_idx = -1;
	int n = 0;

	if (!key || !value) return FALSE;
	if (strcmp (key, "session_tab_count") == 0) {
		cf_session_state.tab_count = (int) g_ascii_strtod (value, NULL);
		if (cf_session_state.tab_count < 0) cf_session_state.tab_count = 0;
		if (cf_session_state.tab_count > TALC_MAX_SESSION_TABS) cf_session_state.tab_count = TALC_MAX_SESSION_TABS;
		return TRUE;
	}
	if (strcmp (key, "session_active_tab") == 0) {
		cf_session_state.active_tab = (int) g_ascii_strtod (value, NULL);
		if (cf_session_state.active_tab < 0) cf_session_state.active_tab = 0;
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_mode%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		cf_session_state.tabs[tab_idx].mode = (int) g_ascii_strtod (value, NULL);
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_number%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		cf_session_state.tabs[tab_idx].number = (int) g_ascii_strtod (value, NULL);
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_angle%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		cf_session_state.tabs[tab_idx].angle = (int) g_ascii_strtod (value, NULL);
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_notation%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		cf_session_state.tabs[tab_idx].notation = (int) g_ascii_strtod (value, NULL);
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_display%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		if (cf_session_state.tabs[tab_idx].display_value) g_free (cf_session_state.tabs[tab_idx].display_value);
		cf_session_state.tabs[tab_idx].display_value = config_string_unquote_dup (value);
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_rpn_len%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		int i, len;
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		if (cf_session_state.tabs[tab_idx].rpn_stack) {
			for (i = 0; i < cf_session_state.tabs[tab_idx].rpn_stack_len; i++) {
				if (cf_session_state.tabs[tab_idx].rpn_stack[i]) g_free (cf_session_state.tabs[tab_idx].rpn_stack[i]);
			}
			g_free (cf_session_state.tabs[tab_idx].rpn_stack);
		}
		len = (int) g_ascii_strtod (value, NULL);
		if (len < 0) len = 0;
		if (len > TALC_SESSION_MAX_ITEMS) len = TALC_SESSION_MAX_ITEMS;
		cf_session_state.tabs[tab_idx].rpn_stack_len = len;
		cf_session_state.tabs[tab_idx].rpn_stack = len > 0 ? g_new0 (char *, (gsize) len) : NULL;
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_mem_len%n", &tab_idx, &n) == 1 && key[n] == '\0') {
		int i, len;
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		if (cf_session_state.tabs[tab_idx].mem_values) {
			for (i = 0; i < cf_session_state.tabs[tab_idx].memory_len; i++) {
				if (cf_session_state.tabs[tab_idx].mem_values[i]) g_free (cf_session_state.tabs[tab_idx].mem_values[i]);
			}
			g_free (cf_session_state.tabs[tab_idx].mem_values);
		}
		len = (int) g_ascii_strtod (value, NULL);
		if (len < 0) len = 0;
		if (len > TALC_SESSION_MAX_ITEMS) len = TALC_SESSION_MAX_ITEMS;
		cf_session_state.tabs[tab_idx].memory_len = len;
		cf_session_state.tabs[tab_idx].mem_values = len > 0 ? g_new0 (char *, (gsize) len) : NULL;
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_rpn_%d%n", &tab_idx, &item_idx, &n) == 2 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		if (item_idx < 0 || item_idx >= cf_session_state.tabs[tab_idx].rpn_stack_len) return TRUE;
		if (cf_session_state.tabs[tab_idx].rpn_stack[item_idx]) g_free (cf_session_state.tabs[tab_idx].rpn_stack[item_idx]);
		cf_session_state.tabs[tab_idx].rpn_stack[item_idx] = config_string_unquote_dup (value);
		return TRUE;
	}
	if (sscanf (key, "session_tab%d_mem_%d%n", &tab_idx, &item_idx, &n) == 2 && key[n] == '\0') {
		if (tab_idx < 0 || tab_idx >= TALC_MAX_SESSION_TABS) return TRUE;
		if (item_idx < 0 || item_idx >= cf_session_state.tabs[tab_idx].memory_len) return TRUE;
		if (cf_session_state.tabs[tab_idx].mem_values[item_idx]) g_free (cf_session_state.tabs[tab_idx].mem_values[item_idx]);
		cf_session_state.tabs[tab_idx].mem_values[item_idx] = config_string_unquote_dup (value);
		return TRUE;
	}
	return FALSE;
}

/*
 * config_file_get_default_prefs - initialize ALL members of the given s_preferences
 *	structure with default values.
 */
static void config_file_get_default_prefs (s_preferences *this_prefs)
{
	/* 1st pref page */
	this_prefs->bkg_color = g_strdup (DEFAULT_BKG_COLOR);
	this_prefs->result_font = g_strdup (DEFAULT_RESULT_FONT);
	this_prefs->result_color = g_strdup (DEFAULT_RESULT_COLOR);
	this_prefs->stack_font = g_strdup (DEFAULT_STACK_FONT);
	this_prefs->stack_color = g_strdup (DEFAULT_STACK_COLOR);
	this_prefs->mod_font = g_strdup (DEFAULT_MOD_FONT);
	this_prefs->act_mod_color = g_strdup (DEFAULT_ACT_MOD_COLOR);
	this_prefs->inact_mod_color = g_strdup (DEFAULT_INACT_MOD_COLOR);
	this_prefs->vis_number = DEFAULT_VIS_NUMBER;
	this_prefs->vis_angle = DEFAULT_VIS_ANGLE;
	this_prefs->vis_notation = DEFAULT_VIS_NOTATION;
	this_prefs->vis_arith = DEFAULT_VIS_ARITH;
	this_prefs->vis_bracket = DEFAULT_VIS_BRACKET;
	
	/* 2nd pref page */
	this_prefs->custom_button_font = DEFAULT_CUSTOM_BUTTON_FONT;
	this_prefs->button_font = g_strdup (DEFAULT_BUTTON_FONT);
	this_prefs->button_width = DEFAULT_BUTTON_WIDTH;
	this_prefs->button_height = DEFAULT_BUTTON_HEIGHT;
	this_prefs->vis_funcs = DEFAULT_VIS_FUNCS;
	this_prefs->vis_dispctrl = DEFAULT_VIS_DISPCTRL;
	this_prefs->vis_logic = DEFAULT_VIS_LOGIC;
	this_prefs->vis_standard = DEFAULT_VIS_STANDARD;
	this_prefs->mode = DEFAULT_MODE;
	
	/* 3rd pref page */
	/* constants - handled different */
	
	/* 4th pref page */
	this_prefs->dec_sep = DEFAULT_DEC_SEP;
	this_prefs->dec_sep_char = g_strdup (DEFAULT_DEC_SEP_CHAR);
	this_prefs->dec_sep_length = DEFAULT_DEC_SEP_LENGTH;
	this_prefs->hex_bits = DEFAULT_HEX_BITS;
	this_prefs->hex_signed = DEFAULT_HEX_SIGNED;
	this_prefs->hex_sep = DEFAULT_HEX_SEP;
	this_prefs->hex_sep_char = g_strdup (DEFAULT_HEX_SEP_CHAR);
	this_prefs->hex_sep_length = DEFAULT_HEX_SEP_LENGTH;
	this_prefs->oct_bits = DEFAULT_OCT_BITS;
	this_prefs->oct_sep = DEFAULT_OCT_SEP;
	this_prefs->oct_sep_char = g_strdup (DEFAULT_OCT_SEP_CHAR);
	this_prefs->oct_sep_length = DEFAULT_OCT_SEP_LENGTH;
	this_prefs->bin_bits = DEFAULT_BIN_BITS;
	this_prefs->bin_signed = DEFAULT_BIN_SIGNED;
	this_prefs->bin_sep = DEFAULT_BIN_SEP;
	this_prefs->bin_sep_char = g_strdup (DEFAULT_BIN_SEP_CHAR);
	this_prefs->bin_sep_length = DEFAULT_BIN_SEP_LENGTH;

	/* 5rd pref page */
	this_prefs->stack_size = DEFAULT_STACK_SIZE;
	this_prefs->def_number = DEFAULT_NUMBER;
	this_prefs->def_angle = DEFAULT_ANGLE;
	this_prefs->def_notation = DEFAULT_NOTATION;
	this_prefs->rem_display = DEFAULT_REM_DISPLAY;
	this_prefs->show_menu = DEFAULT_SHOW_MENU;
}

/*
 * config_file_get_default_consts - fill in default constants, which are not
 *	saved to configuration file. sync with counter inits in write and set_consts!
 */

static void config_file_get_default_consts (s_constant **consts)
{
	*consts = (s_constant *) g_malloc (3 * sizeof (s_constant));
	(*consts)[0].desc = g_strdup (_("Pi"));
	(*consts)[0].name = g_strdup (_("pi"));
	(*consts)[0].value = g_strdup_printf ("%.11f", G_PI);
	(*consts)[1].desc = g_strdup (_("Euler's Number"));
	(*consts)[1].name = g_strdup (_("e"));
	(*consts)[1].value = g_strdup_printf ("%.11f", G_E);
	(*consts)[2].name = NULL;
}

/*
 * config_file_get_default_user_functions - fill in default user functions
 */

static void config_file_get_default_user_functions (s_user_function **this_user_funcs)
{
	*this_user_funcs = (s_user_function *) g_malloc (4 * sizeof (s_user_function));
	(*this_user_funcs)[0].name = g_strdup ("abs");
	(*this_user_funcs)[0].variable = g_strdup ("x");
	(*this_user_funcs)[0].expression = g_strdup_printf ("sqrt(x^2)");
	(*this_user_funcs)[1].name = g_strdup ("sign");
	(*this_user_funcs)[1].variable = g_strdup ("x");
	(*this_user_funcs)[1].expression = g_strdup_printf ("x/abs(x)");	
	(*this_user_funcs)[2].name = g_strdup ("cot");
	(*this_user_funcs)[2].variable = g_strdup ("x");
	(*this_user_funcs)[2].expression = g_strdup_printf ("cos(x)/sin(x)");
	(*this_user_funcs)[3].name = NULL;
}

/*
 * config_file_set_prefs - find the key in the s_prefs_entry struct to retrieve
 *	the variable to set. The way to set this variable is given by key_type.
 * 	used by config_file_read.
 */

void config_file_set_prefs (char *key, char *value)
{
	int		*int_var, counter=0;
	char 		**string_var, *end_ptr;
	gboolean	*bool_var;
	void		*this_var;

	if (config_file_set_session_entry (key, value)) return;
	
	while (prefs_list[counter].key != NULL)	{
		if (g_ascii_strcasecmp (key, prefs_list[counter].key) == 0) break;	
		counter++;
	}
	
	if (prefs_list[counter].key == NULL) {
		fprintf (stderr, _("[%s] configuration file: ignoring unknown entry %s=%s. %s\n"), PACKAGE, key, value, BUG_REPORT);
		return;
	}
	
	this_var = prefs_list[counter].variable;
	switch (prefs_list[counter].key_type)
	{
		case STRING:
			string_var = this_var;
			g_free (*string_var);
			/* version 1.2.4 introduced strings enbodied by pairs of
			 * \"'s. So remove those if they exist.
			 */
			if ((value[0] == '\"') && (value[strlen(value)-1] == '\"'))
			{
				value++;
				value[strlen(value)-1]='\0';
			}
			*string_var = g_strdup (value);
			break;
		case BOOLEAN:
			bool_var = this_var;
			if (g_ascii_strcasecmp (value, "true")  == 0) *bool_var = TRUE;
			else if (g_ascii_strcasecmp (value, "false")  == 0) *bool_var = FALSE;
			else fprintf (stderr, _("[%s] configuration file: %s has to be TRUE or FALSE. Using defaults. %s\n"), PACKAGE, key, BUG_REPORT);
			break;
		case INTEGER:
			int_var = this_var;
			*int_var = (int) g_ascii_strtod (value, &end_ptr);
			if (*end_ptr != '\0')
				fprintf (stderr, _("[%s] configuration file: failed to convert %s to a number properly. Have you changed your locales? %s\n"), PACKAGE, value, BUG_REPORT);
			break;
		default:
			fprintf (stderr, _("[%s] configuration file: ignoring unknown key_type in config structure. %s\n"), PACKAGE, BUG_REPORT);
	}
}

/*
 * config_file_get_mode - if the line is enclosed by [...] and the enclosed
 * 	string is known, change to that mode.
 */

int config_file_get_mode (char *line, char *filename, int old_mode)
{
	gsize	len;
	
	if (line == NULL) return old_mode;
	line = g_strstrip(line);
	if (line[0] == '\0') return old_mode;
	len = strlen(line);
	if (len <= 0) return old_mode;
	if ((line[0] == '[') && (line[len - 1] == ']')) {
		if (strcmp (line, SECTION_GENERAL) == 0) return GENERAL;
		else if (strcmp (line, SECTION_CONSTANTS) == 0) return CONSTANTS;
		else if (strcmp (line, SECTION_USER_FUNCTIONS) == 0) return USER_FUNCTIONS;
		else fprintf (stderr, _("[%s] found unknown section %s in \
configuration file %s. Using preceding section.\n"), PACKAGE, line, filename);
	}
	return old_mode;
}

/*
 * config_file_set_constants - parses line and fills into the s_constants array.
 */

void config_file_set_constants (char *line)
{
	char		*desc, *name, *value;
	static int	nr_consts=0;
	
	desc = line;
	name = strchr (line, ':');
	if (name == NULL) return;
	*name = '\0';
	name++;
	value = strchr (name, '=');
	if (value == NULL) return;
	*value = '\0';
	value++;
	desc = g_strstrip(desc);
	name = g_strstrip(name);
	value = g_strstrip(value);	
	/* allowing desc and name to be "" */
	if (strlen(value) == 0) return;
	nr_consts++;
	cf_constant = (s_constant *) g_realloc (cf_constant, ((gsize) (nr_consts + 1)) * sizeof(s_constant));
	cf_constant[nr_consts-1].desc = g_strdup (desc);
	cf_constant[nr_consts-1].name = g_strdup (name);
	cf_constant[nr_consts-1].value = g_strdup (value);
	/* keep it NULL terminated */
	cf_constant[nr_consts].name = NULL;
}

/* config_file_set_user_functions
 *
 */

void config_file_set_user_functions (char *line)
{
	char 		*name, *variable, *expression;
	static int	nr_user_functions=0;
	
	name = line;
	variable = strchr (line, '(');
	if (variable == NULL) return;
	*variable = '\0';
	variable++;
	expression = strchr (variable, ')');
	if (expression == NULL) return;
	*expression = '\0';
	expression++;
	expression = strchr (expression, '=');
	if (expression == NULL) return;
	expression++;
	name = g_strstrip(name);
	variable = g_strstrip(variable);
	expression = g_strstrip(expression);
	/* allowing name to be "" */
	if ((strlen(variable) == 0) || (strlen(expression) == 0)) return;
	nr_user_functions++;
	cf_user_function = (s_user_function *) g_realloc (cf_user_function, 
		((gsize) (nr_user_functions + 1)) * sizeof(s_user_function));
	cf_user_function[nr_user_functions-1].name = g_strdup (name);
	cf_user_function[nr_user_functions-1].variable = g_strdup (variable);
	cf_user_function[nr_user_functions-1].expression = g_strdup (expression);
	/* keep it NULL terminated */
	cf_user_function[nr_user_functions].name = NULL;
}

/*
 * config_file_read - open/read/close. values are saved to global variable prefs!
 * 	policy: if there is no constants section, pi and e are added as default
 *	constants. of there is the section headline, nothing is added.
 */

s_preferences config_file_read (char *filename)
{
	char		line[MAX_FILE_LINE_LENGTH], *key, *value;
	FILE		*this_file;
	int		mode=GENERAL;
	gboolean 	have_const_section=FALSE, have_user_function_section=FALSE;
	
	this_file = fopen (filename, "r");
	config_file_get_default_prefs (&prefs);
	config_file_session_state_clear (&cf_session_state);
	cf_constant = (s_constant *) g_malloc (sizeof(s_constant));
	cf_constant->desc = NULL;
	if (this_file != NULL) {
		while (fgets (line, MAX_FILE_LINE_LENGTH, this_file) != NULL) {
			if (line[0] != '#') {
				mode = config_file_get_mode (line, filename, mode);
				switch (mode) {
				case GENERAL:
					key = line;
					value = strchr (line, '=');
					if (value == NULL) break;
					*value = '\0';
					value++;
					key = g_strstrip(key);
					value = g_strstrip(value);
					config_file_set_prefs (key, value);
					break;
				case CONSTANTS:
					have_const_section = TRUE;
					config_file_set_constants (line);
					break;
				case USER_FUNCTIONS:
					have_user_function_section = TRUE;
					config_file_set_user_functions (line);
					break;
				}
			}
		}
		fclose (this_file);
	}
	else fprintf (stderr, _("[%s] configuration file: couldn't open configuration file %s for reading. \
Nothing to worry about if you are starting %s for the first time. Using defaults.\n"), PACKAGE, filename, PACKAGE);

	if (have_const_section == FALSE) config_file_get_default_consts (&cf_constant);

	if (have_user_function_section == FALSE) config_file_get_default_user_functions (&cf_user_function);

	/* If locales have changed since last start one of the separator characters
	 * could be the new decimal point. This would lead to confusion.
	 */
	char decPoint = getDecPoint();
	int resetSep = 0;
	if (prefs.dec_sep_char[0] == decPoint) {
		resetSep = 1;
		prefs.dec_sep_char[0] = DEFAULT_DEC_SEP_CHAR[0];
	}
	if (prefs.hex_sep_char[0] == decPoint) {
		resetSep = 1;
		prefs.hex_sep_char[0] = DEFAULT_HEX_SEP_CHAR[0];
	}
	if (prefs.bin_sep_char[0] == decPoint) {
		resetSep = 1;
		prefs.bin_sep_char[0] = DEFAULT_BIN_SEP_CHAR[0];
	}
	if (prefs.oct_sep_char[0] == decPoint) {
		resetSep = 1;
		prefs.oct_sep_char[0] = DEFAULT_OCT_SEP_CHAR[0];
	}
	if (resetSep != 0) 
		fprintf (stderr, _("[%s] configuration file - We reset at least one separator character \
as it coincides with the decimal point. If you recently changed your locales settings, this \
is nothing to worry about.\n"), PACKAGE);

	return prefs;
}

/*
 * config_file_write - open/write/close.
 */

void config_file_write (char *filename, s_preferences this_prefs, s_constant *this_constants, s_user_function *this_user_functions)
{
	int		*int_var, counter=0;
	char 		**string_var;
	gboolean	*bool_var;
	void		*this_var;
	char 		*line=NULL;
	FILE		*this_file;
	
	cf_constant = this_constants;
	cf_user_function = this_user_functions;
	this_file = fopen (filename, "w+");
	/* overwrite local prefs memory with supplied prefs. probably ugly. */
	prefs = this_prefs;
	if (this_file != NULL) {
		fprintf (this_file, "\n%s\n\n", SECTION_GENERAL);
			while (prefs_list[counter].key != NULL) {
				line = NULL;
				this_var = prefs_list[counter].variable;
				switch (prefs_list[counter].key_type) {
				case STRING:
					string_var = this_var;
					line = g_strdup_printf ("%s=\"%s\"\n", prefs_list[counter].key, *string_var);
					break;
				case BOOLEAN:
					bool_var = this_var;
					if (*bool_var == TRUE) line = g_strdup_printf ("%s=true\n", prefs_list[counter].key);
					else if (*bool_var == FALSE) line = g_strdup_printf ("%s=false\n", prefs_list[counter].key);
					else fprintf (stderr, _("[%s] configuration file: strange boolean when writing. Skipping this key. %s\n"), PACKAGE, BUG_REPORT);
					break;
				case INTEGER:
					int_var = this_var;
					line = g_strdup_printf ("%s=%i\n", prefs_list[counter].key, *int_var);
					break;
					default:
						line = g_strdup_printf ("#%s=???\n", prefs_list[counter].key);
						fprintf (stderr, _("[%s] configuration file: ignoring unknown \"key_type\" in \"config_structure\". %s\n"), PACKAGE, BUG_REPORT);
				}
				if (line) {
					fputs (line, this_file);
					g_free (line);
				}
				counter ++;
			}
		counter = 0;
		if (this_prefs.rem_display && cf_session_state.tab_count > 0) {
			int tab_idx, item_idx;
			fprintf (this_file, "session_tab_count=%d\n", cf_session_state.tab_count);
			fprintf (this_file, "session_active_tab=%d\n", cf_session_state.active_tab);
			for (tab_idx = 0; tab_idx < cf_session_state.tab_count; tab_idx++) {
				s_session_tab_state *tab = &cf_session_state.tabs[tab_idx];
				fprintf (this_file, "session_tab%d_mode=%d\n", tab_idx, tab->mode);
				fprintf (this_file, "session_tab%d_number=%d\n", tab_idx, tab->number);
				fprintf (this_file, "session_tab%d_angle=%d\n", tab_idx, tab->angle);
				fprintf (this_file, "session_tab%d_notation=%d\n", tab_idx, tab->notation);
				fprintf (this_file, "session_tab%d_display=\"%s\"\n", tab_idx,
					tab->display_value ? tab->display_value : CLEARED_DISPLAY);
				fprintf (this_file, "session_tab%d_rpn_len=%d\n", tab_idx, tab->rpn_stack_len);
				for (item_idx = 0; item_idx < tab->rpn_stack_len; item_idx++) {
					fprintf (this_file, "session_tab%d_rpn_%d=\"%s\"\n", tab_idx, item_idx,
						(tab->rpn_stack && tab->rpn_stack[item_idx]) ? tab->rpn_stack[item_idx] : CLEARED_DISPLAY);
				}
				fprintf (this_file, "session_tab%d_mem_len=%d\n", tab_idx, tab->memory_len);
				for (item_idx = 0; item_idx < tab->memory_len; item_idx++) {
					fprintf (this_file, "session_tab%d_mem_%d=\"%s\"\n", tab_idx, item_idx,
						(tab->mem_values && tab->mem_values[item_idx]) ? tab->mem_values[item_idx] : CLEARED_DISPLAY);
				}
			}
		}

		fprintf (this_file, "\n%s\n\n", SECTION_CONSTANTS);
		while (cf_constant[counter].name != NULL) {
			fprintf (this_file, "%s:%s=%s\n", cf_constant[counter].desc, 
				cf_constant[counter].name, cf_constant[counter].value);
			counter++;
		}
		counter = 0;
		fprintf (this_file, "\n%s\n\n", SECTION_USER_FUNCTIONS);
		while (cf_user_function[counter].name != NULL) {	
			fprintf (this_file, "%s(%s)=%s\n", cf_user_function[counter].name, 
				cf_user_function[counter].variable, 
				cf_user_function[counter].expression);
			counter++;
		}
		fclose (this_file);
	}	
	else fprintf (stderr, _("[%s] configuration file: couldn't save/write to configuration file %s.\n"), PACKAGE, filename);
}

s_prefs_entry *config_file_get_prefs_list()
{
	return prefs_list;
}

s_constant *config_file_get_constants()
{
	return cf_constant;
}

s_user_function *config_file_get_user_functions()
{
	return cf_user_function;
}
