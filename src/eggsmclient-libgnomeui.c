/*
 * Copyright (C) 2007 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-ui-init.h>

#include "eggsmclient.h"
#include "eggsmclient-libgnomeui.h"
#include "eggdesktopfile.h"

static guint desktop_file_id, mode_id;

static char *desktop_file;
static EggSMClientMode mode;

static void
egg_sm_client_module_set_property (GObject *object, guint param_id,
				   const GValue *value, GParamSpec *pspec)
{
  if (param_id == desktop_file_id)
    {
      g_free (desktop_file);
      desktop_file = g_value_dup_string (value);
    }
  else if (param_id == mode_id)
    mode = g_value_get_int (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
}

static void
egg_sm_client_module_get_property (GObject *object, guint param_id,
				   GValue *value, GParamSpec *pspec)
{
  if (param_id == desktop_file_id)
    g_value_set_string (value, desktop_file);
  else if (param_id == mode_id)
    g_value_set_int (value, mode);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
}

static void
egg_sm_client_post_args_parse (GnomeProgram *app, GnomeModuleInfo *mod_info)
{
  if (desktop_file)
    egg_set_desktop_file (desktop_file);
  egg_sm_client_set_mode (mode);
}

static void
egg_sm_client_module_class_init (GnomeProgramClass *klass,
				 const GnomeModuleInfo *mod_info)
{
  desktop_file_id = gnome_program_install_property (
	klass,
	egg_sm_client_module_get_property,
	egg_sm_client_module_set_property,
	g_param_spec_string (EGG_SM_CLIENT_PARAM_DESKTOP_FILE, NULL, NULL,
			     NULL,
			     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  mode_id = gnome_program_install_property (
	klass,
	egg_sm_client_module_get_property,
	egg_sm_client_module_set_property,
	g_param_spec_int (EGG_SM_CLIENT_PARAM_MODE, NULL, NULL,
			  EGG_SM_CLIENT_MODE_DISABLED, EGG_SM_CLIENT_MODE_NORMAL,
			  EGG_SM_CLIENT_MODE_NORMAL,
			  G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

/**
 * egg_sm_client_module_info_get:
 *
 * Gets a #GnomeModuleInfo for #EggSMClient support.
 *
 * Return value: the #GnomeModuleInfo.
 **/
const GnomeModuleInfo *
egg_sm_client_module_info_get (void)
{
	static GnomeModuleInfo module_info = {
		"eggsmclient", VERSION, N_("Session management"),
		NULL, /* requirements */
		NULL, /* instance_init */
		NULL, egg_sm_client_post_args_parse,
		NULL, /* popt */
		NULL, /* init pass */
		egg_sm_client_module_class_init,
		NULL, /* opt prefix */
		egg_sm_client_get_option_group,
	};

	return &module_info;
}

/**
 * egg_sm_client_libgnomeui_module_info_get:
 *
 * Copies %LIBGNOMEUI_MODULE, but replaces #GnomeClient support with
 * #EggSMClient support.
 *
 * Return value: the #GnomeModuleInfo.
 **/
const GnomeModuleInfo *
egg_sm_client_libgnomeui_module_info_get (void)
{
  static GnomeModuleInfo module_info = { NULL };

  if (!module_info.name)
    {
      int i;

      module_info = *libgnomeui_module_info_get ();
      module_info.name = "libgnomeui+eggsmclient";
      module_info.version = VERSION;
      module_info.description = _("GNOME GUI Library + EggSMClient");

      for (i = 0; module_info.requirements[i].module_info; i++)
	{
	  if (!strcmp (module_info.requirements[i].module_info->name, "gnome-client"))
	    {
	      module_info.requirements[i].required_version = VERSION;
	      module_info.requirements[i].module_info = egg_sm_client_module_info_get ();
	      break;
	    }
	}
    }

  return &module_info;
}

