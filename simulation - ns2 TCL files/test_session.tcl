set ns [new SessionSim] ;# preamble initialization
set node [$ns node]
set group [$ns allocaddr]

set udp [new Agent/UDP] ;# create and configure the source
$udp set dst_ $group
set src [new Application/Traffic/CBR]
$src attach-agent $udp
$ns attach-agent $node $udp
$ns create-session $node $udp ;# create attach session helper to src
set rcvr [new Agent/NULL] ;# configure the receiver
$ns attach-agent $node $rcvr
$ns at 0.0 "$node join-group $rcvr $group" ;# joining the session
$ns at 0.1 "$src start"

$ns at 20.0 "finish"

proc finish {} {
	global ns wnd_trace1 
	$ns flush-trace
	close $wnd_trace1 
	exit 0
}

$ns run
