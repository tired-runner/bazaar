/* bz-async-texture.c
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
#include <libdex.h>

#include "bz-async-texture.h"
#include "bz-util.h"

struct _BzAsyncTexture
{
  GObject parent_instance;

  GFile   *source;
  gboolean loaded;

  gboolean      lazy;
  DexScheduler *scheduler;

  DexFuture    *task;
  GdkPaintable *paintable;
};

static void paintable_iface_init (GdkPaintableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
    BzAsyncTexture,
    bz_async_texture,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_LOADED,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    load,
    Load,
    { GFile *file; },
    BZ_RELEASE_DATA (file, g_object_unref));
static DexFuture *
load_fiber (LoadData *data);
static DexFuture *
load_finally (DexFuture      *future,
              BzAsyncTexture *self);

static void
load (BzAsyncTexture *self);

static void
bz_async_texture_dispose (GObject *object)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  g_clear_object (&self->source);
  dex_clear (&self->scheduler);
  dex_clear (&self->task);
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (bz_async_texture_parent_class)->dispose (object);
}

static void
bz_async_texture_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;
    case PROP_LOADED:
      g_value_set_boolean (value, self->loaded);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_async_texture_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  // BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
    case PROP_LOADED:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_async_texture_class_init (BzAsyncTextureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_async_texture_dispose;
  object_class->get_property = bz_async_texture_get_property;
  object_class->set_property = bz_async_texture_set_property;

  props[PROP_SOURCE] =
      g_param_spec_object (
          "source",
          NULL, NULL,
          G_TYPE_FILE,
          G_PARAM_READABLE);

  props[PROP_LOADED] =
      g_param_spec_boolean (
          "loaded",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_async_texture_init (BzAsyncTexture *self)
{
  self->paintable = gdk_paintable_new_empty (0, 0);
}

static void
paintable_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);

  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  return gdk_paintable_snapshot (self->paintable, snapshot, width, height);
}

static GdkPaintable *
paintable_get_current_image (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);
  return gdk_paintable_get_current_image (self->paintable);
}

static GdkPaintableFlags
paintable_get_flags (GdkPaintable *paintable)
{
  // BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);
  return 0;
}

static int
paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);

  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  return gdk_paintable_get_intrinsic_width (self->paintable);
}

static int
paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);
  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  return gdk_paintable_get_intrinsic_height (self->paintable);
}

static double
paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);
  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->snapshot                   = paintable_snapshot;
  iface->get_current_image          = paintable_get_current_image;
  iface->get_flags                  = paintable_get_flags;
  iface->get_intrinsic_width        = paintable_get_intrinsic_width;
  iface->get_intrinsic_height       = paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = paintable_get_intrinsic_aspect_ratio;
}

BzAsyncTexture *
bz_async_texture_new (GFile *source)
{
  BzAsyncTexture *self = NULL;

  g_return_val_if_fail (G_IS_FILE (source), NULL);

  self            = g_object_new (BZ_TYPE_ASYNC_TEXTURE, NULL);
  self->source    = g_object_ref (source);
  self->lazy      = FALSE;
  self->scheduler = NULL;

  load (self);

  return self;
}

BzAsyncTexture *
bz_async_texture_new_lazy (GFile        *source,
                           DexScheduler *scheduler)
{
  BzAsyncTexture *self = NULL;

  g_return_val_if_fail (G_IS_FILE (source), NULL);
  g_return_val_if_fail (scheduler == NULL || DEX_IS_SCHEDULER (scheduler), NULL);

  self            = g_object_new (BZ_TYPE_ASYNC_TEXTURE, NULL);
  self->source    = g_object_ref (source);
  self->lazy      = TRUE;
  self->scheduler = scheduler != NULL ? dex_ref (scheduler) : NULL;

  return self;
}

gboolean
bz_async_texture_get_loaded (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->loaded;
}

GdkTexture *
bz_async_texture_get_texture (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), NULL);
  g_return_val_if_fail (self->loaded, NULL);
  return GDK_TEXTURE (self->paintable);
}

static void
load (BzAsyncTexture *self)
{
  g_autoptr (LoadData) data    = NULL;
  g_autoptr (DexFuture) future = NULL;

  data       = load_data_new ();
  data->file = g_object_ref (self->source);

  future = dex_scheduler_spawn (
      self->scheduler != NULL
          ? self->scheduler
          : dex_thread_pool_scheduler_get_default (),
      0, (DexFiberFunc) load_fiber,
      load_data_ref (data), load_data_unref);
  future = dex_future_finally (
      future,
      (DexFutureCallback) load_finally,
      g_object_ref (self), g_object_unref);
  self->task = g_steal_pointer (&future);
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
load_finally (DexFuture      *future,
              BzAsyncTexture *self)
{
  const GValue *value = NULL;

  value = dex_future_get_value (future, NULL);
  if (value != NULL)
    {
      g_clear_object (&self->paintable);
      self->paintable = g_value_dup_object (value);

      self->loaded = TRUE;
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOADED]);
    }

  dex_clear (&self->task);

  return NULL;
}
