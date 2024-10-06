#!/bin/sh

echo " " | tee -a ../Makefile
echo -n 'obj-$(CONFIG_FENCEFDMOD)		+= fenceFd_module/' | tee -a ../Makefile

sed -i "/endmenu/i\source \"drivers/fenceFd_module/Kconfig\"" ../Kconfig
sed -i "/endmenu/i\ " ../Kconfig