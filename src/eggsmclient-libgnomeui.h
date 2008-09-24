/* eggsmclient-libgnomeui.h
 * Copyright (C) 2007 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EGG_SM_CLIENT_LIBGNOMEUI_H__
#define __EGG_SM_CLIENT_LIBGNOMEUI_H__

#include "eggsmclient.h"
#include <libgnomeui/gnome-ui-init.h>

G_BEGIN_DECLS

const GnomeModuleInfo *egg_sm_client_module_info_get (void);
#define EGG_SM_CLIENT_MODULE egg_sm_client_module_info_get ()

const GnomeModuleInfo *egg_sm_client_libgnomeui_module_info_get (void);
#define EGG_SM_CLIENT_LIBGNOMEUI_MODULE egg_sm_client_libgnomeui_module_info_get ()

#define EGG_SM_CLIENT_PARAM_DESKTOP_FILE "egg-desktop-file"
#define EGG_SM_CLIENT_PARAM_MODE         "egg-sm-client-mode"


G_END_DECLS

#endif /* __EGG_SM_CLIENT_LIBGNOMEUI_H__ */
