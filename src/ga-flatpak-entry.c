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
#include <glycin-gtk4.h>
#include <xmlb.h>

#include "ga-flatpak-private.h"
#include "ga-paintable-model.h"

enum
{
  NO_ELEMENT,
  PARAGRAPH,
  ORDERED_LIST,
  UNORDERED_LIST,
  LIST_ITEM,
  CODE,
  EMPHASIS,
};

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
compile_appstream_description (XbNode  *node,
                               GString *string,
                               int      parent_kind,
                               int      idx);

static inline void
append_markup_escaped (GString    *string,
                       const char *append);

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
  g_autoptr (GaFlatpakEntry) self         = NULL;
  GBytes *bytes                           = NULL;
  g_autoptr (GError) local_error          = NULL;
  g_autoptr (GKeyFile) key_file           = NULL;
  gboolean         result                 = FALSE;
  const char      *title                  = NULL;
  const char      *description            = NULL;
  const char      *metadata_license       = NULL;
  const char      *project_license        = NULL;
  gboolean         is_floss               = FALSE;
  const char      *project_group          = NULL;
  const char      *developer              = NULL;
  const char      *developer_id           = NULL;
  g_autofree char *long_description       = NULL;
  const char      *remote_name            = NULL;
  g_autoptr (GPtrArray) as_search_tokens  = NULL;
  g_autoptr (GPtrArray) search_tokens     = NULL;
  g_autoptr (GdkTexture) icon_paintable   = NULL;
  g_autoptr (GaPaintableModel) paintables = NULL;

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
      const char  *long_description_raw = NULL;
      AsIcon      *icon                 = NULL;
      AsDeveloper *developer_obj        = NULL;
      GPtrArray   *screenshots          = NULL;

      title = as_component_get_name (component);
      if (title == NULL)
        title = as_component_get_id (component);

      description      = as_component_get_summary (component);
      metadata_license = as_component_get_metadata_license (component);
      project_license  = as_component_get_project_license (component);
      is_floss         = as_component_is_floss (component);
      project_group    = as_component_get_project_group (component);
      as_search_tokens = as_component_get_search_tokens (component);

      developer_obj = as_component_get_developer (component);
      if (developer_obj != NULL)
        {
          developer    = as_developer_get_name (developer_obj);
          developer_id = as_developer_get_id (developer_obj);
        }

      long_description_raw = as_component_get_description (component);
      if (long_description_raw != NULL)
        {
          g_autoptr (XbSilo) silo          = NULL;
          g_autoptr (XbNode) root          = NULL;
          g_autoptr (GString) string       = NULL;
          g_autoptr (GRegex) cleanup_regex = NULL;

          silo = xb_silo_new_from_xml (long_description_raw, &local_error);
          if (silo == NULL)
            goto err;

          root   = xb_silo_get_root (silo);
          string = g_string_new (NULL);

          cleanup_regex = g_regex_new (
              "^ +| +$|\\t+|\\A\\s+|\\s+\\z",
              G_REGEX_MULTILINE,
              G_REGEX_MATCH_DEFAULT,
              NULL);
          g_assert (cleanup_regex != NULL);

          for (int i = 0; root != NULL; i++)
            {
              const char *tail = NULL;
              XbNode     *next = NULL;

              compile_appstream_description (root, string, NO_ELEMENT, i);

              tail = xb_node_get_tail (root);
              if (tail != NULL)
                append_markup_escaped (string, tail);

              next = xb_node_get_next (root);
              g_object_unref (root);
              root = next;
            }

          /* Could be better but bleh */
          g_string_replace (string, "  ", "", 0);
          long_description = g_regex_replace (
              cleanup_regex, string->str, -1, 0,
              "", G_REGEX_MATCH_DEFAULT, NULL);
        }

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

      screenshots = as_component_get_screenshots_all (component);
      if (screenshots != NULL)
        {
          g_autoptr (GListStore) files = NULL;

          files = g_list_store_new (G_TYPE_FILE);

          for (guint i = 0; i < screenshots->len; i++)
            {
              AsScreenshot *screenshot = NULL;
              GPtrArray    *images     = NULL;

              screenshot = g_ptr_array_index (screenshots, i);
              images     = as_screenshot_get_images_all (screenshot);

              for (guint j = 0; j < images->len; j++)
                {
                  AsImage    *image_obj             = NULL;
                  const char *url                   = NULL;
                  g_autoptr (GFile) screenshot_file = NULL;

                  image_obj       = g_ptr_array_index (images, j);
                  url             = as_image_get_url (image_obj);
                  screenshot_file = g_file_new_for_uri (url);

                  if (screenshot_file != NULL)
                    {
                      g_list_store_append (files, screenshot_file);
                      break;
                    }
                }
            }

          if (g_list_model_get_n_items (G_LIST_MODEL (files)) > 0)
            paintables = ga_paintable_model_new (
                dex_scheduler_get_thread_default (),
                G_LIST_MODEL (files));
        }
    }

  search_tokens = g_ptr_array_new_with_free_func (g_free);
  if (as_search_tokens != NULL)
    {
      for (guint i = 0; i < as_search_tokens->len; i++)
        g_ptr_array_add (
            search_tokens,
            g_strdup (g_ptr_array_index (as_search_tokens, i)));
    }

  g_ptr_array_add (search_tokens, g_strdup (self->name));
  g_ptr_array_add (search_tokens, g_strdup (self->runtime));
  g_ptr_array_add (search_tokens, g_strdup (self->command));

  if (title != NULL)
    g_ptr_array_add (search_tokens, g_strdup (title));
  else
    title = self->name;

  if (description != NULL)
    g_ptr_array_add (search_tokens, g_strdup (description));
  if (long_description != NULL)
    g_ptr_array_add (search_tokens, g_strdup (long_description));

  remote_name = flatpak_remote_get_name (remote);
  if (remote_name != NULL)
    g_ptr_array_add (search_tokens, g_strdup (remote_name));

  if (metadata_license != NULL)
    g_ptr_array_add (search_tokens, g_strdup (metadata_license));
  if (project_license != NULL)
    g_ptr_array_add (search_tokens, g_strdup (project_license));
  if (project_group != NULL)
    g_ptr_array_add (search_tokens, g_strdup (project_group));
  if (developer != NULL)
    g_ptr_array_add (search_tokens, g_strdup (developer));
  if (developer_id != NULL)
    g_ptr_array_add (search_tokens, g_strdup (developer_id));
  if (metadata_license != NULL)
    g_ptr_array_add (search_tokens, g_strdup (metadata_license));

  g_object_set (
      self,
      "title", title,
      "description", description,
      "long-description", long_description,
      "remote-repo-name", remote_name,
      "size", flatpak_remote_ref_get_installed_size (rref),
      "icon-paintable", icon_paintable,
      "search-tokens", search_tokens,
      "remote-repo-icon", remote_icon,
      "metadata-license", metadata_license,
      "project-license", project_license,
      "is-floss", is_floss,
      "project-group", project_group,
      "developer", developer,
      "developer-id", developer_id,
      "screenshot-paintables", paintables,
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

gboolean
ga_flatpak_entry_launch (GaFlatpakEntry *self,
                         GError        **error)
{
  FlatpakInstallation *installation = NULL;

  g_return_val_if_fail (GA_IS_FLATPAK_ENTRY (self), FALSE);

  installation = ga_flatpak_instance_get_installation (self->flatpak);

  /* async? */
  return flatpak_installation_launch (
      installation,
      flatpak_ref_get_name (FLATPAK_REF (self->rref)),
      flatpak_ref_get_arch (FLATPAK_REF (self->rref)),
      flatpak_ref_get_branch (FLATPAK_REF (self->rref)),
      flatpak_ref_get_commit (FLATPAK_REF (self->rref)),
      NULL, error);
}

static void
compile_appstream_description (XbNode  *node,
                               GString *string,
                               int      parent_kind,
                               int      idx)
{
  XbNode     *child   = NULL;
  const char *element = NULL;
  const char *text    = NULL;
  int         kind    = NO_ELEMENT;

  child   = xb_node_get_child (node);
  element = xb_node_get_element (node);
  text    = xb_node_get_text (node);

  if (element != NULL)
    {
      if (g_strcmp0 (element, "p") == 0)
        kind = PARAGRAPH;
      else if (g_strcmp0 (element, "ol") == 0)
        kind = ORDERED_LIST;
      else if (g_strcmp0 (element, "ul") == 0)
        kind = UNORDERED_LIST;
      else if (g_strcmp0 (element, "li") == 0)
        kind = LIST_ITEM;
      else if (g_strcmp0 (element, "code") == 0)
        kind = CODE;
      else if (g_strcmp0 (element, "em") == 0)
        kind = EMPHASIS;
    }

  if (string->len > 0 &&
      (kind == PARAGRAPH ||
       kind == ORDERED_LIST ||
       kind == UNORDERED_LIST))
    g_string_append (string, "\n");

  if (kind == EMPHASIS)
    g_string_append (string, "<b>");
  else if (kind == CODE)
    g_string_append (string, "<tt>");

  if (kind == LIST_ITEM)
    {
      switch (parent_kind)
        {
        case ORDERED_LIST:
          g_string_append_printf (string, "%d. ", idx);
          break;
        case UNORDERED_LIST:
          g_string_append (string, "- ");
          break;
        default:
          break;
        }
    }

  if (text != NULL)
    append_markup_escaped (string, text);

  for (int i = 0; child != NULL; i++)
    {
      const char *tail = NULL;
      XbNode     *next = NULL;

      compile_appstream_description (child, string, kind, i);

      tail = xb_node_get_tail (child);
      if (tail != NULL)
        append_markup_escaped (string, tail);

      next = xb_node_get_next (child);
      g_object_unref (child);
      child = next;
    }

  if (kind == EMPHASIS)
    g_string_append (string, "</b>");
  else if (kind == CODE)
    g_string_append (string, "</tt>");
  else
    g_string_append (string, "\n");
}

static inline void
append_markup_escaped (GString    *string,
                       const char *append)
{
  g_autofree char *escaped = NULL;

  escaped = g_markup_escape_text (append, -1);
  g_string_append (string, escaped);
}
