/******************************************************************************
 *
 * Name:	sktimer.h
 * Project:	Genesis, PCI Gigabit Ethernet Adapter
 * Version:	$Revision: 1.1.1.1 $
 * Date:	$Date: 2003/11/17 02:34:19 $
 * Purpose:	Defines for the timer functions
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	(C)Copyright 1989-1998 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *	All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SYSKONNECT
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	This Module contains Proprietary Information of SysKonnect
 *	and should be treated as Confidential.
 *
 *	The information in this file is provided for the exclusive use of
 *	the licensees of SysKonnect.
 *	Such users have the right to use, modify, and incorporate this code
 *	into products for purposes authorized by the license agreement
 *	provided they include this notice and the associated copyright notice
 *	with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * History:
 *
 *	$Log: sktimer.h,v $
 *	Revision 1.1.1.1  2003/11/17 02:34:19  jipark
 *	AirCastle project for developing s3c2510 Solution
 *	
 *	Revision 1.8  1998/09/08 08:48:02  gklug
 *	add: init level handling
 *	
 *	Revision 1.7  1998/08/20 12:31:29  gklug
 *	fix: SK_TIMCTRL needs to be defined
 *	
 *	Revision 1.6  1998/08/19 09:51:00  gklug
 *	fix: remove struct keyword from c-code (see CCC) add typedefs
 *	
 *	Revision 1.5  1998/08/17 13:43:21  gklug
 *	chg: Parameter will be union of 64bit para, 2 times SK_U32 or SK_PTR
 *	
 *	Revision 1.4  1998/08/14 07:09:31  gklug
 *	fix: chg pAc -> pAC
 *	
 *	Revision 1.3  1998/08/07 12:54:24  gklug
 *	fix: first compiled version
 *	
 *	Revision 1.2  1998/08/07 09:35:29  gklug
 *	add: Timer control struct for Adapters context
 *	add: function prototypes
 *	
 *	Revision 1.1  1998/08/05 11:27:01  gklug
 *	First version: adapted from SMT
 *	
 *
 ******************************************************************************/

/*
 * SKTIMER.H	contains all defines and types for the timer functions
 */

#ifndef	_SKTIMER_H_
#define _SKTIMER_H_

#include "h/skqueue.h"

/*
 * SK timer
 * - needed wherever a timer is used. Put this in your data structure
 *   wherever you want.
 */
typedef	struct s_Timer SK_TIMER;

struct s_Timer {
	SK_TIMER	*TmNext ;	/* linked list */
	SK_U32		TmClass ;	/* Timer Event class */
	SK_U32		TmEvent ;	/* Timer Event value */
	SK_EVPARA	TmPara ;	/* Timer Event parameter */
	SK_U32		TmDelta ;	/* delta time */
	int		TmActive ;	/* flag : active/inactive */
} ;

/*
 * Timer control struct.
 * - use in Adapters context name pAC->Tim
 */
typedef	struct s_TimCtrl {
	SK_TIMER	*StQueue ;	/* Head of Timer queue */
} SK_TIMCTRL ;

extern void SkTimerInit(SK_AC *pAC,SK_IOC Ioc, int Level);
extern void SkTimerStop(SK_AC *pAC,SK_IOC Ioc,SK_TIMER *pTimer);
extern void SkTimerStart(SK_AC *pAC,SK_IOC Ioc,SK_TIMER *pTimer,
	SK_U32 Time,SK_U32 Class,SK_U32 Event,SK_EVPARA Para);
extern void SkTimerDone(SK_AC *pAC,SK_IOC Ioc);
#endif	/* _SKTIMER_H_ */
