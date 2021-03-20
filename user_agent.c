#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/socket.h>
#include <linux/netlink.h>

#define MY_SOCK NETLINK_USERSOCK
#define MY_GROUP 21

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
		printf("received payload: %s\n", (char*)NLMSG_DATA((struct nlmsghdr*)rx_buffer));
}

int main(int argc, char** argv)
{
	int nls;
	
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
	}
	
	close(nls);
	exit(EXIT_SUCCESS);
	
	return 0;
}
