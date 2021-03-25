#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/socket.h>
#include <linux/netlink.h>

#include "messaging.h"

static unsigned int packet_id = 0xFFFFFFFF;

int open_nl_sock(void)
{
	int sock;
	struct sockaddr_nl addr;
	
	sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
	if (sock < 0)
	{
		perror("socket: ");
		return -1;
	}
	
	memset((void*)&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = USER_AGENT_ID;
	
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("bind: ");
		close(sock);
		return -1;
	}
		
	return sock;
}

void recv_message(int sock)
{
	struct sockaddr_nl addr;
	struct msghdr msg;
	struct iovec iov;
	
	char rx_buffer[NL_PACKET_SIZE];
	int ret;
	const struct nl_packet* packet = NULL;
	
	iov.iov_base = (void *)rx_buffer;
	iov.iov_len = sizeof(rx_buffer);
	msg.msg_name = (void*)&addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	memset(rx_buffer, 0, sizeof(rx_buffer));
	
	printf("receiving...\n");
	ret = recvmsg(sock, &msg, 0);
	if (ret < 0)
		perror("recvmsg: ");
	else
	{	
		packet = (const struct nl_packet*)NLMSG_DATA((struct nlmsghdr*)rx_buffer);
		printf("received packet: ID = %u, data length = %u, data: '%s'\n", 
			packet->id, packet->data_length, packet->data);
		packet_id = packet->id;
	}
}

void send_message(int sock, const char* message, size_t length)
{
	struct sockaddr_nl addr;
	struct nlmsghdr* nlh = NULL;
	struct iovec iov;
	struct msghdr msg;
	int ret;
	
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = 0;
	addr.nl_groups = 0;
	
	nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(length));
	memset(nlh, 0, NLMSG_SPACE(length));
	nlh->nlmsg_len = NLMSG_SPACE(length);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	
	memcpy(NLMSG_DATA(nlh), message, length);
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	ret = sendmsg(sock, &msg, 0);
	if (ret < 0)
		perror("sendmsg: ");
	
	free(nlh);
}

int main(int argc, char** argv)
{
	int nls;
	struct nl_packet packet;
	
	nls = open_nl_sock();
	if (nls < 0)
	{
		printf("could not open socket\n");
		exit(EXIT_FAILURE);
	}
	
	while (1)
	{
		recv_message(nls);
		usleep(10);
				
		memset(&packet, 0, sizeof(packet));
		packet.id = packet_id;	/* just confirm receiving, no payload */
		send_message(nls, (const char*)&packet, sizeof(packet));
	}
	
	close(nls);
	exit(EXIT_SUCCESS);
	
	return 0;
}
