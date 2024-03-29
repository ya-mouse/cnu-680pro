/*
 * Copyright (c) 2000-2001 Christoph Hellwig.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ident "$Id: vxfs_fshead.c,v 1.1.1.1 2003/11/17 02:35:09 jipark Exp $"

/*
 * Veritas filesystem driver - fileset header routines.
 */
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vxfs.h"
#include "vxfs_inode.h"
#include "vxfs_extern.h"
#include "vxfs_fshead.h"


#ifdef DIAGNOSTIC
static void
vxfs_dumpfsh(struct vxfs_fsh *fhp)
{
	printk("\n\ndumping fileset header:\n");
	printk("----------------------------\n");
	printk("version: %u\n", fhp->fsh_version);
	printk("fsindex: %u\n", fhp->fsh_fsindex);
	printk("iauino: %u\tninodes:%u\n",
			fhp->fsh_iauino, fhp->fsh_ninodes);
	printk("maxinode: %u\tlctino: %u\n",
			fhp->fsh_maxinode, fhp->fsh_lctino);
	printk("nau: %u\n", fhp->fsh_nau);
	printk("ilistino[0]: %u\tilistino[1]: %u\n",
			fhp->fsh_ilistino[0], fhp->fsh_ilistino[1]);
}
#endif

/**
 * vxfs_getfsh - read fileset header into memory
 * @ip:		the (fake) fileset header inode
 * @which:	0 for the structural, 1 for the primary fsh.
 *
 * Description:
 *   vxfs_getfsh reads either the structural or primary fileset header
 *   described by @ip into memory.
 *
 * Returns:
 *   The fileset header structure on success, else Zero.
 */
static struct vxfs_fsh *
vxfs_getfsh(struct inode *ip, int which)
{
	struct buffer_head		*bp;

	bp = vxfs_bread(ip, which);
	if (buffer_mapped(bp)) {
		struct vxfs_fsh		*fhp;

		if (!(fhp = kmalloc(sizeof(*fhp), SLAB_KERNEL)))
			return NULL;
		memcpy(fhp, bp->b_data, sizeof(*fhp));

		brelse(bp);
		return (fhp);
	}

	return NULL;
}

/**
 * vxfs_read_fshead - read the fileset headers
 * @sbp:	superblock to which the fileset belongs
 *
 * Description:
 *   vxfs_read_fshead will fill the inode and structural inode list in @sb.
 *
 * Returns:
 *   Zero on success, else a negative error code (-EINVAL).
 */
int
vxfs_read_fshead(struct super_block *sbp)
{
	struct vxfs_sb_info		*infp = VXFS_SBI(sbp);
	struct vxfs_fsh			*pfp, *sfp;
	struct vxfs_inode_info		*vip, *tip;

	if (!(vip = vxfs_blkiget(sbp, infp->vsi_iext, infp->vsi_fshino))) {
		printk(KERN_ERR "vxfs: unabled to read fsh inode\n");
		return -EINVAL;
	} else if (!VXFS_ISFSH(vip)) {
		printk(KERN_ERR "vxfs: fsh list inode is of wrong type (%x)\n",
				vip->vii_mode & VXFS_TYPE_MASK); 
		return -EINVAL;
	}


#ifdef DIAGNOSTIC
	printk("vxfs: fsh inode dump:\n");
	vxfs_dumpi(vip, infp->vsi_fshino);
#endif

	if (!(infp->vsi_fship = vxfs_get_fake_inode(sbp, vip))) {
		printk(KERN_ERR "vxfs: unabled to get fsh inode\n");
		return -EINVAL;
	}

	if (!(sfp = vxfs_getfsh(infp->vsi_fship, 0))) {
		printk(KERN_ERR "vxfs: unabled to get structural fsh\n");
		return -EINVAL;
	} 

#ifdef DIAGNOSTIC
	vxfs_dumpfsh(sfp);
#endif

	if (!(pfp = vxfs_getfsh(infp->vsi_fship, 1))) {
		printk(KERN_ERR "vxfs: unabled to get primary fsh\n");
		return -EINVAL;
	}

#ifdef DIAGNOSTIC
	vxfs_dumpfsh(pfp);
#endif

	tip = vxfs_blkiget(sbp, infp->vsi_iext, sfp->fsh_ilistino[0]);
	if (!tip || ((infp->vsi_stilist = vxfs_get_fake_inode(sbp, tip)) == NULL)) {
		printk(KERN_ERR "vxfs: unabled to get structual list inode\n");
		return -EINVAL;
	} else if (!VXFS_ISILT(VXFS_INO(infp->vsi_stilist))) {
		printk(KERN_ERR "vxfs: structual list inode is of wrong type (%x)\n",
				VXFS_INO(infp->vsi_stilist)->vii_mode & VXFS_TYPE_MASK); 
		return -EINVAL;
	}

	tip = vxfs_stiget(sbp, pfp->fsh_ilistino[0]);
	if (!tip || ((infp->vsi_ilist = vxfs_get_fake_inode(sbp, tip)) == NULL)) {
		printk(KERN_ERR "vxfs: unabled to get inode list inode\n");
		return -EINVAL;
	} else if (!VXFS_ISILT(VXFS_INO(infp->vsi_ilist))) {
		printk(KERN_ERR "vxfs: inode list inode is of wrong type (%x)\n",
				VXFS_INO(infp->vsi_ilist)->vii_mode & VXFS_TYPE_MASK);
		return -EINVAL;
	}

	return 0;
}
