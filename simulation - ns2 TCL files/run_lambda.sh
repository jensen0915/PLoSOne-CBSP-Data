rm performance.tr

j=0
input=0
for (( i=50; i<=300; i=i+50 ))
do
	j=$(($j + 1))	
	echo run lambda =$i, put folder to $j	
	ns cbsp_3_subflows.tcl $i > 3
	
	./log_grep.sh
	
	mv *.txt $j-/
done

