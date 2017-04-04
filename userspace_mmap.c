#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<stdint.h>
#include<stdlib.h>

#define PAGE_SIZE 4096

#define NUM_BUFFERS 64
#define LOGGING 1

struct mydata
{
        //struct device dev;
        void *tx_queue;
        uint64_t tx_queue_dma_addr;
        void *rx_queue;
        uint64_t rx_queue_dma_addr;
        void *rx_result;
        uint64_t rx_result_dma_addr;
        void *coherent_mem_tx[NUM_BUFFERS];
        uint64_t coherent_mem_tx_dma_addr[NUM_BUFFERS];
        void *coherent_mem_rx[NUM_BUFFERS];
        uint64_t coherent_mem_rx_dma_addr[NUM_BUFFERS];

} __attribute__((packed)) ;



main()
{
	int configfd;
	int waitint;
	int i;
	char *address = NULL;
	struct mydata *data;
	void *tx_queue;

	configfd = open("/sys/kernel/debug/coherent_buffers",O_RDWR);

	if(configfd < 0) {
		perror("Open call failed\n");
		return -1;
	}

	data = malloc(sizeof(struct mydata));

	read(configfd,data,sizeof(struct mydata));

	for(i=0;i<NUM_BUFFERS;i++) {
		printf("i : %d tx phys : %p rx phys : %p\n",i,
		(void *)data->coherent_mem_tx_dma_addr[i],(void *)data->coherent_mem_rx_dma_addr[i]);
	}

	#if 1

	tx_queue =  mmap(NULL,PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, configfd, 0*PAGE_SIZE);

	if(address == MAP_FAILED) {
		perror("mmap operation failed\n");
		return -1;
	}
	else {
		printf("tx_queue at vaddr : %p\n",tx_queue);
	}

	char *ex = (char *) tx_queue;
	for(i=0;i<10;i++) {
		printf("i : %d and ex[%d] = %c\n",i,i,ex[i]);
	}


	#endif

	//scanf("%d",&waitint);


	close(configfd);
	return 0;

}
