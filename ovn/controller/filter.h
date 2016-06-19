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

struct controller_ctx;
struct ovsdb_idl;
struct sbrec_port_binding;
struct lport_index;

void filter_init(struct ovsdb_idl *idl);
void filter_clear(struct ovsdb_idl *idl);
void filter_mark_unused(void);
void filter_remove_unused(struct controller_ctx *ctx,
                          const struct lport_index *lports);
void filter_lport(struct controller_ctx *ctx, const char *lport_name);
void filter_datapath(struct controller_ctx *ctx,
                     const struct sbrec_port_binding *pb);

#endif /* ovn/controller/filter.h */
