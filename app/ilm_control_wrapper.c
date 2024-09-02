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
#include <unistd.h>

#include "ilm_control_wrapper.h"
#include <stdlib.h>

static int pipe_writefd = -1;

void wrap_ilm_init(int pipefd)
{
	pipe_writefd = pipefd;
	if (ilm_init() != ILM_SUCCESS) {
		fprintf(stderr, "%s(%d) ERROR: ilm_init failed.\n", __func__,
			__LINE__);
		exit(EXIT_FAILURE);
	}
}

static void wrap_ilm_exit(ilmErrorTypes ilm_status)
{
	fprintf(stderr, "%s(%d) ERROR: ilm returned: %s\n", __func__, __LINE__,
		ILM_ERROR_STRING(ilm_status));

	ilm_unregisterNotification();
	ilm_destroy();
	exit(EXIT_FAILURE);
}

static int wrap_ilm_module_exists(ilmObjectType type, int id)
{
	t_ilm_uint *IDs;
	t_ilm_int length;
	t_ilm_uint i;
	int exists = 0;

	switch (type) {
	case ILM_SURFACE:
		ilm_getSurfaceIDs(&length, &IDs);
		break;
	case ILM_LAYER:
		ilm_getLayerIDs(&length, &IDs);
		break;
	default:
		break;
	}

	for (i = 0; i < length; i++) {
		if (IDs[i] == id) {
			exists = 1;
			break;
		}
	}
	free(IDs);
	return exists;
}

int wrap_ilm_layer_exists(int id)
{
	return wrap_ilm_module_exists(ILM_LAYER, id);
}

int wrap_ilm_surface_exists(int id)
{
	return wrap_ilm_module_exists(ILM_SURFACE, id);
}

int wrap_ilm_screen_exists(int id)
{
	int exists = 0;

	ilmErrorTypes callResult;
	struct ilmScreenProperties screenProperties;

	callResult = ilm_getPropertiesOfScreen(id, &screenProperties);
	if (ILM_SUCCESS == callResult) {
		exists = 1;
	}

	return exists;
}

void wrap_ilm_get_screen_ids(t_ilm_uint *count, t_ilm_uint **screen_array_n)
{
	ilmErrorTypes callResult;

	t_ilm_uint cnt = 0;
	t_ilm_uint *screen_ary_n = NULL;

	callResult = ilm_getScreenIDs(&cnt, &screen_ary_n);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	*count = cnt;
	*screen_array_n = screen_ary_n;
}

static void wrap_ilm_create_layer(layer_properties_t *prop, int id)
{
	ilmErrorTypes callResult;

	callResult =
		ilm_layerCreateWithDimension(&id, prop->width, prop->height);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}
	ilm_commitChanges();
}

void wrap_ilm_set_layer(layer_properties_t *layer_prop, int id)
{
	if (wrap_ilm_layer_exists(id) == 0) {
		wrap_ilm_create_layer(layer_prop, id);
	}

	layout_properties_t prop = layer_prop->lp;

	ilmErrorTypes callResult;
	callResult = ilm_layerSetDestinationRectangle(
		id, prop.dst_x, prop.dst_y, prop.dst_w, prop.dst_h);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_layerSetSourceRectangle(id, prop.src_x, prop.src_y,
						 prop.src_w, prop.src_h);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_layerSetOpacity(id, prop.opacity);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_layerSetVisibility(id, prop.visibility);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_layerRemoveNotification(id);
	ilm_commitChanges();
}

void wrap_ilm_add_layer_to_screen(int id, t_ilm_layer *layer_array_n,
				  int layers)
{
	ilmErrorTypes callResult;

	callResult = ilm_displaySetRenderOrder(id, layer_array_n, layers);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}
	ilm_commitChanges();
}

void wrap_ilm_remove_layer(int layer_id)
{
	if (wrap_ilm_layer_exists(layer_id) == 0) {
		return;
	}

	ilmErrorTypes callResult;
	callResult = ilm_layerRemove(layer_id);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_layerRemoveNotification(layer_id);
	ilm_commitChanges();
}

