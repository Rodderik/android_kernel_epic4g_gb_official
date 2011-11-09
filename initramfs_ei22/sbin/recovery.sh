#!/sbin/sh
/sbin/busybox mount -o remount,rw rootfs /
/sbin/busybox mount -o remount,rw /dev/block/stl9 /system
/sbin/busybox --install -s /sbin
/sbin/busybox ln -s /sbin/recovery /sbin/amend
/sbin/busybox ln -s /sbin/recovery /sbin/dump_image
/sbin/busybox ln -s /sbin/recovery /sbin/erase_image
/sbin/busybox ln -s /sbin/recovery /sbin/flash_image
/sbin/busybox ln -s /sbin/recovery /sbin/mkyaffs2image
/sbin/busybox ln -s /sbin/recovery /sbin/nandroid
/sbin/busybox ln -s /sbin/recovery /sbin/reboot
/sbin/busybox ln -s /sbin/recovery /sbin/unyaffs
# Fix permissions in /sbin, just in case
/sbin/busybox chmod 755 /sbin/*
# Fix screwy ownerships
for blip in conf default.prop fota.rc init init.goldfish.rc init.rc init.smdkc110.rc lib lpm.rc modules recovery.rc res sbin bin
do
  /sbin/busybox chown root.system /$blip
  /sbin/busybox chown root.system /$blip/*
done
/sbin/busybox chown root.system /lib/modules/*
/sbin/busybox chown root.system /res/images/*
/sbin/busybox test -d /etc || /sbin/busybox mkdir /etc
/sbin/busybox cp /res/etc/recovery.fstab /etc/recovery.fstab
/sbin/busybox mount -o remount,ro rootfs /
/sbin/busybox mount -o remount,ro /dev/block/stl9 /system
/sbin/recovery
