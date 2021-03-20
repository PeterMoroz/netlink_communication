#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>

#include <net/netlink.h>
#include <net/net_namespace.h>

#include <linux/kthread.h>

#define MY_SOCK NETLINK_USERSOCK
#define MY_GROUP 21

struct sock *nl_sock = NULL;
struct task_struct* send_thread = NULL;

void send_message(const char* message, size_t length)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	int ret;
	
	skb = nlmsg_new(length, GFP_KERNEL);
	if (skb == NULL)
	{
		pr_err("could not allocate sk buffer\n");
		return;
	}
	
	pr_info("sending message '%s' (length = %lu)\n", message, length);
	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, length, 0);
	strncpy(nlmsg_data(nlh), message, length);
	
	ret = nlmsg_multicast(nl_sock, skb, 0, MY_GROUP, 0);
	if (ret < 0)
		pr_err("could not send multicast message, returned %d\n", ret);
}

static int sender(void* arg)
{
	char tx_buffer[64];
	int i = 0;
	size_t msg_length;
	
	while (!kthread_should_stop())
	{
		memset(tx_buffer, 0, sizeof(tx_buffer));
		sprintf(tx_buffer, "Message from kernel %02d", i++);		
		msg_length = strlen(tx_buffer);
		send_message(tx_buffer, msg_length);
		
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
	
	return 0;
}


static int __init kern_agent_init(void)
{
	pr_info("kern_agent_init\n");
	nl_sock = netlink_kernel_create(&init_net, MY_SOCK, NULL);
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
	int ret;
	ret = kthread_stop(send_thread);
	if (ret < 0)
		pr_err("error when sender thread stopped, returned %d\n", ret);
	
	netlink_kernel_release(nl_sock);
	pr_info("kern_agent_exit\n");
}

module_init(kern_agent_init);
module_exit(kern_agent_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Moroz");
