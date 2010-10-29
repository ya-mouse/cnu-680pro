/*
 * Compressed rom filesystem for Linux.
 *
 * Copyright (C) 1999 Linus Torvalds.
 *
 * This file is released under the GPL.
 */

/*
 * These are the VFS interfaces to the compressed rom filesystem.
 * The actual compression is based on zlib, see the other files.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <linux/cramfs_fs.h>
#ifdef CONFIG_CRAMFS_MMAP
#include <linux/slab.h>
#endif
#include <asm/semaphore.h>

#include <asm/uaccess.h>

#define CRAMFS_SB_MAGIC u.cramfs_sb.magic
#define CRAMFS_SB_SIZE u.cramfs_sb.size
#define CRAMFS_SB_BLOCKS u.cramfs_sb.blocks
#define CRAMFS_SB_FILES u.cramfs_sb.files
#define CRAMFS_SB_FLAGS u.cramfs_sb.flags

static struct super_operations cramfs_ops;
static struct inode_operations cramfs_dir_inode_operations;
static struct file_operations cramfs_directory_operations;
#ifdef CONFIG_CRAMFS_MMAP
static struct file_operations cramfs_file_operations;
#endif
static struct address_space_operations cramfs_aops;

static DECLARE_MUTEX(read_mutex);

//#define CRAMFS_MMAP_DEBUG 

/* These two macros may change in future, to provide better st_ino
   semantics. */
#define CRAMINO(x)	((x)->offset?(x)->offset<<2:1)
#define OFFSET(x)	((x)->i_ino)

#ifdef CONFIG_CRAMFS_MMAP
static int cramfs_file_mmap(struct file *file, struct vm_area_struct *vma) {
	struct inode *inode = file->f_dentry->d_inode;
	int len = 0;
	char *data;
	int error;

#ifdef CRAMFS_MMAP_DEBUG
	printk("cramfs_file_mmap  ");
#endif

	/* skip if XIP files */
	if ((inode->i_mode & S_ISVTX)) {
#ifdef CRAMFS_MMAP_DEBUG
		printk("skip XIP\n"); 
#endif
		return -ENOSYS;
	}

	/* check if previously read this file */
	if (inode->u.cramfs_i.i_savedptr) {
#ifdef CRAMFS_MMAP_DEBUG
		printk("file=0x%x, inode=0x%x, savedptr=0x%x, mapping=%d\n", 
			file, inode, inode->u.cramfs_i.i_savedptr,
			inode->u.cramfs_i.i_num_mapping); 
#endif
		goto saved_ptr;
	}

	/* first attempt to mmap this file.
	 * read it */
	len = vma->vm_end - vma->vm_start;
	data = kmalloc(len, GFP_KERNEL);
#ifdef CRAMFS_MMAP_DEBUG
	printk("file=0x%x, data=0x%x, len=0x%x\n", file, data, len);
#endif
	error = file->f_op->read(file, (char *)data, len, &file->f_pos);
	if (error < 0) {
		kfree(data);
		return error;
	}
	if (error < len) {
		memset(data+error, '\0', len-error);
	}
	/* increase inode usage counter 
	 * not to delete this inode */
	atomic_inc(&inode->i_count);
	
	/* save this pointer for further mmap */
	inode->u.cramfs_i.i_savedptr = (unsigned long)data;
	inode->u.cramfs_i.i_num_mapping = 0;

saved_ptr:
	/* previously mmapped this file, just get the pointer to it */
	vma->vm_start = inode->u.cramfs_i.i_savedptr;
	inode->u.cramfs_i.i_num_mapping++;
	return 0;
}

#ifdef CONFIG_MMAP_FIX
static int cramfs_file_munmap(struct inode *inode) {
	char *data;
#ifdef CRAMFS_MMAP_DEBUG
	printk("cramfs_file_munmap  inode=0x%x, mapping=%d, ",
		inode, inode->u.cramfs_i.i_num_mapping);
#endif

	/* check for previous saved pointer */
	data = (char *)inode->u.cramfs_i.i_savedptr;
	if(data != NULL) {
		inode->u.cramfs_i.i_num_mapping--;
		if(inode->u.cramfs_i.i_num_mapping == 0) {
			inode->u.cramfs_i.i_savedptr = 0;
			kfree(data);

			/* release usage count. now. this inode can be removed */
			iput(inode);
		}
	}
#ifdef CRAMFS_MMAP_DEBUG
	printk("data=0x%x\n", data);
#endif
	return 0;
}
#endif /* CONFIG_MMAP_FIX */


