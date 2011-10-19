zreladdr-y	:= 0x20008000
params_phys-y	:= 0x20000100

# override for Herring
zreladdr-$(CONFIG_MACH_HERRING)		:= 0x30008000
params_phys-$(CONFIG_MACH_HERRING)	:= 0x30000100

# override for Aries
zreladdr-$(CONFIG_MACH_ARIES)		:= 0x30008000
params_phys-$(CONFIG_MACH_ARIES)	:= 0x30000100

#override for crespo
zreladdr-$(CONFIG_MACH_CRESPO)		:= 0x30008000
params_phys-$(CONFIG_MACH_CRESPO)	:= 0x30000100

#override for Atlas
zreladdr-$(CONFIG_MACH_ATLAS)		:= 0x30008000
params_phys-$(CONFIG_MACH_ATLAS)	:= 0x30000100

zreladdr-$(CONFIG_MACH_FORTE)           := 0x30008000
params_phys-$(CONFIG_MACH_FORTE)        := 0x30000100

#override for Victory
zreladdr-$(CONFIG_MACH_VICTORY)		:= 0x30008000
params_phys-$(CONFIG_MACH_VICTORY)	:= 0x30000100
