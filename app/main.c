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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdint.h>
#include <errno.h>
#include <err.h>

#define DEBUG 0
#if DEBUG
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

pid_t gettid(void)
{
	return syscall(SYS_gettid);
}
#endif

#include "ilm_control_wrapper.h"
#include "comm_parser.h"
static char *json_cfg_path = NULL;

#include <poll.h>
#include "comm_receiver.h"
static int socket_fd = -1;
static int accept_fd = -1;
static int pipe_readfd = -1;

void wait_event_loop(void)
{
	struct pollfd fds[3];

	memset(&fds, 0, sizeof(fds));

	fds[0].fd = pipe_readfd;
	fds[0].events = POLLIN;

	socket_fd = create_server_socket();
	fds[1].fd = socket_fd;
	fds[1].events = POLLIN;

	fds[2].fd = accept_fd;
	fds[2].events = POLLIN;

	while (1) {
		poll(fds, 3, -1);

		/* callback pipe */
		if (fds[0].revents & POLLIN) {
			cbdata data;
			int size = read(pipe_readfd, &data, sizeof(data));
			if (size == -1) {
				fprintf(stderr, "%s(%d) ERROR: pipe read\n",
					__func__, __LINE__);
			}
			switch (data.type) {
			case NTF_TYPE_CREATION_DELECTION:
				wrap_ilm_set_surfaceAddNotification(data.id);
				break;
			case NTF_TYPE_SURFACE_PROP_CHANGE:
				parser_add_ivi_surface_by_event_notification(
					data.id);
				break;
			default:
				break;
			}
		}

		/* socket */
		if (fds[1].revents & POLLIN) {
			if (accept_fd < 0) {
				accept_fd = connect_to_client(socket_fd);
				fds[2].fd = accept_fd;
				fds[2].events = POLLIN;
			}
		}

		/* accept */
		if (fds[2].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			close(accept_fd);
			accept_fd = -1;
			fds[2].fd = accept_fd;
			fds[2].revents &= ~POLLIN;
			continue;
		}

		/* receive command */
		if (fds[2].revents & POLLIN) {
			int resp = -1;
			if (exchange_magiccode_with_client(accept_fd) == 0) {
				int size = acquire_body_size_from_client(
					accept_fd);
				if (size > 0) {
					char *msg = (char *)calloc(size + 1, 1);
					acquire_body_from_client(accept_fd,
								 &msg, size);
					//fprintf (stderr, "%s\n", json_dumps (jobj, sizeof (jobj)));
					resp = parser_parse_recv_command(msg);
					if (msg) {
						free(msg);
					}
				}
			}
			send_response_to_client(accept_fd, resp);
		}
	}
}

static int usage(int ret)
{
	fprintf(stderr,
		" usage \n"
		"    -h,  --help                  display this help and exit \n"
		"    -c,  --path                  Init config file path \n");
	exit(ret);
}

static void parse_option(int argc, char *argv[])
{
	int opt;
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
			json_cfg_path = optarg;
			break;
		default:
			usage(EXIT_FAILURE);
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	if ((argc > 1) && (!strncmp(argv[1], "-", 1))) {
		parse_option(argc, argv);
	}

	int pipefd[2];
	if (pipe(pipefd) < 0) {
		fprintf(stderr, "%s(%d) ERROR: pipe open\n", __func__,
			__LINE__);
		return EXIT_FAILURE;
	}
	pipe_readfd = pipefd[0];
	wrap_ilm_init(pipefd[1]);
	parser_init(json_cfg_path);

	wait_event_loop();

	close(pipefd[0]);
	close(pipefd[1]);

	return EXIT_SUCCESS;
}
