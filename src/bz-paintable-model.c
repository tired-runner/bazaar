/* bz-paintable-model.c
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

#include <glycin-gtk4.h>

#include "bz-async-texture.h"
#include "bz-paintable-model.h"

struct _BzPaintableModel
{
  GObject parent_instance;

  DexScheduler *scheduler;

  GListModel      *input;
  GtkMapListModel *output;
  GHashTable      *cache;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
    BzPaintableModel,
    bz_paintable_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
items_changed (GListModel       *model,
               guint             position,
               guint             removed,
               guint             added,
               BzPaintableModel *self);

static gpointer
map_files_to_textures (GFile            *file,
                       BzPaintableModel *self);

static void
bz_paintable_model_dispose (GObject *object)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (object);

  dex_clear (&self->scheduler);
  g_clear_object (&self->input);
  g_clear_object (&self->output);
  g_clear_pointer (&self->cache, g_hash_table_unref);

  G_OBJECT_CLASS (bz_paintable_model_parent_class)->dispose (object);
}

static void
bz_paintable_model_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_paintable_model_get_model (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_paintable_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_paintable_model_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_paintable_model_class_init (BzPaintableModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_paintable_model_dispose;
  object_class->get_property = bz_paintable_model_get_property;
  object_class->set_property = bz_paintable_model_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_paintable_model_init (BzPaintableModel *self)
{
  self->output = gtk_map_list_model_new (
      NULL, (GtkMapListModelMapFunc) map_files_to_textures,
      self, NULL);
  g_signal_connect (
      self->output, "items-changed",
      G_CALLBACK (items_changed), self);

  self->cache = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      g_object_unref, g_object_unref);
}

static GType
list_model_get_item_type (GListModel *list)
{
  return BZ_TYPE_ASYNC_TEXTURE;
}

static guint
list_model_get_n_items (GListModel *list)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (list);
  return g_list_model_get_n_items (G_LIST_MODEL (self->output));
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (list);
  return g_list_model_get_item (G_LIST_MODEL (self->output), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items   = list_model_get_n_items;
  iface->get_item      = list_model_get_item;
}

BzPaintableModel *
bz_paintable_model_new (DexScheduler *scheduler,
                        GListModel   *model)
{
  BzPaintableModel *self = NULL;

  g_return_val_if_fail (DEX_IS_SCHEDULER (scheduler), NULL);

  self = g_object_new (
      BZ_TYPE_PAINTABLE_MODEL,
      "model", model,
      NULL);
  self->scheduler = dex_ref (scheduler);

  return self;
}

void
bz_paintable_model_set_model (BzPaintableModel *self,
                              GListModel       *model)
{
  g_return_if_fail (BZ_IS_PAINTABLE_MODEL (self));
  g_return_if_fail (model == NULL ||
                    (G_IS_LIST_MODEL (model) &&
                     g_list_model_get_item_type (model) == G_TYPE_FILE));

  g_clear_object (&self->input);
  if (model != NULL)
    self->input = g_object_ref (model);
  gtk_map_list_model_set_model (self->output, self->input);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
bz_paintable_model_get_model (BzPaintableModel *self)
{
  g_return_val_if_fail (BZ_IS_PAINTABLE_MODEL (self), NULL);
  return self->input;
}

static void
items_changed (GListModel       *model,
               guint             position,
               guint             removed,
               guint             added,
               BzPaintableModel *self)
{
  g_list_model_items_changed (
      G_LIST_MODEL (self), position, removed, added);
}

static gpointer
map_files_to_textures (GFile            *file,
                       BzPaintableModel *self)
{
  BzAsyncTexture *result = NULL;

  result = g_hash_table_lookup (self->cache, file);
  if (result == NULL)
    {
      result = bz_async_texture_new_lazy (file, self->scheduler);
      g_hash_table_replace (self->cache, g_object_ref (file), result);
    }

  g_object_unref (file);
  return g_object_ref (result);
}