void wrap_ilm_set_surface(surface_properties_t *surface_prop, int id)
{
	if (wrap_ilm_surface_exists(id) == 0) {
		return;
	}

	layout_properties_t prop = surface_prop->lp;

	ilmErrorTypes callResult;
	callResult = ilm_surfaceSetDestinationRectangle(
		id, prop.dst_x, prop.dst_y, prop.dst_w, prop.dst_h);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_surfaceSetSourceRectangle(id, prop.src_x, prop.src_y,
						   prop.src_w, prop.src_h);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}
	callResult = ilm_surfaceSetOpacity(id, prop.opacity);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}
	callResult = ilm_surfaceSetVisibility(id, prop.visibility);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}
	callResult = ilm_surfaceRemoveNotification(id);
	ilm_commitChanges();
}

void wrap_ilm_add_surface_to_layer(int id, t_ilm_surface *surface_array_n,
				   int surfaces)
{
	ilmErrorTypes callResult;

	callResult = ilm_layerSetRenderOrder(id, surface_array_n, surfaces);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}
	ilm_commitChanges();
}

void wrap_ilm_remove_surface(int layer_id, int surface_id)
{
	if (wrap_ilm_surface_exists(surface_id) == 0) {
		return;
	}

	ilmErrorTypes callResult;
	callResult = ilm_layerRemoveSurface(layer_id, surface_id);
	if (ILM_SUCCESS != callResult) {
		wrap_ilm_exit(callResult);
	}

	callResult = ilm_surfaceRemoveNotification(surface_id);
	ilm_commitChanges();
}

static void print_nofification_mask(t_ilm_notification_mask m)
{
	switch (m) {
	case ILM_NOTIFICATION_VISIBILITY:
		fprintf(stderr, "  ILM_NOTIFICATION_VISIBILITY        (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_OPACITY:
		fprintf(stderr, "  ILM_NOTIFICATION_OPACITY           (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_ORIENTATION:
		fprintf(stderr, "  ILM_NOTIFICATION_ORIENTATION       (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_SOURCE_RECT:
		fprintf(stderr, "  ILM_NOTIFICATION_SOURCE_RECT       (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_DEST_RECT:
		fprintf(stderr, "  ILM_NOTIFICATION_DEST_RECT         (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_CONTENT_AVAILABLE:
		fprintf(stderr, "  ILM_NOTIFICATION_CONTENT_AVAILABLE (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_CONTENT_REMOVED:
		fprintf(stderr, "  ILM_NOTIFICATION_CONTENT_REMOVED   (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_CONFIGURED:
		fprintf(stderr, "  ILM_NOTIFICATION_CONFIGURED        (%d)\n",
			m);
		break;
	case ILM_NOTIFICATION_ALL:
		fprintf(stderr, "  ILM_NOTIFICATION_ALL               (%d)\n",
			m);
		break;
	default:
		break;
	}
}

static void surface_notification_callback(t_ilm_uint surface_id,
					  struct ilmSurfaceProperties *sp,
					  t_ilm_notification_mask m)
{
	if ((unsigned)m & ILM_NOTIFICATION_CONFIGURED) {
		cbdata data;
		data.type = NTF_TYPE_SURFACE_PROP_CHANGE;
		data.id = surface_id;

		write(pipe_writefd, &data, sizeof(cbdata));
	}
}

void wrap_ilm_set_surfaceAddNotification(t_ilm_uint id)
{
	struct ilmSurfaceProperties sp;
	if (!parser_check_registered_surface_in_list_tree(id)) {
		return;
	}
	ilm_surfaceAddNotification(id, &surface_notification_callback);
	ilm_commitChanges();
	ilm_getPropertiesOfSurface(id, &sp);
}

static void notification_callback(ilmObjectType object, t_ilm_uint id,
				  t_ilm_bool created, void *user_data)
{
	(void)user_data;

	if (object == ILM_SURFACE) {
		if (created) {
			cbdata data;
			data.type = NTF_TYPE_CREATION_DELECTION;
			data.id = id;
			write(pipe_writefd, &data, sizeof(cbdata));
		}
	}
}

void wrap_ilm_set_notification_callback()
{
	ilm_registerNotification(notification_callback, NULL);
}
