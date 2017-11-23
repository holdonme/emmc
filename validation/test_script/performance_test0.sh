#! /bin/bash 

#debug=1

execute()
{
    $* >/dev/null
        if [ $? -ne 0 ]; then
        echo
        echo "ERROR: executing $*"
        echo
        exit 1
    fi
}


# Process command line...                                                                
while [ $# -gt 0 ]; do
    case $1 in
        --help | -h)
        usage $0
        ;;
        --device) shift; device=$1; shift; ;;
        --size) shift; size=$1; shift; ;;
        --version) version $0;;
        *) copy="$copy $1"; shift; ;;
esac
done


#check if $device and $size are empty
test -z $size && usage $0
test -z $device && usage $0

if [ ! -b $device ]; then
    echo "[err]============================> $device is not a block device file"
    exit 1;
fi


echo "[info]============================> Create mount point /mnt/test/ for ${device} "
if [ ! -d "/mnt/test" ]; then
    execute "mkdir -p /mnt/test"
else
    echo "[info]============================> /mnt/test have been creat"
fi



cwd=`dirname $0`

#echo "format with different,mount with default para"
echo "[info]============================> format block size=2048/mount default test/system_cache on/emmc_cache on" | tee -a /log/f1_1_0_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 1 --format_mode 0 --mount_mode 0 | tee -a /log/f1_1_0_0.log

echo "[info]============================> format block size=2048/mount default test/system_cache on/emmc_cache off" | tee -a /log/f1_0_0_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 0 --format_mode 0 --mount_mode 0 | tee -a /log/f1_0_0_0.log

echo "[info]============================> format block size=2048/mount default test/system_cache off/emmc_cache on" | tee -a /log/f0_1_0_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 1 --format_mode 0 --mount_mode 0 | tee -a /log/f0_1_0_0.log

echo "[info]============================> format block size=2048/mount default test/system_cache off/emmc_cache off" | tee -a /log/f0_0_0_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 0 --format_mode 0 --mount_mode 0 | tee -a /log/f0_0_0_0.log

echo "[info]============================> format block size=4096/mount default test/system_cache on/emmc_cache on" | tee -a /log/f1_1_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 1 --format_mode 1 --mount_mode 0 | tee -a /log/f1_1_1_0.log

echo "[info]============================> format block size=4096/mount default test/system_cache on/emmc_cache off" | tee -a /log/f1_0_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 0 --format_mode 1 --mount_mode 0 | tee -a /log/f1_0_1_0.log

echo "[info]============================> format block size=4096/mount default test/system_cache off/emmc_cache on" | tee -a /log/f0_1_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 1 --format_mode 1 --mount_mode 0 | tee -a /log/f0_1_1_0.log

echo "[info]============================> format block size=4096/mount default test/system_cache off/emmc_cache off" | tee -a /log/f0_0_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 0 --format_mode 1 --mount_mode 0 | tee -a /log/f0_0_1_0.log

#echo "format with default,mount with different para"
echo "[info]============================> format block size=default/mount different test/system_cache on/emmc_cache on" | tee -a /log/m1_1_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 1 --format_mode 1 --mount_mode 0 | tee -a /log/m1_1_1_0.log

echo "[info]============================> format block size=default/mount different test/system_cache on/emmc_cache off" | tee -a /log/m1_0_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 0 --format_mode 1 --mount_mode 0 | tee -a /log/m1_0_1_0.log

echo "[info]============================> format block size=default/mount different test/system_cache off/emmc_cache on" | tee -a /log/m0_1_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 1 --format_mode 1 --mount_mode 0 | tee -a /log/m0_1_1_0.log

echo "[info]============================> format block size=default/mount different test/system_cache off/emmc_cache off" | tee -a /log/m0_0_1_0.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 0 --format_mode 1 --mount_mode 0 | tee -a /log/m0_0_1_0.log

echo "[info]============================> format block size=default/mount different test/system_cache on/emmc_cache on" | tee -a /log/m1_1_1_1.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 1 --format_mode 1 --mount_mode 1 | tee -a /log/m1_1_1_1.log

echo "[info]============================> format block size=default/mount different test/system_cache on/emmc_cache off" | tee -a /log/m1_0_1_1.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 0 --format_mode 1 --mount_mode 1 | tee -a /log/m1_0_1_1.log

echo "[info]============================> format block size=default/mount different test/system_cache off/emmc_cache on" | tee -a /log/m0_1_1_1.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 1 --format_mode 1 --mount_mode 1 | tee -a /log/m0_1_1_1.log

echo "[info]============================> format block size=default/mount different test/system_cache off/emmc_cache off" | tee -a /log/m0_0_1_1.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 0 --format_mode 1 --mount_mode 1 | tee -a /log/m0_0_1_1.log

echo "[info]============================> format block size=default/mount different test/system_cache on/emmc_cache on" | tee -a /log/m1_1_1_2.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 1 --format_mode 1 --mount_mode 2 | tee -a /log/m1_1_1_2.log

echo "[info]============================> format block size=default/mount different test/system_cache on/emmc_cache off" | tee -a /log/m1_0_1_2.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 0 --format_mode 1 --mount_mode 2 | tee -a /log/m1_0_1_2.log

echo "[info]============================> format block size=default/mount different test/system_cache off/emmc_cache on" | tee -a /log/m0_1_1_2.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 1 --format_mode 1 --mount_mode 2 | tee -a /log/m0_1_1_2.log

echo "[info]============================> format block size=default/mount different test/system_cache off/emmc_cache off" | tee -a /log/m0_0_1_2.log
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 0 --format_mode 1 --mount_mode 2 | tee -a /log/m0_0_1_2.log










echo "----------------------performance test completed!-------------------------" 

exit 0








