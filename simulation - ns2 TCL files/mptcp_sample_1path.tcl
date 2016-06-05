#ns mptcp-sample-node01.tcl > outprintf-node01
#
# sample script for mptcp implementation on ns-2
#
#      Yoshifumi Nishida <nishida@sfc.wide.ad.jp>
#
#
#
#
#
#          --------tcp0                sink0--------
#         /         |                    |          \
#        /          |                    |           \
#       /         n0_0--------r1--------n1_0          \
#      /           /                      \            \
#     /           /                        \            \
#    /           /                          \            \
#  mptcp--------n0                           n1--------mptcpsink
#
#
#
#
#
set ns [new Simulator]

#
# specify to print mptcp option information
#
Trace set show_tcphdr_ 2

#
# setup trace files
#
set f [open out_1path.tr w]
$ns trace-all $f
#set nf [open out_1path.nam w]
#$ns namtrace-all $nf


#
# mptcp sender
#
set n0 [$ns node]
set n0_0 [$ns node]

$ns multihome-add-interface $n0 $n0_0

#
# mptcp receiver
#
set n1 [$ns node]
set n1_0 [$ns node]

$ns multihome-add-interface $n1 $n1_0



#
# intermediate nodes 
#
set r1 [$ns node]

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
$tcp0 set window_ 1000	
$tcp0 set segsize_ 1303
$tcp0 set nodelay_ true;           # disabling nagle  (非常重要, 如果false, tcp layer會等segment full才送packet)
$ns attach-agent $n0_0 $tcp0

set mptcp [new Agent/MPTCP]
$mptcp attach-tcp $tcp0

# method 1: small rtt first
# method 2: small pricing first
$mptcp schedule-mode 1

$ns duplex-link $n0_0 $r1 5Mb 5ms DropTail
$ns duplex-link $n1_0 $r1 5Mb 5ms DropTail

$ns queue-limit $n0_0 $r1 100000000
$ns queue-limit $n1_0 $r1 100000000


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

$ns multihome-attach-agent $n0 $mptcp

#
# create mptcp receiver
#mptcp
set mptcpsink [new Agent/MPTCP]
set sink0 [new Agent/TCP/FullTcp/Sack/Multipath]
$ns attach-agent $n1_0 $sink0 

$mptcpsink attach-tcp $sink0

$ns multihome-attach-agent $n1 $mptcpsink

$ns multihome-connect $mptcp $mptcpsink
$mptcpsink listen

# 一定要給個數值 不然不能跑XD
$mptcpsink set-price 0 1.8

#開啟一個檔案，用來記錄TCP Flow的congestion window變化情況
set wnd_trace0 [open cwnd_1path0.tr w]

proc record {} {
    global ns tcp0 
    global wnd_trace0
    set time 0.05
         
    #讀取C++內cwnd_的變數值
    set curr_cwnd0 [$tcp0 set cwnd_]
    set now [$ns now]
    puts $wnd_trace0 "$now   $curr_cwnd0 "
    $ns at [expr $now+$time] "record"
}

proc finish {} {
    global ns f wnd_trace0
#   global nf
    $ns flush-trace
    close $f
#    close $nf    
    close $wnd_trace0
    exit
}

$ns at 0.1 "record"
#$ns at 0.1 "$ftp start"     
$ns at 0.1 "$trace2 start"
$ns at 550 "finish"

$ns run
