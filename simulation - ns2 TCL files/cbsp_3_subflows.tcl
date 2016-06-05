set init_cwnd 3;		# default before linux 3.0
#set init_cwnd 10;		# default after linux 3.0, google recommend 
set slowStartRestart true;
#set slowStartRestart false;
set simDurationStart 0
set simDurationEnd 150
#set simDurationEnd 300
set dumpInterval 0.01
set delayACK 0.1
set tcp_initial_ssthresh 47;  #65535 Bytes, around 47
#set loss_rate 0.12
#set loss_rate 1
set loss_rate 0.0


#set loss [lindex $argv 0]
#set loss_rate [expr ($loss * 0.01)]

ns-random 0
set windowSize 500;
#set lambda 0;
set lambda 300;
#set lambda 200;
#set lambda 450;
#set lambda 80;
#set lambda 350;
#set lambda [lindex $argv 0]; #paper上面改成delta了!

# method 1: small (s)rtt first
# method 2: small pricing first
# method 3: round robin
# method 4: ETOM (CBSP)
set scheduleMode 4

# For YouTube trace 
#set lowWater 3650000;   #unit bytes
#set highWater 13500000;
# For different lambda cases
#set lowWater 316000;	#around 2 second	
#set highWater 632000;	#around 4 second
set lowWater [lindex $argv 0];
set highWater [lindex $argv 1];

#set bitrate 1271;		# video bitrate for YouTube trace, 158875 B per second 
set bitrate [lindex $argv 2];		# video bitrate for YouTube trace, 158875 B per second 

# mobility 只用1sec的ordering buffer
#set lowWater 158875;				 
#set highWater 317750;	
#set lowWater 317750;				 
#set highWater 635500;	

#set lowWater 476625
#set highWater 953250

#set lowWater 635500;				 
#set highWater 1271000;	
			  

#set lowWater 1271000;				 
#set highWater 2542000;	


#set bitrate 1100;	
#set lowWater 275000;	
#set highWater 550000;	

#set bitrate 2500;		# 312500 B per second 
#set lowWater 625000;	#around 2 second	
#set highWater 1250000;	#around 4 second

#set bitrate 2000;		# 250000 B per second 
#set lowWater 500000;	#around 2 second	
#set highWater 1000000;	#around 4 second


set ns [new Simulator]

#
# specify to print mptcp option information
#
Trace set show_tcphdr_ 2


set n1 [$ns node]; #YouTube server
set n2 [$ns node]; #CBSP	server
set n3 [$ns node]; #Requestor

$ns trace-all [open out.tr w]
#$ns namtrace-all [open out.nam w]

#
# mptcp sender
#

set n2_0 [$ns node]
set n2_1 [$ns node]
set n2_2 [$ns node]

$n2 color green
$n2_0 color red
$n2_1 color red
$n2_2 color red
$ns multihome-add-interface $n2 $n2_0
$ns multihome-add-interface $n2 $n2_1
$ns multihome-add-interface $n2 $n2_2

#
# mptcp receiver
#
set n3_0 [$ns node]
set n3_1 [$ns node]
set n3_2 [$ns node]

$n3 color yellow
$n3_0 color blue
$n3_1 color blue
$n3_2 color blue
$ns multihome-add-interface $n3 $n3_0
$ns multihome-add-interface $n3 $n3_1
$ns multihome-add-interface $n3 $n3_2
  
$ns rtproto Session

#
# create mptcp sender
#
#     1. create subflows with Agent/TCP/FullTcp/Sack/Multipath
#     2. attach subflow on each interface
#     3. create mptcp core 
#     4. attach subflows to mptcp core
#     5. attach mptcp core to core node 
#     6. attach application to mptcp core
#
set tcp2_0 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp2_0 set window_ $windowSize	
$tcp2_0 set segsize_ 1303
$tcp2_0 set ssthresh_ $tcp_initial_ssthresh
$tcp2_0 set nodelay_ true;           # disabling nagle  (非常重要, 如果false, tcp layer會等segment full才送packet)
$tcp2_0 set timestamps_ true
$tcp2_0 set windowInit_ $init_cwnd;	
$tcp2_0 set slow_start_restart_ $slowStartRestart;		
$ns attach-agent $n2_0 $tcp2_0

set tcp2_1 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp2_1 set window_ $windowSize	
$tcp2_1 set segsize_ 1303
$tcp2_1 set ssthresh_ $tcp_initial_ssthresh
$tcp2_1 set nodelay_ true;          
$tcp2_1 set timestamps_ true
$tcp2_1 set windowInit_ $init_cwnd;			
$tcp2_1 set slow_start_restart_ $slowStartRestart;		
$ns attach-agent $n2_1 $tcp2_1