#ifdef MAGIC_ROM_PTR
static int cramfs_romptr(struct file *file, struct vm_area_struct *vma) {
	struct inode *inode = file->f_dentry->d_inode;
	
#ifdef CRAMFS_MMAP_DEBUG
	printk("cramfs_romptr  ");
#endif
	/* skip non-XIP files */
	if(!(inode->i_mode & S_ISVTX)) {
#ifdef CRAMFS_MMAP_DEBUG
		printk("skip non-XIP\n"); 
#endif
		return -ENOSYS;
	}

	/* skip if writable area */
	if(vma->vm_flags & VM_WRITE) {
#ifdef CRAMFS_MMAP_DEBUG
		printk("skip writable\n"); 
#endif
		return -ENOSYS;
	}

	/* get offset from the start of file system */
	vma->vm_offset += (OFFSET(inode) + (PAGE_CACHE_SIZE-1))&(~(PAGE_CACHE_SIZE-1));

	/* get actual address with the help of blk device */
	if(bromptr(inode->i_dev, vma)) { return -ENOSYS;}
#ifdef CRAMFS_MMAP_DEBUG
	printk("offset=0x%x\n", vma->vm_start);
#endif

	return 0; /* no error */
}
#endif /* MAGIC_ROM_PTR */
#endif /* CONFIG_CRAMFS_MMAP */

static struct inode *get_cramfs_inode(struct super_block *sb, struct cramfs_inode * cramfs_inode)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_mode = cramfs_inode->mode;
		inode->i_uid = cramfs_inode->uid;
		inode->i_size = cramfs_inode->size;
		inode->i_blocks = (cramfs_inode->size - 1) / 512 + 1;
		inode->i_blksize = PAGE_CACHE_SIZE;
		inode->i_gid = cramfs_inode->gid;
		inode->i_ino = CRAMINO(cramfs_inode);
		/* inode->i_nlink is left 1 - arguably wrong for directories,
		   but it's the best we can do without reading the directory
	           contents.  1 yields the right result in GNU find, even
		   without -noleaf option. */
		insert_inode_hash(inode);
#ifdef CONFIG_CRAMFS_MMAP
		inode->u.cramfs_i.i_savedptr = 0;
		inode->u.cramfs_i.i_num_mapping = 0;
#endif
		if (S_ISREG(inode->i_mode)) {
#ifdef CONFIG_CRAMFS_MMAP
			inode->i_fop = &cramfs_file_operations;
#else
			inode->i_fop = &generic_ro_fops;
#endif
			inode->i_data.a_ops = &cramfs_aops;
		} else if (S_ISDIR(inode->i_mode)) {
			inode->i_op = &cramfs_dir_inode_operations;
			inode->i_fop = &cramfs_directory_operations;
		} else if (S_ISLNK(inode->i_mode)) {
			inode->i_op = &page_symlink_inode_operations;
			inode->i_data.a_ops = &cramfs_aops;
		} else {
			inode->i_size = 0;
			init_special_inode(inode, inode->i_mode, cramfs_inode->size);
		}
	}
	return inode;
}

/*
 * We have our own block cache: don't fill up the buffer cache
 * with the rom-image, because the way the filesystem is set
 * up the accesses should be fairly regular and cached in the
 * page cache and dentry tree anyway..
 *
 * This also acts as a way to guarantee contiguous areas of up to
 * BLKS_PER_BUF*PAGE_CACHE_SIZE, so that the caller doesn't need to
 * worry about end-of-buffer issues even when decompressing a full
 * page cache.
 */
#define READ_BUFFERS (2)
/* NEXT_BUFFER(): Loop over [0..(READ_BUFFERS-1)]. */
#define NEXT_BUFFER(_ix) ((_ix) ^ 1)

/*
 * BLKS_PER_BUF_SHIFT should be at least 2 to allow for "compressed"
 * data that takes up more space than the original and with unlucky
 * alignment.
 */
#define BLKS_PER_BUF_SHIFT	(2)
#define BLKS_PER_BUF		(1 << BLKS_PER_BUF_SHIFT)
#define BUFFER_SIZE		(BLKS_PER_BUF*PAGE_CACHE_SIZE)

static unsigned char read_buffers[READ_BUFFERS][BUFFER_SIZE];
static unsigned buffer_blocknr[READ_BUFFERS];
static struct super_block * buffer_dev[READ_BUFFERS];
static int next_buffer;

/*
 * Returns a pointer to a buffer containing at least LEN bytes of
 * filesystem starting at byte offset OFFSET into the filesystem.
 */
