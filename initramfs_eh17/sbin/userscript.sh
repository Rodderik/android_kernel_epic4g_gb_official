#!/system/bin/sh
# Remount filesystems RW
busybox mount -o remount,rw / /
busybox mount -o remount,rw /system /system

# Install busybox
busybox --install -s /system/bin
busybox --install -s /system/xbin
busybox ln -s /sbin/recovery /system/bin/busybox
#/system/bin/busybox rm /sbin/busybox

# Setup su binary
busybox cp -f /sbin/su /system/bin/su
chmod 6755 /system/bin/su
busybox ln -s /system/bin/su /system/xbin/su
busybox rm /sbin/su

# Install Superuser.apk (only if not installed)
# make room
if [ ! -f "/system/app/Superuser.apk" ] && [ ! -f "/data/app/Superuser.apk" ] && [[ ! -f "/data/app/com.noshufou.android.su"* ]]; then
	if [ -f "/system/app/Asphalt5_DEMO_SAMSUNG_D700_Sprint_ML_330.apk" ]; then
		busybox rm /system/app/Asphalt5_DEMO_SAMSUNG_D700_Sprint_ML_330.apk
	fi
	if [ -f "/system/app/FreeHDGameDemos.apk" ]; then
		busybox rm /system/app/FreeHDGameDemos.apk
	fi
# copy apk
 	busybox cp /sbin/Superuser.apk /system/app/Superuser.apk
# remove pre-existing data (if exists)
	busybox test -d /data/data/com.noshufou.android.su || busybox rm -r /data/data/com.noshufou.android.su
fi
sync

# Enable init.d support
if [ -d /system/etc/init.d ]
then
	logwrapper busybox run-parts /system/etc/init.d
fi
sync

# Fix screwy ownerships
for blip in conf default.prop fota.rc init init.goldfish.rc init.rc init.smdkc110.rc lib lpm.rc modules recovery.rc res sbin bin
do
	chown root.shell /$blip
	chown root.shell /$blip/*
done

chown root.shell /lib/modules/*
chown root.shell /res/images/*

#setup proper passwd and group files for 3rd party root access
# Thanks DevinXtreme
if [ ! -f "/system/etc/passwd" ]; then
	echo "root::0:0:root:/data/local:/system/bin/sh" > /system/etc/passwd
	chmod 0666 /system/etc/passwd
fi
if [ ! -f "/system/etc/group" ]; then
	echo "root::0:" > /system/etc/group
	chmod 0666 /system/etc/group
fi

# fix busybox DNS while system is read-write
if [ ! -f "/system/etc/resolv.conf" ]; then
	echo "nameserver 8.8.8.8" >> /system/etc/resolv.conf
	echo "nameserver 8.8.4.4" >> /system/etc/resolv.conf
fi
sync
if [ -f "/system/media/bootanimation.zip" ]; then
ln -s /system/media/bootanimation.zip /system/media/sanim.zip
fi

# remount read only and continue
busybox  mount -o remount,ro / /
busybox  mount -o remount,ro /system /system
