/*
 *  made by Roh you-chang(terius90@samsung.com)
 *  
 *  Copyright (C) 2002-2004 SAMSUNG ELECTRONIS
 *
 *       
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/arch/hardware.h>

#define GDMAPENDING_CLR 0x1 
#define gBUSCLK 0
#define BEL_MESSAGE 0x7fffffff
#define PDMA_CH0 0x0
#define PDMA_CH1 0x1

//#define DEBUG  //ryc--

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif
#define NUM_OF_PCISFR 46

static s_SFR pci_sfr[NUM_OF_PCISFR] = {
	{ (void *)0xf0110000,(char*)"PCIHID" , (u32)0xa510144d, (u32)0 },
	{ (void *)0xf0110004, (char*)"PCIHSC"  ,   (u32)0x02b00000, (u32)0 },
	{ (void *)0xf0110008, (char*)"PCIHCODE",   (u32)0x0d800001, (u32)0 },
	{ (void *)0xf011000c, (char*)"PCIHLINE",  (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110010, (char*)"PCIHBAR0" ,   (u32)0x00000008, (u32)0 },
	{ (void *)0xf0110014, (char*)"PCIHBAR1" ,   (u32)0x00000008, (u32)0 },
	{ (void *)0xf0110018, (char*)"PCIHBAR2",    (u32)0x00000001, (u32)0 },
	{ (void *)0xf011002c, (char*)"PCIHSSID", (u32)0xa510144d, (u32)0 },
	{ (void *)0xf0110034, (char*)"PCIHCAP"  ,  (u32)0x000000dc, (u32)0 },
	{ (void *)0xf011003c, (char*)"rPCIHLTIT",  (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110040, (char*)"rPCIHTIMER"  ,  (u32)0x00008080, (u32)0 },
	{ (void *)0xf01100dc, (char*)"rPCIHPMR0"  ,  (u32)0x7e020001, (u32)0 },
	{ (void *)0xf01100e0, (char*)"rPCIHPMR1"  ,  (u32)0x7e020001, (u32)0 },
	{ (void *)0xf0110100, (char*)"PCICON"   , (u32)0x000000b1, (u32)0 },
	{ (void *)0xf0110104, (char*)"PCISET"  , (u32)0x00000400, (u32)0 },
	{ (void *)0xf0110108, (char*)"PCIINTEN",  (u32)0x0, (u32)0 },
	{ (void *)0xf011010c, (char*)"PCIINTST",  (u32)0x0, (u32)0 },
	{ (void *)0xf0110110, (char*)"PCIINTAD",  (u32)0x0, (u32)0 },
	{ (void *)0xf0110114, (char*)"PCIBATAPM", (u32)0x0, (u32)0 },
	{ (void *)0xf0110118, (char*)"PCIBATAPI", (u32)0x0, (u32)0 },
	{ (void *)0xf011011C, (char*)"PCIRCC"  , (u32)0x4000000c, (u32)0 },
	{ (void *)0xf0110120, (char*)"PCIDIAG0"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110124, (char*)"PCIDIAG1"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110128, (char*)"PCIBELAP"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf011012c, (char*)"PCIBELPA"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110130, (char*)"PCIMAIL0"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110134, (char*)"PCIMAIL1"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110138, (char*)"PCIMAIL2"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf011013c, (char*)"PCIMAIL3"  , (u32)0x00000000, (u32)0 },
	{ (void *)0xf0110140, (char*)"PCIBATPA0", (u32)0x0, (u32)0 },
	{ (void *)0xf0110144, (char*)"PCIBAM0" ,  (u32)0xffff0000, (u32)0 },
	{ (void *)0xf0110148, (char*)"PCIBATPA1", (u32)0xf0110000, (u32)0 },
	{ (void *)0xf011014c, (char*)"PCIBAM1",   (u32)0xfffffe00, (u32)0 },
	{ (void *)0xf0110150, (char*)"PCIBATPA2", (u32)0xf0110100, (u32)0 },
	{ (void *)0xf0110154, (char*)"PCIBAM2",	(u32)0xffffff00, (u32)0 },
	{ (void *)0xf0110158, (char*)"PCISWAP",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf0110180, (char*)"PDMACON0",	(u32)0x00001000, (u32)0 },
	{ (void *)0xf0110184, (char*)"PDMASRC0",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf0110188, (char*)"PDMADST0",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf011018c, (char*)"PDMACNT0",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf0110190, (char*)"PDMARUN0",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf01101a0, (char*)"PDMACON1",	(u32)0x00001000, (u32)0 },
	{ (void *)0xf01101a4, (char*)"PDMASRC1",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf01101a8, (char*)"PDMADST1",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf01101ac, (char*)"PDMACNT1",	(u32)0x00000000, (u32)0 },
	{ (void *)0xf01101b0, (char*)"PDMARUN1",	(u32)0x00000000, (u32)0 }
};

unsigned long
PCIMakeConfigAddress(u32 bus, u32 device, u32 function, u32 offset)
{
	u32 address=0;

	if(bus == 0)
	{
		if(device != 0)
		{
			address = AHB_ADDR_PCI_CFG0(device, function, offset);
		}else{
			address = 0xf0110000;
			address |= offset & 0xff;
		}
	}else{
			address = AHB_ADDR_PCI_CFG1(bus, device, function, offset);
	}
	return address;
}

int
smdk2510_pci_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	unsigned int devfn = dev->devfn;
	u32 slot,func;
	u8* v;
	
	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	v = (u8*)PCIMakeConfigAddress(dev->bus->number, slot, func, where);
	*value = *v;

	return PCIBIOS_SUCCESSFUL;
}

int
smdk2510_pci_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	unsigned int devfn = dev->devfn;
	u32 slot,func;
	u16* v;

	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	
	v = (u16*)PCIMakeConfigAddress(dev->bus->number, slot, func, where);	
	*value = *v;

	return PCIBIOS_SUCCESSFUL;
}

int
smdk2510_pci_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	unsigned int devfn = dev->devfn;
	u32 slot,func;
	u32* v;

	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	v = (u32*)PCIMakeConfigAddress(dev->bus->number, slot, func, where);	
	*value = *v;

	return PCIBIOS_SUCCESSFUL;
}

int
smdk2510_pci_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	
	unsigned int devfn = dev->devfn;
	u32 slot,func;
	u8* v;

	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	v = (u8*)PCIMakeConfigAddress(dev->bus->number, slot, func, where);	
	*v = value;

	return PCIBIOS_SUCCESSFUL;
}

int
smdk2510_pci_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	unsigned int devfn = dev->devfn;
	u32 slot,func;
	u16* v;

	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	v = (u16*)PCIMakeConfigAddress(dev->bus->number, slot, func, where);	
	*v = value;

	return PCIBIOS_SUCCESSFUL;
}

int
smdk2510_pci_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	unsigned int devfn = dev->devfn;
	u32 slot,func;
	u32* v;

	slot = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	v = (u32*)PCIMakeConfigAddress(dev->bus->number, slot, func, where);	
	*v = value;

	return PCIBIOS_SUCCESSFUL;
}
static struct pci_ops smdk2510_pci_ops = {
	smdk2510_pci_read_config_byte,
	smdk2510_pci_read_config_word,
	smdk2510_pci_read_config_dword,
	smdk2510_pci_write_config_byte,
	smdk2510_pci_write_config_word,
	smdk2510_pci_write_config_dword,
};

void __init PCI_SfrStore(void)
{
	int i;
				  
	   	for(i=0;i<NUM_OF_PCISFR;i++) 
		{ 
			pci_sfr[i].InitializedValue = *(u32 *)(pci_sfr[i].addr);
			//DBG(" %10x(%10s)  %10x\n",pci_sfr[i].addr,pci_sfr[i].name,pci_sfr[i].InitializedValue);
		}
	
}

void __init PCI_Delay(unsigned long x)
{
	while(--x);
}

void ISR_PCI_Timer_NormalOn(void)
{
	static unsigned long timer_count = 0x0;
	unsigned long pciIopData = 0xfe;

	if(timer_count >= 0x8)
	{
		timer_count = 0x0;
		pciIopData = (pciIopData<<1);
	}
	else
		pciIopData = (pciIopData<<1)+1;

	IIOPDATA1 = pciIopData;
	timer_count++;
#ifdef __PCI_WATCHDOG_RESET	 // currently disabled
	if(pciIsPciSetup)	WATCHDOG=0xe0004000;
#endif // __PCI_WATCHDOG_RESET	
}

void PCI_PDMA_View(u8 ch)
{
 	DBG("\n---PCI_PDMA[%d]_View-------------------------------------   ",ch);

	if(sPDMACON(ch).RE)
	 	DBG("\n[0]PDMA Running... 	");

	if(sPDMACON(ch).IRP)
		DBG("\n[1]Route INT to INTA# 	");
	if(sPDMACON(ch).BSE)
	 	DBG("\n[2]Byte Swap (PCI to PCI, word boudary)	");
	if(sPDMACON(ch).S)
	{
		DBG("\n[4]Source Direction : PCI	");
	 	DBG("\n[11:8]Src Commnad : %s",((sPDMACON(ch).SCM>>1)==0x3) ? "Mem" : "I/O" );
	}
	else 	
	 	DBG("\n[4]Source Direction : AHB 	");

	if(sPDMACON(ch).D)
	{
		DBG("\n[5]Dst Direction : PCI	");
	 	DBG("\n[11:8]Dst Commnad : %s",((sPDMACON(ch).DCM>>1)==0x3) ? "Mem" : "I/O" );
	}
	else 	
	 	DBG("\n[5]Dst Direction : AHB 	");

	if(sPDMACON(ch).SAF)
	 	DBG("\n[6]Src Addr Fixed 	");

	if(sPDMACON(ch).DAF)
	 	DBG("\n[7]Dst Addr Fixed 	");


	if(sPDMACON(ch).ERR)
		{
	 	DBG("\n[0]Done w/ Error 	");
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR
		}
	if(sPDMACON(ch).PRG)
	 	DBG("\n[0]PDMA Programmed by PCI	");
	if(sPDMACON(ch).BSY)
	 	DBG("\n[0]PDMA[%x] Running... 	",ch);

	 	DBG("\nPDMA Src[%x] Addr 		: %8x", ch, rPDMASRC(ch));
	 	DBG("\nPDMA Dst[%x] Addr 		: %8x", ch, rPDMADST(ch));
	 	DBG("\nPDMA Transfer Byte Count[%x] : %8x", ch, rPDMACNT(ch));

 	DBG("\n");
 	
}
void PCI_PCIINTST_View(void)
{
    union { u32 pci_32;  s_PCIINTST s; } uPciIntSt;
	unsigned long pciBellMessage;
	u32 pciExerAddr,pciExerCmd,pciExerIntAddr,pciExerNod; //ryc++
	u32 pciPdma0Available=1;

	uPciIntSt.pci_32 = rPCIINTST;
// Radicalis_hans_begin (03.12.26) - will be done in umask_irq
//	rPCIINTST = uPciIntSt.pci_32;
// Radicalis_hans_end (03.12.26) - will be done in umask_irq

 	DBG("\n---PCI_PCIINTST_View-------------------------------------	");
 
	if(uPciIntSt.s.PRD)
		{
	 	DBG("\n[0]PCI Reset DeAsseerted	");
		}	 	
	if(uPciIntSt.s.PRA)
		{	
		DBG("\n[1]PCI Reset Asserted ,	");
		}
	if(uPciIntSt.s.MFE)
		{
		DBG("\n[2]Master Fatal Error , PCIINTAD[0x%lx]",rPCIINTAD);
#ifdef __PCI_STOP_AT_ERROR
		//GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR 
		}
	if(uPciIntSt.s.MPE)
	{
		printk("\n[3]Master Parity Error , PCIINTAD[0x%lx]",rPCIINTAD);
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR
	}		
	if(uPciIntSt.s.TPE)
	{
		printk("\n[4]Target Parity Error ,	");
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR
	}

	if(uPciIntSt.s.PME && HostMode())
	{
		DBG("\n[5]PME# Asserted  ,	");
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR 
	}
	if(uPciIntSt.s.PME && AgentMode())
		DBG("\n[5]Change of PME_enbale bit 0 to 1 in the PMCSR ,	");
		
	if(uPciIntSt.s.PMC && AgentMode())
		DBG("\n[6]PME_Status bit is cleard ,	");
	if(uPciIntSt.s.PSC && AgentMode())
		DBG("\n[7]Power Status Changed ,	");
	if(uPciIntSt.s.BPA && sPCIINTEN.BPA)
	{
		DBG("\n[8]Door Bell to AHB ,	");
		pciBellMessage = rPCIBELPA & BEL_MESSAGE;
		switch(pciBellMessage)
		{
			case 1: //EXERCISER_START:
					pciExerAddr = rPCIMAIL0;
					pciExerCmd = rPCIMAIL1;
					pciExerIntAddr = rPCIMAIL2;
					pciExerNod = rPCIMAIL3;
					DBG("\n PCI Bell Message [EXERCISER_START]\n pciExerAddr[0x%x]\n pciExerCmd[0x%x]\n pciExerIntAddr[0x%x]\n pciExerNod[0x%x]", pciExerAddr, pciExerCmd, pciExerIntAddr, pciExerNod);
					
					break;
					
			case 2: //EXERCISER_STOP:
					DBG("\n Bell Message [EXERCISER_STOP]");
					break;
					
			default : DBG("\nUnknown Door Bell Message[%x]",pciBellMessage);
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR
		}
		rPCIBELPA = rPCIMAIL0 = rPCIMAIL1 = rPCIMAIL2 = rPCIMAIL3 =0;
	}
	if(uPciIntSt.s.SER)
	{
			DBG("\n[9]SERR# Asserted  ,	");
	}
	if(uPciIntSt.s.INA)
	{
		DBG("\n[10]INTA# Asserted  ,	");
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR 
	}
	if(uPciIntSt.s.DM0)
	{
		DBG("\n[12]PDMA0 Done ,	");
		pciPdma0Available = 1;

		PCI_PDMA_View(PDMA_CH0);
	}
	
	if(uPciIntSt.s.DE0)
	{
		printk("\n[13]PDMA0 Error  ,	");
		PCI_PDMA_View(PDMA_CH0);
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR

	}
	
	if(uPciIntSt.s.DM1)
	{
		DBG("\n[14]PDMA1 Done ,	");

		PCI_PDMA_View(PDMA_CH1);
	}
	
	if(uPciIntSt.s.DE1)
	{
		printk("\n[15]PDMA1 Error ,	");
		PCI_PDMA_View(PDMA_CH1);
#ifdef __PCI_STOP_AT_ERROR
		GlobalDis_Int(); GlobalEn_Int();
#endif //__PCI_STOP_AT_ERROR
	}

	if(uPciIntSt.s.AER)
		printk("\n[16]AHB Error Response  ,	");
	if(uPciIntSt.s.RDE)
		//printk("\n[30]Read Error ,	");
	if(uPciIntSt.s.WRE)
		printk("\n[31]Write Error ,	");
 	DBG("\n------------------------------------------------------	");

}
void PCI_PCISCR_View(void)
{
 	DBG("\n---PCI_PCISCR_View------------------------------------	");
	if(sPCIHCMD.IOE)
	 	DBG("\n[0]I/O Space 	");
	if(sPCIHCMD.MME)
		DBG("\n[1]Memory Space,	");
	if(sPCIHCMD.BME)
		DBG("\n[2]Bus Master	");
	if(sPCIHCMD.SCE)
		DBG("\n[3]Special Cycle ,	");
	if(sPCIHCMD.MWI)
		DBG("\n[4]Memory Write and Invalidate,	");
	if(sPCIHCMD.VGA)
		DBG("\n[5]VGA palette Snoop	");
	if(sPCIHCMD.PEE)
		DBG("\n[6]Parity Error Response,	");
	if(sPCIHCMD.STC)
		DBG("\n[7]Stepping Control ,	");
	if(sPCIHCMD.SER)
		DBG("\n[8]SERR# Enable ,	");
	if(sPCIHCMD.FBE)
		DBG("\n[9]Fast Back to Back Enable  ,	");
	if(sPCIHCMD.CAP)
		DBG("\n[4] Capabilities List	");
	if(sPCIHCMD.M66)
		DBG("\n[5]66MHz Capable ,	");
	if(sPCIHCMD.FBC)
		DBG("\n[7]Fast Back to Back Capable  ,	");
	if(sPCIHCMD.MPE)
		DBG("\n[8]Master Data Parity Error  ,	");
	if(sPCIHCMD.DST==0)
		DBG("\n[9] DEVSEL = Fast,	");
	else if(sPCIHCMD.DST==1)
		DBG("\n[9] DEVSEL = Medium,	");
	else 	if(sPCIHCMD.DST==2)
		DBG("\n[9] DEVSEL = Slow,	");
	if(sPCIHCMD.STA)
		DBG("\n[11]Signaled Target Abort  ,	");
	if(sPCIHCMD.RTA)
		DBG("\n[12]Received Target Abort ,	");
	if(sPCIHCMD.RMA)
		DBG("\n[13]Received Master Abort ,	");
	if(sPCIHCMD.SSE)
		DBG("\n[14]Signaled System Error ,	");
	if(sPCIHCMD.DPE)
		DBG("\n[15]Detected Parity Error ,	");
 	DBG("\n------------------------------------------------------	");
}

void ISR_Timer_NormalOn(void)
{
	static unsigned long timer_count = 0x0;

	if(timer_count >= 0x8) timer_count = 0x0;
	if(timer_count == 0x0) IIOPDATA1 = 0x7e;
	else if(timer_count == 0x1) IIOPDATA1 = 0xbd;
	else if(timer_count == 0x2)	IIOPDATA1 = 0xdb;
	else if(timer_count == 0x3) IIOPDATA1 = 0xe7;
	else if(timer_count == 0x4) IIOPDATA1 = 0xe7;
	else if(timer_count == 0x5) IIOPDATA1 = 0xdb;
	else if(timer_count == 0x6) IIOPDATA1 = 0xbd;
	else IIOPDATA1 = 0x7e;
	timer_count++;
}
/*
static void ISR_PCI_Timer5_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{TimerInterruptClear(TIMER5);   ISR_PCI_Timer_NormalOn(); }
*/

