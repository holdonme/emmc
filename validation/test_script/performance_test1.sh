#! /bin/sh 

debug=0

execute ()
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

#echo "$device $size"


echo "[info]============================> start to partition $device" 
echo "[info]============================> partition size $size" 
#cat << END | fdisk $device
#o
#n
#p
#1
#2050
#+${size}M
#t
#1
#c
#a
#1
#w
#END
echo "****************************************************************"

echo "[info]============================> Create mount point /mnt/test/ for ${device} "
if [ ! -d "/mnt/test" ]; then
    execute "mkdir -p /mnt/test"
else
    echo "[info]============================> /mnt/test have been created"
fi

sleep 5

cwd=`dirname $0`
echo "[info]============================> format block size=default/mount default/system_cache on/emmc_cache on"
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 1 --format_mode 1 --mount_mode 0 | tee -a /log/s1_1_1_0.log
    
echo "[info]============================> format block size=default/mount default/system_cache on/emmc_cache off"
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 1 --emmc_mode 0 --format_mode 1 --mount_mode 0 | tee -a /log/s1_0_1_0.log
      
echo "[info]============================> format block size=default/mount default/system_cache off/emmc_cache on"
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 1 --format_mode 1 --mount_mode 0 | tee -a /log/s0_1_1_0.log
 
echo "[info]============================> format block size=default/mount default/system_cache off/emmc_cache off"
$cwd/perf_test_0/perf_iozone_test_base.sh --device $device --size $size --system_mode 0 --emmc_mode 0 --format_mode 1 --mount_mode 0 | tee -a /log/s0_0_1_0.log
sleep 5

sleep 60


echo "----------------------performance test completed!-------------------------" 


exit 0










