/*
 * pci_timers.h  maded by Roh you-chang
 *
 * Copyright (C) 2002 Samsung electonics Inc. 
 *
 * This file includes the 32-bit timers definitions 
 * for PCI fuction of the S3C2510X RISC microcontroller
 *
 */


#define TIMER0      0 
#define TIMER1      1
#define TIMER2      2
#define TIMER3      3
#define TIMER4      4
#define TIMER5      5

#define nTIMER0     29
#define nTIMER1     30  
#define nTIMER2     31
#define nTIMER3     32
#define nTIMER4     33
#define nTIMER5     34


#define TTIC *(volatile unsigned int*)(IC) // R/W Interrupt
#define TTMOD *(volatile unsigned int*)(TMOD) 
#define TTDATA(channel) *(volatile unsigned int*)(TDATA0 + channel*0x8)  
#define TTCNT(channel) *(volatile unsigned int*)(TCNT0 + channel*0x8) 
#define WATCHDOG *(volatile unsigned int*)(WDT) 

#define TM_RUN(device) (0x1<<(device*4))
#define TM_TOGGLE(device) (0x2<<(device*4))
#define TM_OUT_AS_1(device) (0x4<<(device*4))


#define TM_IN_CLR(device) (0x1<<(device+1))

#define TimerInterruptClear(device) (TTIC = TM_IN_CLR(device))

//=================================================================
// Usable Macros Functions
//=================================================================

#define TimerStop(device)               (TTMOD &= ~TM_RUN(device))
#define TimerStart(device)              (TTMOD |= TM_RUN(device))
#define TimerInterval(device)           (TTMOD &= ~TM_TOGGLE(device))
#define TimerToggle(device)             (TTMOD |= TM_TOGGLE(device))
#define TimerToutAs0(device)            (TTMOD &= ~TM_OUT_AS_1(device))
#define TimerToutAs1(device)            (TTMOD  |= TM_OUT_AS_1(device))