static void	ISR_PCI(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef DEBUG
 	PCI_PCISCR_View(); 
#endif
	PCI_PCIINTST_View();
	rPCIHSC |=  0xffff0000;
 
	//PCI_PCIINTEN_View();
}
static void ISR_Timer0_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{ TimerInterruptClear(TIMER0); ISR_Timer_NormalOn();}
static void ISR_Timer1_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{ TimerInterruptClear(TIMER1); ISR_Timer_NormalOn();}
static void ISR_Timer2_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{ TimerInterruptClear(TIMER2); ISR_Timer_NormalOn();}
static void ISR_Timer3_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{ TimerInterruptClear(TIMER3); ISR_Timer_NormalOn();}
static void ISR_Timer4_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{ TimerInterruptClear(TIMER4); ISR_Timer_NormalOn();}
static void ISR_Timer5_NormalOn(int irq, void *dev_id, struct pt_regs *regs) 
{ TimerInterruptClear(TIMER5); ISR_Timer_NormalOn();}


void TimerData(unsigned long device, unsigned long msec)
{
    TTDATA(device) = gBUSCLK/1000*msec;
}

void TimerNormalOn(unsigned long device, unsigned long msec)
{
    switch(device)
    {
        case 0: request_irq(nTIMER0, &ISR_Timer0_NormalOn,(u32)NULL,"ISR_TIMER0",NULL);  break;
        case 1: request_irq(nTIMER1, &ISR_Timer1_NormalOn,(u32)NULL,"ISR_TIMER1",NULL);  break;
        case 2: request_irq(nTIMER2, &ISR_Timer2_NormalOn,(u32)NULL,"ISR_TIMER2",NULL);  break;
        case 3: request_irq(nTIMER3, &ISR_Timer3_NormalOn,(u32)NULL,"ISR_TIMER3",NULL);  break;
        case 4: request_irq(nTIMER4, &ISR_Timer4_NormalOn,(u32)NULL,"ISR_TIMER4",NULL);  break;
        case 5: request_irq(nTIMER5, &ISR_Timer5_NormalOn,(u32)NULL,"ISR_TIMER5",NULL);  break;
        default: request_irq(nTIMER5, &ISR_Timer5_NormalOn,(u32)NULL,"ISR_TIMER5",NULL);  break;
    }

    TimerInterval(device);
    TimerData(device, msec);
    TimerStart(device);

    switch(device)
    {
        case 0: Enable_Int(INT_IRQ_TIMER0);     break;
        case 1: Enable_Int(INT_IRQ_TIMER1);     break;
        case 2: Enable_Int(INT_IRQ_TIMER2);     break;
        case 3: Enable_Int(INT_IRQ_TIMER3);     break;
        case 4: Enable_Int(INT_IRQ_TIMER4);     break;
        case 5: Enable_Int(INT_IRQ_TIMER5);     break;
        default:    Enable_Int(INT_IRQ_TIMER5);     break;
    }

    GlobalEn_Int();
}