set tcp2_2 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp2_2 set window_ $windowSize	
$tcp2_2 set segsize_ 1303
$tcp2_2 set ssthresh_ $tcp_initial_ssthresh
$tcp2_2 set nodelay_ true;          
$tcp2_2 set timestamps_ true
$tcp2_2 set windowInit_ $init_cwnd;			
$tcp2_2 set slow_start_restart_ $slowStartRestart;		
$ns attach-agent $n2_2 $tcp2_2


set mptcp [new Agent/MPTCP]
$mptcp attach-tcp $tcp2_0
$mptcp attach-tcp $tcp2_1
$mptcp attach-tcp $tcp2_2
$ns multihome-attach-agent $n2 $mptcp



#
# create mptcp receiver
#
set mptcpsink [new Agent/MPTCP]

set sink3_0 [new Agent/TCP/FullTcp/Sack/Multipath]
$sink3_0 set segsize_ 50; 		#reply ACK only, so use 50
#$sink3_0 set interval_ $delayACK;
$ns attach-agent $n3_0 $sink3_0 
$sink3_0 set timestamps_ true

set sink3_1 [new Agent/TCP/FullTcp/Sack/Multipath]
$sink3_1 set segsize_ 50
#$sink3_1 set interval_ $delayACK;
$ns attach-agent $n3_1 $sink3_1 
$sink3_1 set timestamps_ true

set sink3_2 [new Agent/TCP/FullTcp/Sack/Multipath]
$sink3_2 set segsize_ 50
#$sink3_2 set interval_ $delayACK;
$ns attach-agent $n3_2 $sink3_2 
$sink3_2 set timestamps_ true


$mptcpsink attach-tcp $sink3_0
$mptcpsink attach-tcp $sink3_1
$mptcpsink attach-tcp $sink3_2

$ns multihome-attach-agent $n3 $mptcpsink
$ns multihome-connect $mptcp $mptcpsink
$mptcpsink listen

$mptcpsink set-price 0 1
$mptcpsink set-price 1 2
$mptcpsink set-price 2 4

$mptcp schedule-mode $scheduleMode
$mptcp lowWaterBuffSize $lowWater
$mptcp highWaterBuffSize $highWater
$mptcp limitBuffMode 0
$mptcp limitBuffSize 35

#$mptcp limitBuffMode 1
#$mptcp limitBuffSize [lindex $argv 0]
$mptcp video_bitrate $bitrate;		# for YouTube trace

#
# intermediate nodes 
#

set r0 [$ns node];# between YouTube server and CBSP server
set r1 [$ns node];# subflow 1 router
set r2 [$ns node];# subflow 2 router
set r3 [$ns node];# subflow 3 router
set r4 [$ns node];# subflow 1 router
set r5 [$ns node];# subflow 2 router
set r6 [$ns node];# subflow 3 router

set h1 [$ns node];# subflow 1 helper 1	
set h2 [$ns node];# subflow 2 helper 2	
set h3 [$ns node];# subflow 3 helper 3	

# YouTube server to CBSP server (100Mbps, 35ms 兩段, 所以bw/2, delay*2)
#$ns duplex-link $n1 $n2 50Mb 70ms DropTail
#$ns duplex-link $n1 $n2 100Mb 35ms DropTail

$ns duplex-link $n1 $r0 100Mb 5ms DropTail
$ns duplex-link $r0 $n2 100Mb 5ms DropTail
$ns queue-limit $n1 $r0 2000
$ns queue-limit $r0 $n2 2000

#jjkk
#subflow 1 path 

$ns duplex-link $r1 $r4 100Mb 5ms DropTail

#$ns duplex-link $r4 $h1 [expr (1000 - ($lambda * 2) )]Kb 30ms DropTail
$ns duplex-link $r4 $h1 [expr (700 - ($lambda * 2) )]Kb 30ms DropTail

#$ns duplex-link $r4 $h1 300Kb 40ms DropTail

# !@



$ns duplex-link $n2_0 $r1 100Mb 5ms DropTail
$ns queue-limit $n2_0 $r1 200
$ns duplex-link $n2_1 $r2 100Mb 5ms DropTail
$ns queue-limit $n2_1 $r2 200
$ns duplex-link $n2_2 $r3 100Mb 5ms DropTail
$ns queue-limit $n2_2 $r3 200

