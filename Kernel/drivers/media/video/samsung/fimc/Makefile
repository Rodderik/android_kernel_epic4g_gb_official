ifeq ($(CONFIG_MACH_FORTE),y)
obj-$(CONFIG_VIDEO_FIMC)	+= fimc_dev.o fimc_v4l2.o fimc_capture_forte.o fimc_output.o fimc_overlay.o fimc_regs.o
else
obj-$(CONFIG_VIDEO_FIMC)	+= fimc_dev.o fimc_v4l2.o fimc_capture.o fimc_output.o fimc_overlay.o fimc_regs.o
endif
obj-$(CONFIG_VIDEO_FIMC_MIPI)	+= csis.o

ifeq ($(CONFIG_CPU_S5PV210),y)
EXTRA_CFLAGS += -DCONFIG_MIPI_CSI_ADV_FEATURE
endif

EXTRA_CFLAGS += -Idrivers/media/video

ifeq ($(CONFIG_VIDEO_FIMC_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
