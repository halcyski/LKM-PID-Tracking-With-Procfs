## In order to compile and run this kernel:

1. First install Windows subsystem for Linux (WSL):
```Powershell
wsl --install
```

2. Then, follow this guide to install the correct WSL2 kernel headers:
https://learn.microsoft.com/en-us/community/content/wsl-user-msft-kernel-v6


3. Clone this repo into a new project folder. 




Info about Procfs:

The Proc filesystem
- Special, virtual filesystem in Linux that provides a window into the kernel's internal data structure and state. The files and directories in /proc don't exist on disk, they are created in memory by the kernel on the fly when a user tries to access them
- It is a simple yet powerful interface 
- We can create our own virtual files that can interface with user-mode applications. 
- Effectively turning complex kernel communication into simple file I/O

To create a ``procfs`` entry...
- We use the ``proc_create`` function
	- Typically done in the modules ``init`` funciton
	- This function is delcared in ``<linux/proc_fs.h>``
	- basic signature: ``proc_create(name, mode, parent, proc_ops)
		- ``name``: string literal for the filename
		- ``mode``: permissions for the file
		- ``parent``: parent directory within ``/proc``. 
			- using ``NULL`` places the file in the root of ``/proc``
		- ``proc_ops``: pointer to a structure that defines which of our functions handle read, write, and other operations
	- ``proc_create`` must be cleaned up by using ``remove_proc_entry``


Writing the ``proc_ops`` struct is how file operations are connected to your functions. There are two important ones:
- ``proc_read``: called when a user runs a command like ``cat /proc/my_proc``. Its job is to safely copy data from a kernel buffer to the user-space buffer provided by the application. This is done by using the ``copy_to_user()`` function.
- ``proc_write``: called when a user runs a command like ``echo "some data" > /proc/my_proc``. Its job is to safely copy data from the user-space buffer into a kernel buffer for processing. This is done by using the ``copy_from_user()`` function.

The ``proc_ops`` structure can be defined as follows:
```c
static const struct proc_ops proc_ops = {
	.proc_read = proc_read,
	.proc_write = proc_write,
};
```

Accessing a user-space pointer from the kernel is forbidden and will cause a kernel panic. 
- You are required to use ``copy_to_user()`` / ``copy_from_user`` to ensure that memory access is safe and valid.

``copy_to_user()`` and ``copy_from_user()`` are provided in ``<linux/uaccess.h>`` 


NOTE: Both ``proc_read`` and ``proc_write`` should be declared as ``static`` to prevent cluttering up global kernel namespace.

Breaking down the signature of ``ssize_t proc_read(struct file *file, char __user *usr_buf, size_t count, loff_t *ppos)``
- ``ssize_t`` 
	- Return type of "signed size type"
	- If the read from ``proc_read`` was successful, you return the number of bytes copied to the user.
	- If everything from ``proc_read`` has already been read, you return 0 to signal the end of the file (EOF).
	- If an error occurs within the file, you return a "negative error code" (e.g. ``-EFAULT``)
- ``struct file *file`` 
	- Pointer to a kernel structure representing the instance of the file that was opened by the user-space process. 
	- Contains info like permissions and flags.
- ``char __user *usr_buf`` 
	- Pointer to the buffer in user space where your function will write its data. Remember that kernel-space programs cannot write to user-space memory so this pointer must be passed along with a function call to ``copy_to_user()`` to write data.
	- The annotation ``__user`` indicates that the pointer is from an untrusted source (user space) and must not be dereferenced directly.
- ``size_t count``
	- The size (in bytes) of the ``usr_buf``. 
	- The max amount of data the user-space application is ready to receive in the single read operation. You should not write more than ``count`` bytes.
- ``loff_t *ppos`` 
	- "Long file offset pointer"
	- Tracks the current position within the virtual file
	- When the function is called, ``*ppos`` tells you where the read should begin. For the first read, it will be 0.
	- Before you return, you must update ``*ppos``. For example, if you write X bytes, you should add X to ``*ppos``. 
	- If you don't update this value, the kernel will think you haven't advanced in the file and will call your ``proc_read`` function over and over again. A simple check of ``*ppos > 0`` can prevent this with some additional logic.

TL;DR: A user wants to read up to ``count`` bytes. Their buffer is at ``usr_buf``. They are currently at file position ``*ppos``. The job of this function is to copy your virtual kernel data into their buffer, update ``*ppos`` to know where it leaves off, and return the number of written bytes.

Breaking down the signature of ``static ssize_t proc_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos)``
- ``ssize_t``
	- Ditto for ``proc_read`` except you return the number of bytes you successfully read from the buffer. If an error occurs, you return a negative error code
- ``struct file *file``
	- Ditto for ``proc_read``
- ``const char __user *usr_buf
	- Pointer to the buffer in user space that contains the data the user is trying to write
	- ``const`` indicates that from the kernel's perspective, the data cannot be modified
	- ``__user`` marks that the pointer is untrusted. You must use the ``copy_from_user()`` function to safely transfer data from the user-space buffer into a kernel-space buffer before you can work with it. 
	- ``size_t count``
		- How many bytes the user is attempting to write. 
		- A.K.A. size of the data located at ``usr_buf``
	- ``loff_t *ppos`` 
		- Same file offset pointer. 

TL:DR: A user wants to write ``count`` bytes of data to this virtual kernel file. The job of this function is to safely copy that data into the kernel, process it, and return the number of bytes successfully consumed.





