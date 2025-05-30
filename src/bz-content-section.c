/* bz-content-section.c
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

#include "bz-content-section.h"
#include "bz-entry-group.h"

typedef struct
{
  char       *error;
  GListModel *classes;
  char       *title;
  char       *subtitle;
  char       *description;
  GListModel *images;
  GListModel *groups;
} BzContentSectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (BzContentSection, bz_content_section, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_ERROR,
  PROP_CLASSES,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_DESCRIPTION,
  PROP_IMAGES,
  PROP_APPIDS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_content_section_dispose (GObject *object)
{
  BzContentSection        *self = BZ_CONTENT_SECTION (object);
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  g_clear_pointer (&priv->error, g_free);
  g_clear_object (&priv->classes);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_object (&priv->images);
  g_clear_object (&priv->groups);

  G_OBJECT_CLASS (bz_content_section_parent_class)->dispose (object);
}

static void
bz_content_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzContentSection        *self = BZ_CONTENT_SECTION (object);
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ERROR:
      g_value_set_string (value, priv->error);
      break;
    case PROP_CLASSES:
      g_value_set_object (value, priv->classes);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_SUBTITLE:
      g_value_set_string (value, priv->subtitle);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_IMAGES:
      g_value_set_object (value, priv->images);
      break;
    case PROP_APPIDS:
      g_value_set_object (value, priv->groups);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_content_section_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzContentSection        *self = BZ_CONTENT_SECTION (object);
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ERROR:
      g_clear_pointer (&priv->error, g_free);
      priv->error = g_value_dup_string (value);
      break;
    case PROP_CLASSES:
      g_clear_object (&priv->classes);
      priv->classes = g_value_dup_object (value);
      break;
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_SUBTITLE:
      g_clear_pointer (&priv->subtitle, g_free);
      priv->subtitle = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
      break;
    case PROP_IMAGES:
      g_clear_object (&priv->images);
      priv->images = g_value_dup_object (value);
      break;
    case PROP_APPIDS:
      g_clear_object (&priv->groups);
      priv->groups = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_content_section_class_init (BzContentSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_content_section_set_property;
  object_class->get_property = bz_content_section_get_property;
  object_class->dispose      = bz_content_section_dispose;

  props[PROP_ERROR] =
      g_param_spec_string (
          "error",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_CLASSES] =
      g_param_spec_object (
          "classes",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SUBTITLE] =
      g_param_spec_string (
          "subtitle",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_IMAGES] =
      g_param_spec_object (
          "images",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_APPIDS] =
      g_param_spec_object (
          "appids",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_content_section_init (BzContentSection *self)
{
}
