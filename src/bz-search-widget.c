/* bz-search-widget.c
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

#include <libdex.h>

#include "bz-async-texture.h"
#include "bz-screenshot.h"
#include "bz-search-widget.h"

struct _BzSearchWidget
{
  AdwBin parent_instance;

  GListModel   *model;
  BzEntryGroup *selected;
  gboolean      remove;
  BzEntryGroup *previewing;

  guint search_update_timeout;

  GPtrArray  *match_tokens;
  GRegex     *match_regex;
  GHashTable *match_scores;

  /* Template widgets */
  GtkText        *search_bar;
  GtkLabel       *search_text;
  GtkImage       *search_busy;
  GtkCheckButton *regex_check;
  GtkLabel       *regex_error;
  GtkCheckButton *foss_check;
  GtkCheckButton *flathub_check;
  GtkBox         *content_box;
  GtkRevealer    *entry_list_revealer;
  GtkListView    *list_view;
};

G_DEFINE_FINAL_TYPE (BzSearchWidget, bz_search_widget, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_PREVIEWING,
  PROP_TEXT,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SELECT,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
search_changed (GtkEditable    *editable,
                BzSearchWidget *self);

static void
search_activate (GtkText        *text,
                 BzSearchWidget *self);

static gint
cmp_item (BzEntryGroup   *a,
          BzEntryGroup   *b,
          BzSearchWidget *self);

static int
match (BzEntryGroup   *item,
       BzSearchWidget *self);

static void
pending_changed (GtkFilterListModel *model,
                 GParamSpec         *pspec,
                 BzSearchWidget     *self);

static void
selected_item_changed (GtkSingleSelection *model,
                       GParamSpec         *pspec,
                       BzSearchWidget     *self);

static void
list_activate (GtkListView    *list_view,
               guint           position,
               BzSearchWidget *self);

static void
update_filter (BzSearchWidget *self);

static void
emit_idx (BzSearchWidget *self,
          GListModel     *model,
          guint           selected_idx);

static void
bz_search_widget_dispose (GObject *object)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (object);

  g_clear_object (&self->model);
  g_clear_object (&self->selected);
  g_clear_object (&self->previewing);
  g_clear_handle_id (&self->search_update_timeout, g_source_remove);
  g_clear_pointer (&self->match_tokens, g_ptr_array_unref);
  g_clear_pointer (&self->match_regex, g_regex_unref);
  g_clear_pointer (&self->match_scores, g_hash_table_unref);

  G_OBJECT_CLASS (bz_search_widget_parent_class)->dispose (object);
}

static void
bz_search_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_search_widget_get_model (self));
      break;
    case PROP_TEXT:
      g_value_set_string (value, bz_search_widget_get_text (self));
      break;
    case PROP_PREVIEWING:
      g_value_set_object (value, bz_search_widget_get_previewing (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_search_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_search_widget_set_model (self, g_value_get_object (value));
      break;
    case PROP_TEXT:
      bz_search_widget_set_text (self, g_value_get_string (value));
      break;
    case PROP_PREVIEWING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
bz_search_widget_grab_focus (GtkWidget *widget)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (widget);
  return gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static gboolean
is_valid_string (gpointer    object,
                 const char *value)
{
  return value != NULL && *value != '\0';
}

static void
action_move (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  BzSearchWidget    *self     = BZ_SEARCH_WIDGET (widget);
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
regex_toggled_cb (BzSearchWidget  *self,
                  GtkToggleButton *toggle)
{
  update_filter (self);
}

static void
foss_toggled_cb (BzSearchWidget  *self,
                 GtkToggleButton *toggle)
{
  update_filter (self);
}

static void
flathub_toggled_cb (BzSearchWidget  *self,
                    GtkToggleButton *toggle)
{
  update_filter (self);
}

static void
bz_search_widget_class_init (BzSearchWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_search_widget_dispose;
  object_class->get_property = bz_search_widget_get_property;
  object_class->set_property = bz_search_widget_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TEXT] =
      g_param_spec_string (
          "text",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PREVIEWING] =
      g_param_spec_object (
          "previewing",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SELECT] =
      g_signal_new (
          "select",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SELECT],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  widget_class->grab_focus = bz_search_widget_grab_focus;

  g_type_ensure (BZ_TYPE_ASYNC_TEXTURE);
  g_type_ensure (BZ_TYPE_SCREENSHOT);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-search-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_bar);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_text);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_busy);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, regex_check);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, regex_error);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, foss_check);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, flathub_check);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, content_box);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, entry_list_revealer);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, list_view);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_valid_string);
  gtk_widget_class_bind_template_callback (widget_class, regex_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, foss_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, flathub_toggled_cb);

  gtk_widget_class_install_action (widget_class, "move", "i", action_move);
}