$ns duplex-link $h1 $n3_0 10Mb 10ms DropTail
$ns duplex-link $h2 $n3_1 10Mb 10ms DropTail
$ns duplex-link $h3 $n3_2 10Mb 10ms DropTail
#$ns queue-limit $r4 $h1 20

#subflow 2 path

$ns duplex-link $r2 $r5 100Mb 5ms DropTail
$ns queue-limit $r2 $r5 2000

#$ns duplex-link $r5 $h2 [expr (1000 - ($lambda * 1) )]Kb 30ms DropTail
$ns duplex-link $r5 $h2 [expr (700 - ($lambda * 1) )]Kb 30ms DropTail
#$ns queue-limit $r5 $h2 20

#subflow 3 path

$ns duplex-link $r3 $r6 100Mb 5ms DropTail
$ns queue-limit $r3 $r6 2000

#$ns duplex-link $r6 $h3 1000Kb 30ms DropTail
#$ns duplex-link $r6 $h3 700Kb 40ms DropTail
$ns duplex-link $r6 $h3 700Kb 30ms DropTail


#$ns queue-limit $r6 $h3 20
$ns queue-limit $r6 $r3 2000


set YouTubeflow_0 [[$ns link $n1 $r0] queue]
set YouTubeflow_1 [[$ns link $r0 $n2] queue]

set subflow1_0 [[$ns link $n2_0 $r1] queue]
set subflow1_1 [[$ns link $r1 $r4] queue]
set subflow1_2 [[$ns link $r4 $h1] queue]
set subflow1_3 [[$ns link $h1 $n3_0] queue]

set subflow2_0 [[$ns link $n2_1 $r2] queue]
set subflow2_1 [[$ns link $r2 $r5] queue]
set subflow2_2 [[$ns link $r5 $h2] queue]
set subflow2_3 [[$ns link $h2 $n3_1] queue]

set subflow3_0 [[$ns link $n2_2 $r3] queue]
set subflow3_1 [[$ns link $r3 $r6] queue]
set subflow3_1_1 [[$ns link $r6 $r3] queue]
set subflow3_2 [[$ns link $r6 $h3] queue]
set subflow3_3 [[$ns link $h3 $n3_2] queue]




# YouTube server, turn off slow start restart 
set tcp1 [new Agent/TCP/FullTcp]
$tcp1 set segsize_ 1303
$tcp1 set window_ 120	
$tcp1 set ssthresh_ $tcp_initial_ssthresh
$tcp1 set nodelay_ true;
$tcp1 set timestamps_ true
$tcp1 set windowInit_ $init_cwnd;	
$tcp1 set slow_start_restart_ false;		
$ns attach-agent $n1 $tcp1

set sink1 [new Agent/TCP/FullTcp]
$sink1 set segsize_ 50
$ns attach-agent $n2 $sink1 
$sink1 set timestamps_ true
$sink1 listen
$sink1 forward 0 1;		#forwarding mode capture packets from 0 to 1 (注意index)
$ns connect $tcp1 $sink1


#set tcp2 [new Agent/TCP/FullTcp]
#$tcp2 set segsize_ 1303
#$tcp2 set window_ $windowSize	
#$tcp2 set nodelay_ true;
#$tcp2 set timestamps_ true
#$tcp2 set windowInit_ $init_cwnd;	
#$tcp2 set slow_start_restart_ $slowStartRestart;		
#$ns attach-agent $n2 $tcp2

#set sink2 [new Agent/TCP/FullTcp]
#$ns attach-agent $n3 $sink2 
#$sink2 set timestamps_ true
#$sink2 listen
#$ns connect $tcp2 $sink2
#$ns at 20.0 "$tcp2 send 333"



set tfile [new Tracefile]
$tfile filename example-trace

set trace2 [new Application/Traffic/Trace]
$trace2 attach-tracefile $tfile
$trace2 attach-agent $tcp1
Tracefile set debug_ 0

set trace3 [new Application/Traffic/Trace]
$trace3 attach-tracefile $tfile
$trace3 attach-agent $mptcp
$trace3 forwarder-mode 1
$trace3 video-bitrate $bitrate
$trace3 alpha 1.25
#$trace3 alpha [lindex $argv 0]
$trace3 ONOFFmode 1;		# 1 = enable
#$trace3 ONOFFmode 0;		# 0 = disable (要改用YouTube的traffic)

Tracefile set debug_ 0

#$ns at 0.6 "$trace2 send 1800"
#$ns at 0.65 "$trace2 send 600"


