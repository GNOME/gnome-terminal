/*
 *  Copyright © 2008, 2010, 2011, 2022 Christian Persch
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#define G_SETTINGS_ENABLE_BACKEND

#include <cassert>

#include "terminal-settings-bridge-impl.hh"

#include "terminal-app.hh"
#include "terminal-dconf.hh"
#include "terminal-debug.hh"
#include "terminal-libgsystem.hh"
#include "terminal-settings-bridge-generated.h"

#include <gio/gio.h>
#include <gio/gsettingsbackend.h>

enum {
  PROP_SETTINGS_BACKEND = 1,
};

struct _TerminalSettingsBridgeImpl {
  TerminalSettingsBridgeSkeleton parent_instance;

  GSettingsBackend* backend;
  GSettingsBackendClass* backend_class;
  void* tag;
};

struct _TerminalSettingsBridgeImplClass {
  TerminalSettingsBridgeSkeletonClass parent_class;
};

// Note that since D-Bus doesn't support maybe values, we use
// arrays with either zero or one item to send/receive a maybe.
// If we get more than one item, just use the first one.

/* helper functions */

template<typename T>
static inline constexpr auto
IMPL(T* that) noexcept
{
  return reinterpret_cast<TerminalSettingsBridgeImpl*>(that);
}

static inline int
compare_string(void const* a,
               void const* b,
               void* closure)
{
  return strcmp(reinterpret_cast<char const*>(a), reinterpret_cast<char const*>(b));
}

static void
unref_variant0(void* data) noexcept
{
  if (data)
    g_variant_unref(reinterpret_cast<GVariant*>(data));
}

static GVariantType*
type_from_string(GDBusMethodInvocation* invocation,
                 char const* type) noexcept
{
  if (!g_variant_type_string_is_valid(type)) {
    g_dbus_method_invocation_return_error(invocation,
                                          G_DBUS_ERROR,
                                          G_DBUS_ERROR_INVALID_ARGS,
                                          "Invalid type: %s",
                                          type);
    return nullptr;
  }

  return g_variant_type_new(type);
}

static auto
unwrap(GVariantIter* iter) noexcept
{
  gs_unref_variant auto iv = g_variant_iter_next_value(iter);
  return iv ? g_variant_get_variant(iv) : nullptr;
}

static auto
unwrap(GVariant* value) noexcept
{
  auto iter = GVariantIter{};
  g_variant_iter_init(&iter, value);
  return unwrap(&iter);
}

static auto
value(GDBusMethodInvocation* invocation,
      char const* format,
      ...) noexcept
{
  va_list args;
  va_start(args, format);
  auto const v = g_variant_new_va(format, nullptr, &args);
  va_end(args);
  g_dbus_method_invocation_return_value(invocation, v);
  return true;
}

static auto
wrap(GDBusMethodInvocation* invocation,
     GVariant* variant) noexcept
{
  auto builder = GVariantBuilder{};
  g_variant_builder_init(&builder, G_VARIANT_TYPE("av"));
  if (variant)
    g_variant_builder_add(&builder, "v", variant);

  return value(invocation, "(av)", &builder);
}

static auto
nothing(GDBusMethodInvocation* invocation) noexcept
{
  return value(invocation, "()");
}

static auto
novalue(GDBusMethodInvocation* invocation) noexcept
{
  g_dbus_method_invocation_return_error_literal(invocation,
                                                G_DBUS_ERROR,
                                                G_DBUS_ERROR_FAILED,
                                                "No value");
  return true;
}

static auto
success(GDBusMethodInvocation* invocation,
        bool v = true) noexcept
{
  return value(invocation, "(b)", v);
}

/* TerminalSettingsBridge interface implementation */

static gboolean
terminal_settings_bridge_impl_clone_schema(TerminalSettingsBridge* object,
                                           GDBusMethodInvocation* invocation,
                                           char const* schema_id,
                                           char const* path_from,
                                           char const* path_to,
                                           GVariant* asv)
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::clone_schema schema %s from-path %s to-path %s\n",
                        schema_id, path_from, path_to);

  auto const impl = IMPL(object);
  if (terminal_dconf_backend_is_dconf(impl->backend)) {
    auto const schema_source = terminal_app_get_schema_source(terminal_app_get());
    terminal_dconf_clone_schemav(schema_source, schema_id, path_from, path_to, asv);
  }

  return nothing(invocation);
}

