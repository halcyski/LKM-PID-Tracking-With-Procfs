#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/kprobe.h>
#include <linux/capability.h>
#include <linux/user_namespace.h>

MODULE_LICENSE("GPL");

#define PROC_NAME "my_proc"
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

// PID to track
static pid_t PID = 0;
// directory for proc entry
static struct proc_dir_entry* proc_entry;


// This function will be called when the /proc/my_proc file is read
static ssize_t proc_read(struct file *file, char __user *usr_buf, size_t count, loff_t *ppos) {
	char buffer[128]; // generous buffer for output string + PID
	long len = 0; // length of the string to be written to the user buffer
	long pid = READ_ONCE(PID); // read atomically 

	// if this isnt the first read, return 0 to indicate EOF
	if (*ppos != 0) {
		return 0; // EOF
	}

	// len = number of characters written to buffer
	len = scnprintf(buffer, sizeof(buffer) "Currently monitoring PID: %d\n", pid);

	// copy kernel buffer to users buffer 
	if (copy_to_user(usr_buf, buffer, len)) { // send the buffer created with sprintf to the user
		return -EFAULT; // error in copying	
	}

	*ppos = len; // update the position for next read with the length of the buffer
	return len; // return the number of bytes read
}

static ssize_t proc_write(struct file* file, const char __user* usr_buf, size_t count, loff_t* ppos) {
	char buffer[32];
	long pid = 0;

	if (!ns_capable(&init_user_ns, CAP_SYS_ADMIN)) {
		return -EPERM;
	} // check if the user has CAP_SYS_ADMIN capability

	// ensure we dont overflow kernel buffer
	if (count > sizeof(buffer) - 1) {
		return -EINVAL; // invalid argument
	}

	// copy only the minimum of count or buffer size - 1
	n = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buffer, usr_buf, n)) {
		return -EFAULT; // error in copying
	}

	buffer[count] = '\0'; // null terminate the string

	if (kstrtol(buffer, 10, &pid) == 0) {

		if (pid < 0) {
			return -EINVAL; // negative PID is invalid
		}

		WRITE_ONCE(PID, pid);
		pr_info("Now monitoring PID %ld\n", PID);
	}
	else {
		return -EINVAL; // invalid input (kstrtol conversion failed)
	}

	return count;
}

static const struct proc_ops proc_ops = {
	.proc_read = proc_read,
	.proc_write = proc_write,
};

// This function is called when the module is loaded
static int __init my_module_init(void) {
	proc_entry = proc_create(PROC_NAME, 0644, NULL, &proc_ops);

	if (!proc_entry)
		return -ENOMEM;

	pr_info("/proc/%s created\n", PROC_NAME);
	return 0; // yay
}

static void __exit my_module_exit(void) {
	if (proc_entry)
		proc_remove(proc_entry);

	pr_info("/proc/%s removed\n", PROC_NAME);
}

module_init(my_module_init);
module_exit(my_module_exit);
