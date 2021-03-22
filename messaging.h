#ifndef _MESSAGING_H_
#define _MESSAGING_H_


#define NL_PACKET_SIZE 1024
#define NL_DATA_SIZE ((NL_PACKET_SIZE) - (sizeof(unsigned int) + sizeof(unsigned short)))

struct nl_packet
{
	unsigned int id;
	unsigned short data_length;
	unsigned char data[NL_DATA_SIZE];
};

#endif /* _MESSAGING_H_ */