/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015-2017 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_filter.h>
#include <fluent-bit/flb_str.h>
#include <fluent-bit/flb_router.h>

static inline int instance_id(struct flb_filter_plugin *p,
                              struct flb_config *config)
{
    int c = 0;
    struct mk_list *head;
    struct flb_filter_instance *entry;

    mk_list_foreach(head, &config->filters) {
        entry = mk_list_entry(head, struct flb_filter_instance, _head);
        if (entry->p == p) {
            c++;
        }
    }

    return c;
}

void flb_filter_do(void *data, size_t bytes,
                   char *tag, int tag_len,
                   struct flb_config *config)
{
    struct mk_list *head;
    struct flb_filter_instance *f_ins;

    mk_list_foreach(head, &config->filter_plugins) {
        f_ins = mk_list_entry(head, struct flb_filter_instance, _head);
        if (flb_router_match(tag, f_ins->match)) {
            /* Invoke the filter callback */
            f_ins->p->cb_filter(data, bytes,   /* msgpack raw data */
                                tag, tag_len,  /* input tag        */
                                f_ins,         /* filter instance  */
                                f_ins->data,   /* filter priv data */
                                config);
        }
    }
}

int flb_filter_set_property(struct flb_filter_instance *filter, char *k, char *v)
{
    struct flb_config_prop *prop;

    /* Append any remaining configuration key to prop list */
    prop = flb_malloc(sizeof(struct flb_config_prop));
    if (!prop) {
        return -1;
    }

    prop->key = flb_strdup(k);
    prop->val = flb_strdup(v);
    mk_list_add(&prop->_head, &filter->properties);

    return 0;
}

struct flb_filter_instance *flb_filter_new(struct flb_config *config,
                                           char *filter, void *data)
{
    int id;
    struct mk_list *head;
    struct flb_filter_plugin *plugin;
    struct flb_filter_instance *instance = NULL;

    if (!filter) {
        return NULL;
    }

    mk_list_foreach(head, &config->filter_plugins) {
        plugin = mk_list_entry(head, struct flb_filter_plugin, _head);
        if (strcmp(plugin->name, filter) != 0) {
            plugin = NULL;
            continue;
        }
    }

    if (!plugin) {
        return NULL;
    }

    instance = flb_malloc(sizeof(struct flb_filter_instance));
    if (!instance) {
        flb_errno();
        return NULL;
    }

    /* Get an ID */
    id =  instance_id(plugin, config);

    /* format name (with instance id) */
    snprintf(instance->name, sizeof(instance->name) - 1,
             "%s.%i", plugin->name, id);

    instance->id    = id;
    instance->p     = plugin;
    instance->data  = data;
    mk_list_init(&instance->properties);
    mk_list_add(&instance->_head, &config->filters);

    return instance;
}
