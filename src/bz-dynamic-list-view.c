/* bz-dynamic-list-view.c
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

#include "bz-dynamic-list-view.h"

struct _BzDynamicListView
{
  AdwBin parent_instance;

  GListModel *model;
  gboolean    scroll;
  GType       child_type;
  char       *child_prop;

  char *child_type_string;
};

G_DEFINE_FINAL_TYPE (BzDynamicListView, bz_dynamic_list_view, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_SCROLL,
  PROP_CHILD_TYPE,
  PROP_CHILD_PROP,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
refresh (BzDynamicListView *self);

static void
list_item_factory_setup (BzDynamicListView        *self,
                         GtkListItem              *item,
                         GtkSignalListItemFactory *factory);

static void
list_item_factory_teardown (BzDynamicListView        *self,
                            GtkListItem              *item,
                            GtkSignalListItemFactory *factory);

static void
list_item_factory_bind (BzDynamicListView        *self,
                        GtkListItem              *item,
                        GtkSignalListItemFactory *factory);

static void
list_item_factory_unbind (BzDynamicListView        *self,
                          GtkListItem              *item,
                          GtkSignalListItemFactory *factory);

static GtkWidget *
create_list_box_widget (GObject           *object,
                        BzDynamicListView *self);

static void
bz_dynamic_list_view_dispose (GObject *object)
{
  BzDynamicListView *self = BZ_DYNAMIC_LIST_VIEW (object);

  g_clear_pointer (&self->model, g_object_unref);
  g_clear_pointer (&self->child_prop, g_free);
  g_clear_pointer (&self->child_type_string, g_free);

  G_OBJECT_CLASS (bz_dynamic_list_view_parent_class)->dispose (object);
}

static void
bz_dynamic_list_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  BzDynamicListView *self = BZ_DYNAMIC_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_dynamic_list_view_get_model (self));
      break;
    case PROP_SCROLL:
      g_value_set_boolean (value, bz_dynamic_list_view_get_scroll (self));
      break;
    case PROP_CHILD_TYPE:
      g_value_set_string (value, bz_dynamic_list_view_get_child_type (self));
      break;
    case PROP_CHILD_PROP:
      g_value_set_string (value, bz_dynamic_list_view_get_child_prop (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_dynamic_list_view_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BzDynamicListView *self = BZ_DYNAMIC_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_dynamic_list_view_set_model (self, g_value_get_object (value));
      break;
    case PROP_SCROLL:
      bz_dynamic_list_view_set_scroll (self, g_value_get_boolean (value));
      break;
    case PROP_CHILD_TYPE:
      bz_dynamic_list_view_set_child_type (self, g_value_get_string (value));
      break;
    case PROP_CHILD_PROP:
      bz_dynamic_list_view_set_child_prop (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_dynamic_list_view_class_init (BzDynamicListViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_dynamic_list_view_set_property;
  object_class->get_property = bz_dynamic_list_view_get_property;
  object_class->dispose      = bz_dynamic_list_view_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SCROLL] =
      g_param_spec_boolean (
          "scroll",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CHILD_TYPE] =
      g_param_spec_string (
          "child-type",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CHILD_PROP] =
      g_param_spec_string (
          "child-prop",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_dynamic_list_view_init (BzDynamicListView *self)
{
  self->child_type = G_TYPE_INVALID;
}

BzDynamicListView *
bz_dynamic_list_view_new (void)
{
  return g_object_new (BZ_TYPE_DYNAMIC_LIST_VIEW, NULL);
}

GListModel *
bz_dynamic_list_view_get_model (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->model;
}

gboolean
bz_dynamic_list_view_get_scroll (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), FALSE);
  return self->scroll;
}

const char *
bz_dynamic_list_view_get_child_type (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->child_type_string;
}

const char *
bz_dynamic_list_view_get_child_prop (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->child_prop;
}

void
bz_dynamic_list_view_set_model (BzDynamicListView *self,
                                GListModel        *model)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  g_clear_pointer (&self->model, g_object_unref);
  if (model != NULL)
    self->model = g_object_ref (model);

  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

void
bz_dynamic_list_view_set_scroll (BzDynamicListView *self,
                                 gboolean           scroll)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  self->scroll = scroll;
  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SCROLL]);
}

void
bz_dynamic_list_view_set_child_type (BzDynamicListView *self,
                                     const char        *child_type)
{
  GType type = G_TYPE_INVALID;

  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));
  if (child_type != NULL)
    {
      type = g_type_from_name (child_type);
      g_return_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET));
    }

  g_clear_pointer (&self->child_type_string, g_free);

  self->child_type = type;
  if (child_type != NULL)
    self->child_type_string = g_strdup (child_type);

  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD_TYPE]);
}

void
bz_dynamic_list_view_set_child_prop (BzDynamicListView *self,
                                     const char        *child_prop)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  g_clear_pointer (&self->child_prop, g_free);
  if (child_prop != NULL)
    self->child_prop = g_strdup (child_prop);
  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD_PROP]);
}

static void
refresh (BzDynamicListView *self)
{
  adw_bin_set_child (ADW_BIN (self), NULL);
  if (self->model == NULL ||
      self->child_prop == NULL ||
      self->child_type == G_TYPE_INVALID)
    return;

  if (self->scroll)
    {
      GtkWidget          *window    = NULL;
      GtkNoSelection     *selection = NULL;
      GtkListItemFactory *factory   = NULL;
      GtkWidget          *view      = NULL;

      window    = gtk_scrolled_window_new ();
      selection = gtk_no_selection_new (g_object_ref (self->model));
      factory   = gtk_signal_list_item_factory_new ();
      view      = gtk_list_view_new (GTK_SELECTION_MODEL (selection), factory);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_widget_add_css_class (view, "navigation-sidebar");
      gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (view), TRUE);

      g_signal_connect_swapped (factory, "setup", G_CALLBACK (list_item_factory_setup), self);
      g_signal_connect_swapped (factory, "teardown", G_CALLBACK (list_item_factory_teardown), self);
      g_signal_connect_swapped (factory, "bind", G_CALLBACK (list_item_factory_bind), self);
      g_signal_connect_swapped (factory, "unbind", G_CALLBACK (list_item_factory_unbind), self);

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (window), view);
      adw_bin_set_child (ADW_BIN (self), window);
    }
  else
    {
      GtkWidget *list = NULL;

      list = gtk_list_box_new ();
      gtk_list_box_bind_model (
          GTK_LIST_BOX (list), self->model,
          (GtkFlowBoxCreateWidgetFunc) create_list_box_widget,
          self, NULL);

      adw_bin_set_child (ADW_BIN (self), list);
    }
}

static void
list_item_factory_setup (BzDynamicListView        *self,
                         GtkListItem              *item,
                         GtkSignalListItemFactory *factory)
{
  GtkWidget *child = NULL;

  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  child = g_object_new (self->child_type, NULL);
  gtk_list_item_set_child (item, child);
}

static void
list_item_factory_teardown (BzDynamicListView        *self,
                            GtkListItem              *item,
                            GtkSignalListItemFactory *factory)
{
  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  gtk_list_item_set_child (item, NULL);
}

static void
list_item_factory_bind (BzDynamicListView        *self,
                        GtkListItem              *item,
                        GtkSignalListItemFactory *factory)
{
  GObject   *object = NULL;
  GtkWidget *child  = NULL;

  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  object = gtk_list_item_get_item (item);
  child  = gtk_list_item_get_child (item);
  g_object_set (child, self->child_prop, object, NULL);
}

static void
list_item_factory_unbind (BzDynamicListView        *self,
                          GtkListItem              *item,
                          GtkSignalListItemFactory *factory)
{
  GtkWidget *child = NULL;

  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  child = gtk_list_item_get_child (item);
  g_object_set (child, self->child_prop, NULL, NULL);
}

static GtkWidget *
create_list_box_widget (GObject           *object,
                        BzDynamicListView *self)
{
  g_return_val_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL, NULL);

  return g_object_new (self->child_type, self->child_prop, object, NULL);
}

/* End of bz-dynamic-list-view.c */
