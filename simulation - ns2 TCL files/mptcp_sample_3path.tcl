set init_cwnd 3;		# default before linux 3.0
#set init_cwnd 10;		# default after linux 3.0, google recommend 
set slowStartRestart true;
#set slowStartRestart false;
set simDurationStart 0
set simDurationEnd 550
set sendInterval 0.1

#set windowSize 1000;
set windowSize 5000;
#set windowSize 2000;


set ns [new Simulator]

#
# specify to print mptcp option information
#
Trace set show_tcphdr_ 2

#
# setup trace files
#
set f [open out_3path.tr w]
$ns trace-all $f
#set nf [open out_3path.nam w]
#$ns namtrace-all $nf


#
# mptcp sender
#
set n0 [$ns node]
set n0_0 [$ns node]
set n0_1 [$ns node]
set n0_2 [$ns node]
$n0 color green
$n0_0 color red
$n0_1 color red
$n0_2 color red
$ns multihome-add-interface $n0 $n0_0
$ns multihome-add-interface $n0 $n0_1
$ns multihome-add-interface $n0 $n0_2

#
# mptcp receiver
#
set n1 [$ns node]
set n1_0 [$ns node]
set n1_1 [$ns node]
set n1_2 [$ns node]
$n1 color yellow
$n1_0 color blue
$n1_1 color blue
$n1_2 color blue
$ns multihome-add-interface $n1 $n1_0
$ns multihome-add-interface $n1 $n1_1
$ns multihome-add-interface $n1 $n1_2


#
# intermediate nodes 
#
set r1 [$ns node]
set r2 [$ns node]
set r3 [$ns node]

#Agent/TCP/FullTcp set segsize_ 1460

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
set tcp0 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp0 set window_ $windowSize	
$tcp0 set segsize_ 1303
$tcp0 set nodelay_ true;           # disabling nagle  (非常重要, 如果false, tcp layer會等segment full才送packet)
$tcp0 set timestamps_ true
$tcp0 set windowInit_ $init_cwnd;	
$tcp0 set slow_start_restart_ $slowStartRestart;		
$ns attach-agent $n0_0 $tcp0

set tcp1 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp1 set window_ $windowSize	
$tcp1 set segsize_ 1303
$tcp1 set nodelay_ true;           # disabling nagle  (非常重要, 如果false, tcp layer會等segment full才送packet)
$tcp1 set timestamps_ true
$tcp1 set windowInit_ $init_cwnd;			
$tcp1 set slow_start_restart_ $slowStartRestart;		
$ns attach-agent $n0_1 $tcp1

set tcp2 [new Agent/TCP/FullTcp/Sack/Multipath]
$tcp2 set window_ $windowSize	
$tcp2 set segsize_ 1303
$tcp2 set nodelay_ true;           # disabling nagle  (非常重要, 如果false, tcp layer會等segment full才送packet)
$tcp2 set timestamps_ true
$tcp2 set windowInit_ $init_cwnd;			
$tcp2 set slow_start_restart_ $slowStartRestart;		
$ns attach-agent $n0_2 $tcp2

set mptcp [new Agent/MPTCP]
$mptcp attach-tcp $tcp0
$mptcp attach-tcp $tcp1
$mptcp attach-tcp $tcp2

$ns multihome-attach-agent $n0 $mptcp


# method 1: small rtt first
# method 2: small pricing first
# method 3: round robin
$mptcp schedule-mode 1

#$ns_ duplex-link $n(0) $r(0) $bandwidth $delay $delay_std DropTail

#Agent/TCP set timestamps_ true


$ns duplex-link $n0_0 $r1 100Kb 5ms DropTail
$ns duplex-link $n1_0 $r1 100Kb 5ms DropTail

$ns duplex-link $n0_1 $r2 500Kb 5ms DropTail
$ns duplex-link $n1_1 $r2 500Kb 5ms DropTail

