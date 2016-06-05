rm performance.tr

ns cbsp_3_subflows.tcl 250000 500000 1000 > 3
./log_grep.sh
mv *.txt 1-/

ns cbsp_3_subflows.tcl 275000 550000 1100 > 3
./log_grep.sh
mv *.txt 2-/

ns cbsp_3_subflows.tcl 300000 600000 1200 > 3
./log_grep.sh
mv *.txt 3-/

ns cbsp_3_subflows.tcl 312500 625000 1300 > 3
./log_grep.sh
mv *.txt 4-/

ns cbsp_3_subflows.tcl 350000 700000 1400 > 3
./log_grep.sh
mv *.txt 5-/

ns cbsp_3_subflows.tcl 375000 750000 1500 > 3
./log_grep.sh
mv *.txt 6-/