/******************************************************************************
 *
 * Name:	skqueue.c
 * Project:	PCI Gigabit Ethernet Adapter
 * Version:	$Revision: 1.1.1.1 $
 * Date:	$Date: 2003/11/17 02:34:19 $
 * Purpose:	Management of an event queue.
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
 *	$Log: skqueue.c,v $
 *	Revision 1.1.1.1  2003/11/17 02:34:19  jipark
 *	AirCastle project for developing s3c2510 Solution
 *	
 *	Revision 1.14  1998/10/15 15:11:35  gklug
 *	fix: ID_sccs to SysKonnectFileId
 *	
 *	Revision 1.13  1998/09/08 08:47:52  gklug
 *	add: init level handling
 *	
 *	Revision 1.12  1998/09/08 07:43:20  gklug
 *	fix: Sirq Event function name
 *	
 *	Revision 1.11  1998/09/08 05:54:34  gklug
 *	chg: define SK_CSUM is replaced by SK_USE_CSUM
 *	
 *	Revision 1.10  1998/09/03 14:14:49  gklug
 *	add: CSUM and HWAC Eventclass and function.
 *	
 *	Revision 1.9  1998/08/19 09:50:50  gklug
 *	fix: remove struct keyword from c-code (see CCC) add typedefs
 *	
 *	Revision 1.8  1998/08/17 13:43:11  gklug
 *	chg: Parameter will be union of 64bit para, 2 times SK_U32 or SK_PTR
 *	
 *	Revision 1.7  1998/08/14 07:09:11  gklug
 *	fix: chg pAc -> pAC
 *	
 *	Revision 1.6  1998/08/11 12:13:14  gklug
 *	add: return code feature of Event service routines
 *	add: correct Error log calls
 *	
 *	Revision 1.5  1998/08/07 12:53:45  gklug
 *	fix: first compiled version
 *	
 *	Revision 1.4  1998/08/07 09:20:48  gklug
 *	adapt functions to C coding conventions.
 *	
 *	Revision 1.3  1998/08/05 11:29:32  gklug
 *	rmv: Timer event entry. Timer will queue event directly
 *	
 *	Revision 1.2  1998/07/31 11:22:40  gklug
 *	Initial version
 *	
 *	Revision 1.1  1998/07/30 15:14:01  gklug
 *	Initial version. Adapted from SMT
 *	
 *	
 *
 ******************************************************************************/


/*
	Event queue and dispatcher
*/
static const char SysKonnectFileId[] =
	"$Header: /opt/CVS/Radicalis/uClinux-dist/linux-2.4.x/drivers/net/sk98lin/skqueue.c,v 1.1.1.1 2003/11/17 02:34:19 jipark Exp $" ;

#include "h/skdrv1st.h"		/* Driver Specific Definitions */
#include "h/skqueue.h"		/* Queue Definitions */
#include "h/skdrv2nd.h"		/* Adapter Control- and Driver specific Def. */

#ifdef __C2MAN__
/*
	Event queue management.

	General Description:

 */
intro()
{}
#endif

#define PRINTF(a,b,c)

/*
 * init event queue management
 *
 * Must be called during init level 0.
 */
void	SkEventInit(
SK_AC	*pAC,	/* Adapter context */
SK_IOC	Ioc,	/* IO context */
int	Level)	/* Init level */
{
	switch (Level) {
	case SK_INIT_DATA:
		pAC->Event.EvPut = pAC->Event.EvGet = pAC->Event.EvQueue ;
		break;
	default:
		break;
	}
}

/*
 * add event to queue
 */
void	SkEventQueue(
SK_AC		*pAC,	/* Adapters context */
SK_U32		Class,	/* Event Class */
SK_U32		Event,	/* Event to be queued */
SK_EVPARA	Para)	/* Event parameter */
{
	pAC->Event.EvPut->Class = Class ;
	pAC->Event.EvPut->Event = Event ;
	pAC->Event.EvPut->Para = Para ;
	if (++pAC->Event.EvPut == &pAC->Event.EvQueue[SK_MAX_EVENT])
		pAC->Event.EvPut = pAC->Event.EvQueue ;

	if (pAC->Event.EvPut == pAC->Event.EvGet) {
		SK_ERR_LOG(pAC, SK_ERRCL_NORES, SKERR_Q_E001, SKERR_Q_E001MSG) ;
	}
}

/*
 * event dispatcher
 *	while event queue is not empty
 *		get event from queue
 *		send command to state machine
 *	end
 *	return error reported by individual Event function
 *		0 if no error occured.
 */
int	SkEventDispatcher(
SK_AC	*pAC,	/* Adapters Context */
SK_IOC	Ioc)	/* Io context */
{
	SK_EVENTELEM	*pEv ;	/* pointer into queue */
	SK_U32			Class ;
	int			Rtv ;

	pEv = pAC->Event.EvGet ;
	PRINTF("dispatch get %x put %x\n",pEv,pAC->Event.ev_put) ;
	while (pEv != pAC->Event.EvPut) {
		PRINTF("dispatch Class %d Event %d\n",pEv->Class,pEv->Event) ;
		switch(Class = pEv->Class) {
		case SKGE_DRV :		/* Driver Event */
			Rtv = SkDrvEvent(pAC,Ioc,pEv->Event,pEv->Para);
			break ;
		case SKGE_RLMT :	/* RLMT Event */
			Rtv = SkRlmtEvent(pAC,Ioc,pEv->Event,pEv->Para);
			break ;
		case SKGE_I2C :		/* I2C Event */
			Rtv = SkI2cEvent(pAC,Ioc,pEv->Event,pEv->Para);
			break ;
		case SKGE_PNMI :
			Rtv = SkPnmiEvent(pAC,Ioc,pEv->Event,pEv->Para);
			break ;
		case SKGE_HWAC :
			Rtv = SkGeSirqEvent(pAC,Ioc,pEv->Event,pEv->Para);
			break ;
#ifdef	SK_USE_CSUM
		case SKGE_CSUM :
			Rtv = SkCsEvent(pAC,Ioc,pEv->Event,pEv->Para);
			break ;
#endif	/* SK_USE_CSUM */
		default :
			SK_ERR_LOG(pAC, SK_ERRCL_SW, SKERR_Q_E002,
				SKERR_Q_E002MSG) ;
			Rtv = 0;
		}

		if (Rtv != 0) {
			return(Rtv) ;
		}

		if (++pEv == &pAC->Event.EvQueue[SK_MAX_EVENT])
			pEv = pAC->Event.EvQueue ;

		/* Renew get: it is used in queue_events to detect overruns */
		pAC->Event.EvGet = pEv;
	}

	return(0) ;
}

/* End of file */
