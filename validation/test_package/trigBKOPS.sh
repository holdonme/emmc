BKOPS_lv=$(($1))
echo "running until BKOPS level reach $BKOPS_lv"
eCSD_215=$((`cmd8 -r 215 | tail -1 | awk '{print $2}'`))
eCSD_214=$((`cmd8 -r 214 | tail -1 | awk '{print $2}'`))
eCSD_213=$((`cmd8 -r 213 | tail -1 | awk '{print $2}'`))
eCSD_212=$((`cmd8 -r 212 | tail -1 | awk '{print $2}'`))
eMMCSize=$(($eCSD_212+$eCSD_213*256+$eCSD_214*256*256+$eCSD_215*256*256*256))
echo "eMMC size: $eMMCSize sectors"

mmcconfig -s

BKOPS_status=$((`cmd8 -r 246 | tail -1 | awk '{print $2}'`))
# in each loop random write 512MB data with 4KB chunk
loops=$((512*1024/4))
while [ "$BKOPS_status" -lt "$BKOPS_lv" ]
do
	echo "BKOPS status: $BKOPS_status"

	var=0

	while [ "$var" -lt "$loops" ]
	do
	# write address is 4KB aligned 
		start=$(($RANDOM%1024/8*8))
		start=$(($start + $RANDOM%30000*2048))
		
		num=$(($start>>16&255))
		end=$(($start+7))
	
		var=$(($var+1))
		cmd25 -s $start -e $end -d $num

		BKOPS_status=$((`cmd8 -r 246 | tail -1 | awk '{print $2}'`))
		if [ $BKOPS_status -ge $BKOPS_lv ]
		then
			echo "BKOPS status: $BKOPS_status"
			exit 0
		fi

	done
done