//ryc----------------------------------
void	ISR_Gdma0ForPCItest(int irq, void *dev_id, struct pt_regs *regs)
{
	DBG("\n** ISR_Gdma0 performed.") ;
	DIPR(0)=GDMAPENDING_CLR;
	DBG("\nDSAR0[0x%x], DDAR0[0x%x], DTCR0[0x%x]", DSAR(0), DDAR(0), DTCR(0));
}
void	ISR_Gdma1ForPCItest(int irq, void *dev_id, struct pt_regs *regs)
{

	DBG("\n** ISR_Gdma1 performed.") ;
	DIPR(1)=GDMAPENDING_CLR;
	DBG("\nDSAR1[0x%x], DDAR1[0x%x], DTCR1[0x%x]", DSAR(1), DDAR(1), DTCR(1));

 }
void	ISR_Gdma2ForPCItest(int irq, void *dev_id, struct pt_regs *regs)
{
	DBG("\n** ISR_Gdma2 performed.") ;
	DIPR(2)=GDMAPENDING_CLR;
	DBG("\nDSAR2[0x%x], DDAR2[0x%x], DTCR2[0x%x]", DSAR(2), DDAR(2), DTCR(2));
}
void	ISR_Gdma3ForPCItest(int irq, void *dev_id, struct pt_regs *regs)
{
	DBG("\n** ISR_Gdma3 performed.") ;
	DIPR(3)=GDMAPENDING_CLR;
	DBG("\nDSAR3[0x%x], DDAR3[0x%x], DTCR3[0x%x]", DSAR(3), DDAR(3), DTCR(3));
}
void	ISR_Gdma4ForPCItest(int irq, void *dev_id, struct pt_regs *regs)
{
	DBG("\n** ISR_Gdma4 performed.") ;
	DIPR(4) = GDMAPENDING_CLR;
	DBG("\nDSAR4[0x%x], DDAR4[0x%x], DTCR4[0x%x]", DSAR(4), DDAR(4), DTCR(4));
}
void	ISR_Gdma5ForPCItest(int irq, void *dev_id, struct pt_regs *regs)
{
	DBG("\n** ISR_Gdma5 performed.") ;
	DIPR(5) = GDMAPENDING_CLR;
	DBG("\nDSAR5[0x%x], DDAR5[0x%x], DTCR5[0x%x]", DSAR(5), DDAR(5), DTCR(5));
}

