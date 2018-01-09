
# loop 
for i in $(seq 37)
do
echo "writing 12GB count =  $i"
start=41943040;
end=41943040;

while [ $end -lt 61079552 ]
do
end=`expr  $start + 500 `
echo "start = $start, end = $end"
cmd25 -s $start -e $end -c1 -d0xa5
start=`expr  $start + 500 `
done

done
