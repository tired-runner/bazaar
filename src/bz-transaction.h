/* bz-transaction.h
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

#include "bz-entry.h"

G_BEGIN_DECLS

#define BZ_TYPE_TRANSACTION (bz_transaction_get_type ())
G_DECLARE_DERIVABLE_TYPE (BzTransaction, bz_transaction, BZ, TRANSACTION, GObject)

struct _BzTransactionClass
{
  GObjectClass parent_class;
};

BzTransaction *
bz_transaction_new_full (BzEntry **installs,
                         guint     n_installs,
                         BzEntry **updates,
                         guint     n_updates,
                         BzEntry **removals,
                         guint     n_removals);

GListModel *
bz_transaction_get_installs (BzTransaction *self);

GListModel *
bz_transaction_get_updates (BzTransaction *self);

GListModel *
bz_transaction_get_removals (BzTransaction *self);

G_END_DECLS
