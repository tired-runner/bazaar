/* ga-background.c
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

#include <adwaita.h>

#include "ga-background.h"
#include "ga-entry.h"
#include "ga-util.h"

struct _GaBackground
{
  GtkWidget parent_instance;

  guint       timeout;
  GTimer     *timer;
  GListModel *entries;
  GPtrArray  *instances;
  GArray     *sorted_instances;

  GtkEventControllerMotion *motion_controller;
  graphene_point_t          motion_offset;
  graphene_point_t          last_motion_offset;
  graphene_point_t          current_motion_offset;
  double                    motion_offset_start_time;
};

G_DEFINE_FINAL_TYPE (GaBackground, ga_background, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_ENTRIES,
  PROP_MOTION_CONTROLLER,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

GA_DEFINE_DATA (
    instance,
    Instance,
    {
      GskRenderNode *node;
      GskRenderNode *blurred;
      struct
      {
        graphene_point3d_t cur;
        graphene_point3d_t last;
        double             start;
      } position;
      struct
      {
        double cur;
        double last;
        double render;
        double start;
      } scale;
      gboolean hovering;
    },
    GA_RELEASE_DATA (node, gsk_render_node_unref);
    GA_RELEASE_DATA (blurred, gsk_render_node_unref))

static void
ga_background_measure (GtkWidget     *widget,
                       GtkOrientation orientation,
                       int            for_size,
                       int           *minimum,
                       int           *natural,
                       int           *minimum_baseline,
                       int           *natural_baseline);

static void
ga_background_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot);

static void
entries_changed (GListModel   *entries,
                 guint         position,
                 guint         removed,
                 guint         added,
                 GaBackground *self);

static gboolean
tick_timeout (GaBackground *self);

static gint
cmp_instance_position (const guint  *a,
                       const guint  *b,
                       GaBackground *item_area);

static void
move_motion (GtkEventControllerMotion *controller,
             double                    x,
             double                    y,
             GaBackground             *self);

static void
enter_motion (GtkEventControllerMotion *controller,
              double                    x,
              double                    y,
              GaBackground             *self);

static void
leave_motion (GtkEventControllerMotion *controller,
              GaBackground             *self);

static void
update_motion (GaBackground *self,
               double        x,
               double        y,
               gboolean      instant);

static void
ga_background_dispose (GObject *object)
{
  GaBackground *self = GA_BACKGROUND (object);

  if (self->entries != NULL)
    g_signal_handlers_disconnect_by_func (
        self->entries, entries_changed, self);

  if (self->motion_controller != NULL)
    {
      g_signal_handlers_disconnect_by_func (
          self->motion_controller, enter_motion, self);
      g_signal_handlers_disconnect_by_func (
          self->motion_controller, leave_motion, self);
      g_signal_handlers_disconnect_by_func (
          self->motion_controller, move_motion, self);
    }

  g_clear_handle_id (&self->timeout, g_source_remove);
  g_clear_pointer (&self->timer, g_timer_destroy);
  g_clear_object (&self->entries);
  g_clear_pointer (&self->instances, g_ptr_array_unref);
  g_clear_object (&self->motion_controller);

  G_OBJECT_CLASS (ga_background_parent_class)->dispose (object);
}

static void
ga_background_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GaBackground *self = GA_BACKGROUND (object);

  switch (prop_id)
    {
    case PROP_ENTRIES:
      g_value_set_object (value, ga_background_get_entries (self));
      break;
    case PROP_MOTION_CONTROLLER:
      g_value_set_object (value, ga_background_get_entries (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_background_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GaBackground *self = GA_BACKGROUND (object);

  switch (prop_id)
    {
    case PROP_ENTRIES:
      ga_background_set_entries (self, g_value_get_object (value));
      break;
    case PROP_MOTION_CONTROLLER:
      ga_background_set_motion_controller (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_background_class_init (GaBackgroundClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = ga_background_dispose;
  object_class->get_property = ga_background_get_property;
  object_class->set_property = ga_background_set_property;

  props[PROP_ENTRIES] =
      g_param_spec_object (
          "entries",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MOTION_CONTROLLER] =
      g_param_spec_object (
          "motion-controller",
          NULL, NULL,
          GTK_TYPE_EVENT_CONTROLLER_MOTION,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->measure  = ga_background_measure;
  widget_class->snapshot = ga_background_snapshot;
}

static void
ga_background_init (GaBackground *self)
{
  self->timeout = g_timeout_add (
      (1.0 / 60.0) * G_TIME_SPAN_MILLISECOND,
      (GSourceFunc) tick_timeout, self);
  self->timer = g_timer_new ();

  self->sorted_instances = g_array_new (FALSE, TRUE, sizeof (guint));
}

static void
ga_background_measure (GtkWidget     *widget,
                       GtkOrientation orientation,
                       int            for_size,
                       int           *minimum,
                       int           *natural,
                       int           *minimum_baseline,
                       int           *natural_baseline)
{
  *minimum = for_size;
  *natural = for_size;
}

static void
ga_background_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GaBackground    *self          = GA_BACKGROUND (widget);
  double           elapsed       = 0.0;
  int              widget_width  = 0;
  int              widget_height = 0;
  graphene_point_t motion_offset = { 0 };

  if (self->instances == NULL)
    return;

  elapsed       = g_timer_elapsed (self->timer, NULL);
  widget_width  = gtk_widget_get_width (widget);
  widget_height = gtk_widget_get_height (widget);

  if (elapsed - self->motion_offset_start_time > 1.0)
    motion_offset = self->motion_offset;
  else
    graphene_point_interpolate (
        &self->last_motion_offset,
        &self->motion_offset,
        adw_easing_ease (
            ADW_EASE_OUT_QUART,
            (elapsed - self->motion_offset_start_time) / 1.0),
        &motion_offset);
  self->current_motion_offset = motion_offset;

  motion_offset.x = ((double) widget_width / 2.0) -
                    0.05 * (motion_offset.x - (double) widget_width / 2.0);
  motion_offset.y = ((double) widget_height / 2.0) -
                    0.05 * (motion_offset.y - (double) widget_height / 2.0);

  gtk_snapshot_translate (snapshot, &motion_offset);
  gtk_snapshot_perspective (snapshot, 50);

  for (guint i = 0; i < self->sorted_instances->len; i++)
    {
      InstanceData      *instance        = NULL;
      graphene_rect_t    rect            = { 0 };
      graphene_point3d_t instance_offset = { 0 };
      double             instance_scale  = 0.0;

      instance = g_ptr_array_index (
          self->instances,
          g_array_index (self->sorted_instances, guint, i));
      gsk_render_node_get_bounds (instance->node, &rect);

      if (elapsed - instance->position.start > 2.0)
        instance_offset = instance->position.cur;
      else
        graphene_point3d_interpolate (
            &instance->position.last,
            &instance->position.cur,
            adw_easing_ease (
                ADW_EASE_OUT_ELASTIC,
                (elapsed - instance->position.start) / 2.0),
            &instance_offset);
      instance_offset.y += 30.0 * sin (elapsed * instance->position.cur.z / 500.0);
      instance_offset.z = 0.0;

      if (elapsed - instance->scale.start > 1.0)
        instance_scale = instance->scale.cur;
      else
        instance_scale =
            instance->scale.last +
            (instance->scale.cur - instance->scale.last) *
                adw_easing_ease (
                    ADW_EASE_OUT_EXPO,
                    (elapsed - instance->scale.start) / 1.0);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate_3d (snapshot, &instance_offset);
      gtk_snapshot_scale (snapshot, instance_scale, instance_scale);
      gtk_snapshot_append_node (snapshot, instance->blurred);
      gtk_snapshot_restore (snapshot);

      instance->scale.render = instance_scale;
    }
}

GtkWidget *
ga_background_new (void)
{
  return g_object_new (GA_TYPE_BACKGROUND, NULL);
}

void
ga_background_set_entries (GaBackground *self,
                           GListModel   *entries)
{
  g_return_if_fail (GA_IS_BACKGROUND (self));
  g_return_if_fail (G_IS_LIST_MODEL (entries));
  g_return_if_fail (g_list_model_get_item_type (entries) == GA_TYPE_ENTRY);

  if (self->entries != NULL)
    g_signal_handlers_disconnect_by_func (
        self->entries, entries_changed, self);
  g_clear_object (&self->entries);
  self->entries = entries != NULL ? g_object_ref (entries) : NULL;
  if (self->entries != NULL)
    g_signal_connect (self->entries, "items-changed",
                      G_CALLBACK (entries_changed), self);

  g_clear_pointer (&self->instances, g_ptr_array_unref);
  if (self->entries != NULL)
    {
      self->instances = g_ptr_array_new_with_free_func (
          instance_data_unref);
      entries_changed (
          self->entries, 0, 0,
          g_list_model_get_n_items (self->entries),
          self);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

GListModel *
ga_background_get_entries (GaBackground *self)
{
  g_return_val_if_fail (GA_IS_BACKGROUND (self), NULL);

  return self->entries;
}

void
ga_background_set_motion_controller (GaBackground             *self,
                                     GtkEventControllerMotion *controller)
{
  g_return_if_fail (GA_IS_BACKGROUND (self));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

  if (self->motion_controller != NULL)
    {
      g_signal_handlers_disconnect_by_func (
          self->motion_controller, enter_motion, self);
      g_signal_handlers_disconnect_by_func (
          self->motion_controller, leave_motion, self);
      g_signal_handlers_disconnect_by_func (
          self->motion_controller, move_motion, self);
    }
  g_clear_object (&self->motion_controller);
  self->motion_controller = controller != NULL ? g_object_ref (controller) : NULL;
  if (self->motion_controller != NULL)
    {
      g_signal_connect (self->motion_controller, "enter",
                        G_CALLBACK (enter_motion), self);
      g_signal_connect (self->motion_controller, "motion",
                        G_CALLBACK (move_motion), self);
      g_signal_connect (self->motion_controller, "leave",
                        G_CALLBACK (leave_motion), self);
    }
}

GtkEventControllerMotion *
ga_background_get_motion_controller (GaBackground *self)
{
  g_return_val_if_fail (GA_IS_BACKGROUND (self), NULL);

  return self->motion_controller;
}

static void
entries_changed (GListModel   *entries,
                 guint         position,
                 guint         removed,
                 guint         added,
                 GaBackground *self)
{
  if (removed > 0)
    g_ptr_array_remove_range (self->instances, position, removed);

  if (added > 0)
    {
      double elapsed = 0.0;

      elapsed = g_timer_elapsed (self->timer, NULL);

      g_ptr_array_set_size (self->instances, self->instances->len + added);
      for (guint i = 0; i < added; i++)
        {
          g_autoptr (GaEntry) entry         = NULL;
          GdkPaintable *paintable           = NULL;
          int           width               = 0;
          int           height              = 0;
          g_autoptr (GtkSnapshot) snapshot  = NULL;
          g_autoptr (InstanceData) instance = NULL;

          entry     = g_list_model_get_item (entries, position + i);
          paintable = ga_entry_get_icon_paintable (entry);
          width     = gdk_paintable_get_intrinsic_width (paintable);
          height    = gdk_paintable_get_intrinsic_height (paintable);

          snapshot = gtk_snapshot_new ();
          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (
              snapshot,
              &GRAPHENE_POINT_INIT (
                  (-(double) width / 2.0),
                  -(double) height / 2.0));
          gdk_paintable_snapshot (GDK_PAINTABLE (paintable), snapshot, width, height);
          gtk_snapshot_restore (snapshot);

          instance          = instance_data_new ();
          instance->node    = gtk_snapshot_to_node (snapshot);
          instance->blurred = NULL;

          instance->position.cur.x  = g_random_double_range (-500, 500);
          instance->position.cur.y  = g_random_double_range (-500, 500);
          instance->position.cur.z  = g_random_double_range (-1000, -100);
          instance->position.last.x = instance->position.cur.x * 2.0;
          instance->position.last.y = instance->position.cur.y * 2.0;
          instance->position.last.z = instance->position.cur.z;
          instance->position.start  = elapsed;

          instance->scale.cur    = 0.75;
          instance->scale.render = 0.75;
          instance->scale.last   = 0.5;
          instance->scale.start  = elapsed;

          g_ptr_array_index (self->instances, position + i) = instance_data_ref (instance);
        }
    }

  for (guint i = 0; i < self->instances->len; i++)
    {
      InstanceData *instance           = NULL;
      g_autoptr (GtkSnapshot) snapshot = NULL;

      instance = g_ptr_array_index (self->instances, i);
      g_clear_pointer (&instance->blurred, gsk_render_node_unref);

      snapshot = gtk_snapshot_new ();
      gtk_snapshot_push_blur (snapshot, -instance->position.cur.z / 25.0);
      gtk_snapshot_append_node (snapshot, instance->node);
      gtk_snapshot_pop (snapshot);

      instance->blurred = gtk_snapshot_to_node (snapshot);
    }

  g_array_set_size (self->sorted_instances, self->instances->len);
  for (guint i = 0; i < self->sorted_instances->len; i++)
    g_array_index (self->sorted_instances, guint, i) = i;
  g_array_sort_with_data (
      self->sorted_instances,
      (GCompareDataFunc) cmp_instance_position,
      self);
}

static gboolean
tick_timeout (GaBackground *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
  return G_SOURCE_CONTINUE;
}

static gint
cmp_instance_position (const guint  *a,
                       const guint  *b,
                       GaBackground *self)
{
  InstanceData *instance_a = NULL;
  InstanceData *instance_b = NULL;

  instance_a = g_ptr_array_index (self->instances, *a);
  instance_b = g_ptr_array_index (self->instances, *b);

  return instance_a->position.cur.z > instance_b->position.cur.z ? 1 : -1;
}

static void
move_motion (GtkEventControllerMotion *controller,
             double                    x,
             double                    y,
             GaBackground             *self)
{
  update_motion (self, x, y, TRUE);
}

static void
enter_motion (GtkEventControllerMotion *controller,
              double                    x,
              double                    y,
              GaBackground             *self)
{
  update_motion (self, x, y, FALSE);
}

static void
leave_motion (GtkEventControllerMotion *controller,
              GaBackground             *self)
{
  update_motion (
      self,
      self->current_motion_offset.x,
      self->current_motion_offset.y,
      FALSE);
}

static void
update_motion (GaBackground *self,
               double        x,
               double        y,
               gboolean      instant)
{
  int      widget_width  = 0;
  int      widget_height = 0;
  double   elapsed       = 0.0;
  gboolean found_hover   = FALSE;

  if (!instant)
    {
      self->last_motion_offset =
          self->current_motion_offset;
      self->motion_offset_start_time =
          g_timer_elapsed (self->timer, NULL);
    }

  self->motion_offset.x = x;
  self->motion_offset.y = y;

  widget_width  = gtk_widget_get_width (GTK_WIDGET (self));
  widget_height = gtk_widget_get_height (GTK_WIDGET (self));
  elapsed       = g_timer_elapsed (self->timer, NULL);

  for (guint i = self->sorted_instances->len; i > 0; i--)
    {
      InstanceData *instance = NULL;
      gboolean      revert   = FALSE;

      instance = g_ptr_array_index (
          self->instances,
          g_array_index (self->sorted_instances, guint, i - 1));

      if (found_hover)
        revert = instance->hovering;
      else
        {
          graphene_rect_t rect = { 0 };

          gsk_render_node_get_bounds (instance->node, &rect);
          rect.origin.x *= 0.75;
          rect.origin.y *= 0.75;
          rect.size.width *= 0.75;
          rect.size.height *= 0.75;
          rect.origin.x += (double) widget_width / 2.0 + instance->position.cur.x +
                           0.05 * (self->motion_offset.x - (double) widget_width / 2.0);
          rect.origin.y += (double) widget_height / 2.0 + instance->position.cur.y +
                           0.05 * (self->motion_offset.y - (double) widget_height / 2.0);

          if (graphene_rect_contains_point (&rect, &GRAPHENE_POINT_INIT (x, y)))
            {
              if (!instance->hovering)
                {
                  instance->hovering    = TRUE;
                  instance->scale.last  = instance->scale.render;
                  instance->scale.cur   = 1.0;
                  instance->scale.start = elapsed;
                }
              found_hover = TRUE;
            }
          else
            revert = instance->hovering;
        }

      if (revert)
        {
          instance->hovering    = FALSE;
          instance->scale.last  = instance->scale.render;
          instance->scale.cur   = 0.75;
          instance->scale.start = elapsed;
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
