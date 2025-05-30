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

#include "bz-progress-bar.h"
#include "bz-section-view.h"

struct _BzSectionView
{
  AdwBin parent_instance;

  BzContentSection *section;
  GListModel       *classes;

  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzSectionView, bz_section_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_SECTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

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

  g_type_ensure (BZ_TYPE_SECTION_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-section-view.ui");
}

static void
bz_section_view_init (BzSectionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SECTION]);
}

BzContentSection *
bz_section_view_get_section (BzSectionView *self)
{
  g_return_val_if_fail (BZ_IS_SECTION_VIEW (self), NULL);
  return self->section;
}
