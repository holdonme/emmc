#!/bin/bash

#this may need to be modified according to the log provided by customers 
grep -o "CMD [12][834675].\+blks [0-9]\+" $1 > tmp1.log 
sed 's/,/ /g' tmp1.log >tmp2.log

#generate the cmd lines for zedboard 
awk '{
if ($2 == 18) 
{
	printf("cmd%d -s%s -e0x%x -c%s -q\n",$2,$6,strtonum($6)+$8-1,$8);
} else if($2 == 25)
{
printf("cmd%d -s%s -e0x%x -d0x55 -c%s\n",$2,$6,strtonum($6)+$8-1,$8); 
} 
}' tmp2.log >result.cmd

awk 'BEGIN{sum=0;}{
if($2 == 25)
{
sum+=strtonum($8);
} 
} END{print "total write sector count =";print sum}' tmp2.log


rm -f tmp*.log 
