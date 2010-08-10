/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003  David Bordoley
 *  Copyright © 2003-2004 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "terminal-tabs-menu.h"
#include "terminal-screen.h"
#include "terminal-screen-container.h"
#include "terminal-intl.h"

#define TERMINAL_ACCELS_N_TABS_SWITCH (12)

#define LABEL_WIDTH_CHARS 32
#define ACTION_VERB_FORMAT_PREFIX       "JmpTab"
#define ACTION_VERB_FORMAT_PREFIX_LEN   strlen (ACTION_VERB_FORMAT_PREFIX)
#define ACTION_VERB_FORMAT		ACTION_VERB_FORMAT_PREFIX "%x"
#define ACTION_VERB_FORMAT_LENGTH	strlen (ACTION_VERB_FORMAT) + 14 + 1
#define ACTION_VERB_FORMAT_BASE         (16) /* %x is hex */
#define ACCEL_PATH_FORMAT		"<Actions>/Main/TabsSwitch%u"
#define ACCEL_PATH_FORMAT_LENGTH	strlen (ACCEL_PATH_FORMAT) + 14 + 1
#define DATA_KEY			"TerminalTabsMenu::Action"

#define UI_PATH                         "/menubar/Tabs"

#define TERMINAL_TABS_MENU_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), TERMINAL_TYPE_TABS_MENU, TerminalTabsMenuPrivate))

struct _TerminalTabsMenuPrivate
{
	TerminalWindow *window;
	GtkActionGroup *action_group;
	GtkAction *anchor_action;
	guint ui_id;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

static void	terminal_tabs_menu_update		(TerminalTabsMenu *menu);

/* FIXME: this can be severely optimised */
static GByteArray *tabs_id_array = NULL;
static guint n_tabs = 0;

G_DEFINE_TYPE (TerminalTabsMenu, terminal_tabs_menu, G_TYPE_OBJECT)

/* We need to assign unique IDs to tabs, otherwise accels get confused in the
 * tabs menu (bug #339548). We could use a serial #, but the ID is used in the
 * action name which is stored in a GQuark and so we should allocate them
 * efficiently.
 */
static guint
allocate_tab_id (void)
{
        int bit;
        guint b, len;
        guint8 *data;
        guint8 byte, mask;

        if (n_tabs++ == 0)
        {
                g_assert (tabs_id_array == NULL);
                tabs_id_array = g_byte_array_sized_new (16);
        }

        /* Find a free ID */
        len = tabs_id_array->len;
        data = tabs_id_array->data;
        for (b = 0; b < len; ++b)
        {
                if (data[b] != 0xff)
                        break;
        }

        /* Need to append a new byte */
        if (b == len)
        {
                guint8 bytes[] = { 0 };
                g_byte_array_append (tabs_id_array, bytes, G_N_ELEMENTS (bytes));
                g_assert (tabs_id_array->len > b);
        }

        data = tabs_id_array->data + b;
        byte = 0xff ^ *data;
        /* Now find the first free bit */
        bit = g_bit_nth_lsf (byte, -1);
        mask = 1 << bit;
        g_assert (bit >= 0 && bit <= 7);
        g_assert ((*data & mask) == 0);
        /* And mark it as allocated */
        *data |= mask;

        return b * 8 + bit;
}

static void
free_tab_id (GtkAction *action)
{
        const char *name;
        guint id;
        guint8 *data;
        guint b, bit;

        name = gtk_action_get_name (action);
        id = g_ascii_strtoull (name + ACTION_VERB_FORMAT_PREFIX_LEN, NULL,
                               ACTION_VERB_FORMAT_BASE);
        g_assert (id < tabs_id_array->len * 8);

        b = id >> 3;
        bit = id & 0x7;
        data = tabs_id_array->data + b;
        *data &= ~(1 << bit);

        g_assert (n_tabs > 0);
        if (--n_tabs == 0)
        {
                g_assert (tabs_id_array != NULL);
                g_byte_array_free (tabs_id_array, TRUE);
                tabs_id_array = NULL;
        }
}

static void
tab_action_activate_cb (GtkToggleAction *action,
			TerminalTabsMenu *menu)
{
	TerminalTabsMenuPrivate *priv = menu->priv;
	TerminalScreen *screen;

	if (gtk_toggle_action_get_active (action) == FALSE)
	{
		return;
	}

	screen = g_object_get_data (G_OBJECT (action), DATA_KEY);
	g_return_if_fail (screen != NULL);

	if (terminal_window_get_active (priv->window) != screen)
	{
                terminal_window_switch_screen (priv->window, screen);
	}
}

static void
sync_tab_title (TerminalScreen *screen,
		GParamSpec *pspec,
		GtkAction *action)
{
	const char *title;

	title = terminal_screen_get_title (screen);

	g_object_set (action, "label", title, NULL);
}

static void
notebook_page_added_cb (GtkNotebook *notebook,
                        TerminalScreenContainer *container,
			guint position,
			TerminalTabsMenu *menu)
{
	TerminalTabsMenuPrivate *priv = menu->priv;
	GtkAction *action;
	char verb[ACTION_VERB_FORMAT_LENGTH];
	GSList *group;
        TerminalScreen *screen;

        screen = terminal_screen_container_get_screen (container);

	g_snprintf (verb, sizeof (verb), ACTION_VERB_FORMAT, allocate_tab_id ());
  
	action = g_object_new (GTK_TYPE_RADIO_ACTION,
			       "name", verb,
			       "tooltip", _("Switch to this tab"),
			       NULL);

	sync_tab_title (screen, NULL, action);
	/* make sure the action is alive when handling the signal, see bug #169833 */
	g_signal_connect_object (screen, "notify::title",
				 G_CALLBACK (sync_tab_title), action, 0);

	gtk_action_group_add_action_with_accel (priv->action_group, action, NULL);

	group = gtk_radio_action_get_group (GTK_RADIO_ACTION (priv->anchor_action));
	gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);

