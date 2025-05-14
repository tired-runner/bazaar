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

#include "ga-entry.h"
#include "ga-search-widget.h"

struct _GaSearchWidget
{
  AdwBin parent_instance;

  GListModel *model;
  GaEntry    *selected;
  GaEntry    *previewing;

  guint previewing_timeout;

  GPtrArray  *match_tokens;
  GRegex     *match_regex;
  GHashTable *match_scores;

  /* Template widgets */
  GtkText         *search_bar;
  GtkLabel        *search_text;
  AdwSpinner      *search_spinner;
  GtkToggleButton *regex_toggle;
  GtkLabel        *regex_error;
  GtkListView     *list_view;
};

G_DEFINE_FINAL_TYPE (GaSearchWidget, ga_search_widget, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_SELECTED,
  PROP_PREVIEWING,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
search_changed (GtkEditable    *editable,
                GaSearchWidget *self);

static void
regex_toggled (GtkToggleButton *toggle,
               GaSearchWidget  *self);

static void
search_activate (GtkText        *text,
                 GaSearchWidget *self);

static gint
cmp_item (GaEntry        *a,
          GaEntry        *b,
          GaSearchWidget *self);

static int
match (GaEntry        *item,
       GaSearchWidget *self);

static void
pending_changed (GtkFilterListModel *model,
                 GParamSpec         *pspec,
                 GaSearchWidget     *self);

static void
selected_item_changed (GtkSingleSelection *model,
                       GParamSpec         *pspec,
                       GaSearchWidget     *self);

static void
previewing_property_timeout (GaSearchWidget *self);

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
  g_clear_object (&self->previewing);
  g_clear_handle_id (&self->previewing_timeout, g_source_remove);
  g_clear_pointer (&self->match_tokens, g_ptr_array_unref);
  g_clear_pointer (&self->match_regex, g_regex_unref);
  g_clear_pointer (&self->match_scores, g_hash_table_unref);

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
      g_value_set_object (value, ga_search_widget_get_model (self));
      break;
    case PROP_SELECTED:
      g_value_set_object (value, ga_search_widget_get_selected (self));
      break;
    case PROP_PREVIEWING:
      g_value_set_object (value, ga_search_widget_get_previewing (self));
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
    case PROP_PREVIEWING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static char *
format_size (gpointer object,
             guint64  value)
{
  return g_format_size (value);
}

static char *
format_as_link (gpointer    object,
                const char *value)
{
  return g_strdup_printf ("<a href=\"%s\" title=\"%s\">%s</a>",
                          value, value, value);
}

static void
action_move (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  GaSearchWidget    *self     = GA_SEARCH_WIDGET (widget);
  GtkSelectionModel *model    = NULL;
  guint              selected = 0;
  guint              n_items  = 0;

  model   = gtk_list_view_get_model (self->list_view);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

  if (n_items == 0)
    return;

  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));

  if (selected == GTK_INVALID_LIST_POSITION)
    selected = 0;
  else
    {
      int offset = 0;

      offset = g_variant_get_int32 (parameter);
      if (offset < 0 && ABS (offset) > selected)
        selected = n_items + (offset % -(int) n_items);
      else
        selected = (selected + offset) % n_items;
    }

  gtk_list_view_scroll_to (
      self->list_view, selected,
      GTK_LIST_SCROLL_SELECT, NULL);
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
          GA_TYPE_ENTRY,
          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PREVIEWING] =
      g_param_spec_object (
          "previewing",
          NULL, NULL,
          GA_TYPE_ENTRY,
          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Example/ga-search-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, search_bar);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, search_text);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, search_spinner);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, regex_toggle);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, regex_error);
  gtk_widget_class_bind_template_child (widget_class, GaSearchWidget, list_view);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, format_size);
  gtk_widget_class_bind_template_callback (widget_class, format_as_link);

  gtk_widget_class_install_action (widget_class, "move", "i", action_move);
}

