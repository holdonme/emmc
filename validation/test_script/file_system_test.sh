#!/bin/bash
#max_file_size=256000000;
max_file_size=256000000;
file_size=0;

if false;then
while [ $file_size -lt $max_file_size ]
do 
echo $RANDOM >> random.bin
file_size=`ls -l random.bin | awk '{ print $5 }'` 
done
exit 0
fi


if [ $# -gt 0 ];then 
case $1 in 
	-h|--help)
	echo "file system read/write/erase test" 
	exit 1 ;;
esac 
fi

chunk=512;

while [ $chunk -lt 2097152 ]
do 
	if [ ! -f ./random.bin ];then
	echo "no random.bin"
	exit 1
	fi
 
	dd status=none if=./random.bin of=./tmp1.bin bs=$chunk count=500 ;
	for i in `seq 1000`
	do 
		dd status=none if=./tmp1.bin of=./tmp2.bin bs=$chunk count=500;
		diff ./tmp1.bin ./tmp2.bin
		if [ ! $? -eq 0 ];then
			echo "compare fail"
			exit 1
		else
		echo "chunk size = $chunk, file write/read/delete test loop $i " >> fs_test.log 
		fi
		rm -f ./tmp2.bin
	done

	rm -f ./tmp1.bin
	chunk=`expr $chunk \* 2`
	echo "chunk size = $chunk Byte read/write testing"  
done 
