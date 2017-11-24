#!/bin/bash

grep -o "CMD [12][85].\+blks [0-9]\+" $1 > tmp1.log 
#input=$1;
#output=$2;
sed 's/,/ /g' tmp1.log >tmp2.log
#awk '{printf("cmd%d -s%s -e0x%x -d0x55 -c%s -t\n",$2,$6,strtonum($6)+$8-1,$8);}' tmp.log >out.log 
awk '{
if ($2 == 18) 
{
	printf("cmd%d -s%s -e0x%x -c%s\n",$2,$6,strtonum($6)+$8-1,$8);
} else if($2 == 25)
{
printf("cmd%d -s%s -e0x%x -d0x55 -c%s\n",$2,$6,strtonum($6)+$8-1,$8);
} 
}' tmp2.log >result.log

rm -f tmp*.log 
