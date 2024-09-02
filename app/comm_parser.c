// SPDX-License-Identifier: Apache-2.0
/**                                                                                                                                                                                                                       
 * Copyright (c) 2024  Panasonic Automotive Systems, Co., Ltd.                                                                                                                                                            
 *                                                                                                                                                                                                                        
 * Licensed under the Apache License, Version 2.0 (the "License");                                                                                                                                                        
 * you may not use this file except in compliance with the License.                                                                                                                                                       
 * You may obtain a copy of the License at                                                                                                                                                                               
 *                                                                                                                                                                                                                        
 *     http://www.apache.org/licenses/LICENSE-2.0                                                                                                                                                                         
 *                                                                                                                                                                                                                        
 * Unless required by applicable law or agreed to in writing, software                                                                                                                                                    
 * distributed under the License is distributed on an "AS IS" BASIS,                                                                                                                                                      
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                                                                                                                                               
 * See the License for the specific language governing permissions and                                                                                                                                                    
 * limitations under the License.                                                                                                                                                                                         
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>

#include "ilm_control_wrapper.h"
#include "comm_parser.h"

#define UHMI_IVI_WM_VERSION "1.0.0"

#define JSON_KEY_VERSION "version"
#define JSON_KEY_COMMAND "command"
#define JSON_KEY_TARGET "target"
#define JSON_KEY_HOSTNAME "hostname"
#define JSON_KEY_SCREENS "screens"
#define JSON_KEY_INSERTODR "insert_order"
#define JSON_KEY_REFID "referenceID"
#define JSON_KEY_LAYERS "layers"
#define JSON_KEY_SURFACES "surfaces"
#define JSON_KEY_ID "id"
#define JSON_KEY_WIDTH "width"
#define JSON_KEY_HEIGHT "height"
#define JSON_KEY_SRCX "src_x"
#define JSON_KEY_SRCY "src_y"
#define JSON_KEY_SRCW "src_w"
#define JSON_KEY_SRCH "src_h"
#define JSON_KEY_DSTX "dst_x"
#define JSON_KEY_DSTY "dst_y"
#define JSON_KEY_DSTW "dst_w"
#define JSON_KEY_DSTH "dst_h"
#define JSON_KEY_OPACITY "opacity"
#define JSON_KEY_VISIBILITY "visibility"

typedef enum _cmd_type {
	CMD_TYPE_NONE = 0,
	CMD_TYPE_ADD,
	CMD_TYPE_MOD,
} CMD_TYPE;

typedef enum _insert_order {
	INSERT_ORDER_NONE = 0,
	INSERT_ORDER_PREPEND,
	INSERT_ORDER_APPEND,
	INSERT_ORDER_BEFORE,
	INSERT_ORDER_AFTER
} INSERT_ORDER;

typedef struct _insert_info {
	int order;
	int refid;
} insert_info_t;

static insert_info_t insert_info_default = { INSERT_ORDER_APPEND, 0 };

static struct list_head screen_head;
static struct list_head surface_properties_head;

static void init_list(struct list_head *list_head)
{
	TAILQ_INIT(list_head);
}

static int get_list_size(struct list_head *list_head)
{
	int size = 0;
	list_element_t *elm;
	TAILQ_FOREACH(elm, list_head, entry)
	{
		size++;
	}
	return size;
}

static list_element_t *pop_list_element(struct list_head *list_head, int id)
{
	list_element_t *elm;
	TAILQ_FOREACH(elm, list_head, entry)
	{
		if (elm->id == id) {
			TAILQ_REMOVE(list_head, elm, entry);
			return elm;
		}
	}
	return NULL;
}

static list_element_t *get_list_element(struct list_head *list_head, int id)
{
	list_element_t *elm;
	TAILQ_FOREACH(elm, list_head, entry)
	{
		if (elm->id == id) {
			return elm;
		}
	}
	return NULL;
}

static void insert_list_element(struct list_head *list_head,
				list_element_t *target_elm,
				insert_info_t insert_info)
{
	int insert_order = insert_info.order;
	int insert_refId = insert_info.refid;

	list_element_t *ref_elm;
	if ((insert_order == INSERT_ORDER_BEFORE) ||
	    (insert_order == INSERT_ORDER_AFTER)) {
		ref_elm = get_list_element(list_head, insert_refId);
		if (ref_elm == NULL) {
			insert_order = INSERT_ORDER_APPEND;
		}
	}

	switch (insert_order) {
	case INSERT_ORDER_PREPEND:
		TAILQ_INSERT_HEAD(list_head, target_elm, entry);
		break;
	case INSERT_ORDER_APPEND:
		TAILQ_INSERT_TAIL(list_head, target_elm, entry);
		break;
	case INSERT_ORDER_BEFORE:
		TAILQ_INSERT_BEFORE(ref_elm, target_elm, entry);
		break;
	case INSERT_ORDER_AFTER:
		TAILQ_INSERT_AFTER(list_head, ref_elm, target_elm, entry);
		break;
	default:
		break;
	}
}

static int add_surface_properties(surface_properties_t *prop, int surface_id)
{
	list_element_t *surface_properties_elm =
		get_list_element(&surface_properties_head, surface_id);

	if (surface_properties_elm == NULL) {
		surface_properties_elm =
			calloc(1, sizeof(*surface_properties_elm));
		surface_properties_elm->id = surface_id;
		surface_properties_elm->prop =
			calloc(1, sizeof(surface_properties_t));
		insert_list_element(&surface_properties_head,
				    surface_properties_elm,
				    insert_info_default);
	}

	prop->referred_cnt++;
	memcpy(surface_properties_elm->prop, prop,
	       sizeof(surface_properties_t));

	return 0;
}

static void remove_surface_properties(int surface_id)
{
	list_element_t *surface_properties_elm =
		get_list_element(&surface_properties_head, surface_id);
	if (surface_properties_elm) {
		surface_properties_t *prop =
			(surface_properties_t *)surface_properties_elm->prop;
		prop->referred_cnt--;
		if (prop->referred_cnt == 0) {
			pop_list_element(&surface_properties_head, surface_id);

			free(surface_properties_elm->prop);
			free(surface_properties_elm);
		}
	}
}

static list_element_t *add_surface(list_element_t *layer_elm,
				   surface_properties_t *prop, int surface_id,
				   insert_info_t insert_info)
{
	list_element_t *surface_elm = calloc(1, sizeof(*surface_elm));
	surface_elm->id = surface_id;

	add_surface_properties(prop, surface_id);
	insert_list_element(&layer_elm->list_head, surface_elm, insert_info);

	return surface_elm;
}

static int remove_surface(list_element_t *layer_elm, int surface_id)
{
	list_element_t *surface_elm =
		pop_list_element(&layer_elm->list_head, surface_id);
	if (surface_elm) {
		free(surface_elm);
		remove_surface_properties(surface_id);
		return 1;
	}
	return 0;
}

static void remove_all_surface(list_element_t *layer_elm)
{
	while (!TAILQ_EMPTY(&layer_elm->list_head)) {
		list_element_t *surface_elm =
			TAILQ_FIRST(&layer_elm->list_head);
		t_ilm_uint surface_id = surface_elm->id;

		TAILQ_REMOVE(&layer_elm->list_head, surface_elm, entry);
		free(surface_elm);
		remove_surface_properties(surface_id);
	}
}

static list_element_t *add_layer(list_element_t *screen_elm,
				 layer_properties_t *prop, int layer_id,
				 insert_info_t insert_info)
{
	list_element_t *layer_elm = calloc(1, sizeof(*layer_elm));
	layer_elm->id = layer_id;

	layer_elm->prop = calloc(1, sizeof(layer_properties_t));
	memcpy(layer_elm->prop, prop, sizeof(layer_properties_t));

	/* initialize surface list */
	init_list(&layer_elm->list_head);

	insert_list_element(&screen_elm->list_head, layer_elm, insert_info);
	return layer_elm;
}

