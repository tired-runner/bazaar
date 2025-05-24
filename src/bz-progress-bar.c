/* bz-progress-bar.c
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

#include "bz-progress-bar.h"

struct _BzProgressBar
{
  AdwBin parent_instance;

  double fraction;

  AdwAnimation *animation;

  /* Template widgets */
  GtkProgressBar *bar;
};

G_DEFINE_FINAL_TYPE (BzProgressBar, bz_progress_bar, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_FRACTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_progress_bar_dispose (GObject *object)
{
  BzProgressBar *self = BZ_PROGRESS_BAR (object);

  g_clear_object (&self->animation);

  G_OBJECT_CLASS (bz_progress_bar_parent_class)->dispose (object);
}

static void
bz_progress_bar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzProgressBar *self = BZ_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      g_value_set_double (value, bz_progress_bar_get_fraction (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_progress_bar_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzProgressBar *self = BZ_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      bz_progress_bar_set_fraction (self, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_progress_bar_class_init (BzProgressBarClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_progress_bar_dispose;
  object_class->get_property = bz_progress_bar_get_property;
  object_class->set_property = bz_progress_bar_set_property;

  props[PROP_FRACTION] =
      g_param_spec_double (
          "fraction",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-progress-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, BzProgressBar, bar);
}

static void
bz_progress_bar_init (BzProgressBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_progress_bar_new (void)
{
  return g_object_new (BZ_TYPE_PROGRESS_BAR, NULL);
}

void
bz_progress_bar_set_fraction (BzProgressBar *self,
                              double         fraction)
{
  double last_fraction = 0.0;

  g_return_if_fail (BZ_IS_PROGRESS_BAR (self));

  last_fraction  = self->fraction;
  self->fraction = CLAMP (fraction, 0.0, 1.0);

  g_clear_object (&self->animation);

  if (G_APPROX_VALUE (last_fraction, self->fraction, 0.001))
    gtk_progress_bar_set_fraction (self->bar, self->fraction);
  else
    {
      AdwAnimationTarget *target = NULL;

      target          = adw_property_animation_target_new (G_OBJECT (self->bar), "fraction");
      self->animation = adw_timed_animation_new (GTK_WIDGET (self), last_fraction, self->fraction, 50, target);
      // adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->animation), ADW_EASE_IN_OUT_QUAD);

      adw_animation_play (self->animation);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FRACTION]);
}

double
bz_progress_bar_get_fraction (BzProgressBar *self)
{
  g_return_val_if_fail (BZ_IS_PROGRESS_BAR (self), 0.0);
  return self->fraction;
}