$ns duplex-link $n0_2 $r3 1Mb 5ms DropTail
$ns duplex-link $n1_2 $r3 1Mb 5ms DropTail




set subflow1 [[$ns link $n0_0 $r1] queue]
set subflow2 [[$ns link $n0_1 $r2] queue]
set subflow3 [[$ns link $n0_2 $r3] queue]

$ns queue-limit $n0_0 $r1 100000000
$ns queue-limit $n1_0 $r1 100000000

$ns queue-limit $n0_1 $r2 100000000
$ns queue-limit $n1_1 $r2 100000000

$ns queue-limit $n0_2 $r3 100000000
$ns queue-limit $n1_2 $r3 100000000


#set ftp [new Application/FTP]
#$ftp attach-agent $mptcp

set tfile [new Tracefile]
$tfile filename example-trace

set trace2 [new Application/Traffic/Trace]
$trace2 attach-tracefile $tfile
$trace2 attach-agent $mptcp
Tracefile set debug_ 0

source "YouTube_set2.txt"
#$ns at 0.6 "$trace2 send 1800"
#$ns at 0.65 "$trace2 send 600"

#
# create mptcp receiver
#
set mptcpsink [new Agent/MPTCP]

set sink0 [new Agent/TCP/FullTcp/Sack/Multipath]
$ns attach-agent $n1_0 $sink0 
$sink0 set timestamps_ true

set sink1 [new Agent/TCP/FullTcp/Sack/Multipath]
$ns attach-agent $n1_1 $sink1 
$sink1 set timestamps_ true

set sink2 [new Agent/TCP/FullTcp/Sack/Multipath]
$ns attach-agent $n1_2 $sink2 
$sink2 set timestamps_ true

$mptcpsink attach-tcp $sink0
$mptcpsink attach-tcp $sink1
$mptcpsink attach-tcp $sink2

$ns multihome-attach-agent $n1 $mptcpsink

$ns multihome-connect $mptcp $mptcpsink
$mptcpsink listen

$mptcpsink set-price 0 1.8
$mptcpsink set-price 1 2
$mptcpsink set-price 2 2.2


#開啟一個檔案，用來記錄TCP Flow的congestion window變化情況
set wnd_trace0 [open cwnd_3path0.tr w]
set wnd_trace1 [open cwnd_3path1.tr w]
set wnd_trace2 [open cwnd_3path2.tr w]

proc record {} {
    global ns tcp0 tcp1 tcp2
    global wnd_trace0 wnd_trace1 wnd_trace2
    set time 0.05
         
    #讀取C++內cwnd_的變數值
    set curr_cwnd0 [$tcp0 set cwnd_]
    set now [$ns now]
    puts $wnd_trace0 "$now\t$curr_cwnd0 "
   
    set curr_cwnd1 [$tcp1 set cwnd_]
    set now [$ns now]
    puts $wnd_trace1 "$now\t$curr_cwnd1 "
    
    set curr_cwnd2 [$tcp2 set cwnd_]
    set now [$ns now]
    puts $wnd_trace2 "$now\t$curr_cwnd2 "

    $ns at [expr $now+$time] "record"
}

# set monitor [$ns monitor-queue $n0_0 $r1 [open qm.out w] 0.1]

proc finish {} {
	global ns f 
#	global nf
        global wnd_trace0 wnd_trace1 wnd_trace2
	$ns flush-trace
	close $f
#	close $nf   
        close $wnd_trace0 
        close $wnd_trace1 
        close $wnd_trace2
	exit
}

$ns at 0.1 "record"
#$ns at 0.1 "$ftp start"     
$ns at 0.1 "$trace2 start"
#$ns at 50 "finish"
$ns at $simDurationEnd "finish"

 for {set t $simDurationStart} {$t < $simDurationEnd} {set t [expr ($t+$sendInterval)]} {	 
	$ns at $t "$subflow1 printstats 1"	
	$ns at $t "$subflow2 printstats 2"
	$ns at $t "$subflow3 printstats 3"
 }
$ns run