static list_element_t *get_layer(int layer_id)
{
	list_element_t *screen_elm;
	TAILQ_FOREACH(screen_elm, &screen_head, entry)
	{
		list_element_t *layer_elm =
			get_list_element(&screen_elm->list_head, layer_id);
		if (layer_elm) {
			return layer_elm;
		}
	}
	return NULL;
}

static list_element_t *pop_layer(int layer_id)
{
	list_element_t *screen_elm;
	TAILQ_FOREACH(screen_elm, &screen_head, entry)
	{
		list_element_t *layer_elm =
			pop_list_element(&screen_elm->list_head, layer_id);
		if (layer_elm) {
			return layer_elm;
		}
	}
	return NULL;
}

static int remove_layer(int layer_id)
{
	list_element_t *layer_elm = pop_layer(layer_id);
	if (layer_elm) {
		remove_all_surface(layer_elm);
		free(layer_elm->prop);
		free(layer_elm);
		return 1;
	}
	return 0;
}

static list_element_t *add_screen(int screen_id)
{
	list_element_t *screen_elm = calloc(1, sizeof(*screen_elm));
	screen_elm->id = screen_id;

	/* initialize layer list */
	init_list(&screen_elm->list_head);

	insert_list_element(&screen_head, screen_elm, insert_info_default);

	return screen_elm;
}

