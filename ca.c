#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/debugfs.h>
#include<linux/slab.h>
#include<linux/mm.h>
#include <linux/dma-mapping.h> 
#include<asm/uaccess.h>
#include <linux/pci.h>

struct dentry *file;

#define NUM_BUFFERS 64
#define LOGGING 1

struct mydata
{
	void *tx_queue;
	dma_addr_t tx_queue_dma_addr;
	void *rx_queue;
	dma_addr_t rx_queue_dma_addr;
	void *rx_result;
	dma_addr_t rx_result_dma_addr;
	void *bar_virt;
	uint64_t bar_base_addr_phy;
	void *coherent_mem_tx[NUM_BUFFERS];
	dma_addr_t coherent_mem_tx_dma_addr[NUM_BUFFERS];
	void *coherent_mem_rx[NUM_BUFFERS];
	dma_addr_t coherent_mem_rx_dma_addr[NUM_BUFFERS];

} __attribute__((packed)) ;

int op_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int status;
	int offset;
	unsigned int size;
	unsigned int sizearg;
	void *mem = NULL;
	struct mydata *data;

	data = filp->private_data;

	offset = vma->vm_pgoff;


	size = vma->vm_end-vma->vm_start;
	sizearg =  size;
	printk("\n\nvma->vm_start : %lx\n",vma->vm_start);
	printk("vma->vm_end : %lx\n",vma->vm_end);
	printk("size :%u\n",size);
	printk("offset : %u\n\n",offset);

	if(offset == 0) {
		mem = data->tx_queue;
	}
	else if(offset == 1) {
		mem = data->rx_queue;
	}
	else if(offset == 2) {
		mem = data->rx_result;
	}
	else if(offset == 3) {
                mem = data->bar_virt;
        }
	else if(offset >=4 && offset <=67 ) {
		mem = data->coherent_mem_tx[offset-4];
	}
	else if(offset >=68 && offset <=131 ) {
		mem = data->coherent_mem_rx[offset-68];
	}
	else {
		printk("illegal offset : %d\n",offset);
		return -ENOMEM;
	}

	if(offset !=3) {
		status =  remap_pfn_range(vma,vma->vm_start,(virt_to_phys(mem))>>PAGE_SHIFT,sizearg,vma->vm_page_prot);
	}else {
		status =  io_remap_pfn_range(vma,vma->vm_start,(data->bar_base_addr_phy)>>PAGE_SHIFT,sizearg,vma->vm_page_prot);
		//status =  remap_pfn_range(vma,vma->vm_start,(virt_to_phys(mem))>>PAGE_SHIFT,sizearg,vma->vm_page_prot);
		printk("mapping bar to userspace status : %d\n",status);
	}

	size -= sizearg;

	if(size) {
		printk("Not all memory mapped, leftover : %u\n",size);
		return -ENOMEM;
	}	

	return status;
}

ssize_t example_read (struct file *filp, char __user * userdata, size_t size, loff_t *offset)
{

	struct mydata *data;
	int copysize;
	uint64_t bar_len;
	struct pci_dev *dev;	

	//dev = pci_get_device(0x10ee,0x8038, NULL);
	dev = pci_get_device(0x10ee,0x8028, NULL);

	if(!dev) {
		return -ENODEV;
	}

	data = filp->private_data;

	data->bar_base_addr_phy = pci_resource_start(dev, 0);
	bar_len = pci_resource_len(dev, 0);
	/* do not map BARs with length 0. Note that start MAY be 0! */
	if (!bar_len) {
		printk("BAR #%d is not present - skipping\n", 0);
	}

	copysize = size >= sizeof(struct mydata) ? sizeof(struct mydata) : size; 

	copy_to_user(userdata,data,copysize);

	return copysize;	
}

