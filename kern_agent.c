#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>

#include <net/netlink.h>
#include <net/net_namespace.h>

#include <linux/kthread.h>
#include <linux/list.h>

#include "messaging.h"

#define MY_SOCK NETLINK_USERSOCK
#define MY_GROUP 21



struct sock *nl_sock = NULL;
struct task_struct* send_thread = NULL;


struct nl_packet_list_item
{
	struct nl_packet packet;
	struct list_head link;
};


static unsigned nl_packet_seq;

LIST_HEAD(nl_packet_list);
DEFINE_MUTEX(packet_list_mutex);
static unsigned list_length;	/* just for statistics */


static void on_recv_message(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	struct nl_packet* packet = NULL;
	struct list_head* lst_iter = NULL;
	struct nl_packet_list_item* lst_item = NULL;

	nlh = (struct nlmsghdr *)skb->data;
	packet = (struct nl_packet*)nlmsg_data(nlh);
	pr_info("recv: sender PID %d, packet ID %u\n", nlh->nlmsg_pid, packet->id);
		
	mutex_lock(&packet_list_mutex);
	list_for_each(lst_iter, &nl_packet_list)
	{
		lst_item = list_entry(lst_iter, struct nl_packet_list_item, link);
		if (lst_item->packet.id == packet->id)
		{
			list_del(&lst_item->link);
			kfree(lst_item);
			list_length--;
			break;
		}
	}
	mutex_unlock(&packet_list_mutex);
	
	pr_info("recv: length of packet's list %u\n", list_length);
}

static struct netlink_kernel_cfg nl_cfg = 
{
	.input = on_recv_message,
};

void send_message(const char* message, size_t length)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	int ret;
	
	struct nl_packet_list_item* item = kmalloc(sizeof(struct nl_packet_list_item), GFP_KERNEL);
	item->packet.id = nl_packet_seq++;
	memcpy(item->packet.data, message, length);
	item->packet.data_length = length;
	INIT_LIST_HEAD(&item->link);
	
	mutex_lock(&packet_list_mutex);
	list_add(&item->link, &nl_packet_list);
	list_length++;
	mutex_unlock(&packet_list_mutex);
	
	pr_info("send: length of packet's list %u\n", list_length);
	
	skb = nlmsg_new(sizeof(struct nl_packet), GFP_KERNEL);
	if (skb == NULL)
	{
		pr_err("could not allocate sk buffer\n");
		return;
	}
	
	NETLINK_CB(skb).dst_group = 0;
	
	pr_info("send: packet id = %u, total packet length = %lu\n", 
			item->packet.id, sizeof(struct nl_packet));
	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, sizeof(struct nl_packet), 0);
	memcpy(nlmsg_data(nlh), &item->packet, sizeof(struct nl_packet));
	
	/*
	ret = nlmsg_multicast(nl_sock, skb, 0, MY_GROUP, 0);
	if (ret < 0)
		pr_err("could not send multicast message, returned %d\n", ret);
		*/
	ret = nlmsg_unicast(nl_sock, skb, 555);
	if (ret < 0)
		pr_err("could not send unnicast message, returned %d\n", ret);
}

static int sender(void* arg)
{
	int i = 0;
	
	char msg_buffer[256];
	size_t msg_length;
	
	while (!kthread_should_stop())
	{	
		sprintf(msg_buffer, "Message from kernel %02d", i++);
		msg_length = strlen(msg_buffer);
		
		send_message(msg_buffer, msg_length);
				
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
	
	return 0;
}



static int __init kern_agent_init(void)
{
	pr_info("kern_agent_init\n");
	nl_sock = netlink_kernel_create(&init_net, MY_SOCK, &nl_cfg);
	if (nl_sock == NULL)
	{
		pr_err("could not create netlink socket\n");
		return -ENOMEM;
	}
	
	send_thread = kthread_run((void *)sender, NULL, "Netlink sender");
	if (send_thread == NULL)
	{
		pr_err("could not create thread\n");
		netlink_kernel_release(nl_sock);
		return -ENOMEM;
	}
	
	return 0;
}

static void __exit kern_agent_exit(void)
{
	struct list_head* lst_iter = NULL;
	struct nl_packet_list_item* lst_item = NULL;	
	
	int ret;
	ret = kthread_stop(send_thread);
	if (ret < 0)
		pr_err("error when sender thread stopped, returned %d\n", ret);
	
	netlink_kernel_release(nl_sock);
	
	/* TO DO: fix wrong list cleaning which leads to crash when module is unloaded.
	list_for_each(lst_iter, &nl_packet_list)
	{
		lst_item = list_entry(lst_iter, struct nl_packet_list_item, link);
		list_del(&lst_item->link);
		kfree(lst_item);
	}
*/	
	
	pr_info("kern_agent_exit\n");
}

module_init(kern_agent_init);
module_exit(kern_agent_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Moroz");