set cbr1 [new Application/Traffic/CBR]
$cbr1 set packetSize_ 1303
#$cbr1 set interval_ 0.001
#$cbr1 set interval_ 0.01
#$cbr1 set interval_ 0.1
$cbr1 set rate_ 1000Kb
$cbr1 set random_ 1
$cbr1 attach-agent $tcp1

set ftp1 [new Application/FTP]
$ftp1 attach-agent $tcp1


# ------------- cross traffic configuration -----------------------------

set crt1 [$ns node];# cross traffic node 1
set crt2 [$ns node];# cross traffic node 2
set crt3 [$ns node];# cross traffic node 3
set crt4 [$ns node];# cross traffic node 4

$ns duplex-link $crt1 $r3 100Mb 10ms DropTail
$ns duplex-link $crt2 $r6 100Mb 35ms DropTail
$ns duplex-link $crt3 $r2 100Mb 10ms DropTail
$ns duplex-link $crt4 $r5 100Mb 35ms DropTail


#set tcp_crt_1 [new Agent/TCP/FullTcp]
#$ns attach-agent $crt1 $tcp_crt_1
#$tcp_crt_1 set segsize_ 1303
#$tcp_crt_1 set window_ $windowSize	
#$tcp_crt_1 set nodelay_ true;
#$tcp_crt_1 set timestamps_ true
#$tcp_crt_1 set windowInit_ $init_cwnd;	
#$tcp_crt_1 set slow_start_restart_ $slowStartRestart;		

#set sink_crt_1 [new Agent/TCP/FullTcp]
#$ns attach-agent $crt2 $sink_crt_1 
#$sink_crt_1 set timestamps_ true
#$sink_crt_1 listen
#$ns connect $tcp_crt_1 $sink_crt_1

set udp_crt_1 [new Agent/UDP]
$udp_crt_1 set packetSize_ 1460
$ns attach-agent $crt1 $udp_crt_1
#$ns attach-agent $r3 $udp_crt_1
set sink_crt_1 [new Agent/Null]
$ns attach-agent $r6 $sink_crt_1
$ns connect $udp_crt_1 $sink_crt_1


set cbr_crt_1 [new Application/Traffic/CBR]
$cbr_crt_1 set packetSize_ 1400
#$cbr_crt_1 set packetSize_ 58400
#$cbr_crt_1 set interval_ 0.0012
#$cbr_crt_1 set interval_ 0.00205
$cbr_crt_1 set interval_ 0.00245

#$cbr_crt_1 set interval_ 1
#$cbr_crt_1 set interval_ 5
#$cbr_crt_1 set rate_ 64Kb
#$cbr_crt_1 set random_ 1
#$cbr_crt_1 attach-agent $tcp_crt_1
$cbr_crt_1 attach-agent $udp_crt_1


#set tcp_crt_2 [new Agent/TCP/FullTcp]
#$ns attach-agent $crt3 $tcp_crt_2
#$tcp_crt_2 set segsize_ 1303
#$tcp_crt_2 set window_ $windowSize	
#$tcp_crt_2 set nodelay_ true;
#$tcp_crt_2 set timestamps_ true
#$tcp_crt_2 set windowInit_ $init_cwnd;	
#$tcp_crt_2 set slow_start_restart_ $slowStartRestart;		

#set sink_crt_2 [new Agent/TCP/FullTcp]
#$ns attach-agent $crt4 $sink_crt_2 
#$sink_crt_2 set timestamps_ true
#$sink_crt_2 listen
#$ns connect $tcp_crt_2 $sink_crt_2

set udp_crt_2 [new Agent/UDP]
$udp_crt_2 set packetSize_ 1460
$ns attach-agent $crt3 $udp_crt_2
#$ns attach-agent $r2 $udp_crt_2
set sink_crt_2 [new Agent/Null]
#$ns attach-agent $crt4 $sink_crt_2
$ns attach-agent $r5 $sink_crt_2
$ns connect $udp_crt_2 $sink_crt_2

set cbr_crt_2 [new Application/Traffic/CBR]
$cbr_crt_2 set packetSize_ 1420
$cbr_crt_2 set interval_ 0.00232
#$cbr_crt_2 set interval_ 5
#$cbr_crt_2 set rate_ 64Kb
#$cbr_crt_2 set random_ 1
#$cbr_crt_2 attach-agent $tcp_crt_2
$cbr_crt_2 attach-agent $udp_crt_2
 