static void
bz_search_widget_init (BzSearchWidget *self)
{
  GtkCustomFilter    *custom_filter             = NULL;
  GtkFilterListModel *filter_model              = NULL;
  GtkCustomSorter    *custom_sorter             = NULL;
  GtkSortListModel   *sort_model                = NULL;
  g_autoptr (GtkSelectionModel) selection_model = NULL;

  self->match_tokens = g_ptr_array_new_with_free_func (g_free);
  self->match_scores = g_hash_table_new (g_direct_hash, g_direct_equal);

  gtk_widget_init_template (GTK_WIDGET (self));

  custom_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) match, self, NULL);
  filter_model  = gtk_filter_list_model_new (NULL, GTK_FILTER (custom_filter));
  gtk_filter_list_model_set_incremental (filter_model, TRUE);

  custom_sorter = gtk_custom_sorter_new ((GCompareDataFunc) cmp_item, self, NULL);
  sort_model    = gtk_sort_list_model_new (G_LIST_MODEL (filter_model), GTK_SORTER (custom_sorter));

  selection_model = GTK_SELECTION_MODEL (gtk_single_selection_new (G_LIST_MODEL (sort_model)));
  gtk_list_view_set_model (self->list_view, selection_model);

  g_signal_connect (self->search_bar, "changed", G_CALLBACK (search_changed), self);
  g_signal_connect (self->search_bar, "activate", G_CALLBACK (search_activate), self);
  g_signal_connect (filter_model, "notify::pending", G_CALLBACK (pending_changed), self);
  g_signal_connect (selection_model, "notify::selected-item", G_CALLBACK (selected_item_changed), self);
  g_signal_connect (self->list_view, "activate", G_CALLBACK (list_activate), self);
}

GtkWidget *
bz_search_widget_new (GListModel *model,
                      const char *initial)
{
  BzSearchWidget *self = NULL;

  self = g_object_new (
      BZ_TYPE_SEARCH_WIDGET,
      "model", model,
      NULL);

  if (initial != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->search_bar), initial);

  return GTK_WIDGET (self);
}

void
bz_search_widget_set_model (BzSearchWidget *self,
                            GListModel     *model)
{
  GtkSingleSelection *selection_model   = NULL;
  GtkSortListModel   *sort_list_model   = NULL;
  GtkFilterListModel *filter_list_model = NULL;

  g_return_if_fail (BZ_IS_SEARCH_WIDGET (self));
  g_return_if_fail (model == NULL ||
                    (G_IS_LIST_MODEL (model) &&
                     g_list_model_get_item_type (model) == BZ_TYPE_ENTRY_GROUP));

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
bz_search_widget_get_model (BzSearchWidget *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_WIDGET (self), NULL);
  return self->model;
}

BzEntryGroup *
bz_search_widget_get_selected (BzSearchWidget *self,
                               gboolean       *remove)
{
  g_return_val_if_fail (BZ_IS_SEARCH_WIDGET (self), NULL);

  if (remove != NULL)
    *remove = self->remove;
  return self->selected;
}

BzEntryGroup *
bz_search_widget_get_previewing (BzSearchWidget *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_WIDGET (self), NULL);
  return self->previewing;
}

void
bz_search_widget_set_text (BzSearchWidget *self,
                           const char     *text)
{
  gtk_editable_set_text (GTK_EDITABLE (self->search_bar), text);
}

const char *
bz_search_widget_get_text (BzSearchWidget *self)
{
  return gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
}

static void
search_changed (GtkEditable    *editable,
                BzSearchWidget *self)
{
  g_clear_handle_id (&self->search_update_timeout, g_source_remove);
  gtk_revealer_set_reveal_child (self->entry_list_revealer, FALSE);
  self->search_update_timeout = g_timeout_add_once (
      150 /* 150 ms */, (GSourceOnceFunc) update_filter, self);
}

static void
search_activate (GtkText        *text,
                 BzSearchWidget *self)
{
  GtkSelectionModel *model        = NULL;
  guint              selected_idx = 0;

  g_clear_object (&self->selected);

  model        = gtk_list_view_get_model (self->list_view);
  selected_idx = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));

  if (selected_idx != GTK_INVALID_LIST_POSITION)
    emit_idx (self, G_LIST_MODEL (model), selected_idx);
}

