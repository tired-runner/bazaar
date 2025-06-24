/* bz-data-point.c
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

#include "bz-data-point.h"

struct _BzDataPoint
{
  GObject parent_instance;

  double independent;
  double dependent;
  char  *label;
};

G_DEFINE_FINAL_TYPE (BzDataPoint, bz_data_point, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_INDEPENDENT,
  PROP_DEPENDENT,
  PROP_LABEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_data_point_dispose (GObject *object)
{
  BzDataPoint *self = BZ_DATA_POINT (object);

  g_clear_pointer (&self->label, g_free);

  G_OBJECT_CLASS (bz_data_point_parent_class)->dispose (object);
}

static void
bz_data_point_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BzDataPoint *self = BZ_DATA_POINT (object);

  switch (prop_id)
    {
    case PROP_INDEPENDENT:
      g_value_set_double (value, bz_data_point_get_independent (self));
      break;
    case PROP_DEPENDENT:
      g_value_set_double (value, bz_data_point_get_dependent (self));
      break;
    case PROP_LABEL:
      g_value_set_string (value, bz_data_point_get_label (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_data_point_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BzDataPoint *self = BZ_DATA_POINT (object);

  switch (prop_id)
    {
    case PROP_INDEPENDENT:
      bz_data_point_set_independent (self, g_value_get_double (value));
      break;
    case PROP_DEPENDENT:
      bz_data_point_set_dependent (self, g_value_get_double (value));
      break;
    case PROP_LABEL:
      bz_data_point_set_label (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_data_point_class_init (BzDataPointClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_data_point_set_property;
  object_class->get_property = bz_data_point_get_property;
  object_class->dispose      = bz_data_point_dispose;

  props[PROP_INDEPENDENT] =
      g_param_spec_double (
          "independent",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEPENDENT] =
      g_param_spec_double (
          "dependent",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_LABEL] =
      g_param_spec_string (
          "label",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_data_point_init (BzDataPoint *self)
{
}

BzDataPoint *
bz_data_point_new (void)
{
  return g_object_new (BZ_TYPE_DATA_POINT, NULL);
}

double
bz_data_point_get_independent (BzDataPoint *self)
{
  g_return_val_if_fail (BZ_IS_DATA_POINT (self), 0.0);
  return self->independent;
}

double
bz_data_point_get_dependent (BzDataPoint *self)
{
  g_return_val_if_fail (BZ_IS_DATA_POINT (self), 0.0);
  return self->dependent;
}

const char *
bz_data_point_get_label (BzDataPoint *self)
{
  g_return_val_if_fail (BZ_IS_DATA_POINT (self), NULL);
  return self->label;
}

void
bz_data_point_set_independent (BzDataPoint *self,
                               double       independent)
{
  g_return_if_fail (BZ_IS_DATA_POINT (self));

  self->independent = independent;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INDEPENDENT]);
}

void
bz_data_point_set_dependent (BzDataPoint *self,
                             double       dependent)
{
  g_return_if_fail (BZ_IS_DATA_POINT (self));

  self->dependent = dependent;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEPENDENT]);
}

void
bz_data_point_set_label (BzDataPoint *self,
                         const char  *label)
{
  g_return_if_fail (BZ_IS_DATA_POINT (self));

  g_clear_pointer (&self->label, g_free);
  if (label != NULL)
    self->label = g_strdup (label);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LABEL]);
}

/* End of bz-data-point.c */