int mmapfop_close(struct inode *inode,struct file *filp)
{
	struct mydata *data;
	int i;
	struct pci_dev *dev;
	//dev = pci_get_device(0x10ee,0x8038, NULL);
	dev = pci_get_device(0x10ee,0x8028, NULL);

	if(!dev) {
		return -ENODEV;
	}



	data = filp->private_data;

	printk("Freeing tx queue\n");
	dma_free_coherent(&dev->dev,PAGE_SIZE,data->tx_queue,data->tx_queue_dma_addr);
	printk("Freeing rx queue\n");
	dma_free_coherent(&dev->dev,PAGE_SIZE,data->rx_queue,data->rx_queue_dma_addr);
	printk("Freeing rx result \n");
	dma_free_coherent(&dev->dev,PAGE_SIZE,data->rx_result,data->rx_result_dma_addr);

	for(i=0;i<NUM_BUFFERS;i++) {
		dma_free_coherent(&dev->dev,PAGE_SIZE,data->coherent_mem_tx[i],data->coherent_mem_tx_dma_addr[i]);
		dma_free_coherent(&dev->dev,PAGE_SIZE,data->coherent_mem_rx[i],data->coherent_mem_rx_dma_addr[i]);
	}

	kfree(data);

	filp->private_data = NULL;	

	return 0;
}

int mmapfop_open(struct inode *inode, struct file *filp)
{

	struct mydata *data;
	int i;
	struct pci_dev *dev;
	//dev = pci_get_device(0x10ee,0x8038, NULL);
	dev = pci_get_device(0x10ee,0x8028, NULL);

	if(!dev) {
		return -ENODEV;
	}


	data = kmalloc(sizeof(struct mydata),GFP_KERNEL);

	if(!data) {
		printk("kmalloc failed\n");
	}

	filp->private_data = data;

	data->tx_queue = dma_alloc_coherent(&dev->dev,PAGE_SIZE,&data->tx_queue_dma_addr,GFP_KERNEL);
	#if LOGGING
	if(!data->tx_queue) {
		printk("tx queue allocation failed\n");
	}
	else {
		printk("tx queue allocated at kvaddr : %p and dma addr : %p\n",data->tx_queue,(void *) data->tx_queue_dma_addr);
	}
	#endif

	data->rx_queue = dma_alloc_coherent(&dev->dev,PAGE_SIZE,&data->rx_queue_dma_addr,GFP_KERNEL);
	#if LOGGING
	if(!data->rx_queue) {
		printk("rx queue allocation failed\n");
	}
	else {
		printk("rx queue allocated at kvaddr : %p and dma addr : %p\n",data->rx_queue,(void *) data->rx_queue_dma_addr);
	}
	#endif

	data->rx_result = dma_alloc_coherent(&dev->dev,PAGE_SIZE,&data->rx_result_dma_addr,GFP_KERNEL);

	#if LOGGING
	if(!data->rx_queue) {
		printk("rx result allocation failed\n");
	}
	else {
		printk("rx result allocated at kvaddr : %p and dma addr : %p\n",data->rx_result,(void *) data->rx_result_dma_addr);
	}
	#endif
	


	for(i=0;i<NUM_BUFFERS;i++) {
		data->coherent_mem_tx[i] = dma_alloc_coherent(&dev->dev,PAGE_SIZE,&data->coherent_mem_tx_dma_addr[i],GFP_KERNEL);
		#if LOGGING
		if(!data->coherent_mem_tx[i]) {
			printk("coherent allocation failed for tx, i : %d\n",i);
		}
		else {
			printk("coherent allocation for tx, i : %d at kvaddr : %p dma addr : %p\n",
					i,data->coherent_mem_tx[i],(void *) data->coherent_mem_tx_dma_addr[i]);
		}
		#endif
	}


	for(i=0;i<NUM_BUFFERS;i++) {
		data->coherent_mem_rx[i] = dma_alloc_coherent(&dev->dev,PAGE_SIZE,&data->coherent_mem_rx_dma_addr[i],GFP_KERNEL);
		#if LOGGING
		if(!data->coherent_mem_rx[i]) {
			printk("coherent allocation failed for rx, i : %d\n",i);
		}
		else {
			printk("coherent allocation for rx, i : %d at kvaddr : %p dma addr : %p\n",
					i,data->coherent_mem_rx[i],(void *)data->coherent_mem_rx_dma_addr[i]);
		}
		#endif
	}

	return 0;
}

static const struct file_operations mmap_fops = {
	.open  = mmapfop_open,
	.release = mmapfop_close,
	.mmap  = op_mmap,
	.read = example_read,
};

static int __init mmap_module_init(void)
{
	file = debugfs_create_file("coherent_buffers",0644,NULL,NULL,&mmap_fops);
	return 0;
}

static void __exit mmap_module_exit(void)
{
	debugfs_remove(file);
}

module_init(mmap_module_init);
module_exit(mmap_module_exit);
MODULE_LICENSE("GPL");
