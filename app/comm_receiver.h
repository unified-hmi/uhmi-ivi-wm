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

#ifndef __COMM_RECEIVER_H__
#define __COMM_RECEIVER_H__

int create_server_socket();
int connect_to_client(int socket);

int exchange_magiccode_with_client(int fd);
int acquire_body_size_from_client(int fd);
int acquire_body_from_client(int fd, char **msg, unsigned int size);

int send_response_to_client(int fd, int res);

int connect_to_server(void);
int send_data_to_server(int fd, void *buf, int size);
int send_body_size_to_server(int fd, unsigned int size);

int recv_response_from_server(int fd);
void recv_str_response_from_server(int fd, char *res);

#endif //__COMM_RECEIVER_H__
