j=0
for (( i=0; i<=50; i=i+10 ))
do
	j=$(($j+1))
	echo run $i $j
	
	ns cbsp_3_subflows.tcl $i > 3
	grep dumpSentPkts 3 > numSentPkts.txt
	grep dumpSRTT 3 > n_subflowSRTT.txt
	grep dumpRTT 3 > n_subflowRTT_please_ref_rtt_3paths.txt
	grep dumpRTO 3 > n_subflowRTO.txt
	grep dumpRttTs 3 > n_subflowRtt_ts.txt
	grep dumpResults 3 > results.txt
	grep dumpScheduledPath 3 > n_schPath.txt
	grep dumpScheduledLogSubflow1 3 > n_schPath_subflow1.txt
	grep dumpScheduledLogSubflow2 3 > n_schPath_subflow2.txt
	grep dumpScheduledLogSubflow3 3 > n_schPath_subflow3.txt
	grep Dump_inorderTriggerSubflow 3 > n_whoTriggerInorder.txt
	grep Trigger_Subflow_\(0 3 > n_whoTriggerInorder_subflow0.txt
	grep Trigger_Subflow_\(1 3 > n_whoTriggerInorder_subflow1.txt
	grep Trigger_Subflow_\(2 3 > n_whoTriggerInorder_subflow2.txt
	grep dumpSubflow1sentSize 3 > n_dumpSubflowSentSize_subflow1.txt 
	grep dumpSubflow2sentSize 3 > n_dumpSubflowSentSize_subflow2.txt 
	grep dumpSubflow3sentSize 3 > n_dumpSubflowSentSize_subflow3.txt 
	grep dumpoutstandingandavailablecwnd 3 > n_dump_outstanding_and_available_cwnd.txt 
	grep dumpReceivedACKSubflow0SRTT 3 > n_dumpReceivedACKSRTT_subflow1.txt 
	grep dumpReceivedACKSubflow1SRTT 3 > n_dumpReceivedACKSRTT_subflow2.txt 
	grep dumpReceivedACKSubflow2SRTT 3 > n_dumpReceivedACKSRTT_subflow3.txt 
	grep forwardQueue_size 3 > n_forwarding_queue_size.txt 
	grep dumpQueueLengthlink1_ 3 > n_dumpQueueLengthSubflow_link1.txt
	grep dumpQueueLengthlink2_ 3 > n_dumpQueueLengthSubflow_link2.txt
	grep dumpQueueLengthlink3_ 3 > n_dumpQueueLengthSubflow_link3.txt 
	grep dumpQueueLengthlink4_ 3 > n_dumpQueueLengthSubflow_link4.txt 
	grep dumpQueueLengthlink5_ 3 > n_dumpQueueLengthSubflow_link5.txt 
	grep dumpQueueLengthlink6_ 3 > n_dumpQueueLengthSubflow_link6.txt 
	grep dumpQueueLengthlink7_ 3 > n_dumpQueueLengthSubflow_link7.txt 
	grep dumpQueueLengthlink8_ 3 > n_dumpQueueLengthSubflow_link8.txt 
	grep dumpQueueLengthlink9_ 3 > n_dumpQueueLengthSubflow_link9.txt 
	grep dumpQueueLengthlink10_ 3 > n_dumpQueueLengthSubflow_link10.txt 
	grep dumpQueueLengthlink11_ 3 > n_dumpQueueLengthSubflow_link11.txt 
	grep dumpQueueLengthlink12_ 3 > n_dumpQueueLengthSubflow_link12.txt 
	grep dumpQueueLengthlink13_ 3 > n_dumpQueueLengthYouTubeflow_link13.txt 
	grep dumpQueueLengthlink14_ 3 > n_dumpQueueLengthYouTubeflow_link14.txt 
	grep dumpQueueLengthlink15_ 3 > n_dumpQueueLengthSubflow_link15.txt 
	grep Available_Schedule_rate 3 > n_Available_Schedule_rate.txt 
	grep MpFullTcpAgent::sendpacket 3 > n_TCP_SENT_LOG.txt 
	grep dumpcwinpipe 3 > n_dumpcwinpipe.txt 
	grep __RXTX__ 3 > n_RXTX_log.txt 
	grep dumpSubflow1sentSeq 3 > n_dumpSubflowSentSeq_subflow1.txt   
	grep dumpSubflow2sentSeq 3 > n_dumpSubflowSentSeq_subflow2.txt 
	grep dumpSubflow3sentSeq 3 > n_dumpSubflowSentSeq_subflow3.txt 
	grep dump_subflow1_numSACK 3 > n_NumSack_subflow1.txt 
	grep dump_subflow2_numSACK 3 > n_NumSack_subflow2.txt 
	grep dump_subflow3_numSACK 3 > n_NumSack_subflow3.txt 
	awk -f dumpDropSubflow1.awk out.tr > n_dumpDropSubflow1.txt 
	awk -f dumpDropSubflow2.awk out.tr > n_dumpDropSubflow2.txt
	awk -f dumpDropSubflow3.awk out.tr > n_dumpDropSubflow3.txt 
	grep dumpSubflow1_MPTCP_sentSeq_dsn__sending_queue_size 3 > n_dumpsentSeq_dsn__sending_queue_size_subflow1.txt 
	grep dumpSubflow2_MPTCP_sentSeq_dsn__sending_queue_size 3 > n_dumpsentSeq_dsn__sending_queue_size_subflow2.txt 
	grep dumpSubflow3_MPTCP_sentSeq_dsn__sending_queue_size 3 > n_dumpsentSeq_dsn__sending_queue_size_subflow3.txt 
	grep schedulerLog 3 > schedulerLog.txt 
	grep schedulingEnqueue 3 > schedulingEnqueueLog.txt
	grep schedulingDequeue 3 > schedulingDequeueLog.txt
	awk -f dumpReceivedDSN.awk out.tr > DSNreceivedTime.txt
	awk -f dumpSentDSN.awk out.tr > DSNsentTime.txt
	grep DumpStaticExpectTime 3 > DumpStaticExpectTime.txt
	sort -n -k 4 DSNsentTime.txt > Sort_DSNsentTime.txt
	sort -n -k 4 DSNreceivedTime.txt > Sort_DSNreceivedTime.txt
	sort -n -k 6 DumpStaticExpectTime.txt > Sort_DumpStaticExpectTime.txt
	grep dumpSubflow_scheduled_queue 3 > dumpSubflow_scheduled_queue.txt 
	grep availPathCounter 3 > availPathCounter.txt 
	grep dumpplayBuffer 3 > dumpPlayBuffer.txt 
	grep dump_expect_time_subflow0 3 > dump_expect_time_subflow1.txt 
	grep dump_expect_time_subflow1 3 > dump_expect_time_subflow2.txt 
	grep dump_expect_time_subflow2 3 > dump_expect_time_subflow3.txt 
	grep dump_min_path 3 > dump_min_path.txt 
	grep dumpTCPdominateSch 3 > dumpTCPdominateSch.txt 
	
	mv *.txt $j-/
done

