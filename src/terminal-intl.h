/* the intl macros */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#ifndef TERMINAL_INTL_H
#define TERMINAL_INTL_H

#include <config.h>
#include <libintl.h>
#include <locale.h>
#include <glib.h>

G_BEGIN_DECLS

#ifdef _
#undef _
#endif

#ifdef N_
#undef N_
#endif

#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x

G_END_DECLS

#endif /* TERMINAL_INTL_H */