static void *cramfs_read(struct super_block *sb, unsigned int offset, unsigned int len)
{
	struct buffer_head * bh_array[BLKS_PER_BUF];
	struct buffer_head * read_array[BLKS_PER_BUF];
	unsigned i, blocknr, buffer, unread;
	unsigned long devsize;
	int major, minor;
	
	char *data;

	if (!len)
		return NULL;
	blocknr = offset >> PAGE_CACHE_SHIFT;
	offset &= PAGE_CACHE_SIZE - 1;

	/* Check if an existing buffer already has the data.. */
	for (i = 0; i < READ_BUFFERS; i++) {
		unsigned int blk_offset;

		if (buffer_dev[i] != sb)
			continue;
		if (blocknr < buffer_blocknr[i])
			continue;
		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_CACHE_SHIFT;
		blk_offset += offset;
		if (blk_offset + len > BUFFER_SIZE)
			continue;
		return read_buffers[i] + blk_offset;
	}

	devsize = ~0UL;
	major = MAJOR(sb->s_dev);
	minor = MINOR(sb->s_dev);

	if (blk_size[major])
		devsize = blk_size[major][minor] >> 2;

	/* Ok, read in BLKS_PER_BUF pages completely first. */
	unread = 0;
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct buffer_head *bh;

		bh = NULL;
		if (blocknr + i < devsize) {
			bh = getblk(sb->s_dev, blocknr + i, PAGE_CACHE_SIZE);
			if (!buffer_uptodate(bh))
				read_array[unread++] = bh;
		}
		bh_array[i] = bh;
	}

	if (unread) {
		ll_rw_block(READ, unread, read_array);
		do {
			unread--;
			wait_on_buffer(read_array[unread]);
		} while (unread);
	}

	/* Ok, copy them to the staging area without sleeping. */
	buffer = next_buffer;
	next_buffer = NEXT_BUFFER(buffer);
	buffer_blocknr[buffer] = blocknr;
	buffer_dev[buffer] = sb;

	data = read_buffers[buffer];
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct buffer_head * bh = bh_array[i];
		if (bh) {
			memcpy(data, bh->b_data, PAGE_CACHE_SIZE);
			brelse(bh);
		} else
			memset(data, 0, PAGE_CACHE_SIZE);
		data += PAGE_CACHE_SIZE;
	}
	return read_buffers[buffer] + offset;
}
			

static struct super_block * cramfs_read_super(struct super_block *sb, void *data, int silent)
{
	int i;
	struct cramfs_super super;
	unsigned long root_offset;
	struct super_block * retval = NULL;

	set_blocksize(sb->s_dev, PAGE_CACHE_SIZE);
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;

	/* Invalidate the read buffers on mount: think disk change.. */
	for (i = 0; i < READ_BUFFERS; i++)
		buffer_blocknr[i] = -1;

	down(&read_mutex);
	/* Read the first block and get the superblock from it */
	memcpy(&super, cramfs_read(sb, 0, sizeof(super)), sizeof(super));
	up(&read_mutex);

	/* Do sanity checks on the superblock */
	if (super.magic != CRAMFS_MAGIC) {
		/* check at 512 byte offset */
		memcpy(&super, cramfs_read(sb, 512, sizeof(super)), sizeof(super));
		if (super.magic != CRAMFS_MAGIC) {
			printk(KERN_ERR "cramfs: wrong magic\n");
			goto out;
		}
	}

	/* get feature flags first */
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS) {
		printk(KERN_ERR "cramfs: unsupported filesystem features\n");
		goto out;
	}

	/* Check that the root inode is in a sane state */
	if (!S_ISDIR(super.root.mode)) {
		printk(KERN_ERR "cramfs: root is not a directory\n");
		goto out;
	}
	root_offset = super.root.offset << 2;
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		sb->CRAMFS_SB_SIZE=super.size;
		sb->CRAMFS_SB_BLOCKS=super.fsid.blocks;
		sb->CRAMFS_SB_FILES=super.fsid.files;
	} else {
		sb->CRAMFS_SB_SIZE=1<<28;
		sb->CRAMFS_SB_BLOCKS=0;
		sb->CRAMFS_SB_FILES=0;
	}
	sb->CRAMFS_SB_MAGIC=super.magic;
	sb->CRAMFS_SB_FLAGS=super.flags;
	if (root_offset == 0)
		printk(KERN_INFO "cramfs: empty filesystem");
	else if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
		 ((root_offset != sizeof(struct cramfs_super)) &&
		  (root_offset != 512 + sizeof(struct cramfs_super))))
	{
		printk(KERN_ERR "cramfs: bad root offset %lu\n", root_offset);
		goto out;
	}

	/* Set it all up.. */
	sb->s_op = &cramfs_ops;
	sb->s_root = d_alloc_root(get_cramfs_inode(sb, &super.root));
	retval = sb;
