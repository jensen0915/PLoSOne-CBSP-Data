rm performance.tr

j=1
input=0
for (( i=2; i<=12; i=i+2 ))
do
	j=$(($j + 1))	
	echo run loss rate =$i, put folder to $j	
	ns cbsp_3_subflows.tcl $i > 3
	
	./log_grep.sh
	
	mv *.txt $j-/
done

