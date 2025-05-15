/* bz-flatpak-instance.h
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

#include "bz-flatpak-entry.h"

G_BEGIN_DECLS

#define BZ_TYPE_FLATPAK_INSTANCE (bz_flatpak_instance_get_type ())
G_DECLARE_FINAL_TYPE (BzFlatpakInstance, bz_flatpak_instance, BZ, FLATPAK_INSTANCE, GObject)

DexFuture *
bz_flatpak_instance_new (void);

typedef void (*BzFlatpakGatherEntriesFunc) (
    BzEntry *entry,
    gpointer user_data);

DexFuture *
bz_flatpak_instance_ref_remote_apps (BzFlatpakInstance         *self,
                                     BzFlatpakGatherEntriesFunc progress_func,
                                     gpointer                   user_data,
                                     GDestroyNotify             destroy_user_data);

DexFuture *
bz_flatpak_instance_ref_updates (BzFlatpakInstance *self);

typedef void (*BzFlatpakTransactionProgressFunc) (
    BzFlatpakEntry *entry,
    const char     *status,
    gboolean        is_estimating,
    int             progress_num,
    guint64         bytes_transferred,
    guint64         start_time,
    gpointer        user_data);

DexFuture *
bz_flatpak_instance_schedule_transaction (BzFlatpakInstance               *self,
                                          BzFlatpakEntry                 **installs,
                                          guint                            n_installs,
                                          BzFlatpakEntry                 **updates,
                                          guint                            n_updates,
                                          BzFlatpakTransactionProgressFunc progress_func,
                                          gpointer                         user_data,
                                          GDestroyNotify                   destroy_user_data);

G_END_DECLS
