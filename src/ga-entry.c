/* ga-entry.c
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

#include "ga-entry.h"

typedef struct
{
  char         *title;
  char         *description;
  guint64       size;
  GdkPaintable *icon_paintable;
  GPtrArray    *search_tokens;
} GaEntryPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GaEntry, ga_entry, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_TITLE,
  PROP_DESCRIPTION,
  PROP_SIZE,
  PROP_ICON_PAINTABLE,
  PROP_SEARCH_TOKENS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
ga_entry_dispose (GObject *object)
{
  GaEntry        *self = GA_ENTRY (object);
  GaEntryPrivate *priv = ga_entry_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->description, g_free);

  g_clear_object (&priv->icon_paintable);
  g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);

  G_OBJECT_CLASS (ga_entry_parent_class)->dispose (object);
}

static void
ga_entry_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GaEntry        *self = GA_ENTRY (object);
  GaEntryPrivate *priv = ga_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_entry_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GaEntry        *self = GA_ENTRY (object);
  GaEntryPrivate *priv = ga_entry_get_instance_private (self);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_entry_class_init (GaEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ga_entry_set_property;
  object_class->get_property = ga_entry_get_property;
  object_class->dispose      = ga_entry_dispose;

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

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
ga_entry_init (GaEntry *priv)
{
}

const char *
ga_entry_get_title (GaEntry *self)
{
  GaEntryPrivate *priv = NULL;

  g_return_val_if_fail (GA_IS_ENTRY (self), NULL);

  priv = ga_entry_get_instance_private (self);
  return priv->title;
}

const char *
ga_entry_get_description (GaEntry *self)
{
  GaEntryPrivate *priv = NULL;

  g_return_val_if_fail (GA_IS_ENTRY (self), NULL);

  priv = ga_entry_get_instance_private (self);
  return priv->description;
}

guint64
ga_entry_get_size (GaEntry *self)
{
  GaEntryPrivate *priv = NULL;

  g_return_val_if_fail (GA_IS_ENTRY (self), 0);

  priv = ga_entry_get_instance_private (self);
  return priv->size;
}

GdkPaintable *
ga_entry_get_icon_paintable (GaEntry *self)
{
  GaEntryPrivate *priv = NULL;

  g_return_val_if_fail (GA_IS_ENTRY (self), 0);

  priv = ga_entry_get_instance_private (self);
  return priv->icon_paintable;
}

GPtrArray *
ga_entry_get_search_tokens (GaEntry *self)
{
  GaEntryPrivate *priv = NULL;

  g_return_val_if_fail (GA_IS_ENTRY (self), NULL);

  priv = ga_entry_get_instance_private (self);
  return priv->search_tokens;
}
