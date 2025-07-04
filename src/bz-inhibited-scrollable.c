/* bz-inhibited-scrollable.c
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

/* This is a complete hack but I
 * don't see any other way to prevent
 * the annoying jumping in GtkListView etc
 */

#include "config.h"

#include "bz-inhibited-scrollable.h"

struct _BzInhibitedScrollable
{
  GtkWidget parent_instance;

  GtkScrollable *child;

  GtkScrollablePolicy hscroll_policy;
  GtkScrollablePolicy vscroll_policy;
  GtkAdjustment      *hadjustment;
  GtkAdjustment      *vadjustment;
  GtkAdjustment      *child_hadjustment;
  GtkAdjustment      *child_vadjustment;

  GBinding *hscroll_policy_bind;
  GBinding *vscroll_policy_bind;

  struct
  {
    GBinding *lower;
    GBinding *upper;
    GBinding *page_increment;
    GBinding *step_increment;
    GBinding *page_size;
    GBinding *value;
  } vadjustment_binds;
  struct
  {
    GBinding *lower;
    GBinding *upper;
    GBinding *page_increment;
    GBinding *step_increment;
    GBinding *page_size;
    GBinding *value;
  } hadjustment_binds;
};

static void scrollable_iface_init (GtkScrollableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzInhibitedScrollable,
    bz_inhibited_scrollable,
    GTK_TYPE_WIDGET,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, scrollable_iface_init))

enum
{
  PROP_0,

  PROP_SCROLLABLE,

  LAST_PROP,

  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
child_adjustment_value_changed (BzInhibitedScrollable *self,
                                GtkAdjustment         *adjustment);

static void
setup_hadjustments (BzInhibitedScrollable *self);

static void
setup_vadjustments (BzInhibitedScrollable *self);

static void
clear_bindings (BzInhibitedScrollable *self);

static void
bz_inhibited_scrollable_dispose (GObject *object)
{
  BzInhibitedScrollable *self = BZ_INHIBITED_SCROLLABLE (object);

  clear_bindings (self);
  g_clear_object (&self->hadjustment);
  g_clear_object (&self->vadjustment);
  g_clear_pointer ((GtkWidget **) &self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (bz_inhibited_scrollable_parent_class)->dispose (object);
}

static void
bz_inhibited_scrollable_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  BzInhibitedScrollable *self = BZ_INHIBITED_SCROLLABLE (object);

  switch (prop_id)
    {
    case PROP_SCROLLABLE:
      g_value_set_object (value, bz_inhibited_scrollable_get_scrollable (self));
      break;
    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, self->vadjustment);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, self->hscroll_policy);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, self->vscroll_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_inhibited_scrollable_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  BzInhibitedScrollable *self = BZ_INHIBITED_SCROLLABLE (object);

