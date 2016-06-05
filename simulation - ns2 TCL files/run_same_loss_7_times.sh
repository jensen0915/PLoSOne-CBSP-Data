rm performance.tr

j=0
input=0
for (( i=0; i<=12; i=i+2 ))
do
	j=$(($j + 1))	
	echo run  =$j, put folder to $j	
	ns cbsp_3_subflows.tcl > 3
	
	./log_grep.sh
	
	mv *.txt $j-/
done

