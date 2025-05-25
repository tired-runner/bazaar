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
  char         *id;
  char         *unique_id;
  char         *title;
  char         *eol;
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
  GListModel   *share_urls;
  GListModel   *reviews;
  double        average_rating;
  char         *ratings_summary;
} BzEntryPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BzEntry, bz_entry, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_ID,
  PROP_UNIQUE_ID,
  PROP_TITLE,
  PROP_EOL,
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
  PROP_SHARE_URLS,
  PROP_REVIEWS,
  PROP_AVERAGE_RATING,
  PROP_RATINGS_SUMMARY,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_entry_dispose (GObject *object)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->unique_id, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->eol, g_free);
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
  g_clear_object (&priv->share_urls);
  g_clear_object (&priv->reviews);
  g_clear_pointer (&priv->ratings_summary, g_free);

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
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;
    case PROP_UNIQUE_ID:
      g_value_set_string (value, priv->unique_id);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_EOL:
      g_value_set_string (value, priv->eol);
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
    case PROP_SHARE_URLS:
      g_value_set_object (value, priv->share_urls);
      break;
    case PROP_REVIEWS:
      g_value_set_object (value, priv->reviews);
      break;
    case PROP_AVERAGE_RATING:
      g_value_set_double (value, priv->average_rating);
      break;
    case PROP_RATINGS_SUMMARY:
      g_value_set_string (value, priv->ratings_summary);
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
    case PROP_ID:
      g_clear_pointer (&priv->id, g_free);
      priv->id = g_value_dup_string (value);
      break;
    case PROP_UNIQUE_ID:
      g_clear_pointer (&priv->unique_id, g_free);
      priv->unique_id = g_value_dup_string (value);
      break;
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_EOL:
      g_clear_pointer (&priv->eol, g_free);
      priv->eol = g_value_dup_string (value);
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
    case PROP_SHARE_URLS:
      g_clear_object (&priv->share_urls);
      priv->share_urls = g_value_dup_object (value);
      break;
    case PROP_REVIEWS:
      g_clear_object (&priv->reviews);
      priv->reviews = g_value_dup_object (value);
      break;
    case PROP_AVERAGE_RATING:
      priv->average_rating = g_value_get_double (value);
      break;
    case PROP_RATINGS_SUMMARY:
      g_clear_pointer (&priv->ratings_summary, g_free);
      priv->ratings_summary = g_value_dup_string (value);
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

  props[PROP_ID] =
      g_param_spec_string (
          "id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_UNIQUE_ID] =
      g_param_spec_string (
          "unique-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_EOL] =
      g_param_spec_string (
          "eol",
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
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_NAME] =
      g_param_spec_string (
          "remote-repo-name",
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

  props[PROP_SHARE_URLS] =
      g_param_spec_object (
          "share-urls",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_REVIEWS] =
      g_param_spec_object (
          "reviews",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_AVERAGE_RATING] =
      g_param_spec_double (
          "average-rating",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE);

  props[PROP_RATINGS_SUMMARY] =
      g_param_spec_string (
          "ratings-summary",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_init (BzEntry *self)
{
}

const char *
bz_entry_get_id (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->id;
}

const char *
bz_entry_get_unique_id (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->unique_id;
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
bz_entry_get_eol (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->eol;
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

gint
bz_entry_cmp_usefulness (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data)
{
  BzEntryPrivate *priv_a  = bz_entry_get_instance_private (BZ_ENTRY ((gpointer) a));
  BzEntryPrivate *priv_b  = bz_entry_get_instance_private (BZ_ENTRY ((gpointer) b));
  int             a_score = 0;
  int             b_score = 0;

  a_score += priv_a->title != NULL ? 1 : 0;
  a_score += priv_a->description != NULL ? 1 : 0;
  a_score += priv_a->long_description != NULL ? 1 : 0;
  a_score += priv_a->url != NULL ? 1 : 0;
  a_score += priv_a->size > 0 ? 1 : 0;
  a_score += priv_a->icon_paintable != NULL ? 1 : 0;
  a_score += priv_a->remote_repo_icon != NULL ? 1 : 0;
  a_score += priv_a->metadata_license != NULL ? 1 : 0;
  a_score += priv_a->project_license != NULL ? 1 : 0;
  a_score += priv_a->project_group != NULL ? 1 : 0;
  a_score += priv_a->developer != NULL ? 1 : 0;
  a_score += priv_a->developer_id != NULL ? 1 : 0;
  a_score += priv_a->screenshot_paintables != NULL ? 1 : 0;
  a_score += priv_a->share_urls != NULL ? 1 : 0;
  a_score += priv_a->reviews != NULL ? 1 : 0;

  b_score += priv_b->title != NULL ? 1 : 0;
  b_score += priv_b->description != NULL ? 1 : 0;
  b_score += priv_b->long_description != NULL ? 1 : 0;
  b_score += priv_b->url != NULL ? 1 : 0;
  b_score += priv_b->size > 0 ? 1 : 0;
  b_score += priv_b->icon_paintable != NULL ? 1 : 0;
  b_score += priv_b->remote_repo_icon != NULL ? 1 : 0;
  b_score += priv_b->metadata_license != NULL ? 1 : 0;
  b_score += priv_b->project_license != NULL ? 1 : 0;
  b_score += priv_b->project_group != NULL ? 1 : 0;
  b_score += priv_b->developer != NULL ? 1 : 0;
  b_score += priv_b->developer_id != NULL ? 1 : 0;
  b_score += priv_b->screenshot_paintables != NULL ? 1 : 0;
  b_score += priv_b->share_urls != NULL ? 1 : 0;
  b_score += priv_b->reviews != NULL ? 1 : 0;

  return b_score - a_score;
}
