/* bz-backend.h
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

#include "bz-entry.h"
#include "bz-transaction.h"

G_BEGIN_DECLS

#define BZ_TYPE_BACKEND (bz_backend_get_type ())
G_DECLARE_INTERFACE (BzBackend, bz_backend, BZ, BACKEND, GObject)

typedef void (*BzBackendGatherEntriesFunc) (
    BzEntry *entry,
    gpointer user_data);

typedef void (*BzBackendTransactionProgressFunc) (
    BzEntry    *entry,
    const char *status,
    gboolean    is_estimating,
    int         progress_num,
    guint64     bytes_transferred,
    guint64     start_time,
    gpointer    user_data);

struct _BzBackendInterface
{
  GTypeInterface parent_iface;

  /* DexFuture* -> gboolean */
  DexFuture *(*refresh) (BzBackend *self);

  /* DexFuture* -> gboolean */
  DexFuture *(*retrieve_remote_entries) (BzBackend                 *self,
                                         DexScheduler              *home_scheduler,
                                         GPtrArray                 *blocked_names,
                                         BzBackendGatherEntriesFunc progress_func,
                                         gpointer                   user_data,
                                         GDestroyNotify             destroy_user_data);

  /* DexFuture* -> GPtrArray* -> char* */
  DexFuture *(*retrieve_update_ids) (BzBackend *self);

  /* DexFuture* -> gboolean */
  DexFuture *(*schedule_transaction) (BzBackend                       *self,
                                      BzEntry                        **installs,
                                      guint                            n_installs,
                                      BzEntry                        **updates,
                                      guint                            n_updates,
                                      BzEntry                        **removals,
                                      guint                            n_removals,
                                      BzBackendTransactionProgressFunc progress_func,
                                      gpointer                         user_data,
                                      GDestroyNotify                   destroy_user_data);
};

DexFuture *
bz_backend_refresh (BzBackend *self);

DexFuture *
bz_backend_retrieve_remote_entries (BzBackend                 *self,
                                    DexScheduler              *home_scheduler,
                                    GPtrArray                 *blocked_names,
                                    BzBackendGatherEntriesFunc progress_func,
                                    gpointer                   user_data,
                                    GDestroyNotify             destroy_user_data);

DexFuture *
bz_backend_retrieve_remote_entries_with_blocklists (BzBackend                 *self,
                                                    DexScheduler              *home_scheduler,
                                                    GListModel                *blocklists,
                                                    BzBackendGatherEntriesFunc progress_func,
                                                    gpointer                   user_data,
                                                    GDestroyNotify             destroy_user_data);

DexFuture *
bz_backend_retrieve_update_ids (BzBackend *self);

DexFuture *
bz_backend_schedule_transaction (BzBackend                       *self,
                                 BzEntry                        **installs,
                                 guint                            n_installs,
                                 BzEntry                        **updates,
                                 guint                            n_updates,
                                 BzEntry                        **removals,
                                 guint                            n_removals,
                                 BzBackendTransactionProgressFunc progress_func,
                                 gpointer                         user_data,
                                 GDestroyNotify                   destroy_user_data);

DexFuture *
bz_backend_merge_and_schedule_transactions (BzBackend                       *self,
                                            GListModel                      *transactions,
                                            BzBackendTransactionProgressFunc progress_func,
                                            gpointer                         user_data,
                                            GDestroyNotify                   destroy_user_data);

G_END_DECLS