static void
ga_search_widget_init (GaSearchWidget *self)
{
  GtkCustomFilter    *custom_filter             = NULL;
  GtkFilterListModel *filter_model              = NULL;
  GtkCustomSorter    *custom_sorter             = NULL;
  GtkSortListModel   *sort_model                = NULL;
  g_autoptr (GtkSelectionModel) selection_model = NULL;

  self->match_tokens = g_ptr_array_new_with_free_func (g_free);
  self->match_scores = g_hash_table_new (g_direct_hash, g_direct_equal);

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
  g_signal_connect (self->search_bar, "activate", G_CALLBACK (search_activate), self);
  g_signal_connect (self->regex_toggle, "toggled", G_CALLBACK (regex_toggled), self);
  g_signal_connect (filter_model, "notify::pending", G_CALLBACK (pending_changed), self);
  g_signal_connect (selection_model, "notify::selected-item", G_CALLBACK (selected_item_changed), self);
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

gpointer
ga_search_widget_get_previewing (GaSearchWidget *self)
{
  g_return_val_if_fail (GA_IS_SEARCH_WIDGET (self), NULL);
  return self->previewing;
}

static void
search_changed (GtkEditable    *editable,
                GaSearchWidget *self)
{
  update_filter (self);
}

static void
regex_toggled (GtkToggleButton *toggle,
               GaSearchWidget  *self)
{
  update_filter (self);
}

static void
search_activate (GtkText        *text,
                 GaSearchWidget *self)
{
  GtkSelectionModel *model        = NULL;
  guint              selected_idx = 0;

  g_clear_object (&self->selected);

  model        = gtk_list_view_get_model (self->list_view);
  selected_idx = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));

  if (selected_idx != GTK_INVALID_LIST_POSITION)
    {
      self->selected = g_list_model_get_item (G_LIST_MODEL (model), selected_idx);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED]);
    }
}

static gint
cmp_item (GaEntry        *a,
          GaEntry        *b,
          GaSearchWidget *self)
{
  int a_score = 0;
  int b_score = 0;

  a_score = GPOINTER_TO_INT (g_hash_table_lookup (self->match_scores, a));
  b_score = GPOINTER_TO_INT (g_hash_table_lookup (self->match_scores, b));

  if (a_score == b_score)
    {
      /* slightly favor entries with a description */
      if (ga_entry_get_description (a) != NULL)
        a_score++;
      if (ga_entry_get_description (b) != NULL)
        b_score++;

      /* slightly favor entries with an icon */
      if (ga_entry_get_icon_paintable (a) != NULL)
        a_score++;
      if (ga_entry_get_icon_paintable (b) != NULL)
        b_score++;
    }
  if (a_score == b_score)
    {
      const char *a_title = NULL;
      const char *b_title = NULL;
      int         cmp_res = 0;

      a_title = ga_entry_get_title (a);
      b_title = ga_entry_get_title (b);
      cmp_res = g_strcmp0 (a_title, b_title);

      if (cmp_res < 0)
        a_score++;
      if (cmp_res > 0)
        b_score++;
    }

  if (a_score == b_score)
    return 0;
  return a_score < b_score ? 1 : -1;
}

static gboolean
match (GaEntry        *item,
       GaSearchWidget *self)
{
  int        score         = 0;
  GPtrArray *search_tokens = NULL;

  search_tokens = ga_entry_get_search_tokens (item);

  if (self->match_regex != NULL)
    {
      for (guint i = 0; i < search_tokens->len; i++)
        {
          const char *search_token    = NULL;
          g_autoptr (GMatchInfo) info = NULL;

          search_token = g_ptr_array_index (search_tokens, i);
          g_regex_match (self->match_regex, search_token,
                         G_REGEX_MATCH_DEFAULT, &info);

          if (info != NULL)
            score += g_match_info_get_match_count (info);
        }
    }
  else
    {
      if (self->match_tokens->len == 0)
        {
          score++;
          goto done;
        }

      for (guint i = 0; i < self->match_tokens->len; i++)
        {
          int         token_score = 0;
          const char *match_token = NULL;

          match_token = g_ptr_array_index (self->match_tokens, i);
          for (guint j = 0; j < search_tokens->len; j++)
            {
              const char *search_token = NULL;

              search_token = g_ptr_array_index (search_tokens, j);
              if (g_strcmp0 (match_token, search_token) == 0)
                token_score += 5;
              else if (strstr (search_token, match_token) != NULL)
                token_score += 3;
              else if (g_str_match_string (match_token, search_token, TRUE))
                token_score += 1;
            }

          if (token_score > 0)
            score += token_score;
          else
            {
              score = 0;
              goto done;
            }
        }
    }

done:
  g_hash_table_replace (self->match_scores, item, GINT_TO_POINTER (score));
  return score > 0;
}

