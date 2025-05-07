/* ga-search-widget.c
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

#include <flatpak.h>

#include "ga-search-widget.h"

struct _GaSearchWidget
{
  AdwBin parent_instance;

  GListModel *model;
  FlatpakRef *selected;

  /* Template widgets */
  GtkText     *search_bar;
  GtkLabel    *search_text;
  AdwSpinner  *sheet_spinner;
  GtkListView *list_view;
};

G_DEFINE_FINAL_TYPE (GaSearchWidget, ga_search_widget, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_SELECTED,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
search_changed (GtkEditable    *editable,
                GaSearchWidget *self);

static gint
cmp_item (FlatpakRef     *a,
          FlatpakRef     *b,
          GaSearchWidget *self);

static int
match (FlatpakRef     *item,
       GaSearchWidget *self);

static void
activate (GtkListView    *list_view,
          guint           position,
          GaSearchWidget *self);

static void
update_filter (GaSearchWidget *self);

static void
ga_search_widget_dispose (GObject *object)
{
  GaSearchWidget *self = GA_SEARCH_WIDGET (object);

  g_clear_object (&self->model);
  g_clear_object (&self->selected);

  G_OBJECT_CLASS (ga_search_widget_parent_class)->dispose (object);
}

static void
ga_search_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GaSearchWidget *self = GA_SEARCH_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;
    case PROP_SELECTED:
      g_value_set_object (value, self->selected);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_search_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GaSearchWidget *self = GA_SEARCH_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ga_search_widget_set_model (self, g_value_get_object (value));
      break;
    case PROP_SELECTED:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ga_search_widget_class_init (GaSearchWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = ga_search_widget_dispose;
  object_class->get_property = ga_search_widget_get_property;
  object_class->set_property = ga_search_widget_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SELECTED] =
      g_param_spec_object (
          "selected",
          NULL, NULL,
          FLATPAK_TYPE_REF,
          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/ga-search-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, search_bar);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, search_text);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, sheet_spinner);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, list_view);
}

static void
ga_search_widget_init (GaSearchWidget *self)
{
  GtkCustomFilter    *custom_filter   = NULL;
  GtkFilterListModel *filter_model    = NULL;
  GtkCustomSorter    *custom_sorter   = NULL;
  GtkSortListModel   *sort_model      = NULL;
  GtkSelectionModel  *selection_model = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_add_css_class (GTK_WIDGET (self), "global-search");

  custom_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) match, self, NULL);
  filter_model  = gtk_filter_list_model_new (NULL, GTK_FILTER (custom_filter));
  gtk_filter_list_model_set_incremental (filter_model, TRUE);

  custom_sorter = gtk_custom_sorter_new ((GCompareDataFunc) cmp_item, self, NULL);
  sort_model    = gtk_sort_list_model_new (G_LIST_MODEL (filter_model), GTK_SORTER (custom_sorter));

  selection_model = GTK_SELECTION_MODEL (gtk_single_selection_new (G_LIST_MODEL (sort_model)));
  gtk_list_view_set_model (self->list_view, selection_model);

  g_signal_connect (self->search_bar, "changed", G_CALLBACK (search_changed), self);
  g_signal_connect (self->list_view, "activate", G_CALLBACK (activate), self);
}

GtkWidget *
ga_search_widget_new (GListModel *model)
{
  return g_object_new (
      GA_TYPE_SEARCH_WIDGET,
      "model", model,
      NULL);
}

void
ga_search_widget_set_model (GaSearchWidget *self,
                            GListModel     *model)
{

  GtkSingleSelection *selection_model   = NULL;
  GtkSortListModel   *sort_list_model   = NULL;
  GtkFilterListModel *filter_list_model = NULL;

  g_return_if_fail (GA_IS_SEARCH_WIDGET (self));

  g_clear_object (&self->model);
  self->model = model != NULL ? g_object_ref (model) : NULL;

  selection_model   = GTK_SINGLE_SELECTION (gtk_list_view_get_model (self->list_view));
  sort_list_model   = GTK_SORT_LIST_MODEL (gtk_single_selection_get_model (selection_model));
  filter_list_model = GTK_FILTER_LIST_MODEL (gtk_sort_list_model_get_model (sort_list_model));

  gtk_filter_list_model_set_model (
      filter_list_model,
      model != NULL ? G_LIST_MODEL (model) : NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
ga_search_widget_get_model (GaSearchWidget *self)
{
  g_return_val_if_fail (GA_IS_SEARCH_WIDGET (self), NULL);
  return self->model;
}

gpointer
ga_search_widget_get_selected (GaSearchWidget *self)
{
  g_return_val_if_fail (GA_IS_SEARCH_WIDGET (self), NULL);
  return self->selected;
}

static void
search_changed (GtkEditable    *editable,
                GaSearchWidget *self)
{
  update_filter (self);
}

static gint
cmp_item (FlatpakRef     *a,
          FlatpakRef     *b,
          GaSearchWidget *self)
{
  /* Just setting this up for later */
  return 0;
}

static int
match (FlatpakRef     *item,
       GaSearchWidget *self)
{
  const char *search_text = NULL;
  const char *name        = NULL;
  const char *commit      = NULL;

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
  if (search_text == NULL)
    return 0;

  name   = flatpak_ref_get_name (item);
  commit = flatpak_ref_get_commit (item);

  return (g_str_match_string (search_text, name, TRUE) ||
          g_str_match_string (search_text, commit, TRUE))
             ? 1
             : 0;
}

static void
activate (GtkListView    *list_view,
          guint           position,
          GaSearchWidget *self)
{
  GtkSelectionModel *model = NULL;

  g_clear_object (&self->selected);

  model          = gtk_list_view_get_model (self->list_view);
  self->selected = g_list_model_get_item (G_LIST_MODEL (model), position);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED]);
}

static void
update_filter (GaSearchWidget *self)
{
  GtkSingleSelection *selection_model   = NULL;
  GtkSortListModel   *sort_list_model   = NULL;
  GtkFilterListModel *filter_list_model = NULL;
  GtkFilter          *filter            = NULL;

  selection_model   = GTK_SINGLE_SELECTION (gtk_list_view_get_model (self->list_view));
  sort_list_model   = GTK_SORT_LIST_MODEL (gtk_single_selection_get_model (selection_model));
  filter_list_model = GTK_FILTER_LIST_MODEL (gtk_sort_list_model_get_model (sort_list_model));
  filter            = gtk_filter_list_model_get_filter (filter_list_model);

  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}