static void add_exists_surfaces_to_layer(list_element_t *layer_elm)
{
	int surfaces = get_list_size(&layer_elm->list_head);
	t_ilm_surface *surface_array_n =
		malloc(surfaces * sizeof(t_ilm_surface));

	surfaces = 0;
	list_element_t *surface_elm;
	TAILQ_FOREACH(surface_elm, &layer_elm->list_head, entry)
	{
		if (wrap_ilm_surface_exists(surface_elm->id)) {
			surface_array_n[surfaces] = surface_elm->id;
			surfaces++;
		}
	}

	wrap_ilm_add_surface_to_layer(layer_elm->id, surface_array_n, surfaces);

	if (surface_array_n) {
		free(surface_array_n);
	}
}

static void add_layers_to_screen(list_element_t *screen_elm)
{
	int layers = get_list_size(&screen_elm->list_head);
	t_ilm_layer *layer_array_n = malloc(layers * sizeof(t_ilm_layer));

	layers = 0;
	list_element_t *layer_elm;
	TAILQ_FOREACH(layer_elm, &screen_elm->list_head, entry)
	{
		wrap_ilm_set_layer(layer_elm->prop, layer_elm->id);

		add_exists_surfaces_to_layer(layer_elm);

		layer_array_n[layers] = layer_elm->id;
		layers++;
	}

	wrap_ilm_add_layer_to_screen(screen_elm->id, layer_array_n, layers);

	if (layer_array_n) {
		free(layer_array_n);
	}
}

int parser_add_ivi_surface_by_event_notification(t_ilm_uint surface_id)
{
	list_element_t *screen_elm;
	TAILQ_FOREACH(screen_elm, &screen_head, entry)
	{
		list_element_t *layer_elm;
		TAILQ_FOREACH(layer_elm, &screen_elm->list_head, entry)
		{
			list_element_t *ref_surface_elm;
			TAILQ_FOREACH(ref_surface_elm, &layer_elm->list_head,
				      entry)
			{
				if (surface_id == ref_surface_elm->id) {
					list_element_t *surface_properties_elm;
					surface_properties_elm =
						get_list_element(
							&surface_properties_head,
							surface_id);

					wrap_ilm_set_surface(
						surface_properties_elm->prop,
						surface_id);

					add_exists_surfaces_to_layer(layer_elm);
				}
			}
		}
	}

	return 0;
}

int parser_check_registered_surface_in_list_tree(t_ilm_uint surface_id)
{
	if (get_list_element(&surface_properties_head, surface_id)) {
		return 1;
	}
	return 0;
}

static void debug_print_all_list(void)
{
	list_element_t *screen_elm;
	TAILQ_FOREACH(screen_elm, &screen_head, entry)
	{
		fprintf(stderr, "SCR: %d\n", screen_elm->id);
		list_element_t *layer_elm;
		TAILQ_FOREACH(layer_elm, &screen_elm->list_head, entry)
		{
			fprintf(stderr, "   |- LYR: %d\n", layer_elm->id);
			list_element_t *surface_elm;
			TAILQ_FOREACH(surface_elm, &layer_elm->list_head, entry)
			{
				fprintf(stderr, "         |- SFC: %d",
					surface_elm->id);
				list_element_t *surface_properties_elm = NULL;
				surface_properties_elm = get_list_element(
					&surface_properties_head,
					surface_elm->id);
				if (surface_properties_elm) {
					fprintf(stderr, " (SFC Prop Exist) \n");
				} else {
					fprintf(stderr,
						" (SFC Prop None!!!) \n");
				}
			}
		}
	}
}

static int get_json_string_value(json_t *jobject, char *key, char *value)
{
	if (json_is_string(json_object_get(jobject, key))) {
		sprintf(value, "%s",
			json_string_value(json_object_get(jobject, key)));
		return 0;
	}

	fprintf(stderr,
		"%s(%d) Error: json type other than string, or for NULL. [%s]\n",
		__func__, __LINE__, key);

	return -1;
}

static int get_json_integer_value(json_t *jobject, char *key, t_ilm_uint *value)
{
	if (json_is_integer(json_object_get(jobject, key))) {
		*value = json_integer_value(json_object_get(jobject, key));
		return 0;
	}

	fprintf(stderr,
		"%s(%d) Error: json type other than integer, or for NULL. [%s]\n",
		__func__, __LINE__, key);

	return -1;
}