static gint
cmp_item (BzEntryGroup   *a,
          BzEntryGroup   *b,
          BzSearchWidget *self)
{
  int      a_score = 0;
  int      b_score = 0;
  BzEntry *a_entry = NULL;
  BzEntry *b_entry = NULL;

  a_score = GPOINTER_TO_INT (g_hash_table_lookup (self->match_scores, a));
  b_score = GPOINTER_TO_INT (g_hash_table_lookup (self->match_scores, b));

  a_entry = bz_entry_group_get_ui_entry (a);
  b_entry = bz_entry_group_get_ui_entry (b);

  if (a_score == b_score)
    {
      /* slightly favor entries with a description */
      if (bz_entry_get_description (a_entry) != NULL)
        a_score += 1;
      if (bz_entry_get_description (b_entry) != NULL)
        b_score += 1;

      /* greatly favor entries with an icon */
      if (bz_entry_get_icon_paintable (a_entry) != NULL)
        a_score += 2;
      if (bz_entry_get_icon_paintable (b_entry) != NULL)
        b_score += 2;
    }
  /* fallback */
  if (a_score == b_score)
    {
      const char *a_title = NULL;
      const char *b_title = NULL;
      int         cmp_res = 0;

      a_title = bz_entry_get_title (a_entry);
      b_title = bz_entry_get_title (b_entry);
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
match (BzEntryGroup   *item,
       BzSearchWidget *self)
{
  BzEntry   *entry         = NULL;
  GPtrArray *search_tokens = NULL;
  int        score         = 0;

  entry = bz_entry_group_get_ui_entry (item);
  if (entry == NULL)
    return FALSE;

  if (gtk_check_button_get_active (self->foss_check) &&
      !bz_entry_get_is_foss (entry))
    goto done;

  if (gtk_check_button_get_active (self->flathub_check) &&
      !bz_entry_get_is_flathub (entry))
    goto done;

  search_tokens = bz_entry_get_search_tokens (entry);

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
          int         token_score     = 0;
          const char *match_token     = NULL;
          int         match_token_len = 0;

          match_token     = g_ptr_array_index (self->match_tokens, i);
          match_token_len = strlen (match_token);

          for (guint j = 0; j < search_tokens->len; j++)
            {
              const char *search_token = NULL;

              search_token = g_ptr_array_index (search_tokens, j);

              if (g_strcmp0 (match_token, search_token) == 0)
                /* greatly reward exact matches */
                token_score += match_token_len * 5;
              else if (g_str_match_string (match_token, search_token, TRUE))
                {
                  int search_token_len = 0;

                  /* TODO: unify impl with BzSearchEngine */

                  search_token_len = strlen (search_token);

                  if (search_token_len == match_token_len)
                    /* there may be captilization differences, etc */
                    token_score += match_token_len * 4;
                  else
                    {
                      double len_ratio = 0.0;
                      double add       = 0.0;

                      len_ratio = (double) match_token_len / (double) search_token_len;
                      add       = (double) match_token_len * len_ratio;
                      token_score += (int) MAX (round (add), 1.0);
                    }
                }
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
                 BzSearchWidget     *self)
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

      gtk_widget_set_visible (GTK_WIDGET (self->search_busy), FALSE);
      gtk_revealer_set_reveal_child (self->entry_list_revealer, n_items > 0);
    }
}

static void
selected_item_changed (GtkSingleSelection *model,
                       GParamSpec         *pspec,
                       BzSearchWidget     *self)
{
  guint selected = 0;

  g_clear_object (&self->previewing);
  selected = gtk_single_selection_get_selected (model);

  if (selected != GTK_INVALID_LIST_POSITION)
    self->previewing = g_list_model_get_item (G_LIST_MODEL (model), selected);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PREVIEWING]);
}

static void
list_activate (GtkListView    *list_view,
               guint           position,
               BzSearchWidget *self)
{
  GtkSelectionModel *model = NULL;

  model = gtk_list_view_get_model (self->list_view);
  emit_idx (self, G_LIST_MODEL (model), position);
}

static void
update_filter (BzSearchWidget *self)
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
  g_clear_handle_id (&self->search_update_timeout, g_source_remove);

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
  if (search_text != NULL && *search_text != '\0')
    {
      if (gtk_check_button_get_active (self->regex_check))
        {
          g_autoptr (GError) local_error = NULL;

          self->match_regex = g_regex_new (
              search_text, G_REGEX_DEFAULT,
              G_REGEX_MATCH_DEFAULT, &local_error);

          if (self->match_regex != NULL)
            {
              gtk_label_set_label (self->regex_error, NULL);
              gtk_widget_set_tooltip_text (GTK_WIDGET (self->regex_error), NULL);
            }
          else
            {
              gtk_label_set_label (self->regex_error, local_error->message);
              gtk_widget_set_tooltip_text (GTK_WIDGET (self->regex_error), local_error->message);
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
    }

  selection_model   = GTK_SINGLE_SELECTION (gtk_list_view_get_model (self->list_view));
  sort_list_model   = GTK_SORT_LIST_MODEL (gtk_single_selection_get_model (selection_model));
  filter_list_model = GTK_FILTER_LIST_MODEL (gtk_sort_list_model_get_model (sort_list_model));
  filter            = gtk_filter_list_model_get_filter (filter_list_model);

  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);

  if (gtk_filter_list_model_get_pending (filter_list_model) > 0)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->search_busy), TRUE);
      gtk_revealer_set_reveal_child (self->entry_list_revealer, FALSE);
    }
  else
    gtk_revealer_set_reveal_child (
        self->entry_list_revealer,
        g_list_model_get_n_items (
            G_LIST_MODEL (filter_list_model)) > 0);
}

static void
emit_idx (BzSearchWidget *self,
          GListModel     *model,
          guint           selected_idx)
{
  g_autoptr (BzEntryGroup) group = NULL;

  group = g_list_model_get_item (G_LIST_MODEL (model), selected_idx);
  g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}
