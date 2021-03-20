obj-m += kern_agent.o 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -Wall user_agent.c -o user_agent
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f user_agent