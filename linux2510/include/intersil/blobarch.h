/*
 *      IMPORTANT--READ CAREFULLY: The use, reproduction, modification, and
 *      creation of derivative works of this Source Code ("Document") is
 *      governed by a TECHNOLOGY LICENSE AGREEMENT("Agreement"), a legal
 *      agreement between you (either an individual or a single entity) and
 *      No Wires Needed B.V. ("NWN").  By reproducing, modifying, creating
 *      derivative works, accepting, installing, viewing, accessing,
 *      transforming or otherwise using this Document, you agree to be bound
 *      by the terms of the Agreement.  If you do not agree to the terms of
 *      the Agreement, NWN is unwilling to license the rights to use,
 *      reproduce, modify, and create derivative works of this Document to you.
 *      In such event, you may not use, reproduce, modify, create derivative
 *      works, install, view, access, transport, transform or circulate this
 *      Document, and you should promptly contact NWN or the licensor from
 *      which you acquired this Document for instructions on returning or
 *      destroying this Document.
 *
 *      CONFIDENTIAL, PROPRIETARY AND TRADE SECRET INFORMATION:  This Source
 *      Code software contains confidential, proprietary and trade secret
 *      information of No Wires Needed B.V. and should be treated in a
 *      confidential manner.  Furthermore, this Source Code software is
 *      protected by United States copyright laws and international treaties.
 *      Unauthorized use, reproduction, modification, creation of derivative
 *      works or circulation is strictly prohibited.
 *
 *      Copyright (C) 1994-2000 by No Wires Needed B.V. (NWN), The Netherlands,
 *      ALL RIGHTS RESERVED
 *
 */

#ifndef __BLOBARCH_H__
#define __BLOBARCH_H__

/*
 * Blob Information Structure
 */
#define BIS_CME                 0x0000
#define BIS_DEVS                0x0001
#define BIS_DEVDESCR            0x0002
#define BIS_END                 0xFFFF

/*
 * BIS Configuration Element size
 */
#define BIS_CME_SIZE    128

/*
 * BIS Device type definitions
 */
#define BIS_DT_DEBUG            0
#define BIS_DT_802DOT11         1
#define BIS_DT_802DOT3          2
#define BIS_DT_PCMCIA           3
#define BIS_DT_PCI_CB           4
#define BIS_DT_BLUETOOTH        5
#define BIS_DT_UART             6

/*
 * Definitions
 */

/* Error Codes */
#define BER_NONE                0
#define BER_UNSPEC              -1
#define BER_UNSUP               -2
#define BER_CONF                -3
#define BER_MEM                 -4
#define BER_TIMEOUT             -5
#define BER_FRAME               -6
#define BER_OIDUNK              -7
#define BER_OVERFLOW            -8
#define BER_DSTUNKN             -9

/* Operations on Objects */
#define OPGET                   0
#define OPSET                   1
#define OPTRAP                  2

/* Frame State */
#define FSTXQUEUED              0
#define FSTXFRAGMENTED          1
#define FSTXNOTQUEUED           2
#define FSTXREJECTED            3
#define FSTXFAILED              4
#define FSTXSUCCESSFUL          5
#define FSRXFAILED              6
#define FSRXSUCCESSFUL          7
#define FSADDED                 8
#define FSRETURNED              9

/* Requests */
#define REQSERVICE              0x0001
#define REQTRAP                 0x0002

/* Requests for frame devices */
#define REQFRAMERX              0x0004
#define REQFRAMEADD             0x0008
#define REQFRAMERETURN          0x0010

/* Alternate requests for data devices */
#define REQDATAPENDING          0x0004
#define REQDATASENT             0x0008

/* Misc requests */
#define REQWATCHDOG             0x1000

/* State */
#define STSTOPPED               0
#define STREADY                 1
#define STRUNNING               2

/* Device Modes */
#define MODE_NONE               -1
#define MODE_UNKNOWN            -1
#define MODE_PROMISCUOUS        0
#define MODE_NORMAL             1
#define MODE_CLIENT             1
#define MODE_AP                 2

#endif /* __BLOBARCH_H__ */
