#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/socket.h>
#include <linux/netlink.h>

#define MY_SOCK NETLINK_USERSOCK
#define MY_GROUP 21

static int message_id;

int open_nl_sock(void)
{
	int sock, group;
	struct sockaddr_nl addr;
	
	sock = socket(AF_NETLINK, SOCK_RAW, MY_SOCK);
	if (sock < 0)
	{
		perror("socket: ");
		return -1;
	}
	
	memset((void*)&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		perror("bind: ");
		close(sock);
		return -1;
	}
	
	group = MY_GROUP;
	if (setsockopt(sock, 270, NETLINK_ADD_MEMBERSHIP, &group, sizeof(group)) < 0)
	{
		perror("setsockopt: ");
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
	
	char rx_buffer[256];
	int ret;
	const char* msg_payload = NULL;
	const char* p = NULL;
	
	iov.iov_base = (void *)rx_buffer;
	iov.iov_len = sizeof(rx_buffer);
	msg.msg_name = (void*)&addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	printf("receiving...\n");
	ret = recvmsg(sock, &msg, 0);
	if (ret < 0)
		perror("recvmsg: ");
	else
	{
		msg_payload = (const char*)NLMSG_DATA((struct nlmsghdr*)rx_buffer);
		printf("received messae, payload: '%s'\n", msg_payload);
		
		p = msg_payload;
		message_id = -1;
		
		while (*p)
		{
			if (isdigit(*p))
			{
				message_id = atoi(p);
				break;
			}
			p++;
		}
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
	
	strncpy(NLMSG_DATA(nlh), message, length);
	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	printf("sending message, payload '%s', length %lu\n", message, length);
	ret = sendmsg(sock, &msg, 0);
	if (ret < 0)
		perror("sendmsg: ");
	
	free(nlh);
}

int main(int argc, char** argv)
{
	int nls;
	char tx_buffer[64];
	size_t len;
	
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
		
		if (message_id != -1)
		{
			memset(tx_buffer, 0, sizeof(tx_buffer));
			sprintf(tx_buffer, "Received message, ID %02d", message_id);
			len = strlen(tx_buffer);
			send_message(nls, tx_buffer, len);
		}
	}
	
	close(nls);
	exit(EXIT_SUCCESS);
	
	return 0;
}