void PCI_SFR_Register_Display(void)
{
	u32 offset,i;

	printk("PCI_SFR_Register_Display\n");
	printk("-----------------------------\n");
	for(offset =0,i=0 ; offset < 0xe4 ; offset+=4,i++){
		if(!(i%4)) printk("\n");
		printk("[0x%2.2ux] = %8.2lux", offset,r32(PCIHID + offset));
	}
	printk("\n-----------------------------\n");
}
void PCI_BIF_Register_Display(void)
{
	u32 offset,i;

	printk("\nPCI_BIF_Register_Display\n");
	printk("\n-----------------------------\n");
	for(offset =0,i=0 ; offset < 0xb4 ; offset+=4,i++){
		if(!(i%4)) printk("\n");
		printk("[0x%2.2lx] = %8.2lx", offset,r32(PCICON + offset));
	}
	printk("\n-----------------------------\n");
}
/* ryc++ for using external clock */
//#define __PCI_EXT_CLK 1
void __init SMDK2510_PCI_Setup(void)
{

		/* Disable PCI interrupt enable registers */
		rPCIINTEN = 0x0; 

		/* we don't user External Arbitor,External clock,Externel PCI reset */	
#ifndef __PCI_EXT_ARB
		rPCICON |= PCIARBITOR_INT;
#else
		DBG("External Arbitor set\n");
#endif

#ifdef __PCI_EXT_CLK
		rPCIDIAG0 |= PCICLK_EXT;
		printk("External Clock is used !!!\n");
#else
		rPCIRCC |= PCICLK_33M;
		DBG("\nInternal Clock. PCIRCC.M33[%x], SYSCFG[%x], SPLL[%x],UPLL[%x]",sPCIRCC.M33, SYSCFG,SPLL,UPLL);
#endif

#ifdef __PCI_EXT_RST
		rPCIDIAG0 |= PCIRESET_EXT;
		DBG("   External PCI Reset\n");
#endif


#ifdef __PCI_EXT_PULLUP
		rPCIDIAG0 |= PCIPULLUP_EXT;
		DBG("   External PCI Pull-up\n");
#endif

		rPCIRCC &= ~PCIRESETCLK_MASK;

		rPCIRCC |= PCILOG_RESET;
		rPCIRCC |= PCIBUS_RESET;

	// Radicalis_hans_begin (03.12.22)
	//	- old PCI card like realtek ethernet card uses just single mode configuration data transfer
	rPCISET &= 0x11110011;
	// Radicalis_hans_end (03.12.22)

	PCI_Delay(13300000);    // to wait for target to ready	

	/* default value(0x0d8000) was for wireless controller, so ryc changed 
	 * the class code field to set Host bridge(0x060000).
	 */
	rPCIHCODE = 0x06000001; // set Host bridge
	// Initialize PCI Configuration space registers
	rPCIHCMD |= PCIBUSMASTER_ENABLE + PCIMWI_ENABLE + PCIPERR_RESPONSE_ENABLE 
				+ (AgentMode()? PCISERR_ENABLE:0);

// Radicalis_hans_begin (03.12.24) - to use ATS =1 setting
#if 1
	rPCIBATAPM = 0xc0000000;	// PCI memory space in AHB bus
	rPCIBATAPI = 0xdc000000;	// PCI IO space in AHB bus

	rPCIHBAR0  = 0x00000000 | 0x8;	// Memory region
	rPCIBAM0   = 0xfe000000;	// 32M window for physical sdram on AHB
	rPCIBATPA0 = 0x00000000;	// starting from AHB address 0
	
	rPCIHBAR1  = 0x00000000 | 0x8;	// Memory region
	rPCIBAM1   = 0xfffffe00;	// Reset value
	rPCIBATPA1 = 0xf0110000;	// Reset value (starting from AHB address 0xf0110000)
	
	rPCIHBAR2  = 0x00000000 | 0x1;	// IO region
	rPCIBAM2   = 0xfe000000;	// 256Bytes window for physical sdram on AHB
	rPCIBATPA2 = 0x00000000;	// starting from AHB address 0

	sPCICON.ATS = 1;		// Fixed PCI to AHB mapping
#endif
// Radicalis_hans_end (03.12.24)

	rPCIBAM0 = 0xfe000000;
	/* ryc-- for reset problem 
	 * 	this setting should set in prism2_hw_init function(hostap_hw.c)
	 */
#ifndef CONFIG_HOSTAP
	rPCIINTEN = 0xffffffff; //all enable for viewing status. 20031113
#endif
	rPCICON |= PCICONFIG_DONE + PCISYSTEMREAD_READY;

	/* Memory Burst Read enable(Burst Read decrease its performance).*/
	sPCICON.MMP = 1;

	rPCIHSTS |= PCISTATUS_ALL; // rPCISCR All status clear

	// rPCIINTST All status clear
	rPCIINTST = ALL_WRITE1CLR;
	sPCIBELPA.BEL = WRITE0CLR;

	/* ryc-- for reset problem 
	 * 	this setting should set in prism2_hw_init function(hostap_hw.c)
	 */
#ifndef CONFIG_HOSTAP
	Enable_Int(INT_IRQ_PCI_PCCARD); // 20031113
#endif
	printk("S3C2510 PCI host initialized\n");
}

