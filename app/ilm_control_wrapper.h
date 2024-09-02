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

#ifndef __ILM_CONFROL_WRAPPER_H__
#define __ILM_CONFROL_WRAPPER_H__

#include <ilm/ilm_control.h>
#include "comm_parser.h"

typedef enum _ntf_type {
	NTF_TYPE_CREATION_DELECTION = 1,
	NTF_TYPE_SURFACE_PROP_CHANGE,
} ntf_type;

typedef struct _cbdata {
	ntf_type type;
	t_ilm_uint id;
} cbdata;

void wrap_ilm_init(int pipefd);

int wrap_ilm_layer_exists(int id);
int wrap_ilm_surface_exists(int id);
int wrap_ilm_screen_exists(int id);

/* screen control */
void wrap_ilm_get_screen_ids(t_ilm_uint *count, t_ilm_uint **screen_array_n);

/* layer control*/
void wrap_ilm_set_layer(layer_properties_t *prop, int id);
void wrap_ilm_add_layer_to_screen(int id, t_ilm_layer *layer_array_n,
				  int layers);
void wrap_ilm_remove_layer(int layer_id);

/* surface control*/
void wrap_ilm_set_surface(surface_properties_t *prop, int id);
void wrap_ilm_add_surface_to_layer(int id, t_ilm_surface *surface_array_n,
				   int surfaces);
void wrap_ilm_remove_surface(int layer_id, int surface_id);

/* callback event */
void wrap_ilm_set_surfaceAddNotification(t_ilm_uint id);
void wrap_ilm_set_notification_callback(void);

#endif //__ILM_CONFROL_WRAPPER_H__