	/* set this here too, since tab-added comes after notify::active-child */
	if (terminal_window_get_active (priv->window) == screen)
	{
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}

	g_object_set_data (G_OBJECT (screen), DATA_KEY, action);
	g_object_set_data (G_OBJECT (action), DATA_KEY, screen);

	g_signal_connect (action, "activate",
			  G_CALLBACK (tab_action_activate_cb), menu);

	g_object_unref (action);

	terminal_tabs_menu_update (menu);
}

static void
notebook_page_removed_cb (GtkNotebook *notebook,
                          TerminalScreenContainer *container,
			  guint position,
			  TerminalTabsMenu *menu)
{
	TerminalTabsMenuPrivate *priv = menu->priv;
	GtkAction *action;
        TerminalScreen *screen;

        screen = terminal_screen_container_get_screen (container);

	action = g_object_get_data (G_OBJECT (screen), DATA_KEY);
	g_return_if_fail (action != NULL);

        free_tab_id (action);

	g_signal_handlers_disconnect_by_func
		(screen, G_CALLBACK (sync_tab_title), action);

	g_signal_handlers_disconnect_by_func
		(action, G_CALLBACK (tab_action_activate_cb), menu);

	g_object_set_data (G_OBJECT (screen), DATA_KEY, NULL);
 	gtk_action_group_remove_action (priv->action_group, action);

	terminal_tabs_menu_update (menu);
}

static void
notebook_page_reordered_cb (GtkNotebook *notebook,
			    GtkBin *bin,
			    guint position,
			    TerminalTabsMenu *menu)
{
	terminal_tabs_menu_update (menu);
}

static void
notebook_page_switch_cb (GtkNotebook *notebook,
#if GTK_CHECK_VERSION (2, 90, 6)
                         GtkWidget *page,
#else
                         gpointer page,
#endif
                         guint position,
                         TerminalTabsMenu *menu)
{
        TerminalScreenContainer *container;
        TerminalScreen *screen;
        GtkAction *action;

#if GTK_CHECK_VERSION (2, 90, 6)
        container = TERMINAL_SCREEN_CONTAINER (page);
#else
        container = TERMINAL_SCREEN_CONTAINER (gtk_notebook_get_nth_page (notebook, position));
#endif
        screen = terminal_screen_container_get_screen (container);

	action = g_object_get_data (G_OBJECT (screen), DATA_KEY);
        g_signal_handlers_block_by_func (action, G_CALLBACK (tab_action_activate_cb), menu);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
        g_signal_handlers_unblock_by_func (action, G_CALLBACK (tab_action_activate_cb), menu);
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
		  GtkAction *action,
		  GtkWidget *proxy,
		  gpointer dummy)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		GtkLabel *label;

		label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

		gtk_label_set_use_underline (label, FALSE);
		gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars (label, LABEL_WIDTH_CHARS);
	}
}

