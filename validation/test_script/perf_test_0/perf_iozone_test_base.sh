#! /bin/bash 

debug=0

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

#performance test ...
performance_test_cacheoff()
{
test_size=`expr $size / 20` 
echo "[info]============================> test_size = $test_size" 
echo "[info]============================> start performance test(buf cache off)"
if [ $debug -eq 1 ]; then
./iozone -+n -ec -i0 -i1 -i2  -t16 -s ${test_size}m -r1024 -I 
else
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r1k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r2k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r4k -I
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r8k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r16k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r32k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r64k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r128k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r256k -I
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r512k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r1024k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r2048k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r4096k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r8192k -I 
./iozone -+n -wec -i0 -i1 -i2 -t16 -s ${test_size}m -r16384k -I 
fi
}

performance_test_cacheon()
{
test_size=`expr $size / 20` 
echo "[info]============================> test_size = $test_size" 
echo "[info]============================> start performance test(buf cache on)" 
if [ $debug -eq 1 ]; then
./iozone -+n -ewc -i0 -i1 -i2  -t16 -s ${test_size}m -r 1024
else
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r1k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r2k 
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r4k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r8k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r16k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r32k 
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r64k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r128k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r256k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r512k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r1024k 
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r2048k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r4096k  
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r8192k 
./iozone -+n -wec -i0 -i1 -i2  -t16 -d2000 -s ${test_size}m -r16384k  
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
        --system_mode) shift; system_mode=$1; shift; ;;
        --emmc_mode) shift; emmc_mode=$1; shift; ;;
        --format_mode) shift; format_mode=$1; shift; ;;
        --mount_mode) shift; mount_mode=$1; shift; ;;
        --version) version $0;;
        *) copy="$copy $1"; shift; ;;
esac
done

echo "================$system_mode   $emmc_mode   $format_mode  $mount_mode ==================="


# handle various device names.
PARTITION1=${device}1
if [ ! -b ${PARTITION1} ]; then
        PARTITION1=${device}p1
fi

#make partition
if [ -b $PARTITION1 ]; then
    echo "[info]============================> format partition is $PARTITION1" 
else
    echo "[err]============================> Cant find boot partition in /dev"
fi


mount_array=('' '-o data=writeback' '-o data=journal' '-o nodelalloc' '-o nobarrier' '-o discard' '-o journal_checksum')
format_array=('-b 2048' '-b 4096')


echo "[info]============================> format with block size [${format_array[format_mode]}]" 
mkfs.ext4 ${format_array[format_mode]} $PARTITION1 

sleep 5
echo "[info]============================> mount with default para [${mount_array[mount_mode]}]" 
execute "mount -t ext4 ${mount_array[mount_mode]} $PARTITION1 /mnt/test" 
sleep 5
echo "[info]============================> copy test tool to test device" 
execute "cp /bin/mmc_test/iozone /mnt/test"
execute "cd /mnt/test"

if [ $emmc_mode -eq 1 ]; then
    echo "[info]============================> emmc cache on" 
    cmd6 -m3 -a33 -d1
else
    echo "[info]============================> emmc cache off" 
    cmd6 -m3 -a33 -d0
fi

echo "[info]============================> cmd8 read ecsd[33]" 
cmd8 | grep CACHE_CTRL:
sleep 1

if [ $system_mode -eq 1 ]; then
    performance_test_cacheon
else
    performance_test_cacheoff
fi

sleep 5
rm iozone.DUMMY.*
sleep 5
execute "cd /"
sync
sleep 60
echo "[info]============================> umount $PARTITION1 from /mnt/test/" 
execute "umount $PARTITION1" 
sleep 10
 
echo "----------------------performance test completed!-------------------------" 

exit 0








