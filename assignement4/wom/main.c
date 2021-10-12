#include <asm/tlbflush.h>
#include <asm/uaccess.h>

#include <linux/highmem.h>
#include <linux/memory.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/version.h>

MODULE_AUTHOR("Stephan van Schaik");
MODULE_DESCRIPTION("A kernel module that implements Write-Only Memory");
MODULE_LICENSE("GPL");

#define WOM_MAGIC_NUM 0x1337
#define WOM_GET_ADDRESS \
	_IOR(WOM_MAGIC_NUM, 0, unsigned long)

static int device_busy = 0;
static unsigned char *device_mem = NULL;

static int
device_open(struct inode *inode, struct file *file)
{
	if (device_busy)
		return -EBUSY;

	try_module_get(THIS_MODULE);
	device_busy = 1;

	return 0;
}

static int
device_release(struct inode *inode, struct file *file)
{
	device_busy = 0;
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t
device_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	loff_t pos = *ppos;
	size_t i;

	printk(KERN_INFO "read(%llu)\n", pos);

	if (pos >= PAGE_SIZE)
		return 0;

	if (pos + len >= PAGE_SIZE)
		len = PAGE_SIZE - pos;

	for (i = 0; i < len; ++i) {
		*((volatile char *)device_mem + pos + i);
	}

	return len;
}

static ssize_t
device_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	loff_t pos = *ppos;
	int ret;

	if (pos >= PAGE_SIZE)
		return 0;

	if (pos + len >= PAGE_SIZE)
		len = PAGE_SIZE - pos;

	ret = copy_from_user(device_mem + pos, buf, len);

	if (ret < 0)
		return ret;

	return len;
}

static loff_t
device_lseek(struct file *file, loff_t offset, int orig)
{
	loff_t ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	mutex_lock(&file->f_dentry->d_inode->i_mutex);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)
	mutex_lock(&file->f_path.dentry->d_inode->i_mutex);
#else
	inode_lock(file_inode(file));
#endif

	printk(KERN_INFO "device_lseek(%llu, %d)\n", offset, orig);

	switch (orig) {
	case SEEK_CUR:
		offset += file->f_pos;
	case SEEK_SET:
		if ((unsigned long long)offset >= -MAX_ERRNO) {
			ret = -EOVERFLOW;
			break;
		}

		file->f_pos = offset;
		ret = file->f_pos;
		force_successful_syscall_return();
		break;
	default:
		ret = -EINVAL;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	mutex_unlock(&file->f_dentry->d_inode->i_mutex);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)
	mutex_unlock(&file->f_path.dentry->d_inode->i_mutex);
#else
	inode_unlock(file_inode(file));
#endif

	return ret;
}

static long
device_ioctl(struct file *file, unsigned int num, unsigned long param)
{
	if (num != WOM_GET_ADDRESS)
		return -EINVAL;

	return copy_to_user((void *)param, &device_mem, sizeof device_mem);
}

static struct file_operations fops = {
	.open = device_open,
	.release = device_release,
	.read = device_read,
	.write = device_write,
	.llseek = device_lseek,
	.unlocked_ioctl = device_ioctl,
};

static struct miscdevice misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wom",
	.fops = &fops,
	.mode = S_IRWXUGO,
};

int
init_module(void)
{
	int ret;

	device_mem = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (!device_mem) {
		printk(KERN_ALERT "wom: failed to allocate memory for write-only memory.\n");
		return -EFAULT;
	}

	memset(device_mem, 0, PAGE_SIZE);
	strcpy(device_mem, "742527b55fa326108d952fa713231337");

	ret = misc_register(&misc_dev);

	if (ret != 0) {
		printk(KERN_ALERT "wom: failed to register device with %d\n", ret);
		kfree(device_mem);
		return -1;
	}

	printk(KERN_INFO "wom: initialized.\n");

	return 0;
}

void
cleanup_module(void)
{
	misc_deregister(&misc_dev);
	kfree(device_mem);
	printk(KERN_INFO "wom: cleaned up.\n");
}
