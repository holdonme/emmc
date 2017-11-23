#! /bin/bash 

debug=1

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

#performance test eMMC cache off...
performance_test_devlevel()
{
echo "[info]============================>start performance test samsung script(device level)" | tee -a /log/test.log
perftest $PARTITION1 read | tee -a /log/samsung_script.log 

}

performance_test_syslevel()
{
echo "[info]============================>start performance test samsung script(file system level)" | tee -a /log/test.log 
perftest /mnt/test/test.file all | tee -a /log/samsung_script.log 

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
    echo "[info]============================> /mnt/test have been created"
fi


# handle various device names.
PARTITION1=${device}1
if [ ! -b ${PARTITION1} ]; then
        PARTITION1=${device}p1
fi

#make partition
#echo "formating $PARTITION1 ..."
if [ -b $PARTITION1 ]; then
    echo "[info]============================>format partition is $PARTITION1"
else
    echo "[err]============================>Cant find boot partition in /dev"
fi


#echo "format partition $PARTITION1"
echo "[info]============================>format with default para"
time mkfs.ext4 $PARTITION1

sleep 5

#echo "mount $PARTITION1 on /mnt/test/ "
echo "[info]============================>mount with default"
execute "mount -t ext4 $PARTITION1  /mnt/test"
sleep 5

#cd /mnt/test/
#execute "cp /bin/mmc_test/test.file /mnt/test"

echo "[info]============================>write test.file 500M"
dd if=/dev/zero of=/mnt/test/test.file bs=1048576 count=500

echo "[info]============================>test file system level"
performance_test_syslevel


execute "cd /"
sleep 30
sync
sleep 60
echo "[info]============================>umount $PARTITION1 on /mnt/test/"
execute "umount $PARTITION1"


echo "[info]============================>test block level"
performance_test_devlevel
echo "----------------------performance test completed!-------------------------" | tee -a /log/samsung_script.log 









