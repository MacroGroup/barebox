obj-$(CONFIG_ARM_LINUX)	+= armlinux.o
obj-$(CONFIG_CMD_BOOTZ)	+= bootz.o
obj-y	+= div0.o
obj-y	+= findbit.o
obj-y	+= io.o
obj-y	+= io-readsb.o
obj-y	+= io-readsw-armv4.o
obj-y	+= io-readsl.o
obj-y	+= io-writesb.o
obj-y	+= io-writesw-armv4.o
obj-y	+= io-writesl.o
obj-y	+= lib1funcs.o
obj-y	+= ashrdi3.o
obj-y	+= ashldi3.o
obj-y	+= lshrdi3.o
obj-y	+= runtime-offset.o
pbl-y	+= runtime-offset.o
obj-$(CONFIG_ARM_OPTIMZED_STRING_FUNCTIONS)	+= memcpy.o
obj-$(CONFIG_ARM_OPTIMZED_STRING_FUNCTIONS)	+= memset.o
obj-$(CONFIG_ARM_UNWIND) += unwind.o
obj-$(CONFIG_ARM_SEMIHOSTING) += semihosting-trap.o semihosting.o
obj-$(CONFIG_MODULES) += module.o
extra-y += barebox.lds

pbl-y	+= lib1funcs.o
pbl-y	+= ashldi3.o
pbl-y	+= div0.o