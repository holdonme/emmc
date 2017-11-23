eCSD_212=$((`cmd8 -r 212 | tail -1 | awk '{print $2}'`))
eCSD_213=$((`cmd8 -r 213 | tail -1 | awk '{print $2}'`))
eCSD_214=$((`cmd8 -r 214 | tail -1 | awk '{print $2}'`))
eCSD_215=$((`cmd8 -r 215 | tail -1 | awk '{print $2}'`))
sector_count=$(($eCSD_212+$eCSD_213*256+$eCSD_214*256*256+$eCSD_215*256*256*256))
echo "eMMC size: $sector_count sectors"

echo "sequential write to full card with 2M chunk"
start=0
addr_end=$(($sector_count-1))
chunk=4096

mmcconfig -s
while [ "$start" -lt "$addr_end" ]
do
	end=$(($start+$chunk-1))
	if [ $end -gt $addr_end ]
	then
		end=$addr_end
	fi
	data=$(($start>>16&255))
	cmd25 -s $start -e $end -d $data -t
	start=$((start+$chunk))
done

