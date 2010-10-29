#
# drivers/net/wireless/Makefile
#
# Makefile for the Linux Wireless network device drivers.
#

O_TARGET := wireless_net.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=


# Things that need to export symbols
export-objs	:= airo.o orinoco.o hermes.o

obj-$(CONFIG_HOSTAP)	+= hostap.o hostap_crypt.o hostap_crypt_wep.o
obj-$(CONFIG_PCMCIA_HOSTAP)	+= hostap_cs.o
obj-$(CONFIG_PCI_HOSTAP)	+= hostap_pci.o

obj-$(CONFIG_HERMES)		+= orinoco.o hermes.o
obj-$(CONFIG_PCMCIA_HERMES)	+= orinoco_cs.o
obj-$(CONFIG_APPLE_AIRPORT)	+= airport.o
obj-$(CONFIG_PLX_HERMES)	+= orinoco_plx.o

obj-$(CONFIG_AIRO)		+= airo.o
obj-$(CONFIG_AIRO_CS)		+= airo_cs.o airo.o

# Radicalis_choish (03,12,17) for Ralink AP driver
ifeq ($(CONFIG_RALINK_AP),y)
	obj-y += rt2500.o
endif
include $(TOPDIR)/Rules.make
