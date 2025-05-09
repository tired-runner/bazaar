/* ga-flatpak-entry.c
 *
 * Copyright 2025 Adam Masciola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <appstream.h>

#include "ga-flatpak-private.h"

struct _GaFlatpakEntry
{
  GaEntry parent_instance;

  GaFlatpakInstance *flatpak;
  FlatpakRemoteRef  *rref;

  char *name;
  char *runtime;
  char *command;
};

G_DEFINE_FINAL_TYPE (GaFlatpakEntry, ga_flatpak_entry, GA_TYPE_ENTRY)

enum
{
  PROP_0,

  PROP_INSTANCE,
  PROP_NAME,
  PROP_RUNTIME,
  PROP_COMMAND,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
ga_flatpak_entry_dispose (GObject *object)
{
  GaFlatpakEntry *self = GA_FLATPAK_ENTRY (object);

  g_clear_object (&self->flatpak);
  g_clear_object (&self->rref);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->runtime, g_free);
  g_clear_pointer (&self->command, g_free);

  G_OBJECT_CLASS (ga_flatpak_entry_parent_class)->dispose (object);
}

static void
ga_flatpak_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GaFlatpakEntry *self = GA_FLATPAK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_INSTANCE:
      g_value_set_object (value, self->flatpak);
      break;
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_RUNTIME:
      g_value_set_string (value, self->runtime);
      break;
    case PROP_COMMAND:
      g_value_set_string (value, self->command);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_flatpak_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GaFlatpakEntry *self = GA_FLATPAK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_INSTANCE:
    case PROP_NAME:
    case PROP_RUNTIME:
    case PROP_COMMAND:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_flatpak_entry_class_init (GaFlatpakEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ga_flatpak_entry_set_property;
  object_class->get_property = ga_flatpak_entry_get_property;
  object_class->dispose      = ga_flatpak_entry_dispose;

  props[PROP_INSTANCE] =
      g_param_spec_object (
          "instance",
          NULL, NULL,
          GA_TYPE_FLATPAK_INSTANCE,
          G_PARAM_READABLE);

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_RUNTIME] =
      g_param_spec_string (
          "runtime",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_COMMAND] =
      g_param_spec_string (
          "command",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
ga_flatpak_entry_init (GaFlatpakEntry *self)
{
}

GaFlatpakEntry *
ga_flatpak_entry_new_for_remote_ref (GaFlatpakInstance *instance,
                                     FlatpakRemote     *remote,
                                     FlatpakRemoteRef  *rref,
                                     AsComponent       *component,
                                     const char        *appstream_dir,
                                     GdkPaintable      *remote_icon,
                                     GError           **error)
{
  g_autoptr (GaFlatpakEntry) self       = NULL;
  GBytes *bytes                         = NULL;
  g_autoptr (GError) local_error        = NULL;
  g_autoptr (GKeyFile) key_file         = NULL;
  gboolean         result               = FALSE;
  const char      *title                = NULL;
  const char      *description          = NULL;
  g_autofree char *description_fmt      = NULL;
  g_autoptr (GPtrArray) search_tokens   = NULL;
  g_autoptr (GdkTexture) icon_paintable = NULL;

  g_return_val_if_fail (GA_IS_FLATPAK_INSTANCE (instance), NULL);
  g_return_val_if_fail (FLATPAK_IS_REMOTE_REF (rref), NULL);
  g_return_val_if_fail (appstream_dir != NULL, NULL);

  self          = g_object_new (GA_TYPE_FLATPAK_ENTRY, NULL);
  self->flatpak = g_object_ref (instance);
  self->rref    = g_object_ref (rref);

  key_file = g_key_file_new ();
  bytes    = flatpak_remote_ref_get_metadata (rref);

  result = g_key_file_load_from_bytes (
      key_file, bytes, G_KEY_FILE_NONE, &local_error);
  if (!result)
    goto err;

#define GET_STRING(member, group_name, key)       \
  G_STMT_START                                    \
  {                                               \
    self->member = g_key_file_get_string (        \
        key_file, group_name, key, &local_error); \
    if (self->member == NULL)                     \
      goto err;                                   \
  }                                               \
  G_STMT_END

  GET_STRING (name, "Application", "name");
  GET_STRING (runtime, "Application", "runtime");
  GET_STRING (command, "Application", "command");

#undef GET_STRING

  /* TODO: permissions, runtimes */

  if (component != NULL)
    {
      AsIcon *icon = NULL;

      title = as_component_get_name (component);
      if (title == NULL)
        title = as_component_get_id (component);

      description   = as_component_get_summary (component);
      search_tokens = as_component_get_search_tokens (component);

      icon = as_component_get_icon_stock (component);
      if (icon != NULL)
        {
          for (int i = 128; i > 0; i -= 64)
            {
              g_autofree char *resolution = NULL;
              g_autofree char *basename   = NULL;
              g_autoptr (GFile) icon_file = NULL;

              resolution = g_strdup_printf ("%dx%d", i, i);
              basename   = g_strdup_printf ("%s.png", as_icon_get_name (icon));
              icon_file  = g_file_new_build_filename (
                  appstream_dir,
                  "icons",
                  "flatpak",
                  resolution,
                  basename,
                  NULL);
              if (g_file_query_exists (icon_file, NULL))
                {
                  /* Using glycin here is extremely slow for,
                   * some reason so we'll opt for the old method.
                   */
                  icon_paintable = gdk_texture_new_from_file (icon_file, NULL);
                  break;
                }
            }
        }
    }

  if (title == NULL)
    title = self->name;
  if (description != NULL)
    description_fmt = g_strdup_printf (
        "%s (%s)", description, flatpak_remote_get_name (remote));
  else
    description_fmt = g_strdup_printf (
        "No summary found! (%s)", flatpak_remote_get_name (remote));

  if (search_tokens == NULL)
    search_tokens = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (search_tokens, g_strdup (title));
  g_ptr_array_add (search_tokens, g_strdup (description_fmt));
  g_ptr_array_add (search_tokens, g_strdup (self->runtime));
  g_ptr_array_add (search_tokens, g_strdup (self->command));

  g_object_set (
      self,
      "title", title,
      "description", description_fmt,
      "size", flatpak_remote_ref_get_installed_size (rref),
      "icon-paintable", icon_paintable,
      "search-tokens", search_tokens,
      "remote-repo-icon", remote_icon,
      NULL);

  return g_steal_pointer (&self);

err:
  g_propagate_error (error, g_steal_pointer (&local_error));
  return NULL;
}

FlatpakRef *
ga_flatpak_entry_get_ref (GaFlatpakEntry *self)
{
  g_return_val_if_fail (GA_IS_FLATPAK_ENTRY (self), NULL);

  return FLATPAK_REF (self->rref);
}

const char *
ga_flatpak_entry_get_name (GaFlatpakEntry *self)
{
  g_return_val_if_fail (GA_IS_FLATPAK_ENTRY (self), NULL);

  return self->name;
}
