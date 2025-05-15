/* bz-entry.c
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

#include "bz-entry.h"

typedef struct
{
  char         *title;
  char         *description;
  char         *long_description;
  char         *remote_repo_name;
  char         *url;
  guint64       size;
  GdkPaintable *icon_paintable;
  GdkPaintable *remote_repo_icon;
  GPtrArray    *search_tokens;
  char         *metadata_license;
  char         *project_license;
  gboolean      is_floss;
  char         *project_group;
  char         *developer;
  char         *developer_id;
  GListModel   *screenshot_paintables;
} BzEntryPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BzEntry, bz_entry, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_TITLE,
  PROP_DESCRIPTION,
  PROP_LONG_DESCRIPTION,
  PROP_REMOTE_REPO_NAME,
  PROP_URL,
  PROP_SIZE,
  PROP_ICON_PAINTABLE,
  PROP_SEARCH_TOKENS,
  PROP_REMOTE_REPO_ICON,
  PROP_METADATA_LICENSE,
  PROP_PROJECT_LICENSE,
  PROP_IS_FLOSS,
  PROP_PROJECT_GROUP,
  PROP_DEVELOPER,
  PROP_DEVELOPER_ID,
  PROP_SCREENSHOT_PAINTABLES,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_entry_dispose (GObject *object)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->long_description, g_free);
  g_clear_pointer (&priv->remote_repo_name, g_free);
  g_clear_pointer (&priv->url, g_free);
  g_clear_object (&priv->icon_paintable);
  g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);
  g_clear_object (&priv->remote_repo_icon);
  g_clear_pointer (&priv->metadata_license, g_free);
  g_clear_pointer (&priv->project_license, g_free);
  g_clear_pointer (&priv->project_group, g_free);
  g_clear_pointer (&priv->developer, g_free);
  g_clear_pointer (&priv->developer_id, g_free);
  g_clear_object (&priv->screenshot_paintables);

  G_OBJECT_CLASS (bz_entry_parent_class)->dispose (object);
}

static void
bz_entry_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_LONG_DESCRIPTION:
      g_value_set_string (value, priv->long_description);
      break;
    case PROP_REMOTE_REPO_NAME:
      g_value_set_string (value, priv->remote_repo_name);
      break;
    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;
    case PROP_SIZE:
      g_value_set_uint64 (value, priv->size);
      break;
    case PROP_ICON_PAINTABLE:
      g_value_set_object (value, priv->icon_paintable);
      break;
    case PROP_SEARCH_TOKENS:
      g_value_set_boxed (value, priv->search_tokens);
      break;
    case PROP_REMOTE_REPO_ICON:
      g_value_set_object (value, priv->remote_repo_icon);
      break;
    case PROP_METADATA_LICENSE:
      g_value_set_string (value, priv->metadata_license);
      break;
    case PROP_PROJECT_LICENSE:
      g_value_set_string (value, priv->project_license);
      break;
    case PROP_IS_FLOSS:
      g_value_set_boolean (value, priv->is_floss);
      break;
    case PROP_PROJECT_GROUP:
      g_value_set_string (value, priv->project_group);
      break;
    case PROP_DEVELOPER:
      g_value_set_string (value, priv->developer);
      break;
    case PROP_DEVELOPER_ID:
      g_value_set_string (value, priv->developer_id);
      break;
    case PROP_SCREENSHOT_PAINTABLES:
      g_value_set_object (value, priv->screenshot_paintables);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
      break;
    case PROP_LONG_DESCRIPTION:
      g_clear_pointer (&priv->long_description, g_free);
      priv->long_description = g_value_dup_string (value);
      break;
    case PROP_REMOTE_REPO_NAME:
      g_clear_pointer (&priv->remote_repo_name, g_free);
      priv->remote_repo_name = g_value_dup_string (value);
      break;
    case PROP_URL:
      g_clear_pointer (&priv->url, g_free);
      priv->url = g_value_dup_string (value);
      break;
    case PROP_SIZE:
      priv->size = g_value_get_uint64 (value);
      break;
    case PROP_ICON_PAINTABLE:
      g_clear_object (&priv->icon_paintable);
      priv->icon_paintable = g_value_dup_object (value);
      break;
    case PROP_SEARCH_TOKENS:
      g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);
      priv->search_tokens = g_value_dup_boxed (value);
      break;
    case PROP_REMOTE_REPO_ICON:
      g_clear_object (&priv->remote_repo_icon);
      priv->remote_repo_icon = g_value_dup_object (value);
      break;
    case PROP_METADATA_LICENSE:
      g_clear_pointer (&priv->metadata_license, g_free);
      priv->metadata_license = g_value_dup_string (value);
      break;
    case PROP_PROJECT_LICENSE:
      g_clear_pointer (&priv->project_license, g_free);
      priv->project_license = g_value_dup_string (value);
      break;
    case PROP_IS_FLOSS:
      priv->is_floss = g_value_get_boolean (value);
      break;
    case PROP_PROJECT_GROUP:
      g_clear_pointer (&priv->project_group, g_free);
      priv->project_group = g_value_dup_string (value);
      break;
    case PROP_DEVELOPER:
      g_clear_pointer (&priv->developer, g_free);
      priv->developer = g_value_dup_string (value);
      break;
    case PROP_DEVELOPER_ID:
      g_clear_pointer (&priv->developer_id, g_free);
      priv->developer_id = g_value_dup_string (value);
      break;
    case PROP_SCREENSHOT_PAINTABLES:
      g_clear_object (&priv->screenshot_paintables);
      priv->screenshot_paintables = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_class_init (BzEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_entry_set_property;
  object_class->get_property = bz_entry_get_property;
  object_class->dispose      = bz_entry_dispose;

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_LONG_DESCRIPTION] =
      g_param_spec_string (
          "long-description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_URL] =
      g_param_spec_string (
          "remote-repo-name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_NAME] =
      g_param_spec_string (
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SIZE] =
      g_param_spec_uint64 (
          "size",
          NULL, NULL,
          0, G_MAXUINT64, 0,
          G_PARAM_READWRITE);

  props[PROP_ICON_PAINTABLE] =
      g_param_spec_object (
          "icon-paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE);

  props[PROP_SEARCH_TOKENS] =
      g_param_spec_boxed (
          "search-tokens",
          NULL, NULL,
          G_TYPE_PTR_ARRAY,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_ICON] =
      g_param_spec_object (
          "remote-repo-icon",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE);

  props[PROP_METADATA_LICENSE] =
      g_param_spec_string (
          "metadata-license",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_PROJECT_LICENSE] =
      g_param_spec_string (
          "project-license",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_IS_FLOSS] =
      g_param_spec_boolean (
          "is-floss",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_PROJECT_GROUP] =
      g_param_spec_string (
          "project-group",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DEVELOPER] =
      g_param_spec_string (
          "developer",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DEVELOPER_ID] =
      g_param_spec_string (
          "developer-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SCREENSHOT_PAINTABLES] =
      g_param_spec_object (
          "screenshot-paintables",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_init (BzEntry *priv)
{
}

const char *
bz_entry_get_title (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->title;
}

const char *
bz_entry_get_description (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->description;
}

const char *
bz_entry_get_long_description (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->long_description;
}

const char *
bz_entry_get_remote_repo_name (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->remote_repo_name;
}

guint64
bz_entry_get_size (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->size;
}

GdkPaintable *
bz_entry_get_icon_paintable (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->icon_paintable;
}

GPtrArray *
bz_entry_get_search_tokens (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->search_tokens;
}