static gboolean
terminal_settings_bridge_impl_erase_path(TerminalSettingsBridge* object,
                                         GDBusMethodInvocation* invocation,
                                         char const* path)
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::erase_path path %s\n",
                        path);

  auto const impl = IMPL(object);
  if (terminal_dconf_backend_is_dconf(impl->backend)) {
    terminal_dconf_erase_path(path);
  }

  return nothing(invocation);
}

static gboolean
terminal_settings_bridge_impl_get_permission(TerminalSettingsBridge* object,
                                             GDBusMethodInvocation* invocation,
                                             char const* path) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::get_permission path %s\n",
                        path);

  return novalue(invocation);
}

static gboolean
terminal_settings_bridge_impl_get_writable(TerminalSettingsBridge* object,
                                           GDBusMethodInvocation* invocation,
                                           char const* key) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::get_writable key %s\n",
                        key);

  auto const impl = IMPL(object);
  auto const v = impl->backend_class->get_writable(impl->backend, key);
  return success(invocation, v);
}

static gboolean
terminal_settings_bridge_impl_read(TerminalSettingsBridge* object,
                                   GDBusMethodInvocation* invocation,
                                   char const* key,
                                   char const* type,
                                   gboolean default_value) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::read key %s type %s default %d\n",
                        key, type, default_value);

  auto const vtype = type_from_string(invocation, type);
  if (!vtype)
    return true;

  auto const impl = IMPL(object);
  gs_unref_variant auto v = impl->backend_class->read(impl->backend, key, vtype, default_value);
  if (v)
    g_variant_take_ref(v);

  g_variant_type_free(vtype);
  return wrap(invocation, v);
}

static gboolean
terminal_settings_bridge_impl_read_user_value(TerminalSettingsBridge* object,
                                              GDBusMethodInvocation* invocation,
                                              char const* key,
                                              char const* type) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::read_user_value key %s type %s\n",
                        key, type);

  auto const vtype = type_from_string(invocation, type);
  if (!vtype)
    return true;

  auto const impl = IMPL(object);
  gs_unref_variant auto v = impl->backend_class->read_user_value(impl->backend, key, vtype);
  if (v)
    g_variant_take_ref(v);

  g_variant_type_free(vtype);
  return wrap(invocation, v);
}

static gboolean
terminal_settings_bridge_impl_reset(TerminalSettingsBridge* object,
                                    GDBusMethodInvocation* invocation,
                                    char const* key) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::reset key %s\n",
                        key);

  auto const impl = IMPL(object);
  impl->backend_class->reset(impl->backend, key, impl->tag);
  return nothing(invocation);
}

static gboolean
terminal_settings_bridge_impl_subscribe(TerminalSettingsBridge* object,
                                        GDBusMethodInvocation* invocation,
                                        char const* name) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::subscribe name %s\n",
                        name);

  auto const impl = IMPL(object);
  impl->backend_class->subscribe(impl->backend, name);
  return nothing(invocation);
}

static gboolean
terminal_settings_bridge_impl_sync(TerminalSettingsBridge* object,
                                   GDBusMethodInvocation* invocation) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::sync\n");

  auto const impl = IMPL(object);
  impl->backend_class->sync(impl->backend);
  return nothing(invocation);
}

static gboolean
terminal_settings_bridge_impl_unsubscribe(TerminalSettingsBridge* object,
                                          GDBusMethodInvocation* invocation,
                                          char const* name) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::unsubscribe name %s\n",
                        name);

  auto const impl = IMPL(object);
  impl->backend_class->subscribe(impl->backend, name);
  return nothing(invocation);
}

static gboolean
terminal_settings_bridge_impl_write(TerminalSettingsBridge* object,
                                    GDBusMethodInvocation* invocation,
                                    char const* key,
                                    GVariant* value) noexcept
{
  auto const impl = IMPL(object);
  gs_unref_variant auto v = unwrap(value);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::write key %s value %s\n",
                        key, v ? g_variant_print(v, true): "(null)");

  gs_unref_variant auto holder = g_variant_ref_sink(v);
  auto const r = impl->backend_class->write(impl->backend, key, v, impl->tag);
  return success(invocation, r);
}