  switch (prop_id)
    {
    case PROP_SCROLLABLE:
      bz_inhibited_scrollable_set_scrollable (self, g_value_get_object (value));
      break;
    case PROP_HADJUSTMENT:
      g_clear_object (&self->hadjustment);
      self->hadjustment = g_value_dup_object (value);
      setup_hadjustments (self);
      break;
    case PROP_VADJUSTMENT:
      g_clear_object (&self->vadjustment);
      self->vadjustment = g_value_dup_object (value);
      setup_vadjustments (self);
      break;
    case PROP_HSCROLL_POLICY:
      self->hscroll_policy = g_value_get_enum (value);
      break;
    case PROP_VSCROLL_POLICY:
      self->vscroll_policy = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_inhibited_scrollable_size_allocate (GtkWidget *widget,
                                       int        width,
                                       int        height,
                                       int        baseline)
{
  BzInhibitedScrollable *self = BZ_INHIBITED_SCROLLABLE (widget);

  if (self->child != NULL && gtk_widget_should_layout (GTK_WIDGET (self->child)))
    gtk_widget_allocate (GTK_WIDGET (self->child), width, height, baseline, NULL);
}

static void
bz_inhibited_scrollable_class_init (BzInhibitedScrollableClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_inhibited_scrollable_dispose;
  object_class->get_property = bz_inhibited_scrollable_get_property;
  object_class->set_property = bz_inhibited_scrollable_set_property;

  props[PROP_SCROLLABLE] =
      g_param_spec_object (
          "scrollable",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  widget_class->size_allocate = bz_inhibited_scrollable_size_allocate;
}

static void
bz_inhibited_scrollable_init (BzInhibitedScrollable *self)
{
}

static gboolean
scrollable_get_border (GtkScrollable *scrollable,
                       GtkBorder     *border)
{
  BzInhibitedScrollable *self = BZ_INHIBITED_SCROLLABLE (scrollable);

  if (self->child != NULL)
    return gtk_scrollable_get_border (self->child, border);
  else
    return FALSE;
}

static void
scrollable_iface_init (GtkScrollableInterface *iface)
{
  iface->get_border = scrollable_get_border;
}

GtkWidget *
bz_inhibited_scrollable_new (void)
{
  return g_object_new (BZ_TYPE_INHIBITED_SCROLLABLE, NULL);
}

void
bz_inhibited_scrollable_set_scrollable (BzInhibitedScrollable *self,
                                        GtkScrollable         *child)
{
  g_return_if_fail (BZ_IS_INHIBITED_SCROLLABLE (self));
  g_return_if_fail (child == NULL || GTK_IS_SCROLLABLE (child));

  if (self->child == child)
    return;

  if (child != NULL)
    g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (child)) == NULL);

  clear_bindings (self);
  g_clear_pointer ((GtkWidget **) &self->child, gtk_widget_unparent);

  self->child = child;
  if (child != NULL)
    {
      gtk_widget_set_parent (GTK_WIDGET (child), GTK_WIDGET (self));
      self->hscroll_policy_bind = g_object_bind_property (
          self, "hscroll-policy",
          self->child, "hscroll-policy",
          G_BINDING_SYNC_CREATE);
      self->vscroll_policy_bind = g_object_bind_property (
          self, "vscroll-policy",
          self->child, "vscroll-policy",
          G_BINDING_SYNC_CREATE);
      setup_hadjustments (self);
      setup_vadjustments (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SCROLLABLE]);
}

GtkScrollable *
bz_inhibited_scrollable_get_scrollable (BzInhibitedScrollable *self)
{
  g_return_val_if_fail (BZ_IS_INHIBITED_SCROLLABLE (self), NULL);
  return self->child;
}

static void
child_adjustment_value_changed (BzInhibitedScrollable *self,
                                GtkAdjustment         *adjustment)
{
  GtkAdjustment *parent_adjustment = NULL;
  double         child_value       = 0.0;
  double         forced_value      = 0.0;

  if (adjustment == self->child_vadjustment)
    parent_adjustment = self->vadjustment;
  else
    parent_adjustment = self->hadjustment;

  child_value  = gtk_adjustment_get_value (adjustment);
  forced_value = gtk_adjustment_get_value (parent_adjustment);

  if (child_value != forced_value)
    gtk_adjustment_set_value (adjustment, forced_value);
}

static void
setup_hadjustments (BzInhibitedScrollable *self)
{
  g_clear_object (&self->hadjustment_binds.lower);
  g_clear_object (&self->hadjustment_binds.upper);
  g_clear_object (&self->hadjustment_binds.page_increment);
  g_clear_object (&self->hadjustment_binds.step_increment);
  g_clear_object (&self->hadjustment_binds.page_size);
  g_clear_object (&self->hadjustment_binds.value);

  if (self->child_hadjustment != NULL)
    g_signal_handlers_disconnect_by_func (
        self->child_hadjustment,
        child_adjustment_value_changed,
        self);
  g_clear_object (&self->child_hadjustment);

  if (self->child != NULL && self->hadjustment != NULL)
    {
      self->child_hadjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      g_signal_connect_swapped (self->child_hadjustment, "value-changed",
                                G_CALLBACK (child_adjustment_value_changed), self);

      self->hadjustment_binds.lower = g_object_bind_property (
          self->hadjustment, "lower",
          self->child_hadjustment, "lower",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->hadjustment_binds.upper = g_object_bind_property (
          self->hadjustment, "upper",
          self->child_hadjustment, "upper",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->hadjustment_binds.page_increment = g_object_bind_property (
          self->hadjustment, "page-increment",
          self->child_hadjustment, "page-increment",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->hadjustment_binds.step_increment = g_object_bind_property (
          self->hadjustment, "step-increment",
          self->child_hadjustment, "step-increment",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->hadjustment_binds.page_size = g_object_bind_property (
          self->hadjustment, "page-size",
          self->child_hadjustment, "page-size",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->hadjustment_binds.value = g_object_bind_property (
          self->hadjustment, "value",
          self->child_hadjustment, "value",
          G_BINDING_SYNC_CREATE);

      gtk_scrollable_set_hadjustment (self->child, self->child_hadjustment);
    }
}

static void
setup_vadjustments (BzInhibitedScrollable *self)
{
  g_clear_object (&self->vadjustment_binds.lower);
  g_clear_object (&self->vadjustment_binds.upper);
  g_clear_object (&self->vadjustment_binds.page_increment);
  g_clear_object (&self->vadjustment_binds.step_increment);
  g_clear_object (&self->vadjustment_binds.page_size);
  g_clear_object (&self->vadjustment_binds.value);

  if (self->child_vadjustment != NULL)
    g_signal_handlers_disconnect_by_func (
        self->child_vadjustment,
        child_adjustment_value_changed,
        self);
  g_clear_object (&self->child_vadjustment);

  if (self->child != NULL && self->vadjustment != NULL)
    {
      self->child_vadjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      g_signal_connect_swapped (self->child_vadjustment, "value-changed",
                                G_CALLBACK (child_adjustment_value_changed), self);

      self->vadjustment_binds.lower = g_object_bind_property (
          self->vadjustment, "lower",
          self->child_vadjustment, "lower",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->vadjustment_binds.upper = g_object_bind_property (
          self->vadjustment, "upper",
          self->child_vadjustment, "upper",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->vadjustment_binds.page_increment = g_object_bind_property (
          self->vadjustment, "page-increment",
          self->child_vadjustment, "page-increment",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->vadjustment_binds.step_increment = g_object_bind_property (
          self->vadjustment, "step-increment",
          self->child_vadjustment, "step-increment",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->vadjustment_binds.page_size = g_object_bind_property (
          self->vadjustment, "page-size",
          self->child_vadjustment, "page-size",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->vadjustment_binds.value = g_object_bind_property (
          self->vadjustment, "value",
          self->child_vadjustment, "value",
          G_BINDING_SYNC_CREATE);

      gtk_scrollable_set_vadjustment (self->child, self->child_vadjustment);
    }
}

static void
clear_bindings (BzInhibitedScrollable *self)
{
  g_clear_object (&self->hscroll_policy_bind);
  g_clear_object (&self->vscroll_policy_bind);

  g_clear_object (&self->hadjustment_binds.lower);
  g_clear_object (&self->hadjustment_binds.upper);
  g_clear_object (&self->hadjustment_binds.page_increment);
  g_clear_object (&self->hadjustment_binds.step_increment);
  g_clear_object (&self->hadjustment_binds.page_size);
  g_clear_object (&self->hadjustment_binds.value);

  g_clear_object (&self->vadjustment_binds.lower);
  g_clear_object (&self->vadjustment_binds.upper);
  g_clear_object (&self->vadjustment_binds.page_increment);
  g_clear_object (&self->vadjustment_binds.step_increment);
  g_clear_object (&self->vadjustment_binds.page_size);
  g_clear_object (&self->vadjustment_binds.value);

  if (self->child_hadjustment != NULL)
    g_signal_handlers_disconnect_by_func (
        self->child_hadjustment,
        child_adjustment_value_changed,
        self);
  g_clear_object (&self->child_hadjustment);

  if (self->child_vadjustment != NULL)
    g_signal_handlers_disconnect_by_func (
        self->child_vadjustment,
        child_adjustment_value_changed,
        self);
  g_clear_object (&self->child_vadjustment);
}
