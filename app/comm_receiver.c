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
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <errno.h>

#define UHMI_IVI_WM_SOCK "/tmp/uhmi-ivi-wm_sock"
const char MAGIC_CODE[4] = { 0x55, 0x4C, 0x41, 0x30 };

int create_server_socket(void)
{
	struct sockaddr_un un;
	int sock_fd, ret;

	sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}

	unlink(UHMI_IVI_WM_SOCK);

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, UHMI_IVI_WM_SOCK);

	ret = bind(sock_fd, (struct sockaddr *)&un, sizeof(un));
	if (ret < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		close(sock_fd);
		return -1;
	}

	ret = listen(sock_fd, 3);
	if (ret < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		close(sock_fd);
		return -1;
	}

	return sock_fd;
}

int connect_to_client(int socket)
{
	struct sockaddr_un conn_addr = { 0 };
	socklen_t conn_addr_len = sizeof(conn_addr);
	int fd;

	fd = accept(socket, (struct sockaddr *)&conn_addr, &conn_addr_len);
	if (fd < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}

	return fd;
}

int acquire_body_size_from_client(int fd)
{
	char size_str[4];
	unsigned int size = 0;

	/* rcv header */
	if (recv(fd, size_str, sizeof(size_str), 0) < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}
	size = *(unsigned int *)&size_str;
	size = ntohl(size);

	return size;
}

int acquire_body_from_client(int fd, char **msg, unsigned int size)
{
	char *data = *msg;
	/* rcv body */
	unsigned int bytes_read = 0;
	unsigned int len = 0;
	while (bytes_read < size) {
		len = recv(fd, &data[bytes_read], size - bytes_read, 0);
		if (len < 0) {
			fprintf(stderr, "ERR: %s(%d) : %s\n", __FILE__,
				__LINE__, strerror(errno));
			return -1;
		}
		bytes_read += len;
	}

	return 0;
}

int send_str_response_to_client(int fd, char *res, int size)
{
	/* send response */
	if (send(fd, res, size, 0) < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int exchange_magiccode_with_client(int fd)
{
	char magic_str[4];

	/* rcv magiccode */
	if (recv(fd, magic_str, sizeof(magic_str), 0) < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}
	if (strncmp(magic_str, MAGIC_CODE, sizeof(magic_str)) != 0) {
		fprintf(stderr, "magic code error :%s \n", magic_str);
		return -1;
	}

	/* return magic code*/
	send_str_response_to_client(fd, magic_str, sizeof(magic_str));
	return 0;
}

int send_response_to_client(int fd, int res)
{
	/* send response */
	res = htonl(res);
	if (send(fd, &res, sizeof(res), 0) < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int connect_to_server(void)
{
	struct sockaddr_un un;
	int sock_fd, ret;

	sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}

	memset(&un, 0, sizeof(un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, UHMI_IVI_WM_SOCK);

	ret = connect(sock_fd, (struct sockaddr *)&un, sizeof(un));
	if (ret < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		close(sock_fd);
		return -1;
	}

	return sock_fd;
}

int send_body_size_to_server(int fd, unsigned int size)
{
	unsigned int size_n = htonl(size);

	if (send(fd, &size_n, sizeof(size_n), 0) == -1) {
		perror("send");
		return -1;
	}

	return 0;
}

int send_data_to_server(int fd, void *buf, int size)
{
	if (send(fd, buf, size, 0) <= 0) {
		fprintf(stderr, "ERR: %s(%d) : %s\n", __FILE__, __LINE__,
			strerror(errno));
		return -1;
	}

	return 0;
}

int recv_response_from_server(int fd)
{
	int res;

	/* rcv response */
	if (recv(fd, &res, sizeof(res), 0) < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
		return -1;
	}

	return res;
}

void recv_str_response_from_server(int fd, char *res)
{
	/* rcv response */
	if (recv(fd, res, 4, 0) < 0) {
		fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
	}
}