static int get_json_real_value(json_t *jobject, char *key, t_ilm_float *value)
{
	if (json_is_integer(json_object_get(jobject, key))) {
		*value = (t_ilm_float)json_integer_value(
			json_object_get(jobject, key));
		return 0;
	}

	if (json_is_real(json_object_get(jobject, key))) {
		*value = json_real_value(json_object_get(jobject, key));
		return 0;
	}

	fprintf(stderr,
		"%s(%d) Error: json type other than real, or for NULL. [%s]\n",
		__func__, __LINE__, key);
	return -1;
}

static int get_json_array(json_t *jobject, char *key, json_t **array_jobj)
{
	if (json_is_array(json_object_get(jobject, key))) {
		*array_jobj = json_object_get(jobject, key);
		return 0;
	}

	fprintf(stderr,
		"%s(%d) Error: json type other than array, or for NULL. [%s]\n",
		__func__, __LINE__, key);
	return -1;
}

static int parse_target(json_t *jobject, json_t **array_jobj)
{
	if (get_json_array(jobject, JSON_KEY_TARGET, array_jobj) < 0) {
		return -1;
	}
	return 0;
}

static int parse_screens(json_t *jobject, json_t **array_jobj)
{
	if (get_json_array(jobject, JSON_KEY_SCREENS, array_jobj) < 0) {
		return -1;
	}
	return 0;
}

static int parse_layers(json_t *jobject, json_t **array_jobj)
{
	if (get_json_array(jobject, JSON_KEY_LAYERS, array_jobj) < 0) {
		return -1;
	}
	return 0;
}

static int parse_surfaces(json_t *jobject, json_t **array_jobj)
{
	if (get_json_array(jobject, JSON_KEY_SURFACES, array_jobj) < 0) {
		return -1;
	}
	return 0;
}

static int parse_id(json_t *jobject, int *id)
{
	if (get_json_integer_value(jobject, JSON_KEY_ID, id) < 0) {
		return -1;
	}
	return 0;
}

static int parse_hostname(json_t *jobject)
{
	char hostname[32] = { 0 };
	if (get_json_string_value(jobject, JSON_KEY_HOSTNAME, hostname) < 0) {
		fprintf(stderr, "%s(%d) Warning: Not find hostname property\n",
			__func__, __LINE__);
		return -1;
	}

	char local_hostname[32] = { 0 };
	gethostname(local_hostname, sizeof(local_hostname) - 1);

	if (strcmp(local_hostname, hostname)) {
		fprintf(stderr, "%s(%d) Status: %s is not covered.\n", __func__,
			__LINE__, hostname);
		return -1;
	}

	return 0;
}

static int parse_version(json_t *jobject)
{
	char version[32] = { 0 };
	if (get_json_string_value(jobject, JSON_KEY_VERSION, version) < 0) {
		fprintf(stderr, "%s(%d) Warning: Not find version property\n",
			__func__, __LINE__);
		return -1;
	}

	if (strcmp(UHMI_IVI_WM_VERSION, version)) {
		fprintf(stderr,
			"%s(%d) Warning:%s Json format version is illegal. \n",
			__func__, __LINE__, version);
		return -1;
	}

	return 0;
}

static int parse_command(json_t *jobject, char *cmd_name)
{
	if (get_json_string_value(jobject, JSON_KEY_COMMAND, cmd_name) < 0) {
		fprintf(stderr, "%s(%d) Warning: Not find command property\n",
			__func__, __LINE__);
		return -1;
	}

	return 0;
}

static int parse_insert_info(json_t *jobject, insert_info_t *insert_info)
{
	int ret = 0;

	char val[32] = { 0 };
	ret = get_json_string_value(jobject, JSON_KEY_INSERTODR, val);
	if (ret < 0) {
		fprintf(stderr,
			"%s(%d) Warning: Not find insert_order property. \n",
			__func__, __LINE__);
		insert_info->order = INSERT_ORDER_NONE;
		ret = -1;
	} else if (strcmp("append", val) == 0) {
		insert_info->order = INSERT_ORDER_APPEND;
	} else if (strcmp("prepend", val) == 0) {
		insert_info->order = INSERT_ORDER_PREPEND;
	} else if (strcmp("before", val) == 0) {
		insert_info->order = INSERT_ORDER_BEFORE;
	} else if (strcmp("after", val) == 0) {
		insert_info->order = INSERT_ORDER_AFTER;
	} else {
		fprintf(stderr,
			"%s(%d) Warning:%s Json format insert_order is illegal. \n",
			__func__, __LINE__, val);
		insert_info->order = INSERT_ORDER_NONE;
		ret = -1;
	}

	if (get_json_integer_value(jobject, JSON_KEY_REFID,
				   &insert_info->refid) < 0) {
		ret = -1;
	}

	return ret;
}

