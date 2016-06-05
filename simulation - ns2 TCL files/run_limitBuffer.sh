rm performance.tr

echo run 1, put folder to 1	
ns cbsp_3_subflows.tcl 5 > 3
./log_grep.sh
mv *.txt 1-/

echo run 2, put folder to 2	
ns cbsp_3_subflows.tcl 10 > 3
./log_grep.sh
mv *.txt 2-/

echo run 3
ns cbsp_3_subflows.tcl 15 > 3
./log_grep.sh
mv *.txt 3-/

echo run 4
ns cbsp_3_subflows.tcl 20 > 3
./log_grep.sh
mv *.txt 4-/

echo run 5
ns cbsp_3_subflows.tcl 25 > 3
./log_grep.sh
mv *.txt 5-/

echo run 6
ns cbsp_3_subflows.tcl 30 > 3
./log_grep.sh
mv *.txt 6-/

echo run 7
ns cbsp_3_subflows.tcl 35 > 3
./log_grep.sh
mv *.txt 7-/