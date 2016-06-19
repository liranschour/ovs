/* Copyright (c) 2015 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OVN_FILTER_H
#define OVN_FILTER_H 1

/* Database client filtering
 * -------------------------
 *
 * By default the OVSDB IDL replicates the entire contents of each table.  For
 * some tables, however, ovn-controller only needs some rows.  For example, in
 * the Logical_Flow table, it only needs the rows for logical datapaths that
 * are in use directly or indirectly on this hypervisor.  These functions aid
 * in limiting the rows that the IDL replicates.
 */

#include <stdint.h>

struct controller_ctx;
struct ovsdb_idl;
struct sbrec_datapath_binding;
struct lport_index;

void filter_init(struct ovsdb_idl *);
void filter_clear(struct ovsdb_idl *);
void filter_mark_unused(void);
void filter_skip_removal(void);
void filter_remove_unused_elements(struct controller_ctx *,
                                 const struct lport_index *);
void filter_lport(struct controller_ctx *, const char *lport_name);
void filter_datapath(struct controller_ctx *,
                     const struct sbrec_datapath_binding *);
void unfilter_datapath(struct controller_ctx *, int64_t tunnel_key);

#endif /* ovn/controller/filter.h */