static int parse_mod_properties(layout_properties_t *dst_prop, json_t *jobject)
{
	layout_properties_t src_prop;
	memcpy(&src_prop, dst_prop, sizeof(src_prop));

	get_json_integer_value(jobject, JSON_KEY_SRCX, &src_prop.src_x);
	get_json_integer_value(jobject, JSON_KEY_SRCY, &src_prop.src_y);
	get_json_integer_value(jobject, JSON_KEY_SRCW, &src_prop.src_w);
	get_json_integer_value(jobject, JSON_KEY_SRCH, &src_prop.src_h);
	get_json_integer_value(jobject, JSON_KEY_DSTX, &src_prop.dst_x);
	get_json_integer_value(jobject, JSON_KEY_DSTY, &src_prop.dst_y);
	get_json_integer_value(jobject, JSON_KEY_DSTW, &src_prop.dst_w);
	get_json_integer_value(jobject, JSON_KEY_DSTH, &src_prop.dst_h);
	get_json_real_value(jobject, JSON_KEY_OPACITY, &src_prop.opacity);
	get_json_integer_value(jobject, JSON_KEY_VISIBILITY,
			       &src_prop.visibility);

	memcpy(dst_prop, &src_prop, sizeof(src_prop));
	return 0;
}

static int parse_add_properties(layout_properties_t *dst_prop, json_t *jobject)
{
	layout_properties_t src_prop;
	memcpy(&src_prop, dst_prop, sizeof(src_prop));

	if (get_json_integer_value(jobject, JSON_KEY_SRCX, &src_prop.src_x) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_SRCY, &src_prop.src_y) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_SRCW, &src_prop.src_w) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_SRCH, &src_prop.src_h) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_DSTX, &src_prop.dst_x) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_DSTY, &src_prop.dst_y) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_DSTW, &src_prop.dst_w) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_DSTH, &src_prop.dst_h) <
	    0) {
		return -1;
	}
	if (get_json_real_value(jobject, JSON_KEY_OPACITY, &src_prop.opacity) <
	    0) {
		return -1;
	}
	if (get_json_integer_value(jobject, JSON_KEY_VISIBILITY,
				   &src_prop.visibility) < 0) {
		return -1;
	}

	memcpy(dst_prop, &src_prop, sizeof(src_prop));
	return 0;
}

static int parse_layer_properties(layer_properties_t *dst_prop, json_t *jobject,
				  CMD_TYPE type)
{
	layer_properties_t src_prop;
	memcpy(&src_prop, dst_prop, sizeof(src_prop));

	if (type == CMD_TYPE_ADD) {
		if (get_json_integer_value(jobject, JSON_KEY_WIDTH,
					   &src_prop.width) < 0) {
			return -1;
		}
		if (get_json_integer_value(jobject, JSON_KEY_HEIGHT,
					   &src_prop.height) < 0) {
			return -1;
		}
		parse_add_properties(&src_prop.lp, jobject);
	} else {
		get_json_integer_value(jobject, JSON_KEY_WIDTH,
				       &src_prop.width);
		get_json_integer_value(jobject, JSON_KEY_HEIGHT,
				       &src_prop.height);
		parse_mod_properties(&src_prop.lp, jobject);
	}

	memcpy(dst_prop, &src_prop, sizeof(src_prop));
	return 0;
}

static int parse_surface_properties(surface_properties_t *dst_prop,
				    json_t *jobject, CMD_TYPE type)
{
	surface_properties_t src_prop;
	memcpy(&src_prop, dst_prop, sizeof(src_prop));

	if (type == CMD_TYPE_ADD) {
		parse_add_properties(&src_prop.lp, jobject);
	} else {
		parse_mod_properties(&src_prop.lp, jobject);
	}

	memcpy(dst_prop, &src_prop, sizeof(src_prop));
	return 0;
}

static list_element_t *decide_add_or_update_layer(list_element_t *screen_elm,
						  json_t *layer_jobj,
						  int layer_id, CMD_TYPE type,
						  insert_info_t insert_info)
{
	list_element_t *layer_elm = pop_layer(layer_id);
	if (layer_elm == NULL) {
		layer_properties_t layer_prop;
		parse_layer_properties(&layer_prop, layer_jobj, type);

		layer_elm = add_layer(screen_elm, &layer_prop, layer_id,
				      insert_info);

		wrap_ilm_set_layer(&layer_prop, layer_id);
	} else {
		parse_layer_properties(layer_elm->prop, layer_jobj, type);

		insert_list_element(&screen_elm->list_head, layer_elm,
				    insert_info);

		wrap_ilm_set_layer(layer_elm->prop, layer_id);
	}

	return layer_elm;
}

