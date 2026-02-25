#!/bin/bash
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_DIR="$PROJECT_DIR/qemu_test"

echo "--- Building driver ---"
cd "$PROJECT_DIR" && make || exit 1

echo "--- Preparing initramfs ---"
cd "$QEMU_DIR" || exit 1
rm -rf initfs initrd.img
mkdir -p initfs/bin initfs/dev initfs/proc initfs/sys
cp /bin/busybox initfs/bin/
cp "$PROJECT_DIR/my_ramdisk.ko" initfs/

cat <<EOF > initfs/init
#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs none /dev
insmod /my_ramdisk.ko
echo "=== SYSTEM READY ==="
lsblk
/bin/sh
EOF

chmod +x initfs/init
cd initfs && find . | cpio -o -H newc | gzip > ../initrd.img && cd ..

echo "--- Starting QEMU ---"
qemu-system-x86_64 -kernel bzImage -initrd initrd.img -append "console=ttyS0 nosmap nokaslr quiet" -nographic -m 512M -s
