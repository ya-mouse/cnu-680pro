//choish_shared_lib_porting, add file for cramfs support
#ifndef __CRAMFS_FS_I
#define __CRAMFS_FS_I

/* inode in-kernel data  */

struct cramfs_inode_info {
	/* pointer to previously read data.
	 * this pointer is used to mmap regular files
	 * (only valid for non-XIP regular files)
	 * 0 indicates invalid */
	unsigned long i_savedptr;

	int i_num_mapping; /* number of mapping references */
};

#endif