static list_element_t *decide_add_or_update_surface(list_element_t *layer_elm,
						    json_t *surface_jobj,
						    int surface_id,
						    CMD_TYPE type,
						    insert_info_t insert_info)
{
	list_element_t *surface_elm =
		pop_list_element(&layer_elm->list_head, surface_id);
	if (surface_elm == NULL) {
		surface_properties_t surface_prop;
		parse_surface_properties(&surface_prop, surface_jobj, type);

		surface_elm = add_surface(layer_elm, &surface_prop, surface_id,
					  insert_info);

		wrap_ilm_set_surface(&surface_prop, surface_id);
	} else {
		list_element_t *surface_properties_elm;
		surface_properties_elm =
			get_list_element(&surface_properties_head, surface_id);
		parse_surface_properties(surface_properties_elm->prop,
					 surface_jobj, type);

		insert_list_element(&layer_elm->list_head, surface_elm,
				    insert_info);

		wrap_ilm_set_surface(surface_properties_elm->prop, surface_id);
	}

	return surface_elm;
}

static int init_default_config(void)
{
	fprintf(stderr, "%s(%d) WARNING: Init default config\n", __func__,
		__LINE__);

	int screen_idx;

	t_ilm_uint count = 0;
	t_ilm_uint *screen_array_n = NULL;

	wrap_ilm_get_screen_ids(&count, &screen_array_n);

	for (screen_idx = 0; screen_idx < count; screen_idx++) {
		add_screen(screen_array_n[screen_idx]);
	}

	return 0;
}

static int parse_all_in_screen(json_t *jobject)
{
	int screen_idx, layer_idx, surface_idx;

	//1. screens(list)
	json_t *screen_ary_jobj = NULL;
	parse_screens(jobject, &screen_ary_jobj);

	json_t *screen_jobj;
	json_array_foreach(screen_ary_jobj, screen_idx, screen_jobj)
	{
		int screen_id = 0;
		parse_id(screen_jobj, &screen_id);
		if (wrap_ilm_screen_exists(screen_id) == 0) {
			fprintf(stderr, "%s(%d) ERROR: Screen %d not found\n",
				__func__, __LINE__, screen_id);
			return -1;
		}

		list_element_t *screen_elm;
		screen_elm = get_list_element(&screen_head, screen_id);
		if (screen_elm == NULL) {
			screen_elm = add_screen(screen_id);
		}

		//2. layers(list)
		json_t *layer_ary_jobj = NULL;
		parse_layers(screen_jobj, &layer_ary_jobj);

		json_t *layer_jobj;
		json_array_foreach(layer_ary_jobj, layer_idx, layer_jobj)
		{
			int layer_id = 0;
			parse_id(layer_jobj, &layer_id);

			list_element_t *layer_elm;
			layer_elm = decide_add_or_update_layer(
				screen_elm, layer_jobj, layer_id, CMD_TYPE_ADD,
				insert_info_default);

			//3. surfaces(list)
			json_t *surface_ary_jobj = NULL;
			parse_surfaces(layer_jobj, &surface_ary_jobj);

			json_t *surface_jobj;
			json_array_foreach(surface_ary_jobj, surface_idx,
					   surface_jobj)
			{
				int surface_id = 0;
				parse_id(surface_jobj, &surface_id);

				decide_add_or_update_surface(
					layer_elm, surface_jobj, surface_id,
					CMD_TYPE_ADD, insert_info_default);
			}
		}
		add_layers_to_screen(screen_elm);
	}
	return 0;
}

static int parse_init_json_config(char *json_cfg_path)
{
	int target_idx;

	json_error_t jerror;
	json_t *root_jobj;

	root_jobj = json_load_file(json_cfg_path, 0, &jerror);
	if (!root_jobj) {
		fprintf(stderr,
			"%s(%d) WARNING: %s file. Invalid line %d: %s\n",
			__func__, __LINE__, json_cfg_path, jerror.line,
			jerror.text);
		return -1;
	}

	if (parse_version(root_jobj) < 0) {
		/*return -1;*/
	}

	json_t *target_ary_jobj = NULL;
	parse_target(root_jobj, &target_ary_jobj);

	json_t *target_jobj;
	json_array_foreach(target_ary_jobj, target_idx, target_jobj)
	{
		if (parse_hostname(target_jobj) < 0) {
			continue;
		}

		if (parse_all_in_screen(target_jobj) < 0) {
			return -1;
		}
	}

	json_decref(root_jobj);

	return 0;
}

