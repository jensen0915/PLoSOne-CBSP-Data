# 
# 使用方法
# perl measure_mptcp.pl <trace file> 
# perl measure_mptcp_throughput_3path.pl out_3path.tr 

#紀錄檔檔名
$infile=$ARGV[0];

#紀錄檔檔名(單位為秒)
$granularity=$ARGV[1];

$sum=0;
$sum_total=0;
$init=0;
$throughput=0;
$clock=0;
$num=0;

open(DATA, "<$infile")
    || die "Can't open $infile $!";

while(<DATA>) {    
    @x = split(' ');
    if(($x[0] eq "r")&&($x[3]== 5)&&($x[14] == 56)){
        if($init==0){
            $start=$x[1];
            $init=1;
        }
        if($x[1]-$clock <= $granularity){
            #計算單位時間內累積的封包大小
            $sum=$sum+$x[19];
            #計算累積的總封包大小
            $sum_total=$sum_total+$x[19];
        }
        else{
            #計算吞吐量
            $throughput=$sum*8.0/$granularity;
          
            #輸出結果: 時間 吞吐量(bps)
            print STDOUT "$x[1] $throughput\n";

            #設定下次要計算吞吐量的時間
            $clock=$clock+$granularity;

            $sum_total=$sum_total+$x[19];
            $sum=$x[19];
        }       
        $num++;     
        $endtime=$x[1];       
    }    
    if(($x[0] eq "r")&&($x[3]== 6)&&($x[14] == 56)){
        if($init==0){
            $start=$x[1];
            $init=1;
        }
        if($x[1]-$clock <= $granularity){
            #計算單位時間內累積的封包大小
            $sum=$sum+$x[19];
            #計算累積的總封包大小
            $sum_total=$sum_total+$x[19];
        }
        else{
            #計算吞吐量
            $throughput=$sum*8.0/$granularity;
          
            #輸出結果: 時間 吞吐量(bps)
            print STDOUT "$x[1] $throughput\n";

            #設定下次要計算吞吐量的時間
            $clock=$clock+$granularity;

            $sum_total=$sum_total+$x[19];
            $sum=$x[19];
        }       
        $num++;     
        $endtime=$x[1];      
    }    
    if(($x[0] eq "r")&&($x[3]== 7)&&($x[14] == 56)){
        if($init==0){
            $start=$x[1];
            $init=1;
        }
        if($x[1]-$clock <= $granularity){
            #計算單位時間內累積的封包大小
            $sum=$sum+$x[19];
            #計算累積的總封包大小
            $sum_total=$sum_total+$x[19];
        }
        else{
            #計算吞吐量
            $throughput=$sum*8.0/$granularity;
          
            #輸出結果: 時間 吞吐量(bps)
            print STDOUT "$x[1] $throughput\n";

            #設定下次要計算吞吐量的時間
            $clock=$clock+$granularity;

            $sum_total=$sum_total+$x[19];
            $sum=$x[19];
        }       
        $num++;     
        $endtime=$x[1];       
    }
}

#計算最後一次的吞吐量大小
$throughput=$sum*8.0/$granularity;
print STDOUT "$x[1] $throughput\n";
$clock=$clock+granularity;
$sum=0;

print STDOUT "sum_total = $sum_total\n";
print STDOUT "start = $start\n";
print STDOUT "endtime = $endtime\n";
$avgrate=$sum_total*8.0/($endtime-$start);
print STDOUT "Average rate: $avgrate byte per scecond\n";
print STDOUT "num: $num \n";


#關閉檔案 
close DATA;
exit(0);
