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
#include <jansson.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <arpa/inet.h>
#include "../app/comm_receiver.h"

json_t *jobject;
char *json_conf_path = NULL;

int usage(int ret)
{
	fprintf(stderr,
		"    -h,  --help                  display this help and exit.\n"
		"    -c,  --path                  json config file path\n");
	exit(ret);
}

static void parse_option(int argc, char *argv[])
{
	int opt, ret;
	static const struct option options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "path", optional_argument, NULL, 'c' },
		{ 0, 0, NULL, 0 }
	};

	while (1) {
		opt = getopt_long(argc, argv, "hc:", options, NULL);

		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			usage(0);
			break;
		case 'c':
			json_conf_path = optarg;
			break;
		default:
			usage(EXIT_FAILURE);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	parse_option(argc, argv);
	if (json_conf_path == NULL) {
		fprintf(stderr,
			"error: json command file path is not specified \n");
		usage(0);
	}

	json_error_t jerror;
	jobject = json_load_file(json_conf_path, 0, &jerror);
	if (!jobject) {
		fprintf(stderr,
			"%s(%d) WARNING: %s file. Invalid line %d: %s\n",
			__func__, __LINE__, json_conf_path, jerror.line,
			jerror.text);
		usage(EXIT_FAILURE);
	}

	//parse json cmd
	char *cmd =
		(char *)malloc(strlen(json_dumps(jobject, JSON_COMPACT)) + 1);
	sprintf(cmd, "%s", json_dumps(jobject, JSON_COMPACT));
	unsigned int cmdsize = strlen(cmd) + 1;

	//send data
	int fd = connect_to_server();
	char magic[4] = { 0x55, 0x4C, 0x41, 0x30 };
	send_data_to_server(fd, magic, 4);
	send_body_size_to_server(fd, cmdsize);
	send_data_to_server(fd, cmd, cmdsize);

	//receive resp
	char resp[4] = { 0 };
	recv_str_response_from_server(fd, resp);
	if (strncmp(resp, magic, 4) != 0) {
		fprintf(stderr, "recv magic code NG  %s\n", resp);
	}

	int res;
	res = recv_response_from_server(fd);
	fprintf(stderr, "resp: %d \n", res);

	free(cmd);

	return 0;
}
