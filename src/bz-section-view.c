/* bz-section-view.c
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

#include "bz-app-tile.h"
#include "bz-async-texture.h"
#include "bz-entry-group.h"
#include "bz-section-view.h"

struct _BzSectionView
{
  AdwBin parent_instance;

  BzContentSection *section;
  GListModel       *classes;

  /* Template widgets */
  GtkOverlay *banner_text_overlay;
  GtkBox     *banner_text_bg;
  GtkBox     *banner_text;
  GtkFlowBox *tile_flow;
};

G_DEFINE_FINAL_TYPE (BzSectionView, bz_section_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_SECTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_GROUP_ACTIVATED,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static GtkWidget *
create_tile (BzEntryGroup  *group,
             BzSectionView *self);

static void
tile_clicked (GtkButton    *button,
              BzEntryGroup *group);

static void
bz_section_view_dispose (GObject *object)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  g_clear_object (&self->section);
  g_clear_object (&self->classes);

  G_OBJECT_CLASS (bz_section_view_parent_class)->dispose (object);
}

static void
bz_section_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      g_value_set_object (value, bz_section_view_get_section (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_section_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      bz_section_view_set_section (self, g_value_get_object (value));
      break;
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

static void
bz_section_view_class_init (BzSectionViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_section_view_dispose;
  object_class->get_property = bz_section_view_get_property;
  object_class->set_property = bz_section_view_set_property;

  props[PROP_SECTION] =
      g_param_spec_object (
          "section",
          NULL, NULL,
          BZ_TYPE_CONTENT_SECTION,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_GROUP_ACTIVATED] =
      g_signal_new (
          "group-activated",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);
  g_signal_set_va_marshaller (
      signals[SIGNAL_GROUP_ACTIVATED],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_ASYNC_TEXTURE);
  g_type_ensure (BZ_TYPE_APP_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-section-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, banner_text_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, banner_text_bg);
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, banner_text);
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, tile_flow);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
}

static void
bz_section_view_init (BzSectionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_overlay_set_measure_overlay (
      self->banner_text_overlay,
      GTK_WIDGET (self->banner_text),
      TRUE);
  gtk_overlay_set_clip_overlay (
      self->banner_text_overlay,
      GTK_WIDGET (self->banner_text),
      TRUE);
}

GtkWidget *
bz_section_view_new (BzContentSection *section)
{
  return g_object_new (
      BZ_TYPE_SECTION_VIEW,
      "section", section,
      NULL);
}

void
bz_section_view_set_section (BzSectionView    *self,
                             BzContentSection *section)
{
  g_return_if_fail (BZ_IS_SECTION_VIEW (self));
  g_return_if_fail (section == NULL || BZ_IS_CONTENT_SECTION (section));

  g_clear_object (&self->section);

  if (self->classes != NULL)
    {
      guint n_classes = 0;

      n_classes = g_list_model_get_n_items (self->classes);
      for (guint i = 0; i < n_classes; i++)
        {
          g_autoptr (GtkStringObject) string = NULL;
          const char *class                  = NULL;

          string = g_list_model_get_item (self->classes, i);
          class  = gtk_string_object_get_string (string);

          gtk_widget_remove_css_class (GTK_WIDGET (self), class);
        }
    }
  g_clear_object (&self->classes);

  if (section != NULL)
    {
      g_autoptr (GListModel) appids = NULL;

      self->section = g_object_ref (section);
      g_object_get (section, "classes", &self->classes, NULL);

      if (self->classes != NULL)
        {
          guint n_classes = 0;

          n_classes = g_list_model_get_n_items (self->classes);
          for (guint i = 0; i < n_classes; i++)
            {
              g_autoptr (GtkStringObject) string = NULL;
              const char *class                  = NULL;

              string = g_list_model_get_item (self->classes, i);
              class  = gtk_string_object_get_string (string);

              gtk_widget_add_css_class (GTK_WIDGET (self), class);
            }
        }

      g_object_get (section, "appids", &appids, NULL);
      gtk_flow_box_bind_model (
          self->tile_flow, appids,
          (GtkFlowBoxCreateWidgetFunc) create_tile,
          self, NULL);
    }
  else
    gtk_flow_box_bind_model (self->tile_flow, NULL, NULL, NULL, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SECTION]);
}

BzContentSection *
bz_section_view_get_section (BzSectionView *self)
{
  g_return_val_if_fail (BZ_IS_SECTION_VIEW (self), NULL);
  return self->section;
}

static GtkWidget *
create_tile (BzEntryGroup  *group,
             BzSectionView *self)
{
  GtkWidget *button = NULL;
  GtkWidget *tile   = NULL;

  button = gtk_button_new ();
  gtk_widget_add_css_class (button, "card");
  g_signal_connect (button, "clicked", G_CALLBACK (tile_clicked), group);

  tile = bz_app_tile_new ();
  bz_app_tile_set_group (BZ_APP_TILE (tile), group);

  gtk_button_set_child (GTK_BUTTON (button), tile);
  return button;
}

static void
tile_clicked (GtkButton    *button,
              BzEntryGroup *group)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_SECTION_VIEW);
  g_signal_emit (self, signals[SIGNAL_GROUP_ACTIVATED], 0, group);
}
