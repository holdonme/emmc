chunk=8

ZoneCnt=8
ZoneSize=$((0x20000))

CurPattern=$(($1))

if [ "$CurPattern" -ne 0 ]
then
	PrePattern=$(($CurPattern-1))
else
	PrePattern=255
fi

PLAddr=$(($2))
PLSize=$(($3))
Flag=$(($4))
 
PLSection=$(($PLAddr/$ZoneSize))
PLBegin=$(($PLAddr%$ZoneSize))
PLEnd=$(($PLBegin+$PLSize))
index=0

if [ "$PLBegin" -gt 2048 ]
then
	TestBegin=$(($PLBegin-2048))
else
	TestBegin=0
fi

if [ "$PLEnd" -lt "$(($ZoneSize-2048))" ]
then
	TestEnd=$(($PLEnd+2048))
else
	TestEnd=$ZoneSize
fi

echo "$0 $1 $2 $3 $4"
echo "PLBegin: $PLBegin, PLEnd: $PLEnd, PLSection: $PLSection, PrePattern: $PrePattern, CurPattern: $CurPattern"
while [ "$index" -lt "$ZoneCnt" ]
do
	start=$TestBegin
	offset=$(($ZoneSize*$index))
	while [ "$start" -lt "$TestEnd" ]
	do
		end=$(($start+$chunk-1))
		startAddr=$(($start+$offset))
		endAddr=$(($end+$offset))

		if [ "$index" -lt "$PLSection" ]
		then
			if [ "$end" -lt "$PLEnd" ]
			then
				./cmd18 -s $startAddr -e $endAddr -d $CurPattern
				if [ "$?" -ne 0 ]
				then
					echo "data validation failure at $startAddr, pattern should be $CurPattern"
					./cmd18 -s $startAddr -e $endAddr -i LG_failure_dump.log
					exit 1
				fi
			else
				./cmd18 -s $startAddr -e $endAddr -d $PrePattern
				if [ "$?" -ne 0 ]
				then
					echo "data validation failure at $startAddr, pattern should be $PrePattern"
					./cmd18 -s $startAddr -e $endAddr -i LG_failure_dump.log
					exit 1
				fi
			fi
		elif [ "$index" -eq "$PLSection" ]
		then
			if [ "$end" -lt "$PLBegin" ]
			then
				./cmd18 -s $startAddr -e $endAddr -d $CurPattern
				if [ "$?" -ne 0 ]
				then
					echo "data validation failure at $startAddr to $endAddr, pattern should be $CurPattern"
					./cmd18 -s $startAddr -e $endAddr -i LG_failure_dump.log
					exit 1
				fi
			elif [ "$start" -eq "$PLBegin" ]
			then
				endAddr=$(($startAddr+$PLSize-1))
				if [ "$Flag" -eq 1 ]
				then
					./cmd18 -s $startAddr -e $endAddr -i Data_inPL_LGCase.log
				elif [ "$Flag" -eq 2 ]
				then
					./cmd18 -s $startAddr -e $endAddr -u Data_inPL_LGCase.log
					if [ "$?" -ne 0 ]
					then
						echo "data validation failure at power loss address $startAddr to $endAddr"
						exit 1
					fi
				else
					echo "$0 input parameter error: $0 $1 $2 $3 $4"
					exit 1
				fi
				start=$(($PLEnd-$chunk))
			else
				./cmd18 -s $startAddr -e $endAddr -d $PrePattern
				if [ "$?" -ne 0 ]
				then
					echo "data validation failure at $startAddr to $endAddr, pattern should be $PrePattern"
					./cmd18 -s $startAddr -e $endAddr
					exit 1
				fi
			fi
		else
			if [ "$end" -lt "$PLBegin" ]
			then
				./cmd18 -s $startAddr -e $endAddr -d $CurPattern
				if [ "$?" -ne 0 ]
				then
					echo "data validation failure at $startAddr to $endAddr, pattern should be $CurPattern"
					./cmd18 -s $startAddr -e $endAddr
					exit 1
				fi
			else
				./cmd18 -s $startAddr -e $endAddr -d $PrePattern
				if [ "$?" -ne 0 ]
				then
					echo "data validation failure at $startAddr to $endAddr, pattern should be $PrePattern"
					./cmd18 -s $startAddr -e $endAddr
					exit 1
				fi
			fi
		fi
		start=$(($start+$chunk))
	done
	index=$(($index+1))
done
exit 0
