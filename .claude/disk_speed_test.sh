#!/bin/bash
set -u

SIZE_MB=4096
BS=4M

drop_caches () {
    sync
    sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
}

run_one () {
    local label="$1"
    local path="$2"
    echo "=== $label  ($path) ==="
    mkdir -p "$path"
    local tmp="$path/wsl_disk_speed_test.bin"
    rm -f "$tmp"

    drop_caches
    echo "  write ${SIZE_MB} MB ($BS blocks) ..."
    local w
    w=$(dd if=/dev/zero of="$tmp" bs=$BS count=$((SIZE_MB / 4)) conv=fdatasync 2>&1 | tail -1)
    echo "    $w"

    drop_caches
    echo "  read  ${SIZE_MB} MB ..."
    local r
    r=$(dd if="$tmp" of=/dev/null bs=$BS 2>&1 | tail -1)
    echo "    $r"

    rm -f "$tmp"
    echo
}

# Make sure sudo passwordless or already cached
sudo -n true 2>/dev/null && echo "sudo OK (passwordless)" || echo "WARN: sudo may prompt"

run_one "C:  (NVMe -- where the Windows pagefile lives)"  /mnt/c/temp
run_one "F:  (where wsl-swap.vhdx currently lives)"       /mnt/f/temp
run_one "ext4 inside the WSL VM (~/wsl-tmp)"              "$HOME/wsl-tmp"