out:
	return retval;
}

static int cramfs_statfs(struct super_block *sb, struct statfs *buf)
{
	buf->f_type = CRAMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_blocks = sb->CRAMFS_SB_BLOCKS;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = sb->CRAMFS_SB_FILES;
	buf->f_ffree = 0;
	buf->f_namelen = 255;
	return 0;
}

/*
 * Read a cramfs directory entry.
 */
static int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	unsigned int offset;
	int copied;

	/* Offset within the thing. */
	offset = filp->f_pos;
	if (offset >= inode->i_size)
		return 0;
	/* Directory entries are always 4-byte aligned */
	if (offset & 3)
		return -EINVAL;

	copied = 0;
	while (offset < inode->i_size) {
		struct cramfs_inode *de;
		unsigned long nextoffset;
		char *name;
		int namelen, error;

		down(&read_mutex);
		de = cramfs_read(sb, OFFSET(inode) + offset, sizeof(*de)+256);
		up(&read_mutex);
		name = (char *)(de+1);

		/*
		 * Namelengths on disk are shifted by two
		 * and the name padded out to 4-byte boundaries
		 * with zeroes.
		 */
		namelen = de->namelen << 2;
		nextoffset = offset + sizeof(*de) + namelen;
		for (;;) {
			if (!namelen)
				return -EIO;
			if (name[namelen-1])
				break;
			namelen--;
		}
		error = filldir(dirent, name, namelen, offset, CRAMINO(de), de->mode >> 12);
		if (error)
			break;

		offset = nextoffset;
		filp->f_pos = offset;
		copied++;
	}
	return 0;
}

/*
 * Lookup and fill in the inode data..
 */
static struct dentry * cramfs_lookup(struct inode *dir, struct dentry *dentry)
{
	unsigned int offset = 0;
	int sorted = dir->i_sb->CRAMFS_SB_FLAGS & CRAMFS_FLAG_SORTED_DIRS;

	while (offset < dir->i_size) {
		struct cramfs_inode *de;
		char *name;
		int namelen, retval;

		down(&read_mutex);
		de = cramfs_read(dir->i_sb, OFFSET(dir) + offset, sizeof(*de)+256);
		up(&read_mutex);
		name = (char *)(de+1);

		/* Try to take advantage of sorted directories */
		if (sorted && (dentry->d_name.name[0] < name[0]))
			break;

		namelen = de->namelen << 2;
		offset += sizeof(*de) + namelen;

		/* Quick check that the name is roughly the right length */
		if (((dentry->d_name.len + 3) & ~3) != namelen)
			continue;

		for (;;) {
			if (!namelen)
				return ERR_PTR(-EIO);
			if (name[namelen-1])
				break;
			namelen--;
		}
		if (namelen != dentry->d_name.len)
			continue;
		retval = memcmp(dentry->d_name.name, name, namelen);
		if (retval > 0)
			continue;
		if (!retval) {
			d_add(dentry, get_cramfs_inode(dir->i_sb, de));
			return NULL;
		}
		/* else (retval < 0) */
		if (sorted)
			break;
	}
	d_add(dentry, NULL);
	return NULL;
}

struct dentry * cramfs_lookup_renew(struct inode *dir, struct dentry *dentry, struct inode *curr)
{
	unsigned int offset = 0;
	int sorted = dir->i_sb->CRAMFS_SB_FLAGS & CRAMFS_FLAG_SORTED_DIRS;

	unsigned int i;

	// hans

	for(i =0; i < READ_BUFFERS; i++){
		buffer_dev[i] = -1;
		buffer_blocknr[i] = -1;
	}

	while (offset < dir->i_size) {
		struct cramfs_inode *de;
		char *name;
		int namelen, retval;

		down(&read_mutex);

#if 1	// hans
		{
			unsigned j, blocknr;
	
			blocknr = (OFFSET(dir) + offset) >> PAGE_CACHE_SHIFT;

			/* Ok, read in BLKS_PER_BUF pages completely first. */
			for (j = 0; j < BLKS_PER_BUF; j++) {
				struct buffer_head *bh;

				bh = getblk(dir->i_sb->s_dev, blocknr + j, PAGE_CACHE_SIZE);
				//(!buffer_uptodate(bh))
				(bh)->b_state &= ~(1UL << BH_Uptodate);
			}
		}
#endif
		de = cramfs_read(dir->i_sb, OFFSET(dir) + offset, sizeof(*de)+256);
		up(&read_mutex);
		name = (char *)(de+1);

		/* Try to take advantage of sorted directories */
		if (sorted && (dentry->d_name.name[0] < name[0]))
			break;

		namelen = de->namelen << 2;
		offset += sizeof(*de) + namelen;

		/* Quick check that the name is roughly the right length */
		if (((dentry->d_name.len + 3) & ~3) != namelen)
			continue;

		for (;;) {
			if (!namelen)
				return ERR_PTR(-EIO);
			if (name[namelen-1])
				break;
			namelen--;
		}
		if (namelen != dentry->d_name.len)
			continue;
		retval = memcmp(dentry->d_name.name, name, namelen);
		if (retval > 0)
			continue;
		if (!retval) {
			curr->i_ino = CRAMINO(de);
			return NULL;
		}
		/* else (retval < 0) */
		if (sorted)
			break;
	}
	return NULL;
}
static int cramfs_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;
	u32 maxblock, bytes_filled;
	void *pgdata;
	int xip = 0;
	