static void remove_all(void)
{
	list_element_t *screen_elm;
	TAILQ_FOREACH(screen_elm, &screen_head, entry)
	{
		while (!TAILQ_EMPTY(&screen_elm->list_head)) {
			list_element_t *layer_elm =
				TAILQ_FIRST(&screen_elm->list_head);

			TAILQ_REMOVE(&screen_elm->list_head, layer_elm, entry);

			remove_all_surface(layer_elm);

			free(layer_elm->prop);
			free(layer_elm);
		}
	}

	while (!TAILQ_EMPTY(&screen_head)) {
		list_element_t *screen_elm = TAILQ_FIRST(&screen_head);

		TAILQ_REMOVE(&screen_head, screen_elm, entry);
		free(screen_elm);
	}

	debug_print_all_list();
}

int parser_init(char *json_cfg_path)
{
	init_list(&screen_head);
	init_list(&surface_properties_head);

	if (json_cfg_path) {
		parse_init_json_config(json_cfg_path);
	}

	if (TAILQ_EMPTY(&screen_head)) {
		init_default_config();
	}

	wrap_ilm_set_notification_callback();

	debug_print_all_list();
}

static int parse_add_layer_command(json_t *jobject)
{
	int screen_idx, lyr_idx;

	//only when matches screens id
	json_t *screen_ary_jobj = NULL;
	if (parse_screens(jobject, &screen_ary_jobj) < 0) {
		return -1;
	}

	json_t *screen_jobj;
	json_array_foreach(screen_ary_jobj, screen_idx, screen_jobj)
	{
		int screen_id = 0;
		parse_id(screen_jobj, &screen_id);

		insert_info_t insert_info = insert_info_default;
		parse_insert_info(screen_jobj, &insert_info);

		list_element_t *screen_elm =
			get_list_element(&screen_head, screen_id);
		if (screen_elm != NULL) {
			json_t *layer_ary_jobj = NULL;
			if (parse_layers(screen_jobj, &layer_ary_jobj) < 0) {
				return -1;
			}

			json_t *layer_jobj;
			json_array_foreach(layer_ary_jobj, lyr_idx, layer_jobj)
			{
				int layer_id = 0;
				if (parse_id(layer_jobj, &layer_id) < 0) {
					return -1;
				}

				decide_add_or_update_layer(screen_elm,
							   layer_jobj, layer_id,
							   CMD_TYPE_ADD,
							   insert_info);
			}

			add_layers_to_screen(screen_elm);
		}
	}

	return 0;
}

static int parse_remove_layer_command(json_t *jobject)
{
	int layer_idx;

	json_t *layer_ary_jobj = NULL;
	if (parse_layers(jobject, &layer_ary_jobj) < 0) {
		return -1;
	}

	json_t *layer_jobj;
	json_array_foreach(layer_ary_jobj, layer_idx, layer_jobj)
	{
		int layer_id = 0;
		if (parse_id(layer_jobj, &layer_id) < 0) {
			return -1;
		}

		remove_layer(layer_id);

		wrap_ilm_remove_layer(layer_id);
	}

	return 0;
}

static int parse_modify_layer_command(json_t *jobject)
{
	int layer_idx;

	json_t *layer_ary_jobj = NULL;
	if (parse_layers(jobject, &layer_ary_jobj) < 0) {
		return -1;
	}

	json_t *layer_jobj;
	json_array_foreach(layer_ary_jobj, layer_idx, layer_jobj)
	{
		int layer_id = 0;
		if (parse_id(layer_jobj, &layer_id) < 0) {
			return -1;
		}

		list_element_t *layer_elm = get_layer(layer_id);
		if (layer_elm != NULL) {
			parse_layer_properties(layer_elm->prop, layer_jobj,
					       CMD_TYPE_MOD);
			wrap_ilm_set_layer(layer_elm->prop, layer_id);
		}
	}

	return 0;
}

static int parse_add_surface_command(json_t *jobject)
{
	int screen_idx, lyr_idx, surface_idx;
	json_t *screen_ary_jobj = NULL;
	if (parse_screens(jobject, &screen_ary_jobj) < 0) {
		return -1;
	}

	json_t *screen_jobj;
	json_array_foreach(screen_ary_jobj, screen_idx, screen_jobj)
	{
		insert_info_t insert_info = insert_info_default;
		parse_insert_info(screen_jobj, &insert_info);

		json_t *layer_ary_jobj = NULL;
		if (parse_layers(screen_jobj, &layer_ary_jobj) < 0) {
			return -1;
		}

		json_t *layer_jobj;
		json_array_foreach(layer_ary_jobj, lyr_idx, layer_jobj)
		{
			int layer_id = 0;
			parse_id(layer_jobj, &layer_id);

			list_element_t *layer_elm = get_layer(layer_id);
			if (layer_elm != NULL) {
				json_t *surface_ary_jobj = NULL;
				if (parse_surfaces(layer_jobj,
						   &surface_ary_jobj) < 0) {
					return -1;
				}

				json_t *surface_jobj;
				json_array_foreach(surface_ary_jobj,
						   surface_idx, surface_jobj)
				{
					int surface_id = 0;
					if (parse_id(surface_jobj,
						     &surface_id) < 0) {
						return -1;
					}

					decide_add_or_update_surface(
						layer_elm, surface_jobj,
						surface_id, CMD_TYPE_ADD,
						insert_info);
				}
				add_exists_surfaces_to_layer(layer_elm);
			}
		}
	}

	return 0;
}

