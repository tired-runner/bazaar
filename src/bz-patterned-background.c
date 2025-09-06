/* bz-patterned-background.c
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

#include "bz-patterned-background.h"

struct _BzPatternedBackground
{
  GtkWidget parent_instance;

  GtkWidget *widget;
  char      *tint;
};

G_DEFINE_FINAL_TYPE (BzPatternedBackground, bz_patterned_background, GTK_TYPE_WIDGET);

enum
{
  PROP_0,

  PROP_WIDGET,
  PROP_TINT,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_patterned_background_dispose (GObject *object)
{
  BzPatternedBackground *self = BZ_PATTERNED_BACKGROUND (object);

  g_clear_pointer (&self->widget, gtk_widget_unparent);
  g_clear_pointer (&self->tint, g_free);

  G_OBJECT_CLASS (bz_patterned_background_parent_class)->dispose (object);
}

static void
bz_patterned_background_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  BzPatternedBackground *self = BZ_PATTERNED_BACKGROUND (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, bz_patterned_background_get_widget (self));
      break;
    case PROP_TINT:
      g_value_set_string (value, bz_patterned_background_get_tint (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_patterned_background_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  BzPatternedBackground *self = BZ_PATTERNED_BACKGROUND (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      bz_patterned_background_set_widget (self, g_value_get_object (value));
      break;
    case PROP_TINT:
      bz_patterned_background_set_tint (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_patterned_background_size_allocate (GtkWidget *widget,
                                       int        width,
                                       int        height,
                                       int        baseline)
{
  BzPatternedBackground *self = BZ_PATTERNED_BACKGROUND (widget);

  if (self->widget != NULL)
    gtk_widget_allocate (self->widget, width, height, baseline, NULL);
}

static void
bz_patterned_background_snapshot (GtkWidget   *widget,
                                  GtkSnapshot *snapshot)
{
  BzPatternedBackground *self             = BZ_PATTERNED_BACKGROUND (widget);
  double                 width            = 0.0;
  double                 height           = 0.0;
  g_autoptr (GtkSnapshot) prop_snapshot   = NULL;
  g_autoptr (GskRenderNode) prop_node     = NULL;
  graphene_rect_t prop_bounds             = { 0 };
  g_autoptr (GRand) rand                  = NULL;
  g_autoptr (GtkSnapshot) random_snapshot = NULL;
  g_autoptr (GskRenderNode) random_node   = NULL;
  graphene_rect_t random_bounds           = { 0 };

  if (self->widget == NULL)
    return;

  width  = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  prop_snapshot = gtk_snapshot_new ();
  gtk_widget_snapshot_child (widget, self->widget, prop_snapshot);
  prop_node = gtk_snapshot_to_node (prop_snapshot);
  gsk_render_node_get_bounds (prop_node, &prop_bounds);

  rand = g_rand_new ();
  g_rand_set_seed (rand, g_direct_hash (self->widget));

  random_snapshot = gtk_snapshot_new ();
  for (int y = 0; y < 10; y++)
    {
      for (int x = 0; x < 10; x++)
        {
          double coord_x  = 0.0;
          double coord_y  = 0.0;
          double rotation = 0.0;

          coord_x  = (double) x * (prop_bounds.size.width + 20.0);
          coord_y  = (double) y * (prop_bounds.size.height + 20.0);
          rotation = g_rand_double_range (rand, 0.0, 360.0);

          gtk_snapshot_save (random_snapshot);
          gtk_snapshot_translate (
              random_snapshot,
              &GRAPHENE_POINT_INIT (coord_x, coord_y));
          gtk_snapshot_translate (
              random_snapshot,
              &GRAPHENE_POINT_INIT (
                  prop_bounds.origin.x + prop_bounds.size.width / 2.0,
                  prop_bounds.origin.y + prop_bounds.size.height / 2.0));
          gtk_snapshot_rotate (random_snapshot, rotation);
          gtk_snapshot_translate (
              random_snapshot,
              &GRAPHENE_POINT_INIT (
                  -(prop_bounds.origin.x + prop_bounds.size.width / 2.0),
                  -(prop_bounds.origin.y + prop_bounds.size.height / 2.0)));
          gtk_snapshot_append_node (random_snapshot, prop_node);
          gtk_snapshot_restore (random_snapshot);
        }
    }
  random_node = gtk_snapshot_to_node (random_snapshot);
  gsk_render_node_get_bounds (random_node, &random_bounds);

  if (self->tint != NULL)
    gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
  gtk_snapshot_push_repeat (
      snapshot,
      &GRAPHENE_RECT_INIT (0.0, 0.0, width, height),
      &random_bounds);
  gtk_snapshot_append_node (snapshot, random_node);
  gtk_snapshot_pop (snapshot);
  if (self->tint != NULL)
    {
      GdkRGBA rgba = { 0 };

      gdk_rgba_parse (&rgba, self->tint);
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_append_color (
          snapshot, &rgba,
          &GRAPHENE_RECT_INIT (0.0, 0.0, width, height));
      gtk_snapshot_pop (snapshot);
    }
}

static void
bz_patterned_background_class_init (BzPatternedBackgroundClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_patterned_background_set_property;
  object_class->get_property = bz_patterned_background_get_property;
  object_class->dispose      = bz_patterned_background_dispose;

  props[PROP_WIDGET] =
      g_param_spec_object (
          "widget",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TINT] =
      g_param_spec_string (
          "tint",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->size_allocate = bz_patterned_background_size_allocate;
  widget_class->snapshot      = bz_patterned_background_snapshot;
}

static void
bz_patterned_background_init (BzPatternedBackground *self)
{
}

BzPatternedBackground *
bz_patterned_background_new (void)
{
  return g_object_new (BZ_TYPE_PATTERNED_BACKGROUND, NULL);
}

GtkWidget *
bz_patterned_background_get_widget (BzPatternedBackground *self)
{
  g_return_val_if_fail (BZ_IS_PATTERNED_BACKGROUND (self), NULL);
  return self->widget;
}

void
bz_patterned_background_set_widget (BzPatternedBackground *self,
                                    GtkWidget             *widget)
{
  g_return_if_fail (BZ_IS_PATTERNED_BACKGROUND (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (self->widget == widget)
    return;

  if (widget != NULL)
    g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  g_clear_pointer (&self->widget, gtk_widget_unparent);
  self->widget = widget;

  if (widget != NULL)
    gtk_widget_set_parent (widget, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_WIDGET]);
}

const char *
bz_patterned_background_get_tint (BzPatternedBackground *self)
{
  g_return_val_if_fail (BZ_IS_PATTERNED_BACKGROUND (self), NULL);
  return self->tint;
}

void
bz_patterned_background_set_tint (BzPatternedBackground *self,
                                  const char            *tint)
{
  g_return_if_fail (BZ_IS_PATTERNED_BACKGROUND (self));

  g_clear_pointer (&self->tint, g_free);
  if (tint != NULL)
    self->tint = g_strdup (tint);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TINT]);
}

/* End of bz-patterned-background.c */
