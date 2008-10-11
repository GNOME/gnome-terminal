/* Encoding stuff */

/*
 * Copyright © 2002 Red Hat, Inc.
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

#ifndef TERMINAL_ENCODING_H
#define TERMINAL_ENCODING_H

#include <gtk/gtk.h>

typedef struct
{
  int   refcount;
  char *charset;
  char *name;
  guint valid            : 1;
  guint validity_checked : 1;
  guint is_custom        : 1;
  guint is_active        : 1;
} TerminalEncoding;

void terminal_encoding_init (void);

void terminal_encoding_dialog_show (GtkWindow *transient_parent);

GSList* terminal_get_active_encodings (void);

TerminalEncoding* terminal_encoding_ref (TerminalEncoding *encoding);

void terminal_encoding_unref (TerminalEncoding *encoding);

#endif /* TERMINAL_ENCODING_H */