static int parse_remove_surface_command(json_t *jobject)
{
	int lyr_idx, surface_idx;

	json_t *layer_ary_jobj = NULL;
	if (parse_layers(jobject, &layer_ary_jobj) < 0) {
		return -1;
	}

	json_t *layer_jobj;
	json_array_foreach(layer_ary_jobj, lyr_idx, layer_jobj)
	{
		int layer_id = 0;
		if (parse_id(layer_jobj, &layer_id) < 0) {
			return -1;
		}

		list_element_t *layer_elm = get_layer(layer_id);
		if (layer_elm != NULL) {
			json_t *surface_ary_jobj = NULL;
			if (parse_surfaces(layer_jobj, &surface_ary_jobj) < 0) {
				return -1;
			}

			json_t *surface_jobj;
			json_array_foreach(surface_ary_jobj, surface_idx,
					   surface_jobj)
			{
				int surface_id = 0;
				if (parse_id(surface_jobj, &surface_id) < 0) {
					return -1;
				}

				remove_surface(layer_elm, surface_id);

				/* set ivi config */
				wrap_ilm_remove_surface(layer_elm->id,
							surface_id);
			}
		}
	}

	return 0;
}

static int parse_modify_surface_command(json_t *jobject)
{
	json_t *surface_ary_jobj = NULL;
	if (parse_surfaces(jobject, &surface_ary_jobj) < 0) {
		return -1;
	}

	int surface_idx;
	json_t *surface_jobj;
	json_array_foreach(surface_ary_jobj, surface_idx, surface_jobj)
	{
		int surface_id = 0;
		if (parse_id(surface_jobj, &surface_id) < 0) {
			return -1;
		}

		list_element_t *surface_properties_elm;
		surface_properties_elm =
			get_list_element(&surface_properties_head, surface_id);
		if (surface_properties_elm == NULL) {
			return -1;
		}

		parse_surface_properties(surface_properties_elm->prop,
					 surface_jobj, CMD_TYPE_MOD);

		/*set ivi config */
		wrap_ilm_set_surface(surface_properties_elm->prop, surface_id);
	}

	return 0;
}

static int parse_init_screen_command(json_t *jobject)
{
	json_error_t jerror;
	json_t *root_jobj;

	//0. version
	if (parse_version(jobject) < 0) {
		/*return -1;*/
	}

	remove_all();

	if (parse_all_in_screen(jobject) < 0) {
		return -1;
	}

	wrap_ilm_set_notification_callback();

	return 0;
}

int parser_parse_recv_command(char *msg)
{
	/* str to json */
	json_t *jobject;
	json_error_t jerror;
	jobject = json_loads(msg, 0, &jerror);

	if (parse_version(jobject) < 0) {
		/*return -1;*/
	}

	char cmd_name[16] = { 0 };
	if (parse_command(jobject, cmd_name) < 0) {
		fprintf(stderr, "%s(%d) ERROR: Not find command property\n",
			__func__, __LINE__);
	} else {
		if (strcmp("add_surface", cmd_name) == 0) {
			parse_add_surface_command(jobject);
		} else if (strcmp("remove_surface", cmd_name) == 0) {
			parse_remove_surface_command(jobject);
		} else if (strcmp("modify_surface", cmd_name) == 0) {
			parse_modify_surface_command(jobject);
		} else if (strcmp("add_layer", cmd_name) == 0) {
			parse_add_layer_command(jobject);
		} else if (strcmp("remove_layer", cmd_name) == 0) {
			parse_remove_layer_command(jobject);
		} else if (strcmp("modify_layer", cmd_name) == 0) {
			parse_modify_layer_command(jobject);
		} else if (strcmp("initial_screen", cmd_name) == 0) {
			parse_init_screen_command(jobject);
		} else {
			fprintf(stderr,
				"%s(%d) ERROR: Illegal command name %s\n",
				__func__, __LINE__, cmd_name);
		}
	}

	json_decref(jobject);
	debug_print_all_list();

	return 0;
}
