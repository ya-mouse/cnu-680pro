/* $Id: hubdev.h,v 1.1.1.1 2003/11/17 02:35:26 jipark Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2001 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_SN1_HUBDEV_H
#define _ASM_IA64_SN_SN1_HUBDEV_H

extern void hubdev_init(void);
extern void hubdev_register(int (*attach_method)(devfs_handle_t));
extern int hubdev_unregister(int (*attach_method)(devfs_handle_t));
extern int hubdev_docallouts(devfs_handle_t hub);

extern caddr_t hubdev_prombase_get(devfs_handle_t hub);
extern cnodeid_t hubdev_cnodeid_get(devfs_handle_t hub);

#endif /* _ASM_IA64_SN_SN1_HUBDEV_H */
