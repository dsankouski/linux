# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_EXYNOS_ASV)	+= exynos-asv.o
obj-$(CONFIG_EXYNOS_ASV_ARM)	+= exynos5422-asv.o

obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o

obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o


#
# Exynos SoC Specific drivers
#

obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pm.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-pmu.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-el3_mon.o
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-cpu_hotplug.o

obj-$(CONFIG_ARCH_EXYNOS)	+= secmem.o

# CAL
obj-$(CONFIG_PWRCAL)            += pwrcal/
obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_REBOOT)	+= exynos-reboot.o
obj-$(CONFIG_EXYNOS_PM_DVS)        += exynos-pm-dvs.o

# CP
obj-$(CONFIG_SEC_SIPC_MODEM_IF)	+= pmu-cp.o
obj-$(CONFIG_SEC_MODEM_IF)	+= pmu-cp.o

obj-$(CONFIG_ECT)		+= ect_parser.o

obj-$(CONFIG_EXYNOS_HOTPLUG_GOVERNOR)	+= exynos-hotplug_governor.o
