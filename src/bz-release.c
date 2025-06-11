/* bz-release.c
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

#include "bz-release.h"

struct _BzRelease
{
  GObject parent_instance;

  GListModel *issues;
  guint64     timestamp;
  char       *url;
  char       *version;
};

G_DEFINE_FINAL_TYPE (BzRelease, bz_release, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_ISSUES,
  PROP_TIMESTAMP,
  PROP_URL,
  PROP_VERSION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_release_dispose (GObject *object)
{
  BzRelease *self = BZ_RELEASE (object);

  g_clear_pointer (&self->issues, g_object_unref);
  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->version, g_free);

  G_OBJECT_CLASS (bz_release_parent_class)->dispose (object);
}

static void
bz_release_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  BzRelease *self = BZ_RELEASE (object);

  switch (prop_id)
    {
    case PROP_ISSUES:
      g_value_set_object (value, self->issues);
      break;
    case PROP_TIMESTAMP:
      g_value_set_uint64 (value, self->timestamp);
      break;
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;
    case PROP_VERSION:
      g_value_set_string (value, self->version);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_release_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  BzRelease *self = BZ_RELEASE (object);

  switch (prop_id)
    {
    case PROP_ISSUES:
      g_clear_pointer (&self->issues, g_object_unref);
      self->issues = g_value_dup_object (value);
      break;
    case PROP_TIMESTAMP:
      self->timestamp = g_value_get_uint64 (value);
      break;
    case PROP_URL:
      g_clear_pointer (&self->url, g_free);
      self->url = g_value_dup_string (value);
      break;
    case PROP_VERSION:
      g_clear_pointer (&self->version, g_free);
      self->version = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_release_class_init (BzReleaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_release_set_property;
  object_class->get_property = bz_release_get_property;
  object_class->dispose      = bz_release_dispose;

  props[PROP_ISSUES] =
      g_param_spec_object (
          "issues",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_TIMESTAMP] =
      g_param_spec_uint64 (
          "timestamp",
          NULL, NULL,
          0, G_MAXUINT64, (guint64) 0,
          G_PARAM_READWRITE);

  props[PROP_URL] =
      g_param_spec_string (
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_VERSION] =
      g_param_spec_string (
          "version",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_release_init (BzRelease *self)
{
}

/* End of bz-release.c */
