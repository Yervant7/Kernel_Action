#!/bin/bash

patch_files=(
    fs/exec.c
    fs/open.c
    fs/read_write.c
    fs/stat.c
    drivers/input/input.c
)

for i in "${patch_files[@]}"; do

    if grep -q "ksu" "$i"; then
        echo "Warning: $i contains KernelSU"
        continue
    fi

    case $i in

    # fs/ changes
    ## exec.c
    fs/exec.c)
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/exec.c
        ;;

    ## open.c
    fs/open.c)
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/open.c
        ;;

    ## read_write.c
    fs/read_write.c)
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/read_write.c
        ;;

    ## stat.c
    fs/stat.c)
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' fs/stat.c
        ;;

    # drivers/input changes
    ## input.c
    drivers/input/input.c)
        sed -i '/#ifdef CONFIG_KSU/,/#endif/d' drivers/input/input.c
        ;;
    esac

done