set udp_crt_3 [new Agent/UDP]
$udp_crt_3 set packetSize_ 1460
$ns attach-agent $r6 $udp_crt_3
set sink_crt_3 [new Agent/Null]
$ns attach-agent $crt1 $sink_crt_3
$ns connect $udp_crt_3 $sink_crt_3

set cbr_crt_3 [new Application/Traffic/CBR]
$cbr_crt_3 set packetSize_ 1420
$cbr_crt_3 set interval_ 0.0013
$cbr_crt_3 attach-agent $udp_crt_3 
 
# ------------- cross traffic configuration -----------------------------

#開啟一個檔案，用來記錄TCP Flow的congestion window變化情況
set YouTube_wnd_trace [open YouTube_cwnd.txt w]
set YouTube_srtt_trace [open YouTube_srtt.txt w]

set wnd_trace [open cwnd_3paths.txt w]
set rtt_trace [open rtt_3paths.txt w]
set rttvar_trace [open rttvar_3paths.txt w]
set ssthresh_trace [open ssthresh_3paths.txt w]

proc record_2 {} {
    global ns tcp2_0 tcp2_1 tcp2_2 tcp1
    global wnd_trace YouTube_wnd_trace
	global YouTube_srtt_trace	
	global ssthresh_trace
    set time_2 0.05
         
    #讀取C++內cwnd_的變數值
    set now [$ns now]
	set curr_cwnd0 [$tcp2_0 set cwnd_]    
    set curr_cwnd1 [$tcp2_1 set cwnd_]
    set curr_cwnd2 [$tcp2_2 set cwnd_]	
    puts $wnd_trace "$now\t$curr_cwnd0\t$curr_cwnd1\t$curr_cwnd2"
		
	set curr_ssthresh0 [$tcp2_0 set ssthresh_]    
    set curr_ssthresh1 [$tcp2_1 set ssthresh_]
    set curr_ssthresh2 [$tcp2_2 set ssthresh_]	
    puts $ssthresh_trace "$now\t$curr_ssthresh0\t$curr_ssthresh1\t$curr_ssthresh2"
	
	
	set curr_YouTube_cwnd [$tcp1 set cwnd_]
	 puts $YouTube_wnd_trace "$now\t$curr_YouTube_cwnd"
	
	set curr_YouTube_srtt [$tcp1 set srtt_]
	puts $YouTube_srtt_trace "$now\t[expr ((($curr_YouTube_srtt/2)/2)/2 * 0.01)]"	
	
    $ns at [expr $now+$time_2] "record_2"
}


proc record {} {
    global ns tcp2_0 tcp2_1 tcp2_2     
	global rtt_trace 	
	global rttvar_trace
    set time 0.01
         
    #讀取C++內cwnd_的變數值
    set now [$ns now]
	
	set curr_rtt0 [$tcp2_0 set rtt_]    
    set curr_rtt1 [$tcp2_1 set rtt_]    
    set curr_rtt2 [$tcp2_2 set rtt_]    
    puts $rtt_trace "$now\t[expr ($curr_rtt0 * 0.01)]\t[expr ($curr_rtt1 * 0.01)]\t[expr ($curr_rtt2 * 0.01)]"		
	#puts $rtt_trace "$now\t$curr_rtt0\t$curr_rtt1\t$curr_rtt2"		
	
	set curr_rttvar0 [$tcp2_0 set rttvar_]    
    set curr_rttvar1 [$tcp2_1 set rttvar_]    
    set curr_rttvar2 [$tcp2_2 set rttvar_]    
    puts $rttvar_trace "$now\t[expr ($curr_rttvar0 * 0.01)/4.0]\t[expr ($curr_rttvar1 * 0.01)/4.0]\t[expr ($curr_rttvar2 * 0.01)/4.0]"		
		
    $ns at [expr $now+$time] "record"
}

# ====================== loss model =============================
#set durlistA        "1.0 0.2";     # unblocked and blocked state duration avg value
	
#set loss_moduleTwoState [new ErrorModel/TwoStateMarkov $durlistA time]
#set loss_moduleTwoState [new ErrorModel/TwoState $durlistA time]


set loss_module0 [new ErrorModel]
$loss_module0 set rate_ $loss_rate
$loss_module0 unit pkt
$loss_module0 ranvar [new RandomVariable/Uniform]
$loss_module0 drop-target [new Agent/Null]


set loss_module1 [new ErrorModel]
$loss_module1 set rate_ $loss_rate
$loss_module1 unit pkt
$loss_module1 ranvar [new RandomVariable/Uniform]
$loss_module1 drop-target [new Agent/Null]

