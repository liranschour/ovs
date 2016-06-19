/* Copyright (c) 2015, 2016 Nicira, Inc.
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

#include <config.h>
#include "filter.h"

#include "openvswitch/vlog.h"
#include "ovn/lib/ovn-sb-idl.h"
#include "ovn-controller.h"
#include "lport.h"

VLOG_DEFINE_THIS_MODULE(filter);

static struct hmap filtered_dps = HMAP_INITIALIZER(&filtered_dps);
static struct hmap filtered_lps = HMAP_INITIALIZER(&filtered_lps);

struct filtered_dp {
    struct hmap_node hmap_node;
    int64_t tunnel_key;
    struct uuid datapath;
};

struct filtered_lp {
    struct hmap_node hmap_node;
    const char *lport_name;
    bool used;
};

/* Initializes 'idl' so that by default no rows are replicated in tables that
 * ovn-controller does not need to be fully replicated. */
void
filter_init(struct ovsdb_idl *idl)
{
    sbrec_port_binding_add_clause_false(idl);
    sbrec_mac_binding_add_clause_false(idl);
    sbrec_logical_flow_add_clause_false(idl);
    sbrec_multicast_group_add_clause_false(idl);
}

/* Marks all replicated ports as "unused". */
void
filter_mark_unused(void)
{
    struct filtered_lp *lp;

    HMAP_FOR_EACH (lp, hmap_node, &filtered_lps) {
        lp->used = false;
    }
}

/* Clears the filter conditions, so that no rows are replicated. */
void
filter_clear(struct ovsdb_idl *idl)
{
    struct filtered_lp *lp, *next_lp;
    struct filtered_lp *dp, *next_dp;

    HMAP_FOR_EACH_SAFE (lp, next_lp, hmap_node, &filtered_lps) {
        hmap_remove(&filtered_lps, &lp->hmap_node);
        free(lp);
    }
    HMAP_FOR_EACH_SAFE (dp, next_dp, hmap_node, &filtered_dps) {
        hmap_remove(&filtered_dps, &dp->hmap_node);
        free(dp);
    }

    ovsdb_idl_condition_reset(idl, &sbrec_table_port_binding);
    ovsdb_idl_condition_reset(idl, &sbrec_table_logical_flow);
    ovsdb_idl_condition_reset(idl, &sbrec_table_mac_binding);
    ovsdb_idl_condition_reset(idl, &sbrec_table_multicast_group);

    filter_init(idl);
}

static struct filtered_dp *
lookup_dp_by_key(int64_t tunnel_key)
{
    struct filtered_dp *dp;

    HMAP_FOR_EACH_WITH_HASH (dp, hmap_node, tunnel_key, &filtered_dps) {
        if (dp->tunnel_key == tunnel_key) {
            return dp;
        }
    }
    return NULL;
}

/* Un-replicates logical ports that have not been re-added via filter_lport()
 * since the last call to filter_mark_unused(). */
void
filter_remove_unused_lports(struct controller_ctx *ctx,
                            const struct lport_index *lports_index)
{
    struct filtered_lp *lp, *next;

    HMAP_FOR_EACH_SAFE (lp, next, hmap_node, &filtered_lps) {
        if (!lp->used) {
            const struct sbrec_port_binding *pb =
                lport_lookup_by_name(lports_index, lp->lport_name);
            if (!pb) {
                continue;
            }
            if (lookup_dp_by_key(pb->datapath->tunnel_key)) {
                VLOG_DBG("Unfilter Port %s", lp->lport_name);
                sbrec_port_binding_remove_clause_logical_port(ctx->ovnsb_idl,
                                                              OVSDB_F_EQ,
                                                              lp->lport_name);
                hmap_remove(&filtered_lps, &lp->hmap_node);
                free(lp);
            }
        }
    }
}

/* Adds 'lport_name' to the logical ports whose Port_Binding rows are
 * replicated. */
void
filter_lport(struct controller_ctx *ctx, const char *lport_name)
{
    struct filtered_lp *lp;
    size_t hash = hash_string(lport_name, 0);

    HMAP_FOR_EACH_WITH_HASH(lp, hmap_node, hash, &filtered_lps) {
        if (!strcmp(lp->lport_name, lport_name)) {
            lp->used = true;
            return;
        }
    }

    VLOG_DBG("Filter Port %s", lport_name);

    sbrec_port_binding_add_clause_logical_port(ctx->ovnsb_idl,
                                               OVSDB_F_EQ,
                                               lport_name);

    lp = xmalloc(sizeof *lp);
    lp->lport_name = xstrdup(lport_name);
    lp->used = true;
    hmap_insert(&filtered_lps, &lp->hmap_node, hash);
}

/* Adds 'datapath' to the datapaths whose Port_Binding, Mac_Binding,
 * Logical_Flow, and Multicast_Group rows are replicated. */
void
filter_datapath(struct controller_ctx *ctx,
                const struct sbrec_datapath_binding *datapath)
{
    struct filtered_dp *dp;

    dp = lookup_dp_by_key(datapath->tunnel_key);
    if (dp) {
        return;
    }

    dp = xmalloc(sizeof *dp);
    dp->tunnel_key = datapath->tunnel_key;
    dp->datapath = datapath->header_.uuid;
    hmap_insert(&filtered_dps, &dp->hmap_node, datapath->tunnel_key);

    VLOG_DBG("Filter DP "UUID_FMT, UUID_ARGS(&datapath->header_.uuid));
    sbrec_port_binding_add_clause_datapath(ctx->ovnsb_idl,
                                           OVSDB_F_EQ,
                                           &dp->datapath);
    sbrec_mac_binding_add_clause_datapath(ctx->ovnsb_idl,
                                          OVSDB_F_EQ,
                                          &dp->datapath);
    sbrec_logical_flow_add_clause_logical_datapath(ctx->ovnsb_idl,
                                                   OVSDB_F_EQ,
                                                   &dp->datapath);
    sbrec_multicast_group_add_clause_datapath(ctx->ovnsb_idl,
                                              OVSDB_F_EQ,
                                              &dp->datapath);

}

/* Removes 'datapath' from the datapaths whose Port_Binding, Mac_Binding,
 * Logical_Flow, and Multicast_Group rows are replicated. */
void
unfilter_datapath(struct controller_ctx *ctx, int64_t tunnel_key)
{
    struct filtered_dp *dp = lookup_dp_by_key(tunnel_key);

    if (dp) {
        VLOG_DBG("Unfilter DP "UUID_FMT,
                  UUID_ARGS(&dp->datapath));
        sbrec_port_binding_remove_clause_datapath(ctx->ovnsb_idl,
                                                  OVSDB_F_EQ,
                                                  &dp->datapath);
        sbrec_mac_binding_remove_clause_datapath(ctx->ovnsb_idl,
                                                 OVSDB_F_EQ,
                                                 &dp->datapath);
        sbrec_logical_flow_remove_clause_logical_datapath(ctx->ovnsb_idl,
                                                          OVSDB_F_EQ,
                                                          &dp->datapath);
        sbrec_multicast_group_remove_clause_datapath(ctx->ovnsb_idl,
                                                     OVSDB_F_EQ,
                                                     &dp->datapath);
        hmap_remove(&filtered_dps, &dp->hmap_node);
        free(dp);
    }
}