static void
terminal_tabs_menu_set_window (TerminalTabsMenu *menu,
			   TerminalWindow *window)
{
	TerminalTabsMenuPrivate *priv = menu->priv;
	GtkWidget *notebook;
	GtkUIManager *manager;

	priv->window = window;

	manager = GTK_UI_MANAGER (terminal_window_get_ui_manager (window));
	priv->action_group = gtk_action_group_new ("TabsActions");
	gtk_ui_manager_insert_action_group (manager, priv->action_group, -1);
	g_object_unref (priv->action_group);

	priv->anchor_action = g_object_new (GTK_TYPE_RADIO_ACTION,
					    "name", "TabsMenuAnchorAction",
					    NULL);
	gtk_action_group_add_action (priv->action_group, priv->anchor_action);
        g_object_unref (priv->anchor_action);

	g_signal_connect (priv->action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	notebook = terminal_window_get_notebook (window);
	g_signal_connect_object (notebook, "page-added",
				 G_CALLBACK (notebook_page_added_cb), menu, 0);
	g_signal_connect_object (notebook, "page-removed",
				 G_CALLBACK (notebook_page_removed_cb), menu, 0);
	g_signal_connect_object (notebook, "page-reordered",
				 G_CALLBACK (notebook_page_reordered_cb), menu, 0);
	g_signal_connect_object (notebook, "switch-page",
				 G_CALLBACK (notebook_page_switch_cb), menu, 0);
}

static void
terminal_tabs_menu_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	TerminalTabsMenu *menu = TERMINAL_TABS_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			terminal_tabs_menu_set_window (menu, g_value_get_object (value));
			break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
	}
}

static void
terminal_tabs_menu_get_property (GObject *object,
			     guint prop_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	/* no readable properties */
	g_return_if_reached ();
}

static void
terminal_tabs_menu_class_init (TerminalTabsMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = terminal_tabs_menu_set_property;
	object_class->get_property = terminal_tabs_menu_get_property;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window", NULL, NULL,
							      TERMINAL_TYPE_WINDOW,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (TerminalTabsMenuPrivate));

        /* We don't want to save accels, so skip them */
        gtk_accel_map_add_filter ("<Actions>/Main/TabsSwitch*");
}

static void
terminal_tabs_menu_init (TerminalTabsMenu *menu)
{
	menu->priv = TERMINAL_TABS_MENU_GET_PRIVATE (menu);
}

static void
terminal_tabs_menu_clean (TerminalTabsMenu *menu)
{
	TerminalTabsMenuPrivate *p = menu->priv;
	GtkUIManager *manager = GTK_UI_MANAGER (terminal_window_get_ui_manager (p->window));

	if (p->ui_id != 0)
	{
		gtk_ui_manager_remove_ui (manager, p->ui_id);
		gtk_ui_manager_ensure_update (manager);
		p->ui_id = 0;
	}
}

TerminalTabsMenu *
terminal_tabs_menu_new (TerminalWindow *window)
{
	return TERMINAL_TABS_MENU (g_object_new (TERMINAL_TYPE_TABS_MENU,
					     "window", window,
					     NULL));
}

static void
tab_set_action_accelerator (GtkActionGroup *action_group,
			    GtkAction *action,
			    guint tab_number,
			    gboolean is_single_tab)
{
        if (!is_single_tab &&
            tab_number < TERMINAL_ACCELS_N_TABS_SWITCH)
        {
                char accel_path[ACCEL_PATH_FORMAT_LENGTH];

                g_snprintf (accel_path, sizeof (accel_path), ACCEL_PATH_FORMAT, tab_number + 1);
                gtk_action_set_accel_path (action, accel_path);
        }
        else
        {
                gtk_action_set_accel_path (action, NULL);
                return;
        }
}

static void
terminal_tabs_menu_update (TerminalTabsMenu *menu)
{
	TerminalTabsMenuPrivate *p = menu->priv;
	GtkUIManager *manager;
	GtkAction *action;
	GList *tabs = NULL, *l;
	guint i = 0, n;
	gboolean is_single_tab;
	const char *verb;

	terminal_tabs_menu_clean (menu);

	tabs = terminal_window_list_screen_containers (p->window);

	n = g_list_length (tabs);
	if (n == 0) return;

	is_single_tab = (n == 1);

	manager =  GTK_UI_MANAGER (terminal_window_get_ui_manager (p->window));
	p->ui_id = gtk_ui_manager_new_merge_id (manager);

	for (l = tabs; l != NULL; l = l->next)
	{
                TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (l->data);
                GObject *screen = G_OBJECT (terminal_screen_container_get_screen (container));

		action = g_object_get_data (screen, DATA_KEY);
		g_return_if_fail (action != NULL);
  
		verb = gtk_action_get_name (action);

		tab_set_action_accelerator (p->action_group, action, i++, is_single_tab);

		gtk_ui_manager_add_ui (manager, p->ui_id,
				       UI_PATH,
				       verb, verb,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
	}

	g_list_free (tabs);
}
