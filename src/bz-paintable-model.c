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

#include "bz-paintable-model.h"
#include "bz-util.h"

struct _BzPaintableModel
{
  GObject parent_instance;

  DexScheduler *scheduler;

  GListModel *model;
  GHashTable *tracking;
};

static void list_model_iface_init (GListModelInterface *iface);

static void
items_changed (GListModel       *model,
               guint             position,
               guint             removed,
               guint             added,
               BzPaintableModel *self);

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

BZ_DEFINE_DATA (
    load,
    Load,
    {
      BzPaintableModel *self;
      GFile            *file;
    },
    BZ_RELEASE_DATA (self, g_object_unref);
    BZ_RELEASE_DATA (file, g_object_unref));
static DexFuture *
load_fiber (LoadData *data);
static DexFuture *
load_finally (DexFuture *future,
              LoadData  *data);

static void
bz_paintable_model_dispose (GObject *object)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (
        self->model, items_changed, self);

  g_clear_pointer (&self->scheduler, dex_unref);
  g_clear_object (&self->model);
  g_clear_pointer (&self->tracking, g_hash_table_unref);

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
  self->tracking = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      g_object_unref, g_object_unref);
}

static GType
list_model_get_item_type (GListModel *list)
{
  return GDK_TYPE_PAINTABLE;
}

static guint
list_model_get_n_items (GListModel *list)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (list);

  return g_list_model_get_n_items (G_LIST_MODEL (self->model));
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  BzPaintableModel *self = BZ_PAINTABLE_MODEL (list);
  g_autoptr (GFile) file = NULL;
  GdkPaintable *lookup   = NULL;

  file   = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  lookup = g_hash_table_lookup (self->tracking, file);

  if (lookup != NULL)
    return g_object_ref (lookup);
  else
    {
      GdkPaintable *temp        = NULL;
      g_autoptr (LoadData) data = NULL;
      DexFuture *future         = NULL;

      temp = gdk_paintable_new_empty (512, 512);
      g_hash_table_replace (self->tracking, g_object_ref (file), g_object_ref (temp));

      data       = load_data_new ();
      data->self = g_object_ref (self);
      data->file = g_list_model_get_item (G_LIST_MODEL (self->model), position);

      future = dex_scheduler_spawn (
          self->scheduler, 0, (DexFiberFunc) load_fiber,
          load_data_ref (data), load_data_unref);
      future = dex_future_finally (
          future,
          (DexFutureCallback) load_finally,
          load_data_ref (data), load_data_unref);

      /* TODO: make cancellable */
      dex_future_disown (future);

      return temp;
    }
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
  guint old_length = 0;

  g_return_if_fail (BZ_IS_PAINTABLE_MODEL (self));
  g_return_if_fail (model == NULL ||
                    (G_IS_LIST_MODEL (model) &&
                     g_list_model_get_item_type (model) == G_TYPE_FILE));

  if (self->model != NULL)
    {
      old_length = g_list_model_get_n_items (self->model);
      g_signal_handlers_disconnect_by_func (
          self->model, items_changed, self);
    }
  g_clear_object (&self->model);

  if (model != NULL)
    {
      self->model = g_object_ref (model);
      g_signal_connect (
          model, "items-changed",
          G_CALLBACK (items_changed), self);

      g_list_model_items_changed (
          G_LIST_MODEL (self), 0, old_length,
          g_list_model_get_n_items (model));
    }
  else
    g_list_model_items_changed (
        G_LIST_MODEL (self), 0, old_length, 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
bz_paintable_model_get_model (BzPaintableModel *self)
{
  g_return_val_if_fail (BZ_IS_PAINTABLE_MODEL (self), NULL);

  return self->model;
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

static DexFuture *
load_fiber (LoadData *data)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GlyLoader) loader   = NULL;
  g_autoptr (GlyImage) image     = NULL;
  g_autoptr (GlyFrame) frame     = NULL;
  g_autoptr (GdkTexture) texture = NULL;

  loader = gly_loader_new (data->file);
  image  = gly_loader_load (loader, &local_error);
  if (image == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  frame = gly_image_next_frame (image, &local_error);
  if (frame == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  texture = gly_gtk_frame_get_texture (frame);
  return dex_future_new_for_object (texture);
}

static DexFuture *
load_finally (DexFuture *future,
              LoadData  *data)
{
  BzPaintableModel *self  = data->self;
  const GValue     *value = NULL;

  value = dex_future_get_value (future, NULL);

  if (value != NULL)
    {
      guint         n_files   = 0;
      guint         position  = 0;
      GdkPaintable *paintable = NULL;

      n_files = g_list_model_get_n_items (G_LIST_MODEL (self->model));
      for (position = 0; position < n_files; position++)
        {
          g_autoptr (GFile) file = NULL;

          file = g_list_model_get_item (self->model, position);
          if (file == data->file)
            break;
        }

      if (position < n_files)
        {
          paintable = g_value_get_object (value);
          g_hash_table_replace (
              self->tracking,
              g_object_ref (data->file),
              g_object_ref (paintable));

          g_list_model_items_changed (
              G_LIST_MODEL (self), position, 1, 1);
        }
    }

  return dex_future_new_true ();
}