#ifdef CONFIG_CRAMFS_MMAP
#ifdef MAGIC_ROM_PTR
	if((inode->i_mode & S_ISVTX)) { xip = 1;}
#endif
#endif


	maxblock = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	if (page->index < maxblock) {
#ifdef CONFIG_CRAMFS_MMAP
#ifdef MAGIC_ROM_PTR
		if(!xip) {
#endif
#endif
		struct super_block *sb = inode->i_sb;
		u32 blkptr_offset = OFFSET(inode) + page->index*4;
		u32 start_offset, compr_len;

		start_offset = OFFSET(inode) + maxblock*4;
		down(&read_mutex);
		if (page->index)
			start_offset = *(u32 *) cramfs_read(sb, blkptr_offset-4, 4);
		compr_len = (*(u32 *) cramfs_read(sb, blkptr_offset, 4) - start_offset);
		up(&read_mutex);
		pgdata = kmap(page);
		if (compr_len == 0)
			; /* hole */
		else {
			down(&read_mutex);
			bytes_filled = cramfs_uncompress_block(pgdata,
				 PAGE_CACHE_SIZE,
				 cramfs_read(sb, start_offset, compr_len),
				 compr_len);
				up(&read_mutex);
		}
#ifdef CONFIG_CRAMFS_MMAP
#ifdef MAGIC_ROM_PTR
		}
		else { /* XIP */
			struct vm_area_struct vma;
			vma.vm_offset = (OFFSET(inode) + (PAGE_CACHE_SIZE-1))& (~(PAGE_CACHE_SIZE-1));
			vma.vm_offset += (page->index)*PAGE_CACHE_SIZE;
			bromptr(inode->i_dev , &vma);
			pgdata = kmap(page);
#ifdef CRAMFS_MMAP_DEBUG
			printk("readpage XIP   data=0x%x\n",vma.vm_start );
#endif
			memcpy(pgdata, (void *)vma.vm_start, PAGE_CACHE_SIZE);
			bytes_filled = PAGE_CACHE_SIZE;
		}
#endif
#endif
	} else
		pgdata = kmap(page);
	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	kunmap(page);
	flush_dcache_page(page);
	SetPageUptodate(page);
	UnlockPage(page);
	return 0;
}

static struct address_space_operations cramfs_aops = {
	readpage: cramfs_readpage
};

/*
 * Our operations:
 */

/*
 * A directory can only readdir
 */
static struct file_operations cramfs_directory_operations = {
	read:		generic_read_dir,
	readdir:	cramfs_readdir,
};

#ifdef CONFIG_CRAMFS_MMAP
static struct file_operations cramfs_file_operations = {
        llseek:         generic_file_llseek,
        read:           generic_file_read,
        mmap:           cramfs_file_mmap,
#ifdef CONFIG_MMAP_FIX
	munmap:		cramfs_file_munmap,
#endif
#ifdef MAGIC_ROM_PTR
	romptr:		cramfs_romptr,
#endif
};     
#endif

static struct inode_operations cramfs_dir_inode_operations = {
	lookup:		cramfs_lookup,
};

static struct super_operations cramfs_ops = {
	statfs:		cramfs_statfs,
};

static DECLARE_FSTYPE_DEV(cramfs_fs_type, "cramfs", cramfs_read_super);

static int __init init_cramfs_fs(void)
{
	cramfs_uncompress_init();
	return register_filesystem(&cramfs_fs_type);
}

static void __exit exit_cramfs_fs(void)
{
	cramfs_uncompress_exit();
	unregister_filesystem(&cramfs_fs_type);
}

module_init(init_cramfs_fs)
module_exit(exit_cramfs_fs)
MODULE_LICENSE("GPL");