/* Initialization routine for S3C2510 chipset */
void __init pci_host_controller_init(void)
{
	printk("S3C2510 PCI Host Controller Initializing...\n");

	/* check mode */
	if(PCIMode()) /* Check if PCI host mode or not */
		SMDK2510_PCI_Setup();


}

void __init PCI_SfrDisplay(void)
{
	u32 i,pciData;

	DBG("SFR addr(Name)  Current_Value\n");
	for(i=0;i<NUM_OF_PCISFR;i++)
	{
		pciData = *(u32*)(pci_sfr[i].addr);
		DBG("%10x(%10s)    %10x\n",pci_sfr[i].addr,pci_sfr[i].name
				,pci_sfr[i].ResetValue);
	}
}
 /*
  *  pci host initialize & scan bus routine. 
  */ 
void __init smdk2510_pci_init(void *sysdata)
{
	u32 dummy;

	dummy = 0xa510;
	/* Initialize S3C2510 chipset for PCI host operation */
	pci_host_controller_init();

	/* kernel scan bus for detecting devices */
	pci_scan_bus(0,&smdk2510_pci_ops,sysdata);

	if(request_irq(SRC_IRQ_PCI_PCCARD,&ISR_PCI, SA_SHIRQ,"ISR_PCI", &dummy)) {
		printk("Unable to get irq(%d)\n",SRC_IRQ_PCI_PCCARD);
	}
}

