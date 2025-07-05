/* bz-flathub-page.c
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
#include "bz-dynamic-list-view.h"
#include "bz-entry-group.h"
#include "bz-flathub-page.h"
#include "bz-inhibited-scrollable.h"
#include "bz-section-view.h"

struct _BzFlathubPage
{
  AdwBin parent_instance;

  BzFlathubState *state;

  /* Template widgets */
  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzFlathubPage, bz_flathub_page, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_ENTRY_SELECTED,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
bz_flathub_page_dispose (GObject *object)
{
  BzFlathubPage *self = BZ_FLATHUB_PAGE (object);

  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_flathub_page_parent_class)->dispose (object);
}

static void
bz_flathub_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzFlathubPage *self = BZ_FLATHUB_PAGE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, bz_flathub_page_get_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzFlathubPage *self = BZ_FLATHUB_PAGE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      bz_flathub_page_set_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_page_class_init (BzFlathubPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_flathub_page_dispose;
  object_class->get_property = bz_flathub_page_get_property;
  object_class->set_property = bz_flathub_page_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_FLATHUB_STATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_ENTRY_SELECTED] =
      g_signal_new (
          "entry-selected",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);
  g_signal_set_va_marshaller (
      signals[SIGNAL_ENTRY_SELECTED],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_INHIBITED_SCROLLABLE);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_APP_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-flathub-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzFlathubPage, stack);
}

static void
bz_flathub_page_init (BzFlathubPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_flathub_page_new (void)
{
  return g_object_new (BZ_TYPE_FLATHUB_PAGE, NULL);
}

void
bz_flathub_page_set_state (BzFlathubPage  *self,
                           BzFlathubState *state)
{
  g_return_if_fail (BZ_IS_FLATHUB_PAGE (self));
  g_return_if_fail (state == NULL || BZ_IS_FLATHUB_STATE (state));

  g_clear_object (&self->state);
  if (state != NULL)
    self->state = g_object_ref (state);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

BzFlathubState *
bz_flathub_page_get_state (BzFlathubPage *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_PAGE (self), NULL);
  return self->state;
}
