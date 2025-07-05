/* bz-flathub-category.c
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

#include "bz-flathub-category.h"

struct _BzFlathubCategory
{
  GObject parent_instance;

  BzApplicationMapFactory *map_factory;
  char                    *name;
  GListModel              *applications;
};

G_DEFINE_FINAL_TYPE (BzFlathubCategory, bz_flathub_category, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_MAP_FACTORY,
  PROP_NAME,
  PROP_APPLICATIONS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_flathub_category_dispose (GObject *object)
{
  BzFlathubCategory *self = BZ_FLATHUB_CATEGORY (object);

  g_clear_pointer (&self->map_factory, g_object_unref);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->applications, g_object_unref);

  G_OBJECT_CLASS (bz_flathub_category_parent_class)->dispose (object);
}

static void
bz_flathub_category_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzFlathubCategory *self = BZ_FLATHUB_CATEGORY (object);

  switch (prop_id)
    {
    case PROP_MAP_FACTORY:
      g_value_set_object (value, bz_flathub_category_get_map_factory (self));
      break;
    case PROP_NAME:
      g_value_set_string (value, bz_flathub_category_get_name (self));
      break;
    case PROP_APPLICATIONS:
      g_value_take_object (value, bz_flathub_category_dup_applications (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_category_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzFlathubCategory *self = BZ_FLATHUB_CATEGORY (object);

  switch (prop_id)
    {
    case PROP_MAP_FACTORY:
      bz_flathub_category_set_map_factory (self, g_value_get_object (value));
      break;
    case PROP_NAME:
      bz_flathub_category_set_name (self, g_value_get_string (value));
      break;
    case PROP_APPLICATIONS:
      bz_flathub_category_set_applications (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_category_class_init (BzFlathubCategoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_flathub_category_set_property;
  object_class->get_property = bz_flathub_category_get_property;
  object_class->dispose      = bz_flathub_category_dispose;

  props[PROP_MAP_FACTORY] =
      g_param_spec_object (
          "map-factory",
          NULL, NULL,
          BZ_TYPE_APPLICATION_MAP_FACTORY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_APPLICATIONS] =
      g_param_spec_object (
          "applications",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_flathub_category_init (BzFlathubCategory *self)
{
}

BzFlathubCategory *
bz_flathub_category_new (void)
{
  return g_object_new (BZ_TYPE_FLATHUB_CATEGORY, NULL);
}

BzApplicationMapFactory *
bz_flathub_category_get_map_factory (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return self->map_factory;
}

const char *
bz_flathub_category_get_name (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return self->name;
}

GListModel *
bz_flathub_category_dup_applications (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);

  if (self->applications != NULL)
    {
      if (self->map_factory != NULL)
        return bz_application_map_factory_generate (
            self->map_factory, G_LIST_MODEL (self->applications));
      else
        return G_LIST_MODEL (g_object_ref (self->applications));
    }
  else
    return NULL;
}

void
bz_flathub_category_set_map_factory (BzFlathubCategory       *self,
                                     BzApplicationMapFactory *map_factory)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));
  g_return_if_fail (map_factory == NULL || BZ_IS_APPLICATION_MAP_FACTORY (map_factory));

  g_clear_object (&self->map_factory);
  if (map_factory != NULL)
    self->map_factory = g_object_ref (map_factory);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAP_FACTORY]);
}

void
bz_flathub_category_set_name (BzFlathubCategory *self,
                              const char        *name)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

void
bz_flathub_category_set_applications (BzFlathubCategory *self,
                                      GListModel        *applications)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));

  g_clear_pointer (&self->applications, g_object_unref);
  if (applications != NULL)
    self->applications = g_object_ref (applications);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPLICATIONS]);
}

/* End of bz-flathub-category.c */