/*
 *  smdk2510_pci_setup_resources
 *  @*resource : resource structure point for mapping to root->resource
 */
void __init smdk2510_pci_setup_resources(struct resource **resource)
{
	
	struct resource *mem_mem;

	mem_mem = kmalloc(sizeof(*mem_mem), GFP_KERNEL);
	memset(mem_mem, 0, sizeof(*mem_mem));


	mem_mem->flags = IORESOURCE_MEM;
	mem_mem->name  = "SMDk2510 PCI MEM Region";

// Radicalis_hans_begin (03.12.24)
//	allocate_resource(&iomem_pci_resource, mem_mem, 0x20000000,
//			  0x00000000, 0xfffffffe, 0x20000000, NULL, NULL);
	allocate_resource(&iomem_resource, mem_mem, 0x10000000,
			  0xc0000000, 0xcfffffff, 0x10000000, NULL, NULL);
// Radicalis_hans_end (03.12.24)
	resource[0] = &ioport_resource;
	resource[1] = mem_mem;
	resource[2] = NULL;
	
}

/* 
 * CardBus related functions. 
 */

#define DEASSERT_LOGIC_RESET    (sPCIRCC.RSL=1)
#define DEASSERT_RESET  (sPCIRCC.RSB=1)

#define CHECK_CCD   ((sPCCARDPRS.CD1 && sPCCARDPRS.CD2)? 0:1) 
#define     TYPE_CARDBUS    0x1

