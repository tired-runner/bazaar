/* bz-addons-dialog.c
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

#include "bz-addons-dialog.h"
#include "bz-entry.h"

struct _BzAddonsDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;

  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzAddonsDialog, bz_addons_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,

  PROP_ENTRY,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
bz_addons_dialog_dispose (GObject *object)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  g_clear_object (&self->entry);

  G_OBJECT_CLASS (bz_addons_dialog_parent_class)->dispose (object);
}

static void
bz_addons_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_value_set_object (value, self->entry);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_addons_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_clear_object (&self->entry);
      self->entry = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
install_cb (GtkListItem *list_item,
            GtkButton   *button)
{
  BzEntry   *entry = NULL;
  GtkWidget *self  = NULL;

  entry = gtk_list_item_get_item (list_item);

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_ADDONS_DIALOG);
  g_assert (self != NULL);

  g_signal_emit (self, signals[SIGNAL_INSTALL], 0, entry);
}

static void
remove_cb (GtkListItem *list_item,
           GtkButton   *button)
{
}

static void
bz_addons_dialog_class_init (BzAddonsDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_addons_dialog_dispose;
  object_class->get_property = bz_addons_dialog_get_property;
  object_class->set_property = bz_addons_dialog_set_property;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_INSTALL],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/bazaar/bz-addons-dialog.ui");
  gtk_widget_class_bind_template_callback (widget_class, install_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);
}

static void
bz_addons_dialog_init (BzAddonsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_addons_dialog_new (BzEntry *entry)
{
  BzAddonsDialog *addons_dialog = NULL;

  addons_dialog = g_object_new (
      BZ_TYPE_ADDONS_DIALOG,
      "entry", entry,
      NULL);

  return ADW_DIALOG (addons_dialog);
}
