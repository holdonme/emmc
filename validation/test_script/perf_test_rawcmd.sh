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


version()
{
    echo "****************************************************************"
    echo "*         `basename $1` version $VERSION                      *"
    echo "****************************************************************"

    exit 0
}

usage ()
{
    echo "
Usage: `basename $1` <options> [ files for install partition ]
      
Mandatory options:       
   --config              eMMC configure file(e.g /bin/mmc_test/emmmc.config)
          
Optional options:
   --version             this script version
   --help                Print this help message.
"
    exit 1
}

full_card_loop=0
card_size()
{
cmd8 | grep SEC_COUNT: > /log/temp11.log
awk -F ' ' '{print $2}' /log/temp11.log > /log/temp12.log
read sec_num < /log/temp12.log
((sec_num_total=$sec_num))
echo "[debug]============================>sec_num_total =$sec_num_total sec"
rm /log/temp12.log
full_card_loop=`expr $sec_num_total / 524288`
}

time_calculate()
{
temp_count=0
total_times=0
line_count=0
average_times=0

while read line
do
    echo "$line" > /log/temp2.log
    awk -F ':' '{print $2}' /log/temp2.log > /log/temp3.log
    awk -F ' ' '{print $1}' /log/temp3.log > /log/temp4.log
    read temp_count < /log/temp4.log
    total_times=$[ total_times + temp_count ]
    line_count=$[ $line_count + 1 ]
    echo "[waiting]============================>line=$line_count  $line"
done < $1
if [ $debug -eq 1 ]; then
    echo "[debug]============================>total_times =$total_times"
else
    echo "[info]============================>total_times =$total_times"
    rm temp*.log
fi
    average_times=`expr $total_times / $line_count`
    echo "[debug]============================>average_times =$average_times"
return $average_times
}

run_config_file()
{
    echo "[debug]============================>run config file $config"
    chmod 777 $config
    $config

}


config_enhanced_area()
{
echo "[info]============================>scheck if partition have been set"
cmd8 | grep PARTITION_SETTING_COMPLETED: > /log/temp5.log
awk -F ' ' '{print $2}' /log/temp5.log > /log/temp6.log
read erase_group < /log/temp6.log
if [ $erase_group -eq 0 ]; then

cmd8 | grep HC_ERASE_GP_SIZE: > /log/temp5.log
awk -F ' ' '{print $2}' /log/temp5.log > /log/temp6.log
read erase_group < /log/temp6.log
((erase_group_size=$erase_group))
rm temp*.log

cmd8 | grep HC_WP_GRP_SIZE: > /log/temp5.log
awk -F ' ' '{print $2}' /log/temp5.log > /log/temp6.log
read wp_group < /log/temp6.log
((wp_group_size=$wp_group))
rm temp*.log

enh_size=`expr 2048 / $erase_group_size / $wp_group_size`

cmd6 -m3 -a136 -d0
cmd6 -m3 -a137 -d0
cmd6 -m3 -a138 -d0
cmd6 -m3 -a139 -d0

cmd6 -m3 -a140 -d $enh_size
cmd6 -m3 -a141 -d0
cmd6 -m3 -a142 -d0

cmd6 -m3 -a156 -d1
cmd6 -m3 -a155 -d1

echo "[info]============================>set 1G enhanced partition/start addr 0x000000"
echo "[info]============================>need power cycle to enable"
else 

echo "[info]============================>enhanced partition has been set"

fi
}


seq_write_full_card()
{
echo "[info]============================>seq write full card with chunk size=1M"
start_addr=0
end_addr=0
if [ $debug -eq 1 ]; then
    echo "[debug]============================>seq write full card size=2G"
#    n=4
    n=$full_card_loop
else 
    n=$full_card_loop
fi
 
while [ $n -gt 0 ]; do
    end_addr=`expr  $start_addr + 524287` 
#    mmcwrite -s $start_addr -e $end_addr -c 8192 -d0x55 -t
    mmcwrite -s $start_addr -e $end_addr -c 2048 -d0x55 -t
    start_addr=`expr $start_addr + 524288`
    n=$[ $n - 1 ]
    echo "[info]============================>loop=$n"
done

}

