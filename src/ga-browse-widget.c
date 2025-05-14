/* ga-browse-widget.c
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

#include "ga-browse-widget.h"
#include "ga-entry.h"

struct _GaBrowseWidget
{
  AdwBin parent_instance;

  GListModel *model;

  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (GaBrowseWidget, ga_browse_widget, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
ga_browse_widget_dispose (GObject *object)
{
  GaBrowseWidget *self = GA_BROWSE_WIDGET (object);

  g_clear_object (&self->model);

  G_OBJECT_CLASS (ga_browse_widget_parent_class)->dispose (object);
}

static void
ga_browse_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GaBrowseWidget *self = GA_BROWSE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, ga_browse_widget_get_model (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_browse_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GaBrowseWidget *self = GA_BROWSE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ga_browse_widget_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_browse_widget_class_init (GaBrowseWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = ga_browse_widget_dispose;
  object_class->get_property = ga_browse_widget_get_property;
  object_class->set_property = ga_browse_widget_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/ga-browse-widget.ui");
}

static void
ga_browse_widget_init (GaBrowseWidget *self)
{

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ga_browse_widget_new (GListModel *model)
{
  return g_object_new (
      GA_TYPE_BROWSE_WIDGET,
      "model", model,
      NULL);
}

void
ga_browse_widget_set_model (GaBrowseWidget *self,
                            GListModel     *model)
{
  g_return_if_fail (GA_IS_BROWSE_WIDGET (self));
  g_return_if_fail (model == NULL ||
                    (G_IS_LIST_MODEL (model) &&
                     g_list_model_get_item_type (model) == GA_TYPE_ENTRY));

  g_clear_object (&self->model);
  if (model != NULL)
    self->model = g_object_ref (model);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
ga_browse_widget_get_model (GaBrowseWidget *self)
{
  g_return_val_if_fail (GA_IS_BROWSE_WIDGET (self), NULL);
  return self->model;
}
