#include <config.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include "eggcellrendererkeys.h"

#ifndef _
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x
#endif

#define EGG_CELL_RENDERER_TEXT_PATH "egg-cell-renderer-text"
#define USED_MODS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

static void             egg_cell_renderer_keys_finalize      (GObject             *object);
static void             egg_cell_renderer_keys_init          (EggCellRendererKeys *cell_keys);
static void             egg_cell_renderer_keys_class_init    (EggCellRendererKeysClass *cell_keys_class);
static GtkCellEditable *egg_cell_renderer_keys_start_editing (GtkCellRenderer          *cell,
							      GdkEvent                 *event,
							      GtkWidget                *widget,
							      const gchar              *path,
							      GdkRectangle             *background_area,
							      GdkRectangle             *cell_area,
							      GtkCellRendererState      flags);


static void egg_cell_renderer_keys_get_property  (GObject                  *object,
						  guint                     param_id,
						  GValue                   *value,
						  GParamSpec               *pspec);
static void egg_cell_renderer_keys_set_property  (GObject                  *object,
						  guint                     param_id,
						  const GValue             *value,
						  GParamSpec               *pspec);

enum {
  PROP_0,

  /* FIXME make names consistent with something else */
  PROP_ACCEL_KEY,
  PROP_ACCEL_MASK
};

static GtkCellRendererTextClass *parent_class = NULL;

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
        sizeof (EggCellRendererKeys),
	0,              /* n_preallocs */
        (GInstanceInitFunc) egg_cell_renderer_keys_init
      };

      cell_keys_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "EggCellRendererKeys", &cell_keys_info, 0);
    }

  return cell_keys_type;
}

static void
egg_cell_renderer_keys_init (EggCellRendererKeys *cell_keys)
{
}

/* FIXME setup stuff to generate this */
/* VOID:STRING,UINT,FLAGS */
static void
marshal_VOID__STRING_UINT_FLAGS (GClosure     *closure,
                                 GValue       *return_value,
                                 guint         n_param_values,
                                 const GValue *param_values,
                                 gpointer      invocation_hint,
                                 gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_UINT_FLAGS) (gpointer     data1,
                                                        const char  *arg_1,
                                                        guint        arg_2,
                                                        int          arg_3,
                                                        gpointer     data2);
  register GMarshalFunc_VOID__STRING_UINT_FLAGS callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 4);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  
  callback = (GMarshalFunc_VOID__STRING_UINT_FLAGS) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_value_get_string (param_values + 1),
            g_value_get_uint (param_values + 2),
            g_value_get_flags (param_values + 3),
            data2);
}