#define     THREE_VOLTAGE   0x33
#define     FIVE_VOLTAGE    0x50

#define PROVIDE_VCC(n) { sPCCARDCON.VCC=0; if(n==THREE_VOLTAGE) sPCCARDCON.VCC=3; else if (n==FIVE_VOLTAGE) sPCCARDCON.VCC=2;}

int SMDK2510_CardBus_Setup(void)
{
    unsigned int flags;

    save_flags(flags);
    cli();

    /* STEP 1. internal arbiter ON, auto-adress translation ON */
    sPCICON.ARB=1;
    sPCICON.ATS=1;
    sPCICON.RDY =1;

    /* STEP 2. clock UN-mask, auto-clkrun signal */
    //sPCIDIAG0.EXC=1;    /* use external clock */
    sPCIDIAG0.EXC=0;    /* use internal clock */
    sPCIDIAG0.NPU=1;

    sPCIRCC.MSK=0;
    sPCIRCC.M33=0;
    sPCIRCC.ACC=1;

    /* STEP 3 assert pci logic reset */
    sPCIRCC.RSL=1;

    /*
     * STEP 4 setting interrupt-related registers 
     * event mask (0->mask, 1->Unmask)
     * all is unmasked
     * and then pc-card event interrupt enable, all pci interrupt enable also.
     */
    sPCCARDEVM.STC=1;
    sPCCARDEVM.CD1=1;
    sPCCARDEVM.CD2=1;
    sPCCARDEVM.PWC=1;

    sPCIINTEN.PME=1;

	/* mask value to cover SDRAM size (32M == 0x2000000) */
    rPCIBAM0 = 0xfe000000;

    /*
     * 1. test whether cards are already present or not
     * 2. apply Vcc
     * 3. release bus & logic reset
     * 4. enable pc-card interrupts
     * 5. set configuration done
     */


    if(!CHECK_CCD || !sPCCARDPRS.C32) {
        printk( "No card, or not CardBus card.\n");
        return -1;
    }

    printk("CardBus Card is detected: ");

    /* apply appropriate Vcc for card */
    if(sPCCARDPRS.C3V)
    {
        PROVIDE_VCC(THREE_VOLTAGE);
        printk("3 Voltage Card\n");
    }
    else if(sPCCARDPRS.C5V)
    {
        PROVIDE_VCC(FIVE_VOLTAGE);
        printk("5 Voltage Card\n");
    }


    /* wait for card active state */
    while(!sPCCARDCON.ACT)
        ;

    /* deassert logic & bus reset */
    DEASSERT_LOGIC_RESET;
    DEASSERT_RESET;

	PCI_Delay(13300000);    // to wait for target to ready  

    /* all pc-card interrupts enable */
    rPCIINTST = 0xffffffff;
    rPCIINTEN = 0xffffffff;

    /* configuration done */
    sPCICON.CFD=1;

    /* initialize BARs */
    sPCIHSC.CMD=0x7;

    restore_flags(flags);
	return 0;
}


int __init cardbus_controller_init(void)
{
	int ret = -1;
    printk("CardBus Host Controller Initializing...\n");

    /* First, it is checked whether the mode is CardBus or not. */
    if(!PCIMode())
        ret = SMDK2510_CardBus_Setup();
	return ret;
}

void __init smdk2510_cardbus_init(void *sysdata)
{
    if (!cardbus_controller_init())		/* check whether there is a card or not. */
		pci_scan_bus(0,&smdk2510_pci_ops,sysdata);
}

