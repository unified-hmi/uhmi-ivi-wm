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

#ifndef __COMM_PARSER_H__
#define __COMM_PARSER_H__

#include <sys/queue.h>

typedef struct _common_properties {
	t_ilm_uint src_x, src_y, src_w, src_h;
	t_ilm_uint dst_x, dst_y, dst_w, dst_h;
	t_ilm_float opacity;
	t_ilm_bool visibility;
} layout_properties_t;

typedef struct _layer_properties {
	t_ilm_uint width, height;
	layout_properties_t lp;
} layer_properties_t;

typedef struct _surface_properties {
	unsigned int referred_cnt;
	layout_properties_t lp;
} surface_properties_t;

TAILQ_HEAD(list_head, _list_element);
typedef struct _list_element {
	unsigned int id;

	/* Each properties */
	void *prop;

	/* Lower layer object list */
	struct list_head list_head;

	/* Linked list entry */
	TAILQ_ENTRY(_list_element) entry;
} list_element_t;

int parser_init(char *json_cfg_path);
int parser_parse_recv_command(char *msg);

int parser_add_ivi_surface_by_event_notification(t_ilm_uint surface_id);
int parser_check_registered_surface_in_list_tree(t_ilm_uint surface_id);

#endif //__COMM_PARSER_H__
