#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "eggcellrendererkeys.h"

#define EGG_CELL_RENDERER_TEXT_PATH "egg-cell-renderer-text"
#define USED_MODS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

static void             egg_cell_renderer_keys_class_init    (EggCellRendererKeysClass *cell_keys_class);
static GtkCellEditable *egg_cell_renderer_keys_start_editing (GtkCellRenderer          *cell,
							      GdkEvent                 *event,
							      GtkWidget                *widget,
							      const gchar              *path,
							      GdkRectangle             *background_area,
							      GdkRectangle             *cell_area,
							      GtkCellRendererState      flags);


GType
egg_cell_renderer_keys_get_type (void)
{
  static GType cell_keys_type = 0;

  if (!cell_keys_type)
    {
      static const GTypeInfo cell_keys_info =
      {
        sizeof (EggCellRendererKeysClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc)egg_cell_renderer_keys_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRendererText),
	0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      cell_keys_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "EggCellRendererKeys", &cell_keys_info, 0);
    }

  return cell_keys_type;
}

static void
egg_cell_renderer_keys_init (EggCellRendererKeys *cell_keys)
{
}

static void
egg_cell_renderer_keys_class_init (EggCellRendererKeysClass *cell_keys_class)
{
  GTK_CELL_RENDERER_CLASS (cell_keys_class)->start_editing = egg_cell_renderer_keys_start_editing;
}


GtkCellRenderer *
egg_cell_renderer_keys_new (void)
{
  return GTK_CELL_RENDERER (g_object_new (EGG_TYPE_CELL_RENDERER_KEYS, NULL));
}

static void
egg_cell_renderer_keys_editing_done (GtkCellEditable *entry,
				     gpointer         data)
{
  const gchar *path;
  const gchar *new_text;

  if (GTK_ENTRY (entry)->editing_canceled)
    return;

  path = g_object_get_data (G_OBJECT (entry), EGG_CELL_RENDERER_TEXT_PATH);
  new_text = gtk_entry_get_text (GTK_ENTRY (entry));

  gtk_signal_emit_by_name (GTK_OBJECT (data), "edited", path, new_text);
}

static gchar *
convert_keysym_state_to_string(guint keysym,
			       GdkModifierType state)
{
  if (keysym == 0)
    return g_strdup ("Disabled");

  return gtk_accelerator_name (keysym, state);
}

static gboolean 
is_modifier (guint keycode)
{
  gint i;
  gint map_size;
  XModifierKeymap *mod_keymap;
  gboolean retval = FALSE;

  mod_keymap = XGetModifierMapping (gdk_display);

  map_size = 8 * mod_keymap->max_keypermod;
  i = 0;
  while (i < map_size) {
    
    if (keycode == mod_keymap->modifiermap[i]) {
      retval = TRUE;
      break;
    }
    ++i;
  }

  XFreeModifiermap (mod_keymap);

  return retval;
}

static GdkFilterReturn
grab_key_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  XEvent *xevent = (XEvent *)gdk_xevent;
  GtkEntry *entry;
  char *key;
  guint keycode, state;
  char buf[10];
  KeySym keysym;

  if (xevent->type != KeyPress)
    return GDK_FILTER_CONTINUE;
	
  entry = GTK_ENTRY (data);

  keycode = xevent->xkey.keycode;

  if (is_modifier (keycode))
    return GDK_FILTER_CONTINUE;

  state = xevent->xkey.state & USED_MODS;
  
  XLookupString (&xevent->xkey, buf, 0, &keysym, NULL);
  
  key = convert_keysym_state_to_string (keysym,
					state);
  
  gtk_entry_set_text (entry, key != NULL ? key : "");
  g_free (key);
  
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_window_remove_filter (gdk_get_default_root_window (),
			    grab_key_filter, data);
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));
  
  return GDK_FILTER_REMOVE;
}

static void
entry_realize (GtkWidget   *widget,
	       gpointer     data)
{
  gdk_keyboard_grab (gdk_get_default_root_window (), FALSE, GDK_CURRENT_TIME);
  gdk_window_add_filter (gdk_get_default_root_window (), grab_key_filter, widget);
}
		 
static GtkCellEditable *
egg_cell_renderer_keys_start_editing (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const gchar          *path,
				      GdkRectangle         *background_area,
				      GdkRectangle         *cell_area,
				      GtkCellRendererState  flags)
{
  GtkCellRendererText *celltext;
  GtkWidget *entry;
  
  celltext = GTK_CELL_RENDERER_TEXT (cell);

  /* If the cell isn't editable we return NULL. */
  if (celltext->editable == FALSE)
    return NULL;

  entry = g_object_new (GTK_TYPE_ENTRY,
			"has_frame", FALSE,
			NULL);
  g_signal_connect_after (G_OBJECT (entry), "realize", entry_realize, NULL);
  gtk_entry_set_text (GTK_ENTRY (entry), celltext->text);
  g_object_set_data_full (G_OBJECT (entry), EGG_CELL_RENDERER_TEXT_PATH, g_strdup (path), g_free);
  
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
  
  gtk_widget_show (entry);
  gtk_signal_connect (GTK_OBJECT (entry),
		      "editing_done",
		      G_CALLBACK (egg_cell_renderer_keys_editing_done),
		      celltext);
  return GTK_CELL_EDITABLE (entry);

}