static void
egg_cell_renderer_keys_class_init (EggCellRendererKeysClass *cell_keys_class)
{
  GObjectClass *object_class;
  
  object_class = G_OBJECT_CLASS (cell_keys_class);

  parent_class = g_type_class_peek_parent (object_class);
  
  GTK_CELL_RENDERER_CLASS (cell_keys_class)->start_editing = egg_cell_renderer_keys_start_editing;

  object_class->set_property = egg_cell_renderer_keys_set_property;
  object_class->get_property = egg_cell_renderer_keys_get_property;

  object_class->finalize = egg_cell_renderer_keys_finalize;
  
  /* FIXME if this gets moved to a real library, rename the properties
   * to match whatever the GTK convention is
   */
  
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MASK,
                                   g_param_spec_flags ("accel_mask",
                                                       _("Accelerator modifiers"),
                                                       _("Accelerator modifiers"),
                                                       GDK_TYPE_MODIFIER_TYPE,
                                                       0,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_KEY,
                                   g_param_spec_uint ("accel_key",
                                                     _("Accelerator key"),
                                                     _("Accelerator key"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_signal_new ("keys_edited",
                EGG_TYPE_CELL_RENDERER_KEYS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EggCellRendererKeysClass, keys_edited),
                NULL, NULL,
                marshal_VOID__STRING_UINT_FLAGS,
                G_TYPE_NONE, 3,
                G_TYPE_STRING,
                G_TYPE_UINT,
                GDK_TYPE_MODIFIER_TYPE);
}


GtkCellRenderer *
egg_cell_renderer_keys_new (void)
{
  return GTK_CELL_RENDERER (g_object_new (EGG_TYPE_CELL_RENDERER_KEYS, NULL));
}

static void
egg_cell_renderer_keys_finalize (GObject *object)
{
  
  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static gchar *
convert_keysym_state_to_string (guint           keysym,
                                GdkModifierType state)
{
  if (keysym == 0)
    return g_strdup (_("Disabled"));
  else
    return gtk_accelerator_name (keysym, state);
}

static void
egg_cell_renderer_keys_get_property  (GObject                  *object,
                                      guint                     param_id,
                                      GValue                   *value,
                                      GParamSpec               *pspec)
{
  EggCellRendererKeys *keys;

  g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

  keys = EGG_CELL_RENDERER_KEYS (object);
  
  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      g_value_set_uint (value, keys->accel_key);
      break;

    case PROP_ACCEL_MASK:
      g_value_set_flags (value, keys->accel_mask);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
egg_cell_renderer_keys_set_property  (GObject                  *object,
                                      guint                     param_id,
                                      const GValue             *value,
                                      GParamSpec               *pspec)
{
  EggCellRendererKeys *keys;

  g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

  keys = EGG_CELL_RENDERER_KEYS (object);
  
  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      egg_cell_renderer_keys_set_accelerator (keys,
                                              g_value_get_uint (value),
                                              keys->accel_mask);
      break;

    case PROP_ACCEL_MASK:
      egg_cell_renderer_keys_set_accelerator (keys,
                                              keys->accel_key,
                                              g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
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
  while (i < map_size)
    {
      if (keycode == mod_keymap->modifiermap[i])
        {
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
  guint keycode, state;
  char buf[10];
  KeySym keysym;
  EggCellRendererKeys *keys;
  char *path;
  gboolean edited;
  
  if (xevent->type != KeyPress)
    return GDK_FILTER_CONTINUE;
	
  keys = EGG_CELL_RENDERER_KEYS (data);
  
  keycode = xevent->xkey.keycode;

  if (is_modifier (keycode))
    return GDK_FILTER_CONTINUE;

  edited = FALSE;
  
  state = xevent->xkey.state & USED_MODS;
  
  XLookupString ((XKeyEvent*)xevent, buf, sizeof (buf), &keysym, NULL);
  
  if (state == 0 && keysym == GDK_Escape)
    goto out; /* cancel */

  /* clear the accelerator on Backspace */
  if (keys->edit_key != 0 &&
      state == 0 &&
      keysym == GDK_BackSpace)
    keysym = 0;

  edited = TRUE;
  
 out:
  path = g_strdup (g_object_get_data (G_OBJECT (keys->edit_widget),
                                      EGG_CELL_RENDERER_TEXT_PATH));
  
  gdk_keyboard_ungrab (xevent->xkey.time);
  gdk_pointer_ungrab (xevent->xkey.time);
  
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (keys->edit_widget));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (keys->edit_widget));
  keys->edit_widget = NULL;
  keys->filter_window = NULL;
  
  if (edited)
    g_signal_emit_by_name (G_OBJECT (keys), "keys_edited", path,
                           keysym, state);

  g_free (path);
  
  return GDK_FILTER_REMOVE;
}

static void
ungrab_stuff (GtkWidget *widget, gpointer data)
{
  EggCellRendererKeys *keys = EGG_CELL_RENDERER_KEYS (data);

  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_pointer_ungrab (GDK_CURRENT_TIME);

  gdk_window_remove_filter (keys->filter_window,
			    grab_key_filter, data);
}

static void
pointless_eventbox_start_editing (GtkCellEditable *cell_editable,
                               GdkEvent        *event)
{
  /* do nothing, because we are pointless */
}

static void
pointless_eventbox_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = pointless_eventbox_start_editing;
}

static GType
pointless_eventbox_subclass_get_type (void)
{
  static GType eventbox_type = 0;

  if (!eventbox_type)
    {
      static const GTypeInfo eventbox_info =
      {
        sizeof (GtkEventBoxClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkEventBox),
	0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      static const GInterfaceInfo cell_editable_info = {
        (GInterfaceInitFunc) pointless_eventbox_cell_editable_init,
        NULL, NULL };

      eventbox_type = g_type_register_static (GTK_TYPE_EVENT_BOX, "EggCellEditableEventBox", &eventbox_info, 0);
      
      g_type_add_interface_static (eventbox_type,
				   GTK_TYPE_CELL_EDITABLE,
				   &cell_editable_info);
    }

  return eventbox_type;
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
  EggCellRendererKeys *keys;
  GtkWidget *label;
  GtkWidget *eventbox;
  
  celltext = GTK_CELL_RENDERER_TEXT (cell);
  keys = EGG_CELL_RENDERER_KEYS (cell);
  
  /* If the cell isn't editable we return NULL. */
  if (celltext->editable == FALSE)
    return NULL;

  g_return_val_if_fail (widget->window != NULL, NULL);
  
  if (gdk_keyboard_grab (widget->window, FALSE,
                         gdk_event_get_time (event)) != GDK_GRAB_SUCCESS)
    return NULL;

  if (gdk_pointer_grab (widget->window, FALSE,
                        GDK_BUTTON_PRESS_MASK,
                        FALSE, NULL,
                        gdk_event_get_time (event)) != GDK_GRAB_SUCCESS)
    {
      gdk_keyboard_ungrab (gdk_event_get_time (event));
      return NULL;
    }
  
  keys->filter_window = widget->window;
  
  gdk_window_add_filter (keys->filter_window, grab_key_filter, keys);

  eventbox = g_object_new (pointless_eventbox_subclass_get_type (),
                           NULL);
  keys->edit_widget = eventbox;
  g_object_add_weak_pointer (G_OBJECT (keys->edit_widget),
                             (void**) &keys->edit_widget);
  
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  
  gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL,
                        &widget->style->bg[GTK_STATE_SELECTED]);

  gtk_widget_modify_fg (label, GTK_STATE_NORMAL,
                        &widget->style->fg[GTK_STATE_SELECTED]);
  
  if (keys->accel_key != 0)
    gtk_label_set_markup (GTK_LABEL (label),
                          _("Type a new accelerator, or press Backspace to clear"));
  else
    gtk_label_set_text (GTK_LABEL (label),
                        _("Type a new accelerator"));

  gtk_container_add (GTK_CONTAINER (eventbox), label);
  
  g_object_set_data_full (G_OBJECT (keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH,
                          g_strdup (path), g_free);
  
  gtk_widget_show_all (keys->edit_widget);

  g_signal_connect (G_OBJECT (keys->edit_widget), "unrealize",
                    G_CALLBACK (ungrab_stuff), keys);
  
  keys->edit_key = keys->accel_key;
  keys->edit_mask = keys->accel_mask;
  
  return GTK_CELL_EDITABLE (keys->edit_widget);
}

void
egg_cell_renderer_keys_set_accelerator (EggCellRendererKeys *keys,
                                        guint                keyval,
                                        GdkModifierType      mask)
{
  char *text;
  gboolean changed;
  GtkCellRendererText *celltext;
  
  g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (keys));
  
  g_object_freeze_notify (G_OBJECT (keys));

  changed = FALSE;
  
  if (keyval != keys->accel_key)
    {
      keys->accel_key = keyval;
      g_object_notify (G_OBJECT (keys), "accel_key");
      changed = TRUE;
    }

  if (mask != keys->accel_mask)
    {
      keys->accel_mask = mask;

      g_object_notify (G_OBJECT (keys), "accel_mask");
      changed = TRUE;
    }  
  g_object_thaw_notify (G_OBJECT (keys));

  if (changed)
    {
      /* sync string to the key values */
      celltext = GTK_CELL_RENDERER_TEXT (keys);
      text = convert_keysym_state_to_string (keys->accel_key, keys->accel_mask);
      g_object_set (keys, "text", text, NULL);
    }
  
}


void
egg_cell_renderer_keys_get_accelerator (EggCellRendererKeys *keys,
                                        guint               *keyval,
                                        GdkModifierType     *mask)
{
  g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (keys));

  if (keyval)
    *keyval = keys->accel_key;

  if (mask)
    *mask = keys->accel_mask;
}
