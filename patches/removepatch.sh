#!/bin/bash

patch_files=(
    fs/exec.c
    fs/open.c
    fs/read_write.c
    fs/stat.c
    drivers/input/input.c
    drivers/Kconfig
    drivers/Makefile
)

for i in "${patch_files[@]}"; do

    case $i in

    # fs/ changes
    ## exec.c
    fs/exec.c)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/exec.c
        ;;

    ## open.c
    fs/open.c)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/open.c
        ;;

    ## read_write.c
    fs/read_write.c)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/read_write.c
        ;;

    ## stat.c
    fs/stat.c)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/stat.c
        ;;

    # drivers/ changes
    ## input.c
    drivers/input/input.c)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' drivers/input/input.c
        ;;

    ## Kconfig
    drivers/Kconfig)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/menu "KernelSU"/,/endmenu/d' drivers/Kconfig
        ;;

    ## Makefile
    drivers/Makefile)
        if grep -q "ksu" "$i"; then
            echo "Warning: $i contains KernelSU"
        fi
        sed -i '/obj-$(CONFIG_KSU)[[:space:]]\+= ksu\//d' drivers/Makefile
        sed -i '/obj-$(CONFIG_KSU)[[:space:]]\+= kernelsu\//d' drivers/Makefile
        ;;
        
    esac

done