static void
pending_changed (GtkFilterListModel *model,
                 GParamSpec         *pspec,
                 GaSearchWidget     *self)
{
  guint pending = 0;
  guint n_items = 0;

  pending = gtk_filter_list_model_get_pending (model);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

  if (pending == 0)
    {
      if (n_items > 0)
        /* Here to combat weird list view scrolling behavior */
        gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_SELECT, NULL);

      gtk_widget_set_visible (GTK_WIDGET (self->search_spinner), FALSE);
    }
}

static void
selected_item_changed (GtkSingleSelection *model,
                       GParamSpec         *pspec,
                       GaSearchWidget     *self)
{
  g_clear_handle_id (&self->previewing_timeout, g_source_remove);
  g_clear_object (&self->previewing);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PREVIEWING]);
  self->previewing_timeout = g_timeout_add_once (
      500, (GSourceOnceFunc) previewing_property_timeout, self);
}

static void
previewing_property_timeout (GaSearchWidget *self)
{
  GtkSelectionModel *model    = NULL;
  guint              selected = 0;

  self->previewing_timeout = 0;

  g_clear_object (&self->previewing);
  model    = gtk_list_view_get_model (self->list_view);
  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));

  if (selected != GTK_INVALID_LIST_POSITION)
    self->previewing = g_list_model_get_item (G_LIST_MODEL (model), selected);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PREVIEWING]);
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
  const char         *search_text       = NULL;
  gboolean            reset_regex       = FALSE;
  GtkSingleSelection *selection_model   = NULL;
  GtkSortListModel   *sort_list_model   = NULL;
  GtkFilterListModel *filter_list_model = NULL;
  GtkFilter          *filter            = NULL;

  g_ptr_array_set_size (self->match_tokens, 0);
  g_clear_pointer (&self->match_regex, g_regex_unref);
  g_hash_table_remove_all (self->match_scores);

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
  if (search_text != NULL && *search_text != '\0')
    {
      if (gtk_toggle_button_get_active (self->regex_toggle))
        {
          g_autoptr (GError) local_error = NULL;

          self->match_regex = g_regex_new (
              search_text, G_REGEX_DEFAULT,
              G_REGEX_MATCH_DEFAULT, &local_error);

          if (self->match_regex != NULL)
            {
              gtk_label_set_label (self->regex_error, NULL);
              gtk_widget_set_tooltip_text (GTK_WIDGET (self->regex_error), NULL);
              gtk_widget_add_css_class (GTK_WIDGET (self->regex_toggle), "success");
              gtk_widget_remove_css_class (GTK_WIDGET (self->regex_toggle), "error");
            }
          else
            {
              gtk_label_set_label (self->regex_error, local_error->message);
              gtk_widget_set_tooltip_text (GTK_WIDGET (self->regex_error), local_error->message);
              gtk_widget_add_css_class (GTK_WIDGET (self->regex_toggle), "error");
              gtk_widget_remove_css_class (GTK_WIDGET (self->regex_toggle), "success");
            }
        }
      else
        reset_regex = TRUE;

      if (self->match_regex == NULL)
        {
          g_autofree gchar **tokens = NULL;

          tokens = g_strsplit_set (search_text, " \t\n", -1);
          for (gchar **token = tokens; *token != NULL; token++)
            {
              if (**token != '\0')
                g_ptr_array_add (self->match_tokens, *token);
              else
                g_free (*token);
            }
        }
    }
  else
    reset_regex = TRUE;

  if (reset_regex)
    {
      gtk_label_set_label (self->regex_error, NULL);
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->regex_error), NULL);
      gtk_widget_remove_css_class (GTK_WIDGET (self->regex_toggle), "success");
      gtk_widget_remove_css_class (GTK_WIDGET (self->regex_toggle), "error");
    }

  selection_model   = GTK_SINGLE_SELECTION (gtk_list_view_get_model (self->list_view));
  sort_list_model   = GTK_SORT_LIST_MODEL (gtk_single_selection_get_model (selection_model));
  filter_list_model = GTK_FILTER_LIST_MODEL (gtk_sort_list_model_get_model (sort_list_model));
  filter            = gtk_filter_list_model_get_filter (filter_list_model);

  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);

  if (gtk_filter_list_model_get_pending (filter_list_model) > 0)
    gtk_widget_set_visible (GTK_WIDGET (self->search_spinner), TRUE);
}