perf_calculate()
{
echo "[info]============================>read erase count per super block from eMMC"
total_read_size=0
wr_time=0
speed=0

#sed -i '$d' $1
#wc -l $1 > /log/temp0.log
#awk -F ' ' '{print $1}' /log/temp0.log > /log/temp1.log
#read chunk_num < /log/temp1.log
#unit is K*us
total_read_size=`expr $2 \* 1000000 / 2`
#read_size=`expr $test_size + 1 `

#if [ $3 -eq 1 ]; then
#    total_read_size=`expr $2 \* 500`
#else
#    total_read_size=`expr $read_size / 2`
#fi

echo "[info]============================>total_read_size $total_read_size k"
#super_blk_size=
time_calculate "$1"
#unit is us
wr_time=$average_times
#unit K/s
#speed=`expr $total_read_size \* 1000 / $time_ms`
speed=`expr $total_read_size / $wr_time`

if [ $debug -eq 1 ]; then
    echo "[debug]============================>chunk_size=$2 speed=$speed" K/s | tee -a /log/speed.log
else
    echo "[info]============================>chunk_size=$2 speed=$speed" K/s | tee -a /log/speed.log
    rm /log/times.log
    rm /log/temp*.log
fi

}
#unit is sec
chunk_size=('1' '2' '4' '8' '16' '32' '64' '128' '256' '512' '1024' '2048' '4096')
seq_write()
{
echo "[info]============================>seq write test with different chunk size"| tee -a /log/speed.log
#echo "[info]============================>seq write whole card with chunk size 2M"
i=0
addr_start=$1
#addr_end=$[ $1 + 2097151]
addr_end=0
#seq_write_full_card 
#mmcread -s0 -e7 -c2 -d0x55 -t
while [ $i -ne 13 ]; do
     echo "[info]============================>seq write test with chunk size=${chunk_size[i]}"
     if [ $debug -eq 1 ]; then
         j=1
     else
         j=2 
     fi
     
    test_sector=`expr ${chunk_size[i]} \* 1000`
    if [ $test_sector -le $test_size ]; then
        test_size=$test_sector
        j=1
    else
        j=`expr $test_sector / $test_size + 1` 
    fi


     while [ $j -gt 0 ]; do
         addr_end=`expr  $addr_start + $test_size - 1` 
         mmcwrite -s $addr_start -e $addr_end -c ${chunk_size[i]} -d 0x55 -t | tee -a /log/sw_${chunk_size[i]}.log
         addr_start=`expr $addr_start + $test_size`
         j=$[ j - 1 ]
     done
     addr_start=$1
     addr_end=0
     test_size=$set_size_sec
 
perf_calculate "/log/sw_${chunk_size[i]}.log" "${chunk_size[i]}" "0"
i=$[ $i + 1 ]
done
}
seq_read()
{
echo "[info]============================>seq read test with different chunk size" | tee -a /log/speed.log
i=0
addr_start=$1
#addr_end=$[ $1 + 2097151] 
addr_end=0

#echo "[info]============================>seq write full card with chunk size=4M"
echo "[psa]============================>seq write full card 2 times with chunk size=1M"
seq_write_full_card | tee -a /log/psa.log
seq_write_full_card | tee -a /log/psa.log

mmcread -s0 -e7 -c2 -d0x55 -t
while [ $i -ne 13 ]; do
     echo "[info]============================>seq read test with chunk size=${chunk_size[i]}"

    test_sector=`expr ${chunk_size[i]} \* 1000`
    if [ $test_sector -le $test_size ]; then
        test_size=$test_sector
        j=1
    else
        j=`expr $test_sector / $test_size + 1` 
    fi


     while [ $j -gt 0 ]; do
         addr_end=`expr  $addr_start + $test_size - 1` 
         mmcread -s $addr_start -e $addr_end -c ${chunk_size[i]} -d 0x55 -t | tee -a /log/sr_${chunk_size[i]}.log
         addr_start=`expr $addr_start + $test_size`
         j=$[ j - 1 ]
     done
     addr_start=$1
     addr_end=0
     test_size=$set_size_sec
perf_calculate "/log/sr_${chunk_size[i]}.log" "${chunk_size[i]}" "0"
i=$[ $i + 1 ]
done
}

