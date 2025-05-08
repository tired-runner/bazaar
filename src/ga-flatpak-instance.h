/* ga-flatpak-instance.h
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

#pragma once

#include <libdex.h>

#include "ga-flatpak-entry.h"

G_BEGIN_DECLS

#define GA_TYPE_FLATPAK_INSTANCE (ga_flatpak_instance_get_type ())
G_DECLARE_FINAL_TYPE (GaFlatpakInstance, ga_flatpak_instance, GA, FLATPAK_INSTANCE, GObject)

DexFuture *
ga_flatpak_instance_new (void);

DexFuture *
ga_flatpak_instance_ref_installed_apps (GaFlatpakInstance *self);

typedef void (*GaFlatpakGatherEntriesFunc) (
    GaEntry *entry,
    gpointer user_data);

DexFuture *
ga_flatpak_instance_ref_remote_apps (GaFlatpakInstance         *self,
                                     GaFlatpakGatherEntriesFunc progress_func,
                                     gpointer                   user_data,
                                     GDestroyNotify             destroy_user_data);

typedef void (*GaFlatpakInstallProgressFunc) (
    const char *status,
    gboolean    is_estimating,
    int         progress_num,
    guint64     bytes_transferred,
    guint64     start_time,
    gpointer    user_data);

DexFuture *
ga_flatpak_instance_install (GaFlatpakInstance           *self,
                             GaFlatpakEntry              *entry,
                             GaFlatpakInstallProgressFunc progress_func,
                             gpointer                     user_data,
                             GDestroyNotify               destroy_user_data);

G_END_DECLS
