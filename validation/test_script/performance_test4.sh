#! /bin/bash 


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


read_test()
{

time cat /mnt/test/125mb > /dev/null; time sync

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

# handle various device names.
PARTITION1=${device}1
if [ ! -b ${PARTITION1} ]; then
        PARTITION1=${device}p1
#    echo "change partition ..."
fi

echo "[info]============================> Create mount point /mnt/test/ for ${device} "
if [ ! -d "/mnt/test" ]; then
    execute "mkdir -p /mnt/test"
else
    echo "[info]============================> /mnt/test have been creat"
fi

echo "[info]============================> format and mount partition on /mnt/test/"
#mkfs.ext4 $PARTITION1
#execute "mount -t ext4 $PARTITION1 /mnt/test"

test_size=0
df -h /mnt/test > /log/size.temp
sed -n '2p' /log/size.temp > /log/size.temp1
cat /log/size.temp1 | awk -F ' ' '{print $4}' > /log/size.temp2
read test_size < /log/size.temp2
echo "$test_size" | grep -o '[0-9]\+' > log/size.data
read size_data < /log/size.data
echo "$size_data"
    
echo "$test_size" | grep -o '[A-Z]\+' > /log/size.unit
read size_unit < /log/size.unit
#rm /log/size.*
echo "$size_unit"

junk

if [ "$size_unit"x = "G"x ]; then
    test_size=`expr $size_data \* 1048576`
elif [ "$size_unit"x = "M"x ]; then
    test_size=`expr $size_data \* 1024`
else
    echo "[err]============================> test size is too small! test termination"
    exit 0
fi

echo "[info]============================> test size is $test_size K"

bs_array[0]=`expr 4 \* 1024`
bs_array[1]=`expr 8 \* 1024`
bs_array[2]=`expr 16 \* 1024`
bs_array[3]=`expr 32 \* 1024`
bs_array[4]=`expr 64 \* 1024`
bs_array[5]=`expr 128 \* 1024`
bs_array[6]=`expr 256 \* 1024`
bs_array[7]=`expr 512 \* 1024`
bs_array[8]=`expr 1024 \* 1024`

#bs_array=($bs_4k $bs_8k $bs_16k $bs_32k $bs_64k $bs_128k $bs_256k $bs_512k $bs_1024k)

count_array[0]=`expr $test_size / 4`
count_array[1]=`expr $test_size / 8`
count_array[2]=`expr $test_size / 16`
count_array[3]=`expr $test_size / 32`
count_array[4]=`expr $test_size / 64`
count_array[5]=`expr $test_size / 128`
count_array[6]=`expr $test_size / 256`
count_array[7]=`expr $test_size / 512`
count_array[8]=`expr $test_size / 1024`
#count_array=('$count_4k' '$count_8k' '$count_16k' '$count_32k' '$count_64k' '$count_128k' '$count_256k' '$count_512k' '$count_1024k') 

#echo "test size is $test_size"

i=0
echo "==================================================================================="
echo "========================write/read test with diff chunk size======================="
echo "==================================================================================="
while [ $i -ne 9 ];do
echo "[info]============================>write test with chunk size ${bs_array[i]} bytes"
(time dd if=/dev/zero of=/mnt/test/test.file bs=${bs_array[i]} count=${count_array[i]}) | tee -a /log/dd_speed.log
(time sync) | tee -a /log/dd_speed.log
#execute "umount $PARTITION1"
#execute "mount -t ext4 $PARTITION1 /mnt/test"
sleep 5

echo "[info]============================>read test with chunk size ${bs_array[i]} bytes"
(time dd if=/mnt/test/test.file of=/dev/null bs=${bs_array[i]} count=${count_array[i]}) | tee -a /log/dd_speed.log
(time sync) | tee -a /log/dd_speed.log
#echo "${bs_array[i]}====== ${count_array[i]}"
i=`expr $i + 1`
sleep 5

done

echo "[info]============================> umount $PARTITION1"
execute "umount $PARTITION1"
echo "======================================= dd test completed! ======================================= "



#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 8 \* 1024` count=$count_8k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 16 \* 1024` count=$count_16k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 32 \* 1024` count=$count_32k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 64 \* 1024` count=$count_64k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 128 \* 1024` count=$count_128k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 256 \* 1024` count=$count_256k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 512 \* 1024` count=$count_521k; time sync
#time dd if=/dev/zero of=/mnt/test/test.file bs=`expr 1024 \* 1024` count=$count_1024k; time sync