#RandomVariable/Exponential set avg_ 3
RandomVariable/Exponential set avg_ 2
RandomVariable/Pareto set avg_ 0.4
RandomVariable/Pareto set shape_ 1.1

# creating the uniform distribution random variable
set loss_random_variable [new RandomVariable/Uniform]
$loss_random_variable set min_ 0 ;# set the range of the random variable
$loss_random_variable set max_ 100

set loss_module2 [new ErrorModel]
$loss_module2 set rate_ $loss_rate
$loss_module2 unit pkt
#$loss_module2 ranvar $loss_random_variable
$loss_module2 ranvar [new RandomVariable/Uniform]
#$loss_module2 ranvar [new RandomVariable/Exponential]
#$loss_module2 ranvar [new RandomVariable/Pareto]
$loss_module2 drop-target [new Agent/Null]

#jjkk
#$ns lossmodel $loss_module0 $r4 $h1
#$ns lossmodel $loss_module1 $r5 $h2    
#$ns lossmodel $loss_module2 $r6 $h3
#$ns lossmodel $loss_module2 $n2_2 $r3

    
# ===============================================================

# V18 因為有寫ON OFF, rescheduling 所以這邊不active, 封包不會送
$mptcp active 0     
$mptcp active 1
$mptcp active 2
$mptcp active 3
$mptcp active 4
$mptcp active 5
$mptcpsink active 0
$mptcpsink active 1
$mptcpsink active 2
$mptcpsink active 3
$mptcpsink active 4
$mptcpsink active 5

$ns at 0.1 "record"
$ns at 0.1 "record_2"

#for connection setup
$ns at 0.1 "$tcp2_0 send 1303"
$ns at 0.1 "$tcp2_1 send 1303"
$ns at 0.1 "$tcp2_2 send 1303"


$ns at 10.1 "$ftp1 start"     
$ns at 16.1 "$ftp1 stop"     


#$ns at 20.1 "connect $tcp2_0 $sink3_0"
#$ns at 20.1 "$sink3_0 listen"

# 這邊沒辦法清除subflow status後在reactive 
# 有兩個解決方法, 讓斷掉的subflow把封包送完 這樣直接改變status 可繼續送
# 或者是斷掉的subflow就不修復, 直接新增新的subflow

   

#$ns at 0.1 "$cbr1 start"     
#$ns at 550.1 "$cbr1 stop"

#$ns at 30.1 "$cbr_crt_1 start"     
#$ns at 70.1 "$cbr_crt_1 stop"     
#$ns at 40.1 "$cbr_crt_2 start"     
#$ns at 80.1 "$cbr_crt_2 stop"     
#$ns at 20.1 "$cbr_crt_3 start"     
#$ns at 30.1 "$cbr_crt_3 stop"     

#source "YouTube_set2.tr"
#$ns at 0.1 "$trace2 start"

$ns at 0.1 "$trace3 start"

$ns at [expr ($simDurationEnd -1)] "$mptcpsink dump_report"
$ns at $simDurationEnd "finish"

 for {set t $simDurationStart} {$t < $simDurationEnd} {set t [expr ($t+$dumpInterval)]} {	 
	$ns at $t "$subflow1_0 printstats 1"	
	$ns at $t "$subflow1_1 printstats 2"
	$ns at $t "$subflow1_2 printstats 3"
	$ns at $t "$subflow1_3 printstats 4"
	
	$ns at $t "$subflow2_0 printstats 5"	
	$ns at $t "$subflow2_1 printstats 6"
	$ns at $t "$subflow2_2 printstats 7"
	$ns at $t "$subflow2_3 printstats 8"
 
	$ns at $t "$subflow3_0 printstats 9"	
	$ns at $t "$subflow3_1 printstats 10"
	$ns at $t "$subflow3_2 printstats 11"
	$ns at $t "$subflow3_3 printstats 12"
	$ns at $t "$subflow3_1_1 printstats 15"	
	
	$ns at $t "$YouTubeflow_0 printstats 13"
	$ns at $t "$YouTubeflow_1 printstats 14"
 }
 
  
proc finish {} {
	global ns 
	global wnd_trace YouTube_wnd_trace
	global rtt_trace YouTube_srtt_trace ssthresh_trace
	global rttvar_trace
	$ns flush-trace
	close $wnd_trace
	close $rtt_trace
	close $rttvar_trace
	close $ssthresh_trace
	exit 0
}

$ns run