static gboolean
terminal_settings_bridge_impl_write_tree(TerminalSettingsBridge* object,
                                         GDBusMethodInvocation* invocation,
                                         char const* path_prefix,
                                         GVariant* tree_value) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge impl ::write_tree path-prefix %s\n",
                        path_prefix);

  auto const tree = g_tree_new_full(compare_string,
                                    nullptr,
                                    g_free,
                                    unref_variant0);

  auto iter = GVariantIter{};
  g_variant_iter_init(&iter, tree_value);

  char const* key = nullptr;
  GVariantIter* viter = nullptr;
  while (g_variant_iter_loop(&iter, "(&sav)", &key, &viter)) {
    g_tree_insert(tree,
                  g_strconcat(path_prefix, key, nullptr), // adopts
                  unwrap(viter)); // adopts
  }

  auto const impl = IMPL(object);
  auto const v = impl->backend_class->write_tree(impl->backend, tree, impl->tag);

  g_tree_unref(tree);
  return success(invocation, v);
}

static void
terminal_settings_bridge_impl_iface_init(TerminalSettingsBridgeIface* iface) noexcept
{
  iface->handle_clone_schema = terminal_settings_bridge_impl_clone_schema;
  iface->handle_erase_path = terminal_settings_bridge_impl_erase_path;
  iface->handle_get_permission = terminal_settings_bridge_impl_get_permission;
  iface->handle_get_writable = terminal_settings_bridge_impl_get_writable;
  iface->handle_read = terminal_settings_bridge_impl_read;
  iface->handle_read_user_value = terminal_settings_bridge_impl_read_user_value;
  iface->handle_reset = terminal_settings_bridge_impl_reset;
  iface->handle_subscribe = terminal_settings_bridge_impl_subscribe;
  iface->handle_sync= terminal_settings_bridge_impl_sync;
  iface->handle_unsubscribe = terminal_settings_bridge_impl_unsubscribe;
  iface->handle_write = terminal_settings_bridge_impl_write;
  iface->handle_write_tree = terminal_settings_bridge_impl_write_tree;
}

/* GObject class implementation */

G_DEFINE_TYPE_WITH_CODE(TerminalSettingsBridgeImpl,
                        terminal_settings_bridge_impl,
                        TERMINAL_TYPE_SETTINGS_BRIDGE_SKELETON,
                        G_IMPLEMENT_INTERFACE(TERMINAL_TYPE_SETTINGS_BRIDGE,
                                              terminal_settings_bridge_impl_iface_init));

static void
terminal_settings_bridge_impl_init(TerminalSettingsBridgeImpl* impl) /* noexcept */
{
  impl->tag = &impl->tag;
}

static void
terminal_settings_bridge_impl_constructed(GObject* object) noexcept
{
  G_OBJECT_CLASS(terminal_settings_bridge_impl_parent_class)->constructed(object);

  auto const impl = IMPL(object);
  assert(impl->backend);
  impl->backend_class = G_SETTINGS_BACKEND_GET_CLASS(impl->backend);
  assert(impl->backend_class);
}

static void
terminal_settings_bridge_impl_finalize(GObject* object) noexcept
{
  auto const impl = IMPL(object);
  impl->backend_class = nullptr;
  g_clear_object(&impl->backend);

  G_OBJECT_CLASS(terminal_settings_bridge_impl_parent_class)->finalize(object);
}

static void
terminal_settings_bridge_impl_set_property(GObject* object,
                                           guint prop_id,
                                           GValue const* value,
                                           GParamSpec* pspec) noexcept
{
  auto const impl = IMPL(object);

  switch (prop_id) {
  case PROP_SETTINGS_BACKEND:
    impl->backend = G_SETTINGS_BACKEND(g_value_dup_object(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
terminal_settings_bridge_impl_class_init(TerminalSettingsBridgeImplClass* klass) /* noexcept */
{
  auto const gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->constructed = terminal_settings_bridge_impl_constructed;
  gobject_class->finalize = terminal_settings_bridge_impl_finalize;
  gobject_class->set_property = terminal_settings_bridge_impl_set_property;

  g_object_class_install_property
    (gobject_class,
     PROP_SETTINGS_BACKEND,
     g_param_spec_object("settings-backend", nullptr, nullptr,
                         G_TYPE_SETTINGS_BACKEND,
                         GParamFlags(G_PARAM_WRITABLE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS)));
}

/* public API */

/**
*  terminal_settings_bridge_impl_new:
*  @backend: a #GSettingsBackend
*
*  Returns: (transfer full): a new #TerminalSettingsBridgeImpl for @backend
 */
TerminalSettingsBridgeImpl*
terminal_settings_bridge_impl_new(GSettingsBackend* backend)
{
  return reinterpret_cast<TerminalSettingsBridgeImpl*>
    (g_object_new (TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL,
		   "settings-backend", backend,
		   nullptr));
}
