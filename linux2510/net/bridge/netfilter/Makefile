#
# Makefile for the netfilter modules on top of bridging.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET	:= netfilter.o

export-objs := ebtables.o

obj-$(CONFIG_BRIDGE_EBT) += ebtables.o
obj-$(CONFIG_BRIDGE_EBT_T_FILTER) += ebtable_filter.o
obj-$(CONFIG_BRIDGE_EBT_T_NAT) += ebtable_nat.o
obj-$(CONFIG_BRIDGE_EBT_BROUTE) += ebtable_broute.o
obj-$(CONFIG_BRIDGE_DB) += br_db.o
obj-$(CONFIG_BRIDGE_EBT_IPF) += ebt_ip.o
obj-$(CONFIG_BRIDGE_EBT_ARPF) += ebt_arp.o
obj-$(CONFIG_BRIDGE_EBT_VLANF) += ebt_vlan.o
obj-$(CONFIG_BRIDGE_EBT_MARKF) += ebt_mark_m.o
obj-$(CONFIG_BRIDGE_EBT_LOG) += ebt_log.o
obj-$(CONFIG_BRIDGE_EBT_SNAT) += ebt_snat.o
obj-$(CONFIG_BRIDGE_EBT_DNAT) += ebt_dnat.o
obj-$(CONFIG_BRIDGE_EBT_REDIRECT) += ebt_redirect.o
obj-$(CONFIG_BRIDGE_EBT_MARK_T) += ebt_mark.o
include $(TOPDIR)/Rules.make