random_write()
{
echo "[info]============================>random write test with different chunk size" | tee -a /log/speed.log
i=0
addr_start=$1
#addr_end=$[ $1 + 2097151] 
addr_end=0

while [ $i -ne 13 ]; do

echo "[info]============================>seq write full card with chunk size=4M"
seq_write_full_card
mmcread -s0 -e7 -c2 -d0x55 -t
     echo "[info]============================>random write test with chunk size=${chunk_size[i]}"
     if [ $debug -eq 1 ]; then
         j=1
     else
         j=2 
     fi

     while [ $j -gt 0 ]; do
         addr_end=`expr  $addr_start + $test_size - 1` 
         mmcrandwrite -s $addr_start -e $addr_end -c ${chunk_size[i]} -d 0x55 -n 1000 | tee -a /log/rw_${chunk_size[i]}.log
         addr_start=`expr $addr_start + $test_size`
         j=$[ j - 1 ]
     done
     addr_start=$1
     addr_end=0
perf_calculate "/log/rw_${chunk_size[i]}.log" "${chunk_size[i]}" "1"

i=$[ $i + 1 ]
done

}
random_read()
{
echo "[info]============================>random read test with different chunk size" | tee -a /log/speed.log
i=0
addr_start=$1
#addr_end=$[ $1 + 2097151] 
addr_end=0
while [ $i -ne 13 ]; do
     echo "[info]============================>random read test with chunk size=${chunk_size[i]}"
     if [ $debug -eq 1 ]; then
         j=1
     else
         j=2 
     fi
 
     while [ $j -gt 0 ]; do
         addr_end=`expr  $addr_start + $test_size - 1` 
         mmcrandread -s $addr_start -e $addr_end -c ${chunk_size[i]}  -n 1000 | tee -a /log/rr_${chunk_size[i]}.log
         addr_start=`expr $addr_start + $test_size`
         j=$[ j - 1 ]
     done
     addr_start=$1
     addr_end=0

perf_calculate "/log/rr_${chunk_size[i]}.log" "${chunk_size[i]}" "1"

i=$[ $i + 1 ]
done

}



# Process command line...                                                                
while [ $# -gt 0 ]; do
    case $1 in
        --help | -h)
        usage $0
        ;;
        --config) shift; config=$1; shift; ;;
        --version) version $0;;
        *) copy="$copy $1"; shift; ;;
esac
done

#check if $device and $loop are empty
test -z $config && usage $0

if [ ! -d "/log" ]; then
echo "[info]============================> creat ./log/"
    mkdir /log
else
    echo "[info]============================> ./log have been created"
    echo ""
    echo "*******************************<warning>*****************************"
    echo "*          next operation will remove all files in ./log            *"
    echo "*                  Press <ENTER> to continue....                    *"
    echo "*********************************************************************"
read junk
rm -r /log/*.log
fi

#echo "[info]============================>config enhanced area 1G"
#config_enhanced_area

run_config_file


echo "[info]============================>get eMMC size"
card_size
cmd6 -m3 -a179 -d0
sleep 1
cmd6 -m3 -a33 -d0
sleep 1
cmd8 | tee -a /log/ecsd.log

if [ $debug -eq 1 ]; then
    echo "[debug]============================>test size 200M"
#    test_size=40959
    set_size_sec=409600
    test_size=$set_size_sec
else
    echo "[debug]============================>test size 256M"
    test_size=524287
fi
 


#echo "[info]============================>SLC partition test"
#seq_write "0"
#seq_read "0"
#random_write "0"
#random_read "0"

echo "[info]============================>MLC partition test"
#seq_write "2097152"
#seq_read "2097152"
#random_write "2097152"
#random_read "2097152"

seq_read "0"
random_read "0"
seq_write "0"
random_write "0"

#w=50
#while [ $w -gt 0 ]; do
#        seq_write_full_card | tee -a /log/psa.log
#         w=$[ w - 1 ]
#done


echo "----------------------performance test completed!-------------------------"
exit 0







