/*
 * Copyright (C) 2011 WIDE Project.  All rights reserved.
 *
 * Yoshifumi Nishida  <nishida@sfc.wide.ad.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ip.h"
#include "flags.h"
#include "random.h"
#include "template.h"
#include "mptcp.h"
#include <math.h>
#include <time.h>
#include <algorithm>
#include <iostream>

// method 1: small rtt first
// method 2: small pricing first
// method 3: round robin
// method 4: ETOM
int method = 0;
#define ETOM_PacketSize (1369.0 * 8.0)
#define max_SchQueue_size 1000000
//#define max_SchQueue_size 3000
//#define max_SchQueue_size 300
//#define max_SchQueue_size 50
//#define max_SchQueue_size 20
//#define max_SchQueue_size 10

bool enableTrafficPacing = false;
bool RTTwithSchQueueMode = false;
//bool RTTwithSchQueueMode = true;
//bool enableRandSelectSameRTTlink = false;
bool enableRandSelectSameRTTlink = true;		
int NumQueueThreshold = 10;  // pacing use only 
int clientPlayoutStatus = 1;

double lowWaterBuff = -1;
double highWaterBuff = -1;
int limitBuffMode = 0;
int limitBuffSize = 0;
double video_bitrate = 0;

static class MptcpClass:public TclClass
{
public:
  MptcpClass ():TclClass ("Agent/MPTCP")
  {
  }
  TclObject *create (int, const char *const *)
  {
    return (new MptcpAgent ());
  }
}

class_mptcp;
//----------------------------------------------------
int mptcp_total_number = 0;
int record_subflow_sent_ptks[MAX_SUBFLOW] = {0};
int record_subflow_scheduled_queue[MAX_SUBFLOW] = {0};
double record_subflow_rtt[MAX_SUBFLOW] = {0};
double record_subflow_srtt[MAX_SUBFLOW] = {0};
double record_subflow_rtt_ts[MAX_SUBFLOW] = {0};
double record_subflow_rto[MAX_SUBFLOW] = {0};
int record_subflow_cwndReset[MAX_SUBFLOW] = {0};
long currentSendQueueSize = 0;
double assignPricing[MAX_SUBFLOW] = {0};
double total_throughput = 0;
double time_throughput[1000] = {0};
double total_delay = 0;
double total_reordering_delay = 0;
double total_pricing = 0;
double total_counter = 0;
double total_counter_reorder = 0;
double subflow1_reordering_delay = 0;
double subflow2_reordering_delay = 0;
double subflow3_reordering_delay = 0;
double subflow1_counter_reorder = 0;
double subflow2_counter_reorder = 0;
double subflow3_counter_reorder = 0;
double total_out_of_order_pkt = 0;

double record_subflow_sentCounter[MAX_SUBFLOW] = {0};
double record_subflow_idleTime[MAX_SUBFLOW] = {0};
int    record_subflow_Busy[MAX_SUBFLOW] = {0};
int    record_subflow_Util[10000][MAX_SUBFLOW] = {0};
int    record_subflow_SentPkts[1000][MAX_SUBFLOW] = {0};
int    record_subflow_ackno[MAX_SUBFLOW] = {0};
int    record_eschedulingQueueSize = 0;

int playout_Status = 0;		
double startup_startTime = 0;
double startup_delay = 0;
double pause_startTime = 0;
double pause_cum_Time = 0;
double lastPauseTime = 0;

//----------------------------------------------------
MptcpAgent::MptcpAgent ():Agent (PT_TCP), sub_num_ (0), total_bytes_ (0),
mcurseq_ (1), mackno_ (1), expect_seq(1), curPath (-1), lastPath (-1), stimer(this), ctimer(this), playTimer(this),  isSender(false), enablePtimer(false), playBuffer_(0) 
{ 
    mptcp_id = mptcp_total_number;
    mptcp_total_number++;
	//-----------------------ETOM-------------------------
    if(mptcp_id == 0){
        for(int i=0;i<sub_num_;i++){
            for(int j=0; j<MAX_ARRAY_SIZE; j++){
                //scheduled_queue_packet tmp(0,0);
                //scheduled_queue[i][j] = tmp;
                scheduled_queue_global_seq[i][j]= 0;
                scheduled_queue_packet_length[i][j]= 0;
                sended_queue_global_seq[i][j] = 0;
				sended_queue_pkt_length[i][j] = 0;
				sended_queue_local_seq[i][j] = 0;
                sended_queue_arrive_time[i][j] = 0.0;
				sended_queue_estimated_transmission_time[i][j] = 0.0;
				sended_queue_use_for_scheduled[i][j] = false;
				temp_scheduled_flag[i][j] = false;
            }
			
			scheduled_queue_num[sub_num_] = {0};  
			scheduled_queue_index[sub_num_] = {0};
			sended_queue_num[sub_num_] = {0};    
			sended_queue_index[sub_num_] = {0};
        }        
    }    
    real_mcurseq_ = 0;
    real_total_bytes_ = 0;
    out_of_order_num_ = 0;
    total_packet_num_ = 0;
    noterase = 0;	

	/*
	sub_bandwidth[0] = 180; //Kbps
    sub_bandwidth[1] = 400;
    sub_bandwidth[2] = 1200;
	*/
	
	/*
	sub_bandwidth[0] = 100; //Kbps
    sub_bandwidth[1] = 400;
    sub_bandwidth[2] = 700;
	*/

	sub_bandwidth[0] = 100; //Kbps
    sub_bandwidth[1] = 400;
    sub_bandwidth[2] = 700;
	sub_bandwidth[3] = 500;
	sub_bandwidth[4] = 500;
	sub_bandwidth[5] = 500;
	
	
	/*
	sub_bandwidth[0] = 700; //Kbps
    sub_bandwidth[1] = 700;
    sub_bandwidth[2] = 700;
	*/
	
	/*
	sub_bandwidth[0] = 160; //Kbps
    sub_bandwidth[1] = 320;
    sub_bandwidth[2] = 2000;
	*/
		
	/*
	sub_bandwidth[0] = 600; //Kbps
    sub_bandwidth[1] = 1200;
    sub_bandwidth[2] = 2400;
	*/
	
    //-----------------------ETOM-------------------------
		
#ifdef MPTCP_debug
    printf("mptcp_id:%d, mptcp_total_number:%d\n", mptcp_id, mptcp_total_number);		
#endif
}

void
MptcpAgent::delay_bind_init_all ()
{
  Agent::delay_bind_init_all ();
}

/* haven't implemented yet */
#if 0
int
MptcpAgent::delay_bind_dispatch (const char *cpVarName,
                                 const char *cpLocalName,
                                 TclObject * opTracer)
{
  return Agent::delay_bind_dispatch (cpVarName, cpLocalName, opTracer);
}

void
MptcpAgent::TraceAll ()
{
}

void
MptcpAgent::TraceVar (const char *cpVar)
{
}

void
MptcpAgent::trace (TracedVar * v)
{
  if (eTraceAll == TRUE)
    TraceAll ();
  else
    TraceVar (v->name ());
}
#endif

int
MptcpAgent::command (int argc, const char *const *argv)
{
  if (argc == 2) {
    if (strcmp (argv[1], "listen") == 0) {
      for (int i = 0; i < sub_num_; i++) {
        if (subflows_[i].tcp_->command (argc, argv) != TCL_OK)
          return (TCL_ERROR);
      }
      return (TCL_OK);
    }
	if (strcmp (argv[1], "dump_report") == 0) {
      
        FILE *fp;
		fp = fopen("time_throughput.txt","aw");     /* open file pointer */
		for ( int index = 0; index <= ((int)Scheduler::instance().clock()); index ++ ) {			 
			 fprintf(fp,"%d\t%f\n", index, time_throughput[index]);
		}
		fclose(fp);	  
		
		fp = fopen("time_subflow_util.txt","aw");     /* open file pointer */
		for ( int index = 0; index <= ((int)Scheduler::instance().clock())*10; index ++ ) {			 
			double subflowCounter = 0;
			double util = 0;
			for ( int i = 0; i < 3; i ++ ) {	
				if ( record_subflow_Util[index][i] == 1) {
					subflowCounter += 1;
				}
			}
			if ( subflowCounter > 0 )
				util = subflowCounter/3;
			fprintf(fp,"%d\t%f\n", index, util);
		}
		fclose(fp);	  
		
		fp = fopen("time_sentPkt_throughput(including_ofo_pkts).txt","aw");     /* open file pointer */
		for ( int index = 0; index <= (int)Scheduler::instance().clock(); index ++ ) {			 			
			double total_pkts = 0;
			for ( int i = 0; i < 3; i ++ ) {	
				total_pkts += record_subflow_SentPkts[index][i];				
			}
			fprintf(fp,"%d\t\%f\t%f\n", index, total_pkts, (total_pkts * 1369.0));
		}
		fclose(fp);	
		
      return (TCL_OK);
    }
    if (strcmp (argv[1], "reset") == 0) {

      /* reset used flag information */
      bool used_dst[dst_num_];
      for (int j = 0; j < dst_num_; j++) used_dst[j] = false;

      for (int i = 0; i < sub_num_; i++) {
        for (int j = 0; j < dst_num_; j++) {

          /* if this destination is already used by other subflow, don't use it */
          if (used_dst[j]) continue;

          if (check_routable (i, dsts_[j].addr_, dsts_[j].port_)) {
            subflows_[i].daddr_ = dsts_[j].addr_;
            subflows_[i].dport_ = dsts_[j].port_;
            subflows_[i].tcp_->daddr () = dsts_[j].addr_;
            subflows_[i].tcp_->dport () = dsts_[j].port_;
            used_dst[j] = true; 
            break;
          }
        }
      }
      subflows_[0].tcp_->mptcp_set_primary ();
      return (TCL_OK);
    }
  }
  if (argc == 3) {
    if (strcmp (argv[1], "attach-tcp") == 0) {
      int id = get_subnum ();
      subflows_[id].tcp_ = (MpFullTcpAgent *) TclObject::lookup (argv[2]);
      subflows_[id].used = true;
      subflows_[id].addr_ = subflows_[id].tcp_->addr ();
      subflows_[id].port_ = subflows_[id].tcp_->port ();
      subflows_[id].tcp_->mptcp_set_core (this);
      sub_num_++;
	  printf("attach-tcp id %d sub_num_ %d\n", id, sub_num_);
      return (TCL_OK);
    }	
    else if (strcmp (argv[1], "set-multihome-core") == 0) {
      core_ = (Classifier *) TclObject::lookup (argv[2]);
      if (core_ == NULL) {
        return (TCL_ERROR);
      }
      return (TCL_OK);
    }
	else if (strcmp (argv[1], "schedule-mode") == 0) {
      method = atoi (argv[2]);
      return (TCL_OK);
    }	
	else if (strcmp (argv[1], "lowWaterBuffSize") == 0) {
      lowWaterBuff = atoi (argv[2]);
      return (TCL_OK);
    }
	else if (strcmp (argv[1], "highWaterBuffSize") == 0) {
      highWaterBuff = atoi (argv[2]);
      return (TCL_OK);
    }
	else if (strcmp (argv[1], "limitBuffMode") == 0) {
      limitBuffMode = atoi (argv[2]);
      return (TCL_OK);
    }
	else if (strcmp (argv[1], "limitBuffSize") == 0) {
      limitBuffSize = atoi (argv[2]);
      return (TCL_OK);
    }
	else if (strcmp (argv[1], "video_bitrate") == 0) {
      video_bitrate = atoi (argv[2]);
      return (TCL_OK);
    }
	else if (strcmp (argv[1], "active") == 0) {
      int activeID = atoi (argv[2]);
	  subflows_[activeID].status = true;
	  printf("active event! activeID %d\n", activeID);
	  
	  return (TCL_OK);
    }
	else if (strcmp (argv[1], "inactive") == 0) {
      int inactiveID = atoi (argv[2]);
	  subflows_[inactiveID].status = false;
	  
	  // copy outstanding packets to rescheduling queue 
	  int localIndex = inactiveID;
	  int start_search_index = sended_queue_index[localIndex];						
	  int end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];
	  int tempValue = 0;
	  int tempLength = 0;
		printf("1 inactiveID event! inactiveID %d, start_search_index %d, end_search_index %d, record_subflow_ackno[localIndex] %d\n", inactiveID, start_search_index, end_search_index, record_subflow_ackno[localIndex]);
				
		for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {										 
			if ( sended_queue_local_seq[localIndex][search_index] < record_subflow_ackno[localIndex] ) {
				//printf(" deleted sended_queue_global_seq %d\n", sended_queue_global_seq[localIndex][search_index]); 
				sended_queue_global_seq[localIndex][search_index] = 0;
				sended_queue_pkt_length[localIndex][search_index] = 0;
				sended_queue_local_seq[localIndex][search_index] = 0;
				sended_queue_arrive_time[localIndex][search_index] = 0;
				sended_queue_use_for_scheduled[localIndex][search_index] = false;
				sended_queue_estimated_transmission_time[localIndex][search_index] = 0;
				                             				
			} else {
				printf("1 move to rescheduling queue, sended_queue_local_seq %d, sended_queue_pkt_length %d\n", sended_queue_global_seq[localIndex][search_index], sended_queue_pkt_length[localIndex][search_index]); 
				reschedulingQueue_DSN.push_back(sended_queue_global_seq[localIndex][search_index]);
				reschedulingQueue_len.push_back(sended_queue_pkt_length[localIndex][search_index]);
				
				sended_queue_global_seq[localIndex][search_index] = 0;
				sended_queue_pkt_length[localIndex][search_index] = 0;
				sended_queue_local_seq[localIndex][search_index] = 0;
				sended_queue_arrive_time[localIndex][search_index] = 0;
				sended_queue_use_for_scheduled[localIndex][search_index] = false;
				sended_queue_estimated_transmission_time[localIndex][search_index] = 0;				
			}
		}

		// copy scheduled queue to reschedule queue
		localIndex = inactiveID;
		start_search_index = scheduled_queue_index[localIndex];						
		end_search_index = scheduled_queue_index[localIndex] + scheduled_queue_num[localIndex];
		
		printf("2 inactiveID event! inactiveID %d, start_search_index %d, end_search_index %d\n", inactiveID, start_search_index, end_search_index);
			
		for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
			printf("2 move to rescheduling queue, search_index %d end_search_index %d   scheduled_queue_global_seq %d   scheduled_queue_packet_length %d\n", search_index, start_search_index, scheduled_queue_global_seq[localIndex][search_index], scheduled_queue_packet_length[localIndex][search_index] );									 
			reschedulingQueue_DSN.push_back(scheduled_queue_global_seq[localIndex][search_index]);
			reschedulingQueue_len.push_back(scheduled_queue_packet_length[localIndex][search_index]);			
			
			scheduled_queue_global_seq[localIndex][search_index] = 0;
			scheduled_queue_packet_length[localIndex][search_index] = 0;
		}
		
		//initial 
		sended_queue_index[localIndex] = 0;
		sended_queue_num[localIndex] = 0;  
		scheduled_queue_index[localIndex] = 0;
		scheduled_queue_num[localIndex] = 0;
		
		// resorting 
		for ( int i = 0; i < reschedulingQueue_DSN.size(); i++ ) {
			for ( int j = i+1; j < reschedulingQueue_DSN.size(); j++ ) {
				if ( reschedulingQueue_DSN[i] > reschedulingQueue_DSN[j] ) {
					tempValue = reschedulingQueue_DSN[i];
					tempLength = reschedulingQueue_len[i];
					reschedulingQueue_DSN[i] = reschedulingQueue_DSN[j];
					reschedulingQueue_len[i] = reschedulingQueue_len[j];
					reschedulingQueue_DSN[j] = tempValue;
					reschedulingQueue_len[i] = tempLength;					
				}
			}
		}
		
		for ( int i = 0; i < reschedulingQueue_DSN.size(); i++ ) {
			printf("check %d reschedulingQueue_DSN %d reschedulingQueue_len %d\n", i, reschedulingQueue_DSN[i], reschedulingQueue_len[i] );
		}	
		
	  //abort();
	  return (TCL_OK);
    }
  }
  if (argc == 4) {
    if (strcmp (argv[1], "add-multihome-destination") == 0) {
      add_destination (atoi (argv[2]), atoi (argv[3]));
      return (TCL_OK);
    }	
	else if (strcmp (argv[1], "set-price") == 0) {
      int id = atoi (argv[2]);
      subflows_[id].price_ = atof (argv[3]);
	  assignPricing[id] = atof (argv[3]);
#ifdef MPTCP_debug	  
	  printf("mptcp_id = %d, subflow %d set pricing = %f\n", mptcp_id, id, subflows_[id].price_);
#endif	  
	  stimer.resched(0);
      return (TCL_OK);
    }
  }
  if (argc == 6) {
    if (strcmp (argv[1], "add-multihome-interface") == 0) {
      /* argv[2] indicates the addresses of the mptcp session */

      /* find the id for tcp bound to this address */
      int id = find_subflow (atoi (argv[2]));
      if (id < 0) {
        fprintf (stderr, "cannot find tcp bound to interface addr [%s]",
                 argv[2]);
        return (TCL_ERROR);
      }
      subflows_[id].tcp_->port () = atoi (argv[3]);
      subflows_[id].port_ = atoi (argv[3]);
      subflows_[id].target_ = (NsObject *) TclObject::lookup (argv[4]);
      subflows_[id].link_ = (NsObject *) TclObject::lookup (argv[5]);
      if (subflows_[id].target_ == NULL || subflows_[id].link_ == NULL)
        return (TCL_ERROR);

      return (TCL_OK);
    }
  }
  return (Agent::command (argc, argv));
}

int
MptcpAgent::get_subnum ()
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (!subflows_[i].used)
      return i;
  }
  return -1;
}

int
MptcpAgent::find_subflow (int addr, int port)
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (subflows_[i].addr_ == addr && subflows_[i].port_ == port)
      return i;
  }
  return -1;
}

int
MptcpAgent::find_subflow (int addr)
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (subflows_[i].addr_ == addr)
      return i;
  }
  return -1;
}

void
MptcpAgent::recv (Packet * pkt, Handler * h)
{  
  hdr_ip *iph = hdr_ip::access (pkt);
  hdr_tcp *tcph = hdr_tcp::access (pkt);
  hdr_cmn *ch = hdr_cmn::access (pkt);

  /* find subflow id from the destination address */
  
  
  int id = find_subflow (iph->daddr ());  
  	
#ifdef MPTCP_debug  
  printf("\n at %f mptcp_id:%d, MptcpAgent:recv() from subflow[%d], ch->timestamp() = %f, updated rtt = %f, rtt_ts = %f, cwnd = %f\n",  Scheduler::instance().clock(), mptcp_id, id, ch->timestamp(), subflows_[id].tcp_->mptcp_get_srtt(), subflows_[id].tcp_->mptcp_get_rtt_ts (), subflows_[id].tcp_->mptcp_get_cwnd ());
#endif
  
  if (id < 0) {
    fprintf (stderr,
             "MptcpAgent:recv() fatal error. cannot find destination\n");
    abort ();
  }
  
  /* processing mptcp options */
  if (tcph->mp_capable ()) {
    /* if we receive mpcapable option, return the same option as response */
    subflows_[id].tcp_->mpcapable_ = true;
  }
  if (tcph->mp_join ()) {
    /* if we receive mpjoin option, return the same option as response */
    subflows_[id].tcp_->mpjoin_ = true;
  }
  if (tcph->mp_ack () && subflows_[id].status == true) {
	// should be only sender run this block
	record_subflow_rtt[id] = subflows_[id].tcp_->mptcp_get_rtt();
	record_subflow_srtt[id] = subflows_[id].tcp_->mptcp_get_srtt();
	record_subflow_rtt_ts[id] = subflows_[id].tcp_->mptcp_get_rtt_ts ();
	
	// estimate bandwidth
	double interval = 0;		
	double alpha = 0.9;
	//double alpha = 0.6;
	double newBW = 0;
	double oldBW = 0;
	if ( subflows_[id].lastReceivedAckTime_ != 0 ) {
		interval = Scheduler::instance().clock() - subflows_[id].lastReceivedAckTime_;
		newBW = (ETOM_PacketSize/interval)/1000;  //unit kbps
		oldBW = sub_bandwidth[id];
		sub_bandwidth[id] = alpha * sub_bandwidth[id] + (1-alpha)/2 * (newBW + oldBW);				
		printf("measurementBW\t%f\t%d\t%f\t%d\t%f\t%f\t%f\n", Scheduler::instance().clock(), (id+1), sub_bandwidth[id], tcph->ackno(), interval, subflows_[id].lastReceivedAckTime_, newBW);
	}	
	
	record_subflow_ackno[id] = tcph->ackno();
	subflows_[id].lastReceivedAckTime_ = Scheduler::instance().clock();
	
	subflows_[id].outstand_pkt_ -=1;				
	printf("r	outstand_pkt_ subflows_[0] %d, subflows_[1] %d, subflows_[2] %d \n", subflows_[0].outstand_pkt_, subflows_[1].outstand_pkt_, subflows_[2].outstand_pkt_);
	
    /* when we receive mpack, erase the acked record */
	// 這邊tcph->ackno 有時候receiver那邊是cumulatived ack, 所以數量不會跟sending packet對稱
#ifdef MPTCP_debug	
	printf("time : %f subflows_[%d] receive mpack dsnack %d, tcp ackno %d\n", Scheduler::instance().clock(), id, tcph->mp_ack (), tcph->ackno());
#endif	

	if ( subflows_[id].tcp_->getpipectrlStatu() == true ) {							
		subflows_[id].schQuota_ += 1;
		printf("in recover mode, recv ACK, schQuota_ %d\n", subflows_[id].schQuota_);
	}	

	// test ok, Jensen, 2014-03-19
	bool notFind = true;
	while (notFind == true) {	
		notFind = false;
		int localIndex = id;
		int start_search_index = sended_queue_index[localIndex];						
		int end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];
		//printf(" [clear outdated log] start_search_index %d, end_search_index %d, sended_queue_arrive_time %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
				
		for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
			//printf(" [debug only] search_index %d, DSN %d, TCP seq %d, arrive_time %f, estimated_transmission_time %f\n", search_index, sended_queue_global_seq[localIndex][search_index], sended_queue_local_seq[localIndex][search_index], sended_queue_arrive_time[localIndex][search_index], sended_queue_estimated_transmission_time[localIndex][search_index] );									 
			if ( sended_queue_local_seq[localIndex][search_index] < tcph->ackno() ) {
				//printf(" deleted sended_queue_local_seq %d\n", sended_queue_local_seq[localIndex][search_index]); 
				sended_queue_global_seq[localIndex][search_index] = 0;
				sended_queue_pkt_length[localIndex][search_index] = 0;
				sended_queue_local_seq[localIndex][search_index] = 0;
				sended_queue_arrive_time[localIndex][search_index] = 0;
				sended_queue_use_for_scheduled[localIndex][search_index] = false;
				sended_queue_estimated_transmission_time[localIndex][search_index] = 0;
				sended_queue_index[localIndex]++;
					if(sended_queue_index[localIndex] >(MAX_ARRAY_SIZE -1))
						sended_queue_index[localIndex] = sended_queue_index[localIndex] - MAX_ARRAY_SIZE;
				sended_queue_num[localIndex]--;                                    
				notFind = true;
				break;
			}
		}	
	}
						
	//printf("[after clear] sended_queue_index %d, sended_queue_num %d\n", sended_queue_index[id], sended_queue_num[id]);
							

	subflows_[id].numReceivedPackets_ += 1;
	subflows_[id].lossRate_ = (double)subflows_[id].tcp_->get_numRetransmited_packets()/((double)subflows_[id].tcp_->get_numRetransmited_packets() + subflows_[id].numReceivedPackets_);
	printf("subflows_[%d].lossRate_  %f  num loss packets %d, num Received packets %f\n", id, subflows_[id].lossRate_, subflows_[id].tcp_->get_numRetransmited_packets(), subflows_[id].numReceivedPackets_);	
	printf("dumpReceivedACKSubflow%dSRTT\t%f\t%f\t%f\t%d\n", id, Scheduler::instance().clock(), subflows_[id].tcp_->mptcp_get_srtt(), subflows_[id].tcp_->mptcp_get_rtxcur(), tcph->ackno());	
    subflows_[id].tcp_->mptcp_remove_mapping (tcph->mp_ack ());
  }

  if (tcph->mp_dsn() && subflows_[id].status == true) {
	// should be only recevier run this block
      /* when we receive mpdata, update new mapping */
      subflows_[id].tcp_->mpack_ = true;
#ifdef MPTCP_debug	  
      printf(" @@@ time : %f subflows_[%d]:\n", Scheduler::instance().clock(), id);
#endif	  
      //printf(" call mptcp_recv_add_mapping \n");
      subflows_[id].tcp_->mptcp_recv_add_mapping (tcph->mp_dsn (),
                                                tcph->mp_subseq (),
                                                tcph->mp_dsnlen ());
	  //printf(" after call mptcp_recv_add_mapping \n");											
		/* make sure packet will be return to the src addr of the packet */
		subflows_[id].tcp_->daddr () = iph->saddr ();
		subflows_[id].tcp_->dport () = iph->sport ();
		Packet* pkt_ = pkt->copy();			
						
		total_pricing += subflows_[id].price_;
#ifdef MPTCP_debug		
		printf("			[total_pricing %f] subflows_[%d].price_ = %f\n", total_pricing, id, subflows_[id].price_);
#endif
	
		double pktReceivedTime = 0;
		int inorderSubflowID = -1;
		int inorderTriggerSubflow = id;
		int beforeInorderOutofOrderPkts = reordering_buffer_.size();
		int inorderSeq = -1;
		bool inorderEvent = false;
		bool readTargetSeq = false;
		double tempCC = 0;
		double inorderPktNum = 0;			
			
      /* reordering*/
      while(1){		  
          iph = hdr_ip::access (pkt);
          tcph = hdr_tcp::access (pkt);
		  hdr_cmn *ch = hdr_cmn::access (pkt);          
		  
		  int id_ = 0;
		  id_ = find_subflow (iph->daddr ());
		  
//#ifdef MPTCP_debug		  
          printf(" 	 	 %f [check out of order] expect_seq = %ld, recv %d[%d], current out of order packets %ld\n", Scheduler::instance().clock(), expect_seq, tcph->mp_dsn(), id_, reordering_buffer_.size() );
//#endif		  

          if(expect_seq == tcph->mp_dsn()){            
			  if ( readTargetSeq == false ) {
				readTargetSeq = true;
				inorderSeq = tcph->mp_dsn();
			  }
		  
              mackno_ = expect_seq;
			  inorderEvent = true;
			  // performance metrics 
#ifdef MPTCP_debug			  
			  printf("inorder case and update mackno_ = %d\n", mackno_);
#endif			  
			  double packetDelay = Scheduler::instance().clock() - ch->timestamp();
			  double reorderingDelay = 0;
			  
			  if ( pktReceivedTime > 0 ) {
					reorderingDelay = Scheduler::instance().clock() - pktReceivedTime;
					total_counter_reorder++;
					printf(" ************** DSN %d reorderingDelay = %f, time = %f, pktReceivedTime = %f\n", expect_seq, reorderingDelay, Scheduler::instance().clock(), pktReceivedTime);
					
					if ( sub_num_ == 3) {
						if ( inorderSubflowID == 0 ) {
							subflow1_reordering_delay += reorderingDelay;
							subflow1_counter_reorder += 1;
						} else if ( inorderSubflowID == 1 ) {
							subflow2_reordering_delay += reorderingDelay;
							subflow2_counter_reorder += 1;
						} else if ( inorderSubflowID == 2 ) {
							subflow3_reordering_delay += reorderingDelay;
							subflow3_counter_reorder += 1;
						} else {
							fprintf (stderr, "fatal error. inorderSubflowID = %d\n", inorderSubflowID);
							abort();
						}					
						
						printf(" ************** total_reordering_delay = %f, total_counter_reorder = %f\n", total_reordering_delay, total_counter_reorder);
						printf(" ************** subflow1_reordering_delay = %f, subflow1_counter_reorder = %f\n", subflow1_reordering_delay, subflow1_counter_reorder);
						printf(" ************** subflow2_reordering_delay = %f, subflow2_counter_reorder = %f\n", subflow2_reordering_delay, subflow2_counter_reorder);
						printf(" ************** subflow3_reordering_delay = %f, subflow3_counter_reorder = %f\n", subflow3_reordering_delay, subflow3_counter_reorder);
					}
			  }
			  inorderSubflowID = -1;
			  
			  //if ( enablePtimer == false ) {					
			  if ( enablePtimer == false && Scheduler::instance().clock() > 10.0 ) {								  
			  //if ( enablePtimer == false && Scheduler::instance().clock() > 3.0 ) {								  
				enablePtimer = true;
				playTimer.resched(0.1);
				//startup_startTime = Scheduler::instance().clock();
				startup_startTime = 10;
				printf("rebuffer status, enable enablePtimer = %f limitBuffMode %d limitBuffSize %d\n", Scheduler::instance().clock(), limitBuffMode, limitBuffSize);	
				printf("rebuffer status, lowWaterBuff = %f highWaterBuff %f\n", lowWaterBuff, highWaterBuff);	
				printf("rebuffer status, video_bitrate = %f\n", video_bitrate);	
			  }			  
			  
			  if ( pktReceivedTime > lastPauseTime || pktReceivedTime == 0 ) {
				  inorderPktNum++;		
					
			  	  if ( playBuffer_ > (highWaterBuff + 158875) )
						;			//drop packet case 
					// 這是要避免很多out-of-order packets變inorder時, playBuffer突然爆滿的case
				  else {			  						
						if ( limitBuffMode == 1 ) {
							if ( inorderPktNum <= limitBuffSize )
								playBuffer_ += tcph->mp_dsnlen();	
						} else 
							playBuffer_ += tcph->mp_dsnlen();
						
				  }		  			  
			  }
			  //printf("rebuffer status,  --  %f playBuffer_ %f, pktReceivedTime %f lastPauseTime %f\n", Scheduler::instance().clock(), playBuffer_, pktReceivedTime, lastPauseTime);	
			  			  
//#ifdef MPTCP_debug			  
			  printf("size = %d, data length = %d, sending time = %f, delay = %f, reorDelay = %f\n", ch->size(), tcph->mp_dsnlen(), ch->timestamp(), packetDelay, reorderingDelay);
//#endif			  
			  total_throughput += tcph->mp_dsnlen();			  
			  time_throughput[(int)Scheduler::instance().clock()] += tcph->mp_dsnlen();
			  total_delay += packetDelay;
			  total_reordering_delay += reorderingDelay;
			  total_counter++;			  

#ifdef MPTCP_debug			  
			  printf("			[total throughput %f] [delay %f] [counter %f]\n", total_throughput, total_delay, total_counter);
			  printf("			[avg throughput %f] [avg delay %f]\n", total_throughput/total_counter, total_delay/total_counter);
			  
			  					
			  //printf("			subflow 0 price %f\n", subflows_[0].price_);
			  //printf("			subflow 1 price %f\n", subflows_[1].price_);
			  //printf("			subflow 2 price %f\n", subflows_[2].price_);
	
#endif			  		
			  
			  //fine next one out of order pakcet 
			  expect_seq = expect_seq + tcph->mp_dsnlen();
			  //printf("	new expect_seq = %d, next = %d\n", expect_seq, tcph->mp_dsnlen() );
			  
              if(!reordering_buffer_.empty()){
                  vector < reordering_buffer >::iterator it;
                  int find = 0;
				  int removed = 0;
				  while(removed == 0) {
					  //printf("    start remove while\n"); 		
					  removed = 1;	
					  for (it = reordering_buffer_.begin (); it != reordering_buffer_.end (); ++it) {
						  struct reordering_buffer *p = &*it;						  
						  //printf("              p->re_pktseq %d\n", p->re_pktseq); 	
						  if (expect_seq == p->re_pktseq) {
							  pkt = p->re_pkt;
							  h = p->re_h;			
							  inorderSubflowID = p->re_subflowID;
							  pktReceivedTime = p->receivedTime;
							  //printf("    removed packet in out of order buffer\n"); 		
							  it = reordering_buffer_.erase (it);
							  find = 1;
							  removed = 0;
							  break;
						  }
						  // sometime the receiver will received duplicated packets 
						  // due to TCP congestion control algorithm 
						  // so need to remove them in out of order packet
						  if (expect_seq > p->re_pktseq) {
						  	  //printf("    removed packet in out of order buffer\n"); 		
							  it = reordering_buffer_.erase (it);
							  removed = 0;
							  break;
						  }
					  }					  
				  }
                  if(find == 0) {
					  //printf("   from for break while(1) \n"); 	
                      break; // break while(1)                 
			      }
              }
              else {
				  //printf("   from if break while(1) \n"); 	
                  break;  // break while(1)
			  }
          }
          else{              
              struct reordering_buffer tmp (tcph->mp_dsn(), id, Scheduler::instance().clock(), pkt, h);
              reordering_buffer_.push_back (tmp);       
			  total_out_of_order_pkt += 1;
			  
			  //if ( enablePtimer == false ) {					
			  if ( enablePtimer == false && Scheduler::instance().clock() > 10.0 ) {					
			  //if ( enablePtimer == false && Scheduler::instance().clock() > 3.0 ) {					
				enablePtimer = true;
				playTimer.resched(0.1);
				//startup_startTime = Scheduler::instance().clock();
				startup_startTime = 10;
				printf("rebuffer status, enable enablePtimer = %f limitBuffMode %d limitBuffSize %d\n", Scheduler::instance().clock(), limitBuffMode, limitBuffSize);	
				printf("rebuffer status, lowWaterBuff = %f highWaterBuff %f\n", lowWaterBuff, highWaterBuff);	
				printf("rebuffer status, video_bitrate = %f\n", video_bitrate);	
			  }		
			  
#ifdef MPTCP_debug			  			  
			  printf("		 %f out of order, out of order packet = %ld\n", Scheduler::instance().clock(), reordering_buffer_.size() );			   
#endif			  
              break;
          }		    
      }    
	  
	  if ( inorderEvent == true ) {
			int numInorderPacket = beforeInorderOutofOrderPkts - reordering_buffer_.size();
				printf("numInorderPacket %d beforeInorderOutofOrderPkts %d reordering_buffer_.size() %ld\n", numInorderPacket, beforeInorderOutofOrderPkts, reordering_buffer_.size() );
			// if numInorderPacket = 0, that meaning this packet is expected packet, but not 
			// clearn multiple out of order packets next expect_seq not yet received.
			if (numInorderPacket != 0) 
				printf("Dump_inorderTriggerSubflow\tTrigger_Subflow_(%d)\t%f\t%d\t%d\n", inorderTriggerSubflow, Scheduler::instance().clock(),   numInorderPacket, inorderSeq );	
	  }
	  
	  
	  //printf(" @@@ call subflow's recv function[%d] \n", id);
	  
	  /* call subflow's recv function */		  
	  subflows_[id].tcp_->recv (pkt_, h);			  
	  send_control();		
  }
  else{
    /* make sure packet will be return to the src addr of the packet */
	 
    //printf(" ___ call subflow's recv function[%d] \n", id);
    subflows_[id].tcp_->daddr () = iph->saddr ();
    subflows_[id].tcp_->dport () = iph->sport ();

    /* call subflow's recv function */
    subflows_[id].tcp_->recv (pkt, h);	
    send_control();
  }
}

/*
 * add possible destination address
 */
void
MptcpAgent::add_destination (int addr, int port)
{
  for (int i = 0; i < MAX_SUBFLOW; i++) {
    if (dsts_[i].active_)
      continue;
    dsts_[i].addr_ = addr;
    dsts_[i].port_ = port;
    dsts_[i].active_ = true;
    dst_num_++;
    return;
  }
  fprintf (stderr, "fatal error. cannot add destination\n");
  abort ();
}

/*
 * check if this subflow can reach to the specified address
 */
bool
MptcpAgent::check_routable (int sid, int addr, int port)
{
  Packet *p = allocpkt ();
  hdr_ip *iph = hdr_ip::access (p);
  iph->daddr () = addr;
  iph->dport () = port;
  bool
    result = (static_cast < Classifier * >
              (subflows_[sid].target_)->classify (p) > 0) ? true : false;
  Packet::free (p);

  return result;
}

void
MptcpAgent::sendmsg (int nbytes, const char * /*flags */ )
{
//改成收到event先放在queue裡面, 不然如果event產生, TCP都busy時, 該event會消失
	  //printf("mptcp_id:%d, MptcpAgent::sendmsg(), nbytes = %d, total_bytes_ = %d, TCP_MAXSEQ = %d\n", mptcp_id, nbytes, total_bytes_, TCP_MAXSEQ);
	  
  //if (nbytes == -1)
//    total_bytes_ = TCP_MAXSEQ;
//  else
    //total_bytes_ = nbytes;
		
	
	// test code
	/*
	transmissionQueue_.push_back(100);
	transmissionQueue_.push_back(10);
	transmissionQueue_.push_back(1);
	
	for (int i = 0; i < transmissionQueue_.size(); i++) {
		printf("%d ",transmissionQueue_[i]);
	}                                                   
	printf("\n\n"); 	
		
	sort(transmissionQueue_.begin(), transmissionQueue_.end());
	printf("current content (after sort): "); 		
	for (int i = 0; i < transmissionQueue_.size(); i++) {
		printf("%d ",transmissionQueue_[i]);
	}               
	printf("\n\n"); 		
	abort();	
	*/
	
	/*
	int Height = 10;
	transmissionQueue2D_.resize(Height);
	transmissionQueue2D_[0].push_back(100);
	transmissionQueue2D_[0].push_back(10);
	transmissionQueue2D_[0].push_back(1);
	//printf(" %d %d %d ", transmissionQueue2D_[0].size(), transmissionQueue2D_[1].size(), transmissionQueue2D_[2].size()); 
	for (int i = 0; i < transmissionQueue2D_[0].size(); i++) {
		printf("%d ",transmissionQueue2D_[0][i]);
	}                                                   
	printf("\n\n"); 		
	
	sort(transmissionQueue2D_[0].begin(), transmissionQueue2D_[0].end());
	
	for (int i = 0; i < transmissionQueue2D_[0].size(); i++) {
		printf("%d ",transmissionQueue2D_[0][i]);
	}                                                   
	printf("\n\n"); 	
	printf("current content (after sort): "); 
	for (int i = 0; i < transmissionQueue2D_[0].size(); i++) {
		printf("%d ",transmissionQueue2D_[0][i]);
	}                                                   
	printf("\n\n"); 		
	transmissionQueue2D_[0].erase(transmissionQueue2D_[0].begin()+1); 
	printf("current content (after erase): "); 
	for (int i = 0; i < transmissionQueue2D_[0].size(); i++) {
		printf("%d ",transmissionQueue2D_[0][i]);
	}                                                 
	abort();	
	*/
	// test code
		
	if ( nbytes > 0 ) {
		sendQueue_.push_back(nbytes);
		currentSendQueueSize = sendQueue_.size();
		
		isSender = true;
#ifdef MPTCP_debug			  		
/*
		printf("\n[packet arrival event] event size = %d, current queue size = %d\n", nbytes, sendQueue_.size() );
		if(!sendQueue_.empty()){
			for (int i = 0; i < sendQueue_.size(); i++) {
				printf("%d ",sendQueue_[i]);
			}                                          
			printf("\n\n"); 	
		 }
*/		 
#endif				 
    }	
  send_control();
}

/*
 * control sending data
 */
void
MptcpAgent::send_control()
{
  bool notMatchTarget = false;  
  double curTime = Scheduler::instance().clock();
  static double preNotMatchTargetTime = -1;
  static double NumSchTime = 0;
  static double NumDominateTime = 0;
  static double NumMultipleSubflowSchTime = 0;
  static int cur_choice = 0;
  int statusCheck1_[MAX_SUBFLOW] = {0};	
  int statusCheck2_[MAX_SUBFLOW] = {0};	
  int PileStatus_[MAX_SUBFLOW] = {0};	
  int outstanding_[MAX_SUBFLOW] = {0};	
  double dumpCwnd_[MAX_SUBFLOW] = {0};	
  int availablecwnd_[MAX_SUBFLOW] = {0};	
  int checkAvailableResult = 0;	
  int checkAvailableResult2 = 0;
  
//#ifdef MPTCP_debug	
  printf("!!!!!! at %f MptcpAgent::send_control() mptcp_id:%d\n", Scheduler::instance().clock(), mptcp_id);  
//#endif
  
  if ( curTime != preNotMatchTargetTime )		//加速simulation 
  while( notMatchTarget == false ) {
  notMatchTarget = true;
    
  total_bytes_ = 0;
  if( !sendQueue_.empty() || !reschedulingQueue_DSN.empty() ){ 	  
	for(int i = 0; i < reschedulingQueue_DSN.size(); i++) {	  
		total_bytes_ = reschedulingQueue_len[i];	
		 break;		 
	}	
  
	  if ( total_bytes_ == 0 ) {
	  	  for(int i = 0; i < sendQueue_.size(); i++) {	  
			total_bytes_ = sendQueue_[i];
#ifdef MPTCP_debug			  		
			printf("at %f MptcpAgent::send_control() mptcp_id:%d, take packet, total_bytes_ = %d\n", Scheduler::instance().clock(), mptcp_id, total_bytes_);
			printf(" sendQueue_ size = %ld\n", sendQueue_.size() );
#endif		
			break;		 
		  }			  
	  }
	  record_eschedulingQueueSize = reschedulingQueue_DSN.size();
  }
      
   //update 'statistic'
  for (int i = 0; i < sub_num_ && total_bytes_ > 0; i++) {
	  record_subflow_rtt[i] = subflows_[i].tcp_->mptcp_get_rtt();
	  record_subflow_srtt[i] = subflows_[i].tcp_->mptcp_get_srtt();
	  record_subflow_rtt_ts[i] = subflows_[i].tcp_->mptcp_get_rtt_ts();
	  record_subflow_rto[i] = subflows_[i].tcp_->mptcp_get_rtxcur();
  } 
	
  // use for when sendQueue_.size() is zero but the scheduled queue still has packet		
  //if (total_bytes_<= 0 && method == 4 && mptcp_id == 0){//(total_bytes =< 0)&&(method == 4)
  if (total_bytes_<= 0 && (method == 4 || (method == 1 && RTTwithSchQueueMode == true)) && mptcp_id == 0){//(total_bytes =< 0)&&(method == 4)
            printf("send case by total_bytes = %d <= 0---------------------\n", total_bytes_);
            bool slow_start = false;
            int sendbytes[MAX_SUBFLOW] = {0};
			
            for(int i=0; i< sub_num_; i++){
                int mss = subflows_[i].tcp_->size ();
                double cwnd = subflows_[i].tcp_->mptcp_get_cwnd () * mss;				
	            double rtt = subflows_[i].tcp_->mptcp_get_srtt();
                int ssthresh = subflows_[i].tcp_->mptcp_get_ssthresh () * mss;
                int maxseq = subflows_[i].tcp_->mptcp_get_maxseq ();
                int backoff = subflows_[i].tcp_->mptcp_get_backoff ();
                int highest_ack = subflows_[i].tcp_->mptcp_get_highest_ack ();
                int dupacks = subflows_[i].tcp_->mptcp_get_numdupacks();	         
                if (backoff >= 4) continue;
                int outstanding;
	            if ( highest_ack == -1 ) 
		            outstanding = subflows_[i].outstand_pkt_ * mss;
	            else 
		            outstanding = maxseq - highest_ack - dupacks * mss;
				outstanding_[i] = outstanding;
				
                if (outstanding <= 0) outstanding = 0;
                if (cwnd < ssthresh) {
                    /* allow only one subflow to do slow start at the same time */
                    if (!slow_start) {
                        slow_start = true;
                        subflows_[i].tcp_->mptcp_set_slowstart (true);
                    }
                    else
                        subflows_[i].tcp_->mptcp_set_slowstart (true);
                }	  	  
                sendbytes[i] = cwnd - outstanding;
				availablecwnd_[i] = sendbytes[i];

#ifdef MPTCP_debug			  	  
	            printf("      		index %d available sendbytes = %d, cwnd = %f, outstanding = %d, mss = %d,\n rtt 1 = %f, rtt 2 = %f, pricing = %f\n", i, sendbytes[i], cwnd, outstanding, mss, subflows_[i].rtt_, rtt, subflows_[i].price_);
#endif	  
	  
	            record_subflow_rtt[i] = subflows_[i].tcp_->mptcp_get_rtt();	  
				statusCheck1_[i] = checkAvailableResult;	
				statusCheck2_[i] = checkAvailableResult2;	
				PileStatus_[i] = subflows_[i].tcp_->getpipectrlStatu();	
		
#ifdef MPTCP_debug			
		printf("ETOM scheduled queue part  checkAvailableResult = %d, checkAvailableResult2 = %d, getpipectrlStatu = %d\n", checkAvailableResult, checkAvailableResult2, PileStatus_[i]);
#endif		
		
				checkAvailableResult = subflows_[i].tcp_->checkAvailable(mss);	
				checkAvailableResult2 = 1;
		
				if ( highest_ack > 0 ) {
					printf("dump_subflow%d_numSACK\t%f\t", (i+1), Scheduler::instance().clock());	
					checkAvailableResult2 = subflows_[i].tcp_->mptcp_foutput(mss);
					record_subflow_cwndReset[i] = subflows_[i].tcp_->getCwndResetTime();
				}	
						
				// ETOM traffic pacing 
				if ( enableTrafficPacing == true ) {
					//int pendingPkts = scheduled_queue_num[0] + scheduled_queue_num[1] + scheduled_queue_num[2] + sendQueue_.size();
					int pendingPkts = sendQueue_.size();
					printf("  pendingPkts = %d, lastSentTime_ %f \n", pendingPkts, subflows_[i].lastSentTime_);
					//if ( scheduled_queue_num[i] < NumQueueThreshold && subflows_[i].tcp_->mptcp_get_rtxcur() < 0.5 ) {
					//if ( scheduled_queue_num[i] < NumQueueThreshold && scheduled_queue_num[i] > 0 ) {	
					if ( scheduled_queue_num[i] < NumQueueThreshold && scheduled_queue_num[i] > 0 && i == 2 ) {	
						double minBW = (sub_bandwidth[i] * 1000)/5;
						double limitBW = (( (sub_bandwidth[i] * 1000) - minBW)/NumQueueThreshold) *scheduled_queue_num[i] + minBW;
						//double pacingDelay = (limitBW * ETOM_PacketSize)/1000;
						double pacingDelay = ETOM_PacketSize/limitBW;
						double tGap = curTime - subflows_[i].lastSentTime_;
						printf(" subflow %d minBW = %f, limitBW %f, scheduled_queue_num %d \n", i, minBW, limitBW, scheduled_queue_num[i]);
						
						/*
						double maxRTO = 0;
						if ( subflows_[0].tcp_->mptcp_get_rtxcur() > maxRTO ) 
							maxRTO = subflows_[0].tcp_->mptcp_get_rtxcur();
						if ( subflows_[1].tcp_->mptcp_get_rtxcur() > maxRTO ) 
							maxRTO = subflows_[1].tcp_->mptcp_get_rtxcur();	
						if ( subflows_[2].tcp_->mptcp_get_rtxcur() > maxRTO ) 
							maxRTO = subflows_[2].tcp_->mptcp_get_rtxcur();
						*/	
							
						//double pacingLength = maxRTO/NumQueueThreshold;
						//double pacingLength = 0.5/NumQueueThreshold;
						
						pacingDelay = subflows_[2].tcp_->mptcp_get_rtxcur()/NumQueueThreshold;
						
						printf("  tGap = %f, pacingDelay %f \n", tGap, pacingDelay);
						if ( tGap < pacingDelay ) {  
							checkAvailableResult = 0;							
							printf("  disable %d this time\n", i);
						}
					}						
				}
			
				// if this path cannot transmit whole packet, still transmit?
				if ( sendbytes[i] < mss || checkAvailableResult == 0 || checkAvailableResult2 == 0 || subflows_[i].status == false) {  
                //if (sendbytes[i] < mss) { 	  
	                printf(" sendbyte[%d] !!!!!!!!! unavailable now !!!!!!!!! \n", i);
	                continue;
                }
				
                if(scheduled_queue_num[i] > 0){
                    mss = subflows_[i].tcp_->size ();
                    real_mcurseq_ = mcurseq_;
                    real_total_bytes_ = total_bytes_;              
                    //dequeue a packet from scheduled queue and transmit it.

                    sendbytes[i] = deSchQueue(i);					
                    total_bytes_ = sendbytes[i];					
					
                    //enqeue a packet to sended queue 
#ifdef MPTCP_debug			 					
	                printf("enqueue a sended packet-------------------\n");
#endif
					
					/*
					int localIndex = i;
					int start_search_index = sended_queue_index[localIndex];						
					int end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];
					printf(" start_search_index %d, end_search_index %d, sended_queue_arrive_time[j][start_search_index] %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
							
					for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
						printf(" [debug only] search_index %d, DSN %d, arrive_time %f, estimated_transmission_time %f\n", search_index, sended_queue_global_seq[localIndex][search_index], sended_queue_arrive_time[localIndex][search_index], sended_queue_estimated_transmission_time[localIndex][search_index] );									 
					}
					*/
					
	                if (sendbytes[i] > mss) 
	                    sendbytes[i] = mss;
			
					enSendQueue(i, sendbytes[i]);	
		
//#ifdef MPTCP_debug			  		
	                printf("[schedulerLog] directly from schedueld queue      		%f mptcp_id:%d, packet-scheduled to subflows_[%d], sendbytes = %d, mcurseq_ %d\n", Scheduler::instance().clock(), mptcp_id, i, sendbytes[i], mcurseq_);        
//#endif	   
					printf("dumpSubflow%d_MPTCP_sentSeq_dsn__sending_queue_size\t%f\t%d\t%d\t%d\t%d\n", (i + 1), Scheduler::instance().clock(), (subflows_[i].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1), scheduled_queue_num[i]);			
					printf("dumpAllSubflowMPTCP_sentSeq_dsn__sending_queue_size\t%f\t%d\t%d\t%d\t%d\t%d\n", Scheduler::instance().clock(), (i + 1), (subflows_[i].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1), scheduled_queue_num[i]);			
					
					// mcurseq_ is DSN, 
	                subflows_[i].tcp_->mptcp_add_mapping (mcurseq_, sendbytes[i]);
	                subflows_[i].outstand_pkt_ +=1;		
					printf("s2	outstand_pkt_ subflows_[0] %d, subflows_[1] %d, subflows_[2] %d \n", subflows_[0].outstand_pkt_, subflows_[1].outstand_pkt_, subflows_[2].outstand_pkt_);
	                record_subflow_sent_ptks[i] +=1; 			
					record_subflow_Busy[i] = 1;		
					record_subflow_Util[(int)(Scheduler::instance().clock()*10)][i] = 1;
					record_subflow_SentPkts[(int)Scheduler::instance().clock()][i] += 1;
					
                    subflows_[i].tcp_->sendmsg (sendbytes[i]);  
					
					if ( Scheduler::instance().clock() > 10 && subflows_[i].lastSentTime_ > 10 ) {
						record_subflow_sentCounter[i] += 1;
						record_subflow_idleTime[i] += Scheduler::instance().clock() - subflows_[i].lastSentTime_;
					}	
					
					subflows_[i].lastSentTime_ = curTime;
							
					//if ( sub_num_ == 3 ) {
						int targetIndex = i;									
						printf("dumpSubflow%d_MPTCP_sentSeq_dsn__sending_queue_size\t%f\t%d\t%d\t%d\t%d\n", (targetIndex + 1), Scheduler::instance().clock(), (subflows_[targetIndex].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1), scheduled_queue_num[targetIndex]);			
						printf("dumpAllSubflowMPTCP_sentSeq_dsn__sending_queue_size\t%f\t%d\t%d\t%d\t%d\t%d\n", Scheduler::instance().clock(), (targetIndex + 1), (subflows_[targetIndex].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1), scheduled_queue_num[targetIndex]);			
						printf("dumpSubflow%dsentSize\t%f\t%d\n", (targetIndex + 1), Scheduler::instance().clock(), sendbytes[targetIndex]);
						
						for ( int j = 0; j < sub_num_ ; j++ ) {
							if ( availablecwnd_[j] < 0 )
								availablecwnd_[j] = 0;
							if ( outstanding_[j] < 0 )
								outstanding_[j] = 0;					
						}
										
						printf("dumpoutstandingandavailablecwnd\t%f\t%d\t%d\t%d\t%d\t%d\t%d\n", Scheduler::instance().clock(), outstanding_[0], outstanding_[1], outstanding_[2], availablecwnd_[0], availablecwnd_[1], availablecwnd_[2]);
					//}					
                    
					mcurseq_ = real_mcurseq_;
					total_bytes_ = real_total_bytes_;
					printf("[schedulerLog] current mcurseq_ %d\n",mcurseq_);
                    
                }//if(scheduled_queue_num[i] != 0)
            }//for      
        }//else if(method == 4) 
		
		
		
  if (total_bytes_ > 0) {
    /* one round */
    bool slow_start = false;
	bool enable_ETOM = false;
		
	int sendbytes[MAX_SUBFLOW] = {0};	
	int targetIndex = -1;	
	int schIndex = -1;
	int fastlink = -1;
	double minRTT = 999999;
	double schMinRTT = 999999;
	double minPrice = 999999;
	double schMinPrice = 999999;
	
	bool availPath[MAX_SUBFLOW] = {false};
	int availPathCounter = 0;
	int numofConnectedPath = 0;
	bool connectEstablishedPath[MAX_SUBFLOW] = {false};
	
	int randAssignIndex[10] = {-1};
	int randCounter = 0;
	bool needRandom = false;
	bool newSubflow = false;
	bool sentBySchQueue = false;
	//----------------------ETOM
    int atleastonepath = -1;
    //----------------------ETOM
	
    for (int i = 0; i < sub_num_ && total_bytes_ > 0; i++) {
	//for (int i = (sub_num_-1); i >= 0 && total_bytes_ > 0; i--) {
      int mss = subflows_[i].tcp_->size ();
      double cwnd = subflows_[i].tcp_->mptcp_get_cwnd () * mss;
	  dumpCwnd_[i] = subflows_[i].tcp_->mptcp_get_cwnd ();
	  double rtt = subflows_[i].tcp_->mptcp_get_srtt();
	   
	  subflows_[i].rtt_ = rtt;
      int ssthresh = subflows_[i].tcp_->mptcp_get_ssthresh () * mss;
      int maxseq = subflows_[i].tcp_->mptcp_get_maxseq ();
      int backoff = subflows_[i].tcp_->mptcp_get_backoff ();
      int highest_ack = subflows_[i].tcp_->mptcp_get_highest_ack ();
      int dupacks = subflows_[i].tcp_->mptcp_get_numdupacks();
	  subflows_[i].price_ = assignPricing[i];
	  
#if 1
	  // we don't utlize a path which has lots of timeouts
      if (backoff >= 4) continue;
#endif

      /* too naive logic to calculate outstanding bytes? */
	  int outstanding;
	  if ( highest_ack == -1 && subflows_[i].status == true) {
		outstanding = subflows_[i].outstand_pkt_ * mss;
		newSubflow = true;		
		printf(" newSubflow ! \n");
	  } else 
		outstanding = maxseq - highest_ack - dupacks * mss;
	  
	  outstanding_[i] = outstanding;
	  
#ifdef MPTCP_debug			  	  
	  printf("      		-> subflows_[%d] outstanding = %d, maxseq = %d, highest_ack = %d, dupacks = %d\n", i, outstanding, maxseq, highest_ack, dupacks);
#endif	  
	  
      if (outstanding <= 0) outstanding = 0;

      if (cwnd < ssthresh) {
        /* allow only one subflow to do slow start at the same time */
        if (!slow_start) {
          slow_start = true;
          subflows_[i].tcp_->mptcp_set_slowstart (true);
        }
        else
#if 0
          subflows_[i].tcp_->mptcp_set_slowstart (false);
#else
          /* allow to do slow-start simultaneously */
          subflows_[i].tcp_->mptcp_set_slowstart (true);
#endif
      }
	  	  
		 if ( method == 1 ) {
			if ( subflows_[i].rtt_ < schMinRTT ) {			
				schIndex = i;
				schMinRTT = subflows_[i].rtt_;		
			} 
		} else if ( method == 2 ) {
			if ( subflows_[i].price_ < schMinPrice ) {
				schIndex = i;
				schMinPrice = subflows_[i].price_;				
			}	
	   }  
		  
      sendbytes[i] = cwnd - outstanding;
	  availablecwnd_[i] = sendbytes[i];
#ifdef MPTCP_debug			  	  
	  printf("      		index %d available sendbytes = %d, cwnd = %f, outstanding = %d, mss = %d,\n rtt 1 = %f, (s)rtt 2 = %f, pricing = %f\n", i, sendbytes[i], cwnd, outstanding, mss, subflows_[i].rtt_, rtt, subflows_[i].price_);
#endif	  
				
		checkAvailableResult = subflows_[i].tcp_->checkAvailable(mss);	
		checkAvailableResult2 = 1;
		
		if ( highest_ack > 0 ) {
			printf("dump_subflow%d_numSACK\t%f\t", (i+1), Scheduler::instance().clock());	
			checkAvailableResult2 = subflows_[i].tcp_->mptcp_foutput(mss);
			numofConnectedPath++;
			
			record_subflow_cwndReset[i] = subflows_[i].tcp_->getCwndResetTime();
			connectEstablishedPath[i] = true;
		}
		
		statusCheck1_[i] = checkAvailableResult;	
	    statusCheck2_[i] = checkAvailableResult2;	
	    PileStatus_[i] = subflows_[i].tcp_->getpipectrlStatu();	
		
#ifdef MPTCP_debug			
		printf("  checkAvailableResult = %d, checkAvailableResult2 = %d, getpipectrlStatu = %d\n", checkAvailableResult, checkAvailableResult2, PileStatus_[i]);
#endif		
		
// ETOM traffic pacing 
		if ( enableTrafficPacing == true && highest_ack > 0) {
			//int pendingPkts = scheduled_queue_num[0] + scheduled_queue_num[1] + scheduled_queue_num[2] + sendQueue_.size();
			int pendingPkts = sendQueue_.size();
			printf("  pendingPkts = %d, lastSentTime_ %f \n", pendingPkts, subflows_[i].lastSentTime_);
			//if ( scheduled_queue_num[i] < NumQueueThreshold && subflows_[i].tcp_->mptcp_get_rtxcur() < 0.5 ) {
			//if ( scheduled_queue_num[i] < NumQueueThreshold && scheduled_queue_num[i] > 0 ) {	
			if ( scheduled_queue_num[i] < NumQueueThreshold && scheduled_queue_num[i] > 0 && i == 2 ) {	
				double minBW = (sub_bandwidth[i] * 1000)/5;
				double limitBW = (( (sub_bandwidth[i] * 1000) - minBW)/NumQueueThreshold) *scheduled_queue_num[i] + minBW;
				//double pacingDelay = (limitBW * ETOM_PacketSize)/1000;
				double pacingDelay = ETOM_PacketSize/limitBW;
				double tGap = curTime - subflows_[i].lastSentTime_;
				printf(" subflow %d minBW = %f, limitBW %f, scheduled_queue_num %d \n", i, minBW, limitBW, scheduled_queue_num[i]);
				
				/*
				double maxRTO = 0;
				if ( subflows_[0].tcp_->mptcp_get_rtxcur() > maxRTO ) 
					maxRTO = subflows_[0].tcp_->mptcp_get_rtxcur();
				if ( subflows_[1].tcp_->mptcp_get_rtxcur() > maxRTO ) 
					maxRTO = subflows_[1].tcp_->mptcp_get_rtxcur();	
				if ( subflows_[2].tcp_->mptcp_get_rtxcur() > maxRTO ) 
					maxRTO = subflows_[2].tcp_->mptcp_get_rtxcur();
				*/	
					
				//double pacingLength = maxRTO/NumQueueThreshold;
				//double pacingLength = 0.5/NumQueueThreshold;
				
				pacingDelay = subflows_[2].tcp_->mptcp_get_rtxcur()/NumQueueThreshold;
				
				printf("  tGap = %f, pacingDelay %f \n", tGap, pacingDelay);
				if ( tGap < pacingDelay ) {  
					checkAvailableResult = 0;							
					printf("  disable %d this time\n", i);
				}
			}						
		}
		
	  // if this path cannot transmit whole packet, still transmit?
      if ( sendbytes[i] < mss || checkAvailableResult == 0 || checkAvailableResult2 == 0 || subflows_[i].status == false) {        //用這行表示當sendbytes比mss小的時候 不會送 (封包數量會下降 $↓ 但效率↓)
//if (sendbytes[i] < mss || dupacks > 0) {  
	  //if (sendbytes[i] <= 0) {       //用這行表示當sendbytes有空間 就送 (封包數量會上降 $↑ 效率↑)
			
#ifdef MPTCP_debug				
			printf(" !!!!!!!!! unavailable now !!!!!!!!! \n");
#endif			
			continue;
      }
	  
	   availPathCounter++;
	   availPath[i] = true;
				
	   if ( method == 1 ) {
			if ( subflows_[i].rtt_ < minRTT ) {			
				targetIndex = i;
				minRTT = subflows_[i].rtt_;			
				
				if ( randCounter > 0 ) {
					randCounter = 0;
					needRandom = false;
				}
					
				randAssignIndex[randCounter] = i;
				randCounter++;				
#ifdef MPTCP_debug			  				
				printf("      minRTT = %f, targetIndex = %d\n", minRTT, targetIndex);
#endif				
			} else if ( subflows_[i].rtt_ == minRTT ) {
				randAssignIndex[randCounter] = i;
				randCounter++;			
				targetIndex = i;	//prefer use subflow 2 or 3
				
				if ( enableRandSelectSameRTTlink == true )
					needRandom = true;	
			}			
	   } else if ( method == 2 ) {
			if ( subflows_[i].price_ < minPrice ) {
				targetIndex = i;
				minPrice = subflows_[i].price_;				
#ifdef MPTCP_debug			  				
				printf("      minPrice = %f, targetIndex = %d\n", minPrice, targetIndex);
#endif				
			}	
	   } else if ( method == 3 || method == 4) {
			//----------------------ETOM
            atleastonepath = i;	
#ifdef MPTCP_debug			  				
	        printf(" sendbyte[%d]: %d  is OK now !!!!!!!!! \n", i, sendbytes[i]);
#endif
            //----------------------ETOM
			
			if ( subflows_[i].rtt_ < minRTT && newSubflow == false ) {			
				fastlink = i;
				minRTT = subflows_[i].rtt_;		
			}

			if ( newSubflow == true ) {
			// new connection needs to send packet first				
				targetIndex = i;
			}				
			
	   } else {
			fprintf (stderr,"MptcpAgent::send_control(), fatal error. scheduler cannot find assigned method\n");
				abort ();
	   }
	   		
#if 0
      if (!slow_start) {
         double cwnd_i = subflows_[i].tcp_->mptcp_get_cwnd ();
         /*
            As recommended in 4.1 of draft-ietf-mptcp-congestion-05
            Update alpha only if cwnd_i/mss_i != cwnd_new_i/mss_i.
         */
         if (abs(subflows_[i].tcp_->mptcp_get_last_cwnd () - cwnd_i) < 1) {
            calculate_alpha ();
         }
         subflows_[i].tcp_->mptcp_set_last_cwnd (cwnd_i);
      }
#endif
    }
	// end of for
	
	if ( availPathCounter >= 1 ) {
		printf("availPathCounter\t%f\t%d\t%d\t%d\t%d\t%f\t%f\t%f\n", curTime, availPathCounter, availPath[0], availPath[1], availPath[2], dumpCwnd_[0], dumpCwnd_[1], dumpCwnd_[2]);
		NumMultipleSubflowSchTime++;
	}
	
	if ( numofConnectedPath > 1 ) {
		enable_ETOM = true;
	}
		
	// random select same RTT subflows 	
	if ( method == 1 && needRandom == true ) {
		int randomValue = uniform_integer(0, randCounter);
		//int randomValue = cur_choice % randCounter;
		//cur_choice ++;
		targetIndex = randAssignIndex[randomValue];
		schIndex = targetIndex;
//#ifdef MPTCP_debug			 						
		printf(" same RTT case ! randCounter = %d\n", randCounter);		
		printf(" random results, targetIndex %d, randomValue %d \n", targetIndex, randomValue);
//#endif			
		
		//for(int j = 0; j < 100 ; j ++) {
		//	printf(" random test,  %d \n", uniform_integer(0, 3));
		//}
		//abort ();
		// uniform_integer(0, 3), output range = 0~2
	}
		
	noterase = 0;
	//if ( targetIndex >= 0 && method == 1 && RTTwithSchQueueMode == true && numofConnectedPath == sub_num_) {
	if ( method == 1 && RTTwithSchQueueMode == true ) {
		double local_Min_RTT = 999999;
		int local_target = -1;		
		int sendable_path = -1;
		bool enQueueEvent = false;
		
		if ( targetIndex >= 0 ) {
			printf("had available path to send packet, targetIndex %d \n", targetIndex);
			if(scheduled_queue_num[targetIndex] != 0){
				printf("send packet by scheduled queue total byte > 0\n");
				int mss = subflows_[targetIndex].tcp_->size ();
				real_mcurseq_ = mcurseq_;
				real_total_bytes_ = total_bytes_;              
				//dequeue a packet from scheduled queue and transmit it.

				if ( reschedulingQueue_DSN.size() > 0 ) {
					enSchQueue(targetIndex, reschedulingQueue_len[0], reschedulingQueue_DSN[0]);
					sendbytes[targetIndex] = deSchQueue(targetIndex);					
					reschedulingQueue_DSN.erase(reschedulingQueue_DSN.begin()); 
					reschedulingQueue_len.erase(reschedulingQueue_len.begin()); 
					printf("get packet from _reschQ_, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[targetIndex]);
				} else {
					sendbytes[targetIndex] = deSchQueue(targetIndex);
					total_bytes_ = sendbytes[targetIndex];
					printf("get packet from schQ, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[targetIndex]);
				}			
							
				//enqeue a packet to sended queue 
#ifdef MPTCP_debug			 					
				printf("enqueue a sended packet-------------------\n");
#endif
				enSendQueue(targetIndex, total_bytes_);
				noterase = 1;
				sentBySchQueue = true;
			}
		}
		if ( sentBySchQueue == false ) {
			printf("do packet scheduled scheduled_queue_num %d, scheduled_queue_num %d, scheduled_queue_num %d\n", scheduled_queue_num[0], scheduled_queue_num[1], scheduled_queue_num[2]);
			int randCounter2 = 0;
					
			for (int k = 0; k < sub_num_ ; k++) {			
				if ( subflows_[k].rtt_ <= local_Min_RTT && scheduled_queue_num[k] < max_SchQueue_size && connectEstablishedPath[k] == true) {			
					local_target = k;									
					local_Min_RTT = subflows_[k].rtt_;				
					
					if  ( availPath[k] == true )
						sendable_path = k;
				}			
				printf("%d, subflows_[k].rtt_ %f, local_Min_RTT %f local_target %d\n", k , subflows_[k].rtt_, local_Min_RTT, local_target);				
			}
						
			
			if ( enableRandSelectSameRTTlink == true ) 			
			for (int k = 0; k < sub_num_ ; k++) {
				if ( subflows_[k].rtt_ == local_Min_RTT && scheduled_queue_num[k] < max_SchQueue_size) {			
					randAssignIndex[randCounter2] = k;
					randCounter2++;								
				}			
			}
			
			printf("randCounter2 %d \n", randCounter2 );	
			
			if ( local_target == -1 ) {
				// all scheduled queue full or connection not yet setup 
				if ( targetIndex >= 0 )
					;
				else 
					targetIndex = -1;			
			} else {
				//enqueue 		
				
				if ( randCounter2 > 1 ) {
					int randomValue = uniform_integer(0, randCounter2);
					//int randomValue = cur_choice % randCounter2;
					//cur_choice ++;
					targetIndex = randAssignIndex[randomValue];
//#ifdef MPTCP_debug			 										
					printf(" random results, targetIndex %d, randomValue %d \n", targetIndex, randomValue);
//#endif					
				} else 
					targetIndex = local_target;
//#ifdef MPTCP_debug			 									
				printf("		RTT RTTwithSchQueueMode, targetIndex %d \n", targetIndex);
//#endif				
				int mss = subflows_[targetIndex].tcp_->size ();   
				if (mss > total_bytes_)
					mss = total_bytes_;                    
				
				if ( subflows_[targetIndex].sch_seq == 0 ) 
					subflows_[targetIndex].sch_seq = 1;
				else 
					subflows_[targetIndex].sch_seq += mss;
				
				//這邊的sch_seq還是會跟recvACK那邊或是ns2 out.tr上面的號碼對不上..
				printf("dumpScheduledPath\t%f\t%d\t%f\t%f\t%f\t%d\t%d\t%d\n", Scheduler::instance().clock(), (targetIndex + 1), subflows_[0].rtt_, subflows_[1].rtt_, subflows_[2].rtt_, subflows_[targetIndex].sch_seq, mcurseq_, (int)(sendQueue_.size()-1));	
								
				printf("dumpScheduledLogSubflow%d\t%f\t%d\t%d\t%d\t%d\n", (targetIndex + 1), Scheduler::instance().clock(), subflows_[targetIndex].sch_seq, mcurseq_, scheduled_queue_num[targetIndex], (int)(sendQueue_.size()-1));					
								
				enSchQueue(targetIndex, mss, 0);	

				targetIndex = -1;
				notMatchTarget = false;		
				enQueueEvent = true;
			}		
		}
	}	
			
	//RR	
	if ( method == 3 || (method == 4 && enable_ETOM == false) ) {
		for(int index = curPath+1; index < sub_num_; index++) {
			if ( availPath[index] == true ) {
				targetIndex = index; 
				curPath = index;
#ifdef MPTCP_debug			 
				printf("      1 RR targetIndex = %d\n", targetIndex);
#endif				
				break;
			}	
		}		
		if ( targetIndex == -1 ) {
			for(int index = 0; index < sub_num_; index++) {
				if ( availPath[index] == true ) {
					targetIndex = index; 
					curPath = index;
#ifdef MPTCP_debug			 					
					printf("     2 RR targetIndex = %d\n", targetIndex);
#endif					
					break;
				}	
			}
		}
	}
	
	//----------------------ETOM
    if ( method == 4 && enable_ETOM == true && atleastonepath >= 0 && newSubflow == false ) {//ETOM			 					
	    printf("%f  method = %d, atleastonepath = %d\n", curTime, method, atleastonepath);
                
		if(scheduled_queue_num[atleastonepath] != 0){
			printf("send packet by scheduled queue total byte > 0\n");
			int mss = subflows_[atleastonepath].tcp_->size ();
			real_mcurseq_ = mcurseq_;
			real_total_bytes_ = total_bytes_;              
			//dequeue a packet from scheduled queue and transmit it.

			if ( reschedulingQueue_DSN.size() > 0 ) {
					enSchQueue(atleastonepath, reschedulingQueue_len[0], reschedulingQueue_DSN[0]);
					sendbytes[atleastonepath] = deSchQueue(atleastonepath);
					reschedulingQueue_DSN.erase(reschedulingQueue_DSN.begin()); 
					reschedulingQueue_len.erase(reschedulingQueue_len.begin()); 
					printf("get packet from _reschQ_, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[atleastonepath]);
			} else {
					sendbytes[atleastonepath] = deSchQueue(atleastonepath);
					total_bytes_ = sendbytes[atleastonepath];
					printf("get packet from schQ, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[atleastonepath]);
			}			
				
			targetIndex = atleastonepath;                   
			
			//enqeue a packet to sended queue 
#ifdef MPTCP_debug			 					
			printf("enqueue a sended packet-------------------\n");
#endif
			
			/*
			int localIndex = atleastonepath;
			int start_search_index = sended_queue_index[localIndex];						
			int end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];
			printf(" start_search_index %d, end_search_index %d, sended_queue_arrive_time[j][start_search_index] %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
					
			for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
				printf(" [debug only] search_index %d, DSN %d, arrive_time %f, estimated_transmission_time %f\n", search_index, sended_queue_global_seq[localIndex][search_index], sended_queue_arrive_time[localIndex][search_index], sended_queue_estimated_transmission_time[localIndex][search_index] );									 
			}
			*/
			
			enSendQueue(atleastonepath, total_bytes_);
			noterase = 1;
		}
		else{
			//find the min path
//#ifdef MPTCP_debug			 					
			printf("----- send packet by SWTC -----\n");
//#endif

			int min_path = atleastonepath;
			double min_time = 100000.0;
			double expect_time = 0.0;
			double waiting_time = 0;
			double estimated_prePkt_Tx_delay = 0;
			double cwnd_next_open_time = 0;
			int directSendCase[100];
			
			//SWTC algorithm
			for(int j=0; j< sub_num_; j++){				
				if ( connectEstablishedPath[j] == true && subflows_[j].status == true) {
					double srtt = subflows_[j].tcp_->mptcp_get_srtt();
					double TxDelay = ETOM_PacketSize/((sub_bandwidth[j])*1000);										
					double coefficient = 1/pow(1-subflows_[j].lossRate_, 2) - 1/(1-subflows_[j].lossRate_) + 1;			
					directSendCase[j] = 0;
					//int numPreSentData = 0;
					int localIndex = j;
					int start_search_index = sended_queue_index[localIndex];						
					int end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];					
					printf(" start_search_index %d, end_search_index %d, sended_queue_arrive_time[j][start_search_index] %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
					/*				
					for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
						printf(" [debug only] search_index %d, DSN %d, arrive_time %f, estimated_transmission_time %f, numPreSentData %d\n", search_index, sended_queue_global_seq[localIndex][search_index], sended_queue_arrive_time[localIndex][search_index], sended_queue_estimated_transmission_time[localIndex][search_index], numPreSentData );									 
						if ( sended_queue_estimated_transmission_time[localIndex][search_index] > curTime ) 
							numPreSentData++;
					}
					*/
					
					//scheduled_queue_num: how many packets in scheduled queue?
					//printf("\nsubflow[%d] numPreSentData %d outstand_pkt_ %d scheduled_queue_num %d\n", j, numPreSentData, subflows_[j].outstand_pkt_, scheduled_queue_num[j]);
					printf("\nsubflow[%d] outstand_pkt_ %d scheduled_queue_num %d\n", j, subflows_[j].outstand_pkt_, scheduled_queue_num[j]);
					
					int temp_index = sended_queue_index[j] + sended_queue_num[j];	//this index is for current sending packet 
					temp_index--;
					//printf("last one sended_queue_global_seq[%d][%d] %d, sended_queue_estimated_transmission_time %f\n", j, temp_index, sended_queue_global_seq[j][temp_index], sended_queue_estimated_transmission_time[j][temp_index]);
					
					// 估算 bottleneck 的累加transmission delay 
					double temp_Estimated_delay = sended_queue_estimated_transmission_time[j][temp_index] - curTime;
					estimated_prePkt_Tx_delay = ( temp_Estimated_delay > 0) ? temp_Estimated_delay : 0 ;
					printf("estimated_prePkt_Tx_delay %f\n", estimated_prePkt_Tx_delay);
					
					if ( subflows_[j].tcp_->getpipectrlStatu() == false && subflows_[j].updateOutstandPktFlag_ == true ) {
						subflows_[j].updateOutstandPktFlag_ = false;
						printf("just leave recover mode, reset flag\n");
					}
					
					
					if ( availPath[j] == true ) {
						// this packet can immediately send out
						// 我傳這個packets 到底要等多久
						//waiting_time = (subflows_[j].outstand_pkt_ + scheduled_queue_num[j] + 1) * TxDelay;
						waiting_time = (scheduled_queue_num[j] + 1) * TxDelay + estimated_prePkt_Tx_delay;
						//waiting_time = (numPreSentData + scheduled_queue_num[j] + 1) * TxDelay;
						directSendCase[j] = 1;
						printf("directly send case, TxDelay %f\n", TxDelay);
					} 
					
					/*else if ( subflows_[j].tcp_->getpipectrlStatu() == true ) { 
						// in TCP recover mode, this path cannot use now.
												
						if ( subflows_[j].updateOutstandPktFlag_ == false ) {
							subflows_[j].outstand_pkt_recover_ = subflows_[j].outstand_pkt_ - 1;
							subflows_[j].updateOutstandPktFlag_ = true;
							subflows_[j].schQuota_ = 0;
							printf("enter recover mode, outstand_pkt_recover %d\n", subflows_[j].outstand_pkt_recover_);
						}				
						
						printf("in TCP recover mode\n");
											
						// if this link still active and received packet
						//if ( subflows_[j].outstand_pkt_ < subflows_[j].outstand_pkt_recover_ )  {
						if ( subflows_[j].schQuota_ > 0 && j == fastlink)  {
							printf("if this link still active and received packet\n");	
							subflows_[j].schQuota_ -= 1;
							waiting_time = 0;
						} else 
							waiting_time = 999999;
					} */
					
					else {
						if ( sended_queue_num[j] == 0 ) {
							// no any previously sent packet, but cwnd is open? 
							// impossible case
							fprintf (stderr,"MptcpAgent::send_control(), fatal error. SWTC, sended_queue_num = 0, but cwnd open\n");
							abort ();							
						} else {						
							// has sent out packets, calculated the fastest return ACK
							
							double curr_cwnd = subflows_[j].tcp_->mptcp_get_cwnd();
							double estimated_cwnd = 0;
							double cwnd_visitor = 0;
							double num_RTT = 0;			
							bool moreThanOriginalCwnd = false;
														
														
							// step 1: found ACK returned time from outstanding packet
							double ackReturneTime = 0;
								
							localIndex = j;
							start_search_index = sended_queue_index[localIndex];						
							end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];
							//printf(" start_search_index %d, end_search_index %d, sended_queue_arrive_time[j][start_search_index] %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
											
							/*
							for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
								printf(" [debug only] search_index %d, DSN %d, TCP seq %d, arrive_time %f, estimated_transmission_time %f\n", search_index, sended_queue_global_seq[localIndex][search_index], sended_queue_local_seq[localIndex][search_index], sended_queue_arrive_time[localIndex][search_index], sended_queue_estimated_transmission_time[localIndex][search_index] );									 
							}
							*/							
							
							for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
								// timeout happened will brings this case
								if( sended_queue_arrive_time[j][search_index] > 0 && sended_queue_arrive_time[j][search_index] <= curTime ) {
									temp_scheduled_flag[j][search_index] = true;									
									break;
								}
																
								if ( sended_queue_use_for_scheduled[j][search_index] == false && temp_scheduled_flag[j][search_index] == false ) {
									temp_scheduled_flag[j][search_index] = true;
									//printf(" target search_index %d, DSN %d, TCP Seq %d, arrive_time %f\n", search_index, sended_queue_global_seq[j][search_index], sended_queue_local_seq[j][search_index], sended_queue_arrive_time[j][search_index] );									 
									ackReturneTime = sended_queue_arrive_time[j][search_index];
									break;
								}
							}
							if ( ackReturneTime > 0 )
								cwnd_next_open_time = ackReturneTime - curTime; 	
							
							
							// step 2: calculate ACK return time based on RTT 
							// 進到這個loop就是全部的outstanding pkts都找不到 才會算RTT
							if ( ackReturneTime == 0 ) {
								subflows_[j].tcp_->set_esti_cwnd( curr_cwnd );
								for(int estCWND_index = 1; estCWND_index <= (subflows_[j].outstand_pkt_ + scheduled_queue_num[j]) ; estCWND_index ++ ) {
									estimated_cwnd = subflows_[j].tcp_->esti_cwnd();
									cwnd_visitor++;
									//printf("%d, cwnd_visitor %f,   estimated_cwnd %f curr_cwnd %f\n", estCWND_index, cwnd_visitor, estimated_cwnd, curr_cwnd);							
									
									/*
									if ( cwnd_visitor > (int)curr_cwnd && moreThanOriginalCwnd == false ) {
										cwnd_visitor = 1;
										moreThanOriginalCwnd = true;									
										num_RTT++;
										printf("1 moreThanOriginalCwnd num_RTT %f\n", num_RTT);							
									} else if ( cwnd_visitor > (int) estimated_cwnd && moreThanOriginalCwnd == true ) {
										cwnd_visitor = 1;									
										num_RTT++;
										printf("2 moreThanOriginalCwnd num_RTT %f\n", num_RTT);							
									}
									*/
									if ( cwnd_visitor > (int) estimated_cwnd ) {
										cwnd_visitor = 1;									
										num_RTT++;
										//printf("2 moreThanOriginalCwnd num_RTT %f\n", num_RTT);							
									}
								}							
								cwnd_next_open_time = num_RTT * (srtt * coefficient);								
								printf("num_RTT %f\n", num_RTT);
							}							
							//abort();															
														
							//if ( cwnd_next_open_time > srtt ) 
							//	cwnd_next_open_time = srtt;							
							
							// if all outstanding ack used to calculated waiting_time, 
							// this subflow should be blocked until new data segment sends out							
							//if ( ackReturneTime == 0 )
							//	cwnd_next_open_time = 999999;
						}							
						//waiting_time = ((subflows_[j].outstand_pkt_ +  scheduled_queue_num[j] + 1) * TxDelay) + cwnd_next_open_time;	
						
						waiting_time = ((scheduled_queue_num[j] + 1) * TxDelay) + cwnd_next_open_time + estimated_prePkt_Tx_delay; 
						
						//waiting_time = ((numPreSentData +  scheduled_queue_num[j] + 1) * TxDelay) + cwnd_next_open_time;
						printf("TxDelay %f, cwnd_next_open_time %f, estimated_prePkt_Tx_delay %f\n", TxDelay, cwnd_next_open_time, estimated_prePkt_Tx_delay);
					}
					
					expect_time = (srtt * coefficient) + waiting_time;
					printf(" %f		---> subflow[%d] [expect_time: %f],    srtt %f (coefficient %f), waiting_time %f\n", curTime, j, expect_time, srtt, coefficient, waiting_time);
					
					if ( waiting_time != 999999 ) 
						printf("dump_expect_time_subflow%d\t%f\t%f\t%f\t%f\t%f\n", j, curTime, expect_time, srtt, coefficient, waiting_time);
					
					//限制 schedule 數量
					//if(expect_time <= min_time){
					if(expect_time <= min_time && (scheduled_queue_num[j] < max_SchQueue_size || (j == atleastonepath)) ){
					//if(expect_time <= min_time && ( (scheduled_queue_num[j] < max_SchQueue_size && subflows_[j].lossRate_ < 0.01 ) || (j == atleastonepath)) ){
						
						
						min_time = expect_time;
						min_path = j;							
					}
				}   // end of connectEstablishedPath[j] == true                 
			}  // end of for
			
			printf("dump_min_path\t%d\t%f\t%f\t%d\n", min_path, curTime, expect_time, directSendCase[min_path]);
			
			/*
			if ( directSendCase == false && min_path == 0 ) {
				fprintf (stderr,"test!, subflow 0 send by enqueue wait transmission case\n");
				abort ();
			}
			*/
			
			// update scheduled flag, 
			for( int j=0; j < sub_num_; j ++ ){ 
				int start_search_index = sended_queue_index[j];						
				int end_search_index = sended_queue_index[j] + sended_queue_num[j];
//				printf("[update scheduled flag] start_search_index %d, end_search_index %d\n", start_search_index, end_search_index);							
				for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
					if ( j == min_path && temp_scheduled_flag[j][search_index] == true ) {
						sended_queue_use_for_scheduled[j][search_index] = true;
						temp_scheduled_flag[j][search_index] = false;
//						printf("make sure targeted search_index %d, DSN %d, TCP Seq %d, arrive_time %f\n", search_index, sended_queue_global_seq[j][search_index], sended_queue_local_seq[j][search_index], sended_queue_arrive_time[j][search_index] );									 												
					} else if (j != min_path && temp_scheduled_flag[j][search_index] == true ) {
						sended_queue_use_for_scheduled[j][search_index] = false;
						temp_scheduled_flag[j][search_index] = false;
//						printf("untargeted (not min path scope) search_index %d, DSN %d, TCP seq %d, arrive_time %f\n", search_index, sended_queue_global_seq[j][search_index], sended_queue_local_seq[j][search_index], sended_queue_arrive_time[j][search_index] );									 												
					}					
				}
			}
			
					
			printf("DumpStaticExpectTime\t%f\t%f\t%f\t%d\t%d\n", curTime, min_time, (curTime + min_time), min_path, mcurseq_);
					
			printf("	min_path %d  atleastonepath %d\n", min_path, atleastonepath);
			if(min_path == atleastonepath){
				printf("		send packet by available path is the fastest path \n");
				// available path is the fastest path
				targetIndex = atleastonepath;
								
				if ( reschedulingQueue_DSN.size() > 0 ) {
					real_mcurseq_ = mcurseq_;
					real_total_bytes_ = total_bytes_;       
					noterase = 1;
					
					mcurseq_ = reschedulingQueue_DSN[0];
					sendbytes[targetIndex] = reschedulingQueue_len[0];
					reschedulingQueue_DSN.erase(reschedulingQueue_DSN.begin()); 
					reschedulingQueue_len.erase(reschedulingQueue_len.begin()); 
					printf("get packet from _reschQ_, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[targetIndex]);					
				} else {
					int mss = subflows_[targetIndex].tcp_->size ();
					if (sendbytes[targetIndex] > total_bytes_)
						sendbytes[targetIndex] = total_bytes_;
					if (sendbytes[targetIndex] > mss) 
						sendbytes[targetIndex] = mss;        
										
					printf("get packet from sending queue, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[targetIndex]);
				}	
				
//#ifdef MPTCP_debug			 					
				printf("[schedulerLog] %f min_path directly sent packet, subflow %d------------- scheduled mcurseq_ %d\n", curTime, atleastonepath, mcurseq_);
//#endif
				//enqeue a packet to sended queue    
				//struct sended_queue_packet tmp(mcurseq_, Scheduler::instance().clock() + 0.09472 + (1359.0*8.0)/((sub_bandwidth[min_path])*1000));
				//sended_queue[min_path].push_back(tmp);   
				
				/*
				int localIndex = min_path;
				int start_search_index = sended_queue_index[localIndex];						
				int end_search_index = sended_queue_index[localIndex] + sended_queue_num[localIndex];
				printf(" start_search_index %d, end_search_index %d, sended_queue_arrive_time[j][start_search_index] %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
						
				for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
					printf(" [debug only] search_index %d, DSN %d, TCP Seq %d, arrive_time %f, estimated_transmission_time %f\n", search_index, sended_queue_global_seq[localIndex][search_index], sended_queue_local_seq[localIndex][search_index], sended_queue_arrive_time[localIndex][search_index], sended_queue_estimated_transmission_time[localIndex][search_index] );									 
				}
				*/
				
				enSendQueue(min_path, sendbytes[targetIndex]);
				
			}
			else{   
				printf("		send packet by unavailable fastest path \n");
				// unavailable fastest path, do schedule
				
				if ( reschedulingQueue_DSN.size() > 0 ) {
					enSchQueue(min_path, reschedulingQueue_len[0], reschedulingQueue_DSN[0]);					
					reschedulingQueue_DSN.erase(reschedulingQueue_DSN.begin()); 
					reschedulingQueue_len.erase(reschedulingQueue_len.begin()); 
					printf("get packet from _reschQ_, mcurseq_ %d, sendbytes %d\n", mcurseq_, sendbytes[min_path]);					
				} else {
					int mss = subflows_[min_path].tcp_->size ();   
					if (mss > total_bytes_)
						mss = total_bytes_;                    				
					enSchQueue(min_path, mss, 0);		
				}	
				
				targetIndex = -1;
				notMatchTarget = false;
			}
		}                                
    }
    //----------------------ETOM
	
	//if ( sub_num_ == 3 ) {
		printf("dumpcwinpipe\t%f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", Scheduler::instance().clock(), statusCheck1_[0], statusCheck2_[0], PileStatus_[0], subflows_[0].tcp_->getcwin(), subflows_[0].tcp_->getpipe(), statusCheck1_[1], statusCheck2_[1], PileStatus_[1], subflows_[1].tcp_->getcwin(), subflows_[1].tcp_->getpipe(), statusCheck1_[2], statusCheck2_[2], PileStatus_[2], subflows_[2].tcp_->getcwin(), subflows_[2].tcp_->getpipe());	
	//}		
				
	if (targetIndex >= 0) {
		printf("		core sendmsg function, targetIndex %d\n", targetIndex);
		//use different scheduling algorithms 
		int mss = subflows_[targetIndex].tcp_->size ();		
	  if (sendbytes[targetIndex] > total_bytes_)
			sendbytes[targetIndex] = total_bytes_;
	  if (sendbytes[targetIndex] > mss) 
			sendbytes[targetIndex] = mss;
	   
		// only update sended information 
	  if (method == 4 && enable_ETOM == false) {	  
			enSendQueue(targetIndex, sendbytes[targetIndex]);
	  }
		
//#ifdef MPTCP_debug			  		
	   printf("[schedulerLog] %f mptcp_id:%d, packet-scheduled to subflows_[%d], sendbytes = %d, mcurseq_ %d\n", curTime, mptcp_id, targetIndex, sendbytes[targetIndex], mcurseq_); 
//#endif	   
	   
	   if ( sentBySchQueue == false ) {
		   if ( subflows_[targetIndex].sch_seq == 0 ) 
				subflows_[targetIndex].sch_seq = 1;
			else 
				subflows_[targetIndex].sch_seq += mss;
		}
	   
	   //if ( sub_num_ == 3 ) {
			if ( sentBySchQueue == false ) {
				printf("dumpScheduledPath\t%f\t%d\t%f\t%f\t%f\t%d\t%d\t%d\n", Scheduler::instance().clock(), (targetIndex + 1), subflows_[0].rtt_, subflows_[1].rtt_, subflows_[2].rtt_, subflows_[targetIndex].sch_seq, mcurseq_, (int)(sendQueue_.size()-1));				
				printf("dumpScheduledLogSubflow%d\t%f\t%d\t%d\t%d\t%d\n", (targetIndex + 1), Scheduler::instance().clock(), subflows_[targetIndex].sch_seq, mcurseq_, scheduled_queue_num[targetIndex], (int)(sendQueue_.size()-1));			
			}	
			printf("dumpSubflow%d_MPTCP_sentSeq_dsn__sending_queue_size\t%f\t%d\t%d\t%d\t%d\n", (targetIndex + 1), Scheduler::instance().clock(), (subflows_[targetIndex].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1), scheduled_queue_num[targetIndex]);			
			printf("dumpAllSubflowMPTCP_sentSeq_dsn__sending_queue_size\t%f\t%d\t%d\t%d\t%d\t%d\n", Scheduler::instance().clock(), (targetIndex + 1), (subflows_[targetIndex].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1), scheduled_queue_num[targetIndex]);			
			
			for ( int j = 0; j < sub_num_ ; j++ ) {
				if ( availablecwnd_[j] < 0 )
					availablecwnd_[j] = 0;
				if ( outstanding_[j] < 0 )
					outstanding_[j] = 0;					
			}
							
			printf("dumpoutstandingandavailablecwnd\t%f\t%d\t%d\t%d\t%d\t%d\t%d\n", Scheduler::instance().clock(), outstanding_[0], outstanding_[1], outstanding_[2], availablecwnd_[0], availablecwnd_[1], availablecwnd_[2]);
			
		//}

		subflows_[targetIndex].tcp_->mptcp_add_mapping (mcurseq_, sendbytes[targetIndex]);
		subflows_[targetIndex].outstand_pkt_ +=1;		
		printf("s	outstand_pkt_ subflows_[0] %d, subflows_[1] %d, subflows_[2] %d \n", subflows_[0].outstand_pkt_, subflows_[1].outstand_pkt_, subflows_[2].outstand_pkt_);
		record_subflow_sent_ptks[targetIndex] +=1;        
		record_subflow_Busy[targetIndex] = 1;
		record_subflow_Util[(int)(Scheduler::instance().clock()*10)][targetIndex] = 1;
		record_subflow_SentPkts[(int)Scheduler::instance().clock()][targetIndex] += 1;
		subflows_[targetIndex].tcp_->sendmsg (sendbytes[targetIndex]);

		if ( Scheduler::instance().clock() > 10 && subflows_[targetIndex].lastSentTime_ > 10 ) {
			record_subflow_sentCounter[targetIndex] += 1;
			record_subflow_idleTime[targetIndex] += Scheduler::instance().clock() - subflows_[targetIndex].lastSentTime_;
		}	
							
		subflows_[targetIndex].lastSentTime_ = curTime;
		
		if ( noterase == 1) {
			mcurseq_ = real_mcurseq_;
			printf("[schedulerLog]  not erase case (not changed mcurseq_)\n");
		}
		else {
			mcurseq_ += sendbytes[targetIndex];
			printf("[schedulerLog]  mcurseq_ += sendbytes[targetIndex]; case \n");
		}
        total_bytes_ -= sendbytes[targetIndex];
		NumSchTime+= 1;
		double Available_Schedule_rate = NumMultipleSubflowSchTime/NumSchTime;
        printf("Available_Schedule_rate\t%f\n", Available_Schedule_rate);
		
		if ( targetIndex != schIndex )
			NumDominateTime += 1;
		printf("dumpTCPdominateSch\t%f\t%d\t%d\t%f\n", Scheduler::instance().clock(), (targetIndex + 1), (schIndex + 1), NumDominateTime/NumSchTime);
		
		
#ifdef MPTCP_debug			  		
	  	printf("[schedulerLog] %f @@ current mcurseq_ = %d, total_bytes_ = %d, subflows_[%d]_outstand_pkt_ = %d\n", curTime, mcurseq_, total_bytes_, targetIndex, subflows_[targetIndex].outstand_pkt_);
#endif		
		
		// 這邊先不考慮 total_bytes_ 大於 mss 的case (p->length - sendbytes) == 0), 同fulltcp disabling nagle
		for(int i = 0; i < sendQueue_.size(); i++) {	  
			//----------------------ETOM
			if(noterase == 1)
                break;
			//----------------------ETOM
					 	
#ifdef MPTCP_debug			  			
			printf("      		sendQueue_[i] = %d  sendbytes = %d\n", sendQueue_[i], sendbytes[targetIndex] );
#endif			
			if ( (sendQueue_[i] - sendbytes[targetIndex]) == 0 ) {
				sendQueue_.erase (sendQueue_.begin()+i); 
				currentSendQueueSize = sendQueue_.size();				
#ifdef MPTCP_debug			  				
				printf("      		sent ok! and new queue size = %ld\n", sendQueue_.size() );
#endif				
			} else {			
				fprintf (stderr,"MptcpAgent::send_control(), fatal error. %f subflow %d sendQueue_(%d) - sendbytes(%d) != 0\n", Scheduler::instance().clock(), (targetIndex+1), sendQueue_[i], sendbytes[targetIndex]);
				abort ();
			}
			break;	
		}	  	
		
		notMatchTarget = false;			
			
	} else {
#ifdef MPTCP_debug			
		printf("not found available subflows\n");
#endif		
		preNotMatchTargetTime = curTime; 			
	}	
  } 	
  }
  
  // turn on timer to send packets from scheduled queue 
  if( isSender == true ) {
	if( sendQueue_.empty() ) {
		printf("start ctimer \n");	
		ctimer.resched(0.01);
	} else {
		ctimer.force_cancel();
	}
  }  
}

void MptcpAgent::enSchQueue(int subflowIndex, int mss, int DSN) {

	if ( subflows_[subflowIndex].status != true ) {
		fprintf (stderr, 
             "MptcpAgent::enSchQueue fatal error. subflow inactive but call enSendQueue\n");
		abort();
	}

	int temp_index = 0;
	double curTime = Scheduler::instance().clock();
	temp_index = scheduled_queue_index[subflowIndex] + scheduled_queue_num[subflowIndex];
#ifdef MPTCP_debug			 					
	printf("temp_index =  %d + %d-------------------\n", scheduled_queue_index[subflowIndex], scheduled_queue_num[subflowIndex]);
#endif
	if(temp_index >(MAX_ARRAY_SIZE -1))
		temp_index = temp_index - MAX_ARRAY_SIZE;

	//printf("dumpScheduledPath\t%f\t%d\t%f\t%f\t%f\t%d\t%d\t%d\n", curTime, (subflowIndex + 1), subflows_[0].rtt_, subflows_[1].rtt_, subflows_[2].rtt_, (subflows_[subflowIndex].tcp_->mptcp_get_curseq() + 1), mcurseq_, (int)(sendQueue_.size()-1));				
		
	if ( DSN == 0 )	{
		scheduled_queue_global_seq[subflowIndex][temp_index] = mcurseq_;                       	
		
		if(mss > total_bytes_){					
			scheduled_queue_packet_length[subflowIndex][temp_index] = total_bytes_;
			
			printf("case 1 [schedulerLog] [schedulingEnqueue] %f subflow [%d] enqueue a scheduled packet, mcurseq_: %d, length: %d-------------------\n", curTime, subflowIndex, scheduled_queue_global_seq[subflowIndex][temp_index], mss);
			printf("case 1 scheduled_queue_num[%d] : %d\n", subflowIndex, scheduled_queue_num[subflowIndex]);
			
			mcurseq_ += total_bytes_ ;
			printf("case 1 [schedulerLog] current (next) _ mcurseq_ %d\n",mcurseq_);
			
	#ifdef MPTCP_debug			 					
			printf("case 1 total_bytes_: %d - mss: %d-------------------\n", total_bytes_, mss);
	#endif
			total_bytes_ -= total_bytes_;
	#ifdef MPTCP_debug			 					
			printf("case 1 total_bytes_:%d -------------------\n", total_bytes_);
	#endif
		}
		else{					
			scheduled_queue_packet_length[subflowIndex][temp_index] = mss;					
			
			printf("case 2 [schedulerLog] [schedulingEnqueue] %f subflow [%d] enqueue a scheduled packet, mcurseq_: %d, length: %d-------------------\n", curTime, subflowIndex, scheduled_queue_global_seq[subflowIndex][temp_index], mss);
			printf("case 2 scheduled_queue_num[%d] : %d\n", subflowIndex, scheduled_queue_num[subflowIndex]);
					
			mcurseq_ += mss ;
			printf("case 2 [schedulerLog] current (next) mcurseq_ %d\n",mcurseq_);
			
	#ifdef MPTCP_debug			 					
			printf("case 2 total_bytes_: %d - mss: %d-------------------\n", total_bytes_, mss);
	#endif
			total_bytes_ -= mss;
	#ifdef MPTCP_debug			 					
			printf("case 2 total_bytes_:%d -------------------\n", total_bytes_);
	#endif
		}
		
	}
	else {
		scheduled_queue_global_seq[subflowIndex][temp_index] = DSN;      
		scheduled_queue_packet_length[subflowIndex][temp_index] = mss;		
	}
	scheduled_queue_num[subflowIndex]++;			
	record_subflow_scheduled_queue[subflowIndex] = scheduled_queue_num[subflowIndex];
		
	//sorting DSN (test ok!, by Jensen 2014-08-11)
	int localIndex = subflowIndex;
	int start_search_index = scheduled_queue_index[localIndex];						
	int end_search_index = scheduled_queue_index[localIndex] + scheduled_queue_num[localIndex];
	int tempValue = 0;
	int tempLength = 0;
	//printf(" [clear outdated log] start_search_index %d, end_search_index %d, sended_queue_arrive_time %f\n", start_search_index, end_search_index, sended_queue_arrive_time[localIndex][start_search_index]);
	
	/*
	for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
		printf("search_index %d end_search_index %d   scheduled_queue_global_seq %d   scheduled_queue_packet_length %d\n", search_index, start_search_index, scheduled_queue_global_seq[subflowIndex][search_index], scheduled_queue_packet_length[subflowIndex][search_index] );									 
	}
	*/
	
	if ( DSN > 0 ) {
		for ( int i = start_search_index; i < end_search_index; i++ ) {
			for ( int j = i+1; j < end_search_index; j++ ) {
				if ( scheduled_queue_global_seq[subflowIndex][i] > scheduled_queue_global_seq[subflowIndex][j] ) {
					tempValue = scheduled_queue_global_seq[subflowIndex][i];
					tempLength = scheduled_queue_packet_length[subflowIndex][i];
					scheduled_queue_global_seq[subflowIndex][i] = scheduled_queue_global_seq[subflowIndex][j];
					scheduled_queue_packet_length[subflowIndex][i] = scheduled_queue_packet_length[subflowIndex][j];
					scheduled_queue_global_seq[subflowIndex][j] = tempValue;
					scheduled_queue_packet_length[subflowIndex][i] = tempLength;					
				}
			}
		}
		/*
		for ( int search_index = start_search_index; search_index < end_search_index; search_index++ ) {
			printf(" [after sort] search_index %d end_search_index %d   scheduled_queue_global_seq %d   scheduled_queue_packet_length %d\n", search_index, start_search_index, scheduled_queue_global_seq[subflowIndex][search_index], scheduled_queue_packet_length[subflowIndex][search_index] );									 
		}
		*/
	}
	
	
	//----------------------------------------------
	// due to this packet already assigned to scheduled queue, 
	// so earse it from sendQueue
	if ( DSN == 0 )
		for(int i = 0; i < sendQueue_.size(); i++) {	  	
	#ifdef MPTCP_debug			  			
				printf("      		sendQueue_[i] = %d  sendbytes = %d\n", sendQueue_[i], sendbytes[targetIndex] );
	#endif			
				if ( (sendQueue_[i] - mss) == 0 ) {
					sendQueue_.erase(sendQueue_.begin()+i); 
					currentSendQueueSize = sendQueue_.size();				
	#ifdef MPTCP_debug			  				
					printf("      		sent ok! and new queue size = %ld\n", sendQueue_.size() );
	#endif				
				} else {			
					fprintf(stderr,"MptcpAgent::send_control(), fatal error. sendQueue_[i](%d) - sendbytes(%d) != 0\n", sendQueue_[i], mss);
					abort();
				}
				break;	
		}	  		
	//----------------------------------------------
}

int MptcpAgent::deSchQueue(int subflowIndex) {

	if ( subflows_[subflowIndex].status != true ) {
		fprintf (stderr, 
             "MptcpAgent::deSchQueue fatal error. subflow inactive but call enSendQueue\n");
		abort();
	}

	int temp_sch_index;
	double curTime = Scheduler::instance().clock();
	int dataLength = 0;
	temp_sch_index = scheduled_queue_index[subflowIndex];
	
	//important !!
	mcurseq_ = scheduled_queue_global_seq[subflowIndex][temp_sch_index];
		
//#ifdef MPTCP_debug			 					
	printf("[schedulerLog] [schedulingDequeue] %f [total byte < 0] dequeue a scheduled packet (mcurseq_ %d) ------- subflow %d, current mcurseq_ (real_mcurseq_) %d, real_total_bytes_ %d\n", curTime, mcurseq_, subflowIndex, real_mcurseq_, real_total_bytes_);
//#endif
	dataLength = scheduled_queue_packet_length[subflowIndex][temp_sch_index];
	printf("// temp_sch_index %d, scheduled_queue_global_seq %d, scheduled_queue_packet_length %d\n", temp_sch_index, scheduled_queue_global_seq[subflowIndex][temp_sch_index], scheduled_queue_packet_length[subflowIndex][temp_sch_index]); 
		
	scheduled_queue_global_seq[subflowIndex][temp_sch_index] = 0;
	scheduled_queue_packet_length[subflowIndex][temp_sch_index] = 0;   
	scheduled_queue_index[subflowIndex]++;
	if(scheduled_queue_index[subflowIndex] > (MAX_ARRAY_SIZE -1))
		scheduled_queue_index[subflowIndex] = 0;
	//printf("scheduled_queue_num[%d]: %d-------------------\n", subflowIndex, scheduled_queue_num[subflowIndex]);
	scheduled_queue_num[subflowIndex]--;      
	record_subflow_scheduled_queue[subflowIndex] = scheduled_queue_num[subflowIndex];					
	
	printf("\nscheduled_queue_num[0]: %d-------------------\n", scheduled_queue_num[0]);
	printf("scheduled_queue_num[1]: %d-------------------\n", scheduled_queue_num[1]);
	printf("scheduled_queue_num[2]: %d-------------------\n", scheduled_queue_num[2]);
	return dataLength;
}

void MptcpAgent::enSendQueue(int subflowIndex, int bytes) {
	
	if ( subflows_[subflowIndex].status != true ) {
		fprintf (stderr, 
             "MptcpAgent::enSendQueue fatal error. subflow inactive but call enSendQueue\n");
		abort();
	}
	
	double curTime = Scheduler::instance().clock();
	double TxDelay = ETOM_PacketSize/(sub_bandwidth[subflowIndex]*1000);
	int temp_en_send_index = sended_queue_index[subflowIndex] + sended_queue_num[subflowIndex];
	
	printf(" 1 temp_en_send_index %d, sended_queue_index %d, sended_queue_num %d\n", temp_en_send_index, sended_queue_index[subflowIndex],sended_queue_num[subflowIndex]);
	
	if(temp_en_send_index >(MAX_ARRAY_SIZE -1))
		temp_en_send_index = temp_en_send_index - MAX_ARRAY_SIZE;
		
	double temp_Estimated_delay = sended_queue_estimated_transmission_time[subflowIndex][(temp_en_send_index-1)] - curTime;
	double estimated_prePkt_Tx_delay = ( temp_Estimated_delay > 0) ? temp_Estimated_delay : 0 ;			
	
	sended_queue_global_seq[subflowIndex][temp_en_send_index] = mcurseq_;
	sended_queue_pkt_length[subflowIndex][temp_en_send_index] = bytes;
	sended_queue_local_seq[subflowIndex][temp_en_send_index] = subflows_[subflowIndex].tcp_->get_tcp_seq();
	
	//sended_queue_arrive_time[subflowIndex][temp_en_send_index] = (Scheduler::instance().clock() + subflows_[subflowIndex].tcp_->mptcp_get_srtt() + TxDelay);
	sended_queue_arrive_time[subflowIndex][temp_en_send_index] = (Scheduler::instance().clock() + subflows_[subflowIndex].tcp_->mptcp_get_srtt() + TxDelay + estimated_prePkt_Tx_delay);
	sended_queue_use_for_scheduled[subflowIndex][temp_en_send_index] = false;
	
	printf("%f TxDelay %f, mcurseq_ %d TCP seq %d\n", Scheduler::instance().clock(), TxDelay, mcurseq_, subflows_[subflowIndex].tcp_->get_tcp_seq() );
	printf(" sended_queue_arrive_time [%d] [%d] = %f\n", subflowIndex, temp_en_send_index, sended_queue_arrive_time[subflowIndex][temp_en_send_index]);
	
	if ( temp_en_send_index > 0 ) {
		int preIndex = temp_en_send_index - 1;
		double LastTxTime = sended_queue_estimated_transmission_time[subflowIndex][preIndex];
		printf(" LastTxTime %f\n", LastTxTime);
		if ( (Scheduler::instance().clock() - LastTxTime) > TxDelay ) 
			sended_queue_estimated_transmission_time[subflowIndex][temp_en_send_index] = Scheduler::instance().clock() + TxDelay;
		else 
			sended_queue_estimated_transmission_time[subflowIndex][temp_en_send_index] = LastTxTime + TxDelay;							
		printf(" sended_queue_estimated_transmission_time [%d] [%d] = %f, sended_queue_global_seq %d\n", subflowIndex, temp_en_send_index, sended_queue_estimated_transmission_time[subflowIndex][temp_en_send_index], sended_queue_global_seq[subflowIndex][temp_en_send_index]);
	}
	
	sended_queue_num[subflowIndex]++;
	
	printf("\nsended_queue_num[0]: %d-------------------\n", sended_queue_num[0]);
	printf("sended_queue_num[1]: %d-------------------\n", sended_queue_num[1]);
	printf("sended_queue_num[2]: %d-------------------\n", sended_queue_num[2]);
}

int
MptcpAgent::uniform_integer(int start,int end){
	//srand(time(0));
	int base=rand();
	if(base==RAND_MAX)
		return uniform_integer(start,end);
	int range=end-start;
	int remainder=RAND_MAX%range;
	int bucket=RAND_MAX/range;
	if(base<RAND_MAX-remainder)
		return start+base/bucket;
	else
		return uniform_integer(start,end);
}


/*
 *  calculate alpha based on the equation in draft-ietf-mptcp-congestion-05
 *
 *  Peforme ths following calculation

                                      cwnd_i
                                 max --------
                                  i         2
                                      rtt_i
             alpha = tot_cwnd * ----------------
                               /      cwnd_i \ 2
                               | sum ---------|
                               \  i   rtt_i  /


 */

void
MptcpAgent::calculate_alpha ()
{
//printf(" calculate_alpha \n" );  
  double max_i = 0.001;
  double sum_i = 0;
  double avr_i = 0;
  int totalcwnd = 0;

  for (int i = 0; i < sub_num_; i++) {
	if ( subflows_[i].status == true ) {
	#if 1
		int backoff = subflows_[i].tcp_->mptcp_get_backoff ();
		// we don't utlize a path which has lots of timeouts
		if (backoff >= 4) 
		   continue;
	#endif

		double rtt_i  = subflows_[i].tcp_->mptcp_get_srtt ();
		int cwnd_i = (int)subflows_[i].tcp_->mptcp_get_cwnd ();
		//printf("calculate_alpha mptcp_id = %d, i %d rtt_i %f, cwnd_i %d \n", mptcp_id, i, rtt_i, cwnd_i);
		if (rtt_i < 0.000001) // too small. Let's not update alpha
		  return; 

		double tmp_i = cwnd_i / (rtt_i * rtt_i);
		if (max_i < tmp_i)
		  max_i = tmp_i;
		avr_i += tmp_i;

		sum_i += cwnd_i / rtt_i;
		totalcwnd += cwnd_i;
	}
  }
  if (sum_i < 0.001)
    return;

  alpha_ = totalcwnd * max_i / (sum_i * sum_i);
  //printf("calculate_alpha\t%f mptcp_id = %d, alpha_ %f, totalcwnd %d, max_i %f, sum_i %f\n", Scheduler::instance().clock(), mptcp_id, alpha_, totalcwnd, max_i, sum_i);
}

/*
 * create ack block based on data ack information
 */
void
MptcpAgent::set_dataack (int ackno, int length)
{
  bool found = false;
  vector < dack_mapping >::iterator it = dackmap_.begin ();

  while (it != dackmap_.end ()) {
    struct dack_mapping *p = &*it;

    /* find matched block for this data */
    if (p->ackno <= ackno && p->ackno + p->length >= ackno &&
        p->ackno + p->length < ackno + length) {
      p->length = ackno + length - p->ackno;
      found = true;
      break;
    }
    else
      ++it;
  }

  /* if there's no matching block, add new one */
  if (!found) {
    struct dack_mapping tmp_map = { ackno, length };
    dackmap_.push_back (tmp_map);
  }

  /* re-calculate cumlative ack and erase old records */
  it = dackmap_.begin ();
  while (it != dackmap_.end ()) {
    struct dack_mapping *p = &*it;
    if (mackno_ >= p->ackno && mackno_ <= p->ackno + p->length)
      mackno_ = ackno + length;
    if (mackno_ > p->ackno + p->length) {
      it = dackmap_.erase (it);
    }
    else
      ++it;
  }
}

void MptcpAgent::reset_timer(double time)
{
	// time
	// total throughput
	// avg delay with out of order
	// total pricing
	// sending pakcets in queue
	// reordering packets in queue 
	double avgDelay = 0;
	double avgReOnlyDelay = 0;
	double avgReOnlyDelay_subflow1 = 0;
	double avgReOnlyDelay_subflow2 = 0;
	double avgReOnlyDelay_subflow3 = 0;
	static double avgOutoforderPkts = 0;
	static double sumOutoforderPkts = 0;
	static double statisticCount = 0;
	static int maxOutofOrderNum = 0;
	static bool initFlag = true;
	if ( total_counter > 0 )
		avgDelay = total_delay/total_counter;
	if ( total_counter_reorder > 0 ) 
		avgReOnlyDelay = total_reordering_delay/total_counter_reorder;
	if ( subflow1_counter_reorder > 0 ) 	
		avgReOnlyDelay_subflow1 = subflow1_reordering_delay/subflow1_counter_reorder;
	if ( subflow2_counter_reorder > 0 ) 	
		avgReOnlyDelay_subflow2 = subflow2_reordering_delay/subflow2_counter_reorder;
	if ( subflow3_counter_reorder > 0 ) 	
		avgReOnlyDelay_subflow3 = subflow3_reordering_delay/subflow3_counter_reorder;		
	
	if ( Scheduler::instance().clock() >= 10 ) {
		if ( sumOutoforderPkts > 0 && initFlag == false) {
		sumOutoforderPkts += reordering_buffer_.size();
		statisticCount += 1;
		avgOutoforderPkts = sumOutoforderPkts/statisticCount;
			
		if ( reordering_buffer_.size() > maxOutofOrderNum )
			maxOutofOrderNum = reordering_buffer_.size();		
		}
		
		if ( reordering_buffer_.size() > 0 && initFlag == true) {
			sumOutoforderPkts += reordering_buffer_.size();
			statisticCount += 1;
			initFlag = false;
		}	
	}	
	
	//if ( sub_num_ == 3 ) {
		printf("dumpSentPkts\t%f\t%d\t%d\t%d\n", Scheduler::instance().clock(), record_subflow_sent_ptks[0], record_subflow_sent_ptks[1], record_subflow_sent_ptks[2]);
		
		printf("dumpSRTT\t%f\t%f\t%f\t%f\n", Scheduler::instance().clock(), record_subflow_srtt[0], record_subflow_srtt[1], record_subflow_srtt[2]);
		printf("dumpRTT\t%f\t%f\t%f\t%f\n", Scheduler::instance().clock(), record_subflow_rtt[0], record_subflow_rtt[1], record_subflow_rtt[2]);
		printf("dumpRTO\t%f\t%f\t%f\t%f\n", Scheduler::instance().clock(), record_subflow_rto[0], record_subflow_rto[1], record_subflow_rto[2]);
		
		printf("dumpRttTs\t%f\t%f\t%f\t%f\n", Scheduler::instance().clock(), record_subflow_rtt_ts[0],record_subflow_rtt_ts[1], record_subflow_rtt_ts[2]);		
		printf("dumpResults\t%f\t%f\t%f\t%f\t%f\t%ld\t%ld\t%f\t%f\t%f\t%f\t%d\t%f\t%d\n", Scheduler::instance().clock(), total_throughput, avgDelay, avgReOnlyDelay, total_pricing, currentSendQueueSize, reordering_buffer_.size(), total_out_of_order_pkt, avgReOnlyDelay_subflow1, avgReOnlyDelay_subflow2, avgReOnlyDelay_subflow3, maxOutofOrderNum, avgOutoforderPkts, record_eschedulingQueueSize);	
		
		if ( record_subflow_sentCounter[0] > 0 && record_subflow_sentCounter[1] > 0 && record_subflow_sentCounter[2] > 0 ) {
			printf("dumpSubflowIdle\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n", Scheduler::instance().clock(), record_subflow_sentCounter[0],record_subflow_sentCounter[1], record_subflow_sentCounter[2], record_subflow_idleTime[0],record_subflow_idleTime[1], record_subflow_idleTime[2], (record_subflow_idleTime[0]/record_subflow_sentCounter[0]), (record_subflow_idleTime[1]/record_subflow_sentCounter[1]), (record_subflow_idleTime[2]/record_subflow_sentCounter[2]));		
		}
		printf("dumpSubflow_scheduled_queue\t%f\t%d\t%d\t%d\n", Scheduler::instance().clock(), record_subflow_scheduled_queue[0], record_subflow_scheduled_queue[1], record_subflow_scheduled_queue[2]);		
		printf("dumpCWNDResetTime\t%f\t%d\t%d\t%d\n", Scheduler::instance().clock(), record_subflow_cwndReset[0], record_subflow_cwndReset[1], record_subflow_cwndReset[2]);
		
	//}
	
	double NumSubflows = 3;
	double NumSbuflowsWork = 0;
	double Utilization = 0;
	for (int i = 0; i < 3; i ++ ) {
		if ( record_subflow_Busy[i] == 1 )
			NumSbuflowsWork+=1;
	}	
	
	if ( NumSbuflowsWork > 0 ) 
		Utilization = NumSbuflowsWork/NumSubflows;
	printf("dumpUtilization\t%f\t%f\n", Scheduler::instance().clock(), Utilization);
	
	for (int i = 0; i < 3; i ++ ) {
		record_subflow_Busy[i] = {0};
	}	
	
	//printf("%f\t%f\t%f\t%f\t%f\t%ld\t%ld\n", Scheduler::instance().clock(), total_throughput, avgDelay, avgReOnlyDelay, total_pricing, currentSendQueueSize, reordering_buffer_.size());	
	stimer.resched(time);

	
	 /*
	 	 printf("		 %f out of order, out of order packet = %d\n", Scheduler::instance().clock(), reordering_buffer_.size() );	
	 printf("			[total throughput %f] [delay %f] [counter %f]\n", total_throughput, total_delay, total_counter);
			  printf("			[avg throughput %f] [avg delay %f] [total_pricing %f] \n", total_throughput/total_counter, total_delay/total_counter, total_pricing);
			  */
}

void MptcpAgent::checkBufferSatatus(double time)
{	
	//double video_bitrate = 1271;   //unit  1271kbps/sec	
	//double decoding_rate = 15887.5;	//unit  158.875KB/sec, 158875B/sec 15887.5/100ms
	
	if ( video_bitrate == 0 ) {
		fprintf(stderr,"MptcpAgent::checkBufferSatatus: Please set video_bitrate %f\n", video_bitrate);
		abort();	
	}
	
	double decoding_rate = (video_bitrate/8)*100;	//unit  158.875KB/sec, 158875B/sec 15887.5/100ms
	const double decodeLength = (decoding_rate/8);
	bool videoFinish = false;	
	bool skipPCI = false;
	
	//YouTube trace
	//double videoContent = 40447442;	// you need to know your videoContentLength 	
						   
	//FTP traffic simulates YouTube trace	
	double videoContent = 10000000;	
	//double videoContent = 15000000;	
	                      
	static double cumulativedWatchContent = 0;
	static double minPCI = 2;
	static double avgPCI = 0;
	static double totalPlayTime = 0;
	static double PCI = 0;
	static bool reportFP = false;
	// 0, idle
	// 1, playing
	// 2, pause
	// 3, playing and re-connection
	// 4, playing and disconnection
	
	//if ( playBuffer_ > (lowWaterBuff/2) && startup_startTime > 0 && startup_delay == 0) { 	
	// 實驗 不同lambda使用這個 其他都是上面那個
	
	if ( lowWaterBuff == -1 || highWaterBuff == -1 ) {
		fprintf(stderr,"MptcpAgent::checkBufferSatatus: Please set lowWaterBuff %f and highWaterBuff %f\n", lowWaterBuff, highWaterBuff);
		abort();	
	}
	
	if ( playBuffer_ > lowWaterBuff && startup_startTime > 0 && startup_delay == 0) { 
		printf("rebuffer event, decoding_rate (per 100ms) = %f\n", decoding_rate);
		playout_Status = 1;		
		clientPlayoutStatus = 1;
		startup_delay = Scheduler::instance().clock() - startup_startTime;
		printf("rebuffer event, startup_delay = %f\n", startup_delay);		
	}	 
	
	if ( startup_delay > 0 ) {
		if ( playBuffer_ > 0 && (playout_Status == 1 || playout_Status == 3 || playout_Status == 4) ) {
			playBuffer_ -= decoding_rate;
			cumulativedWatchContent += decoding_rate;			
		}
		
		if ( playBuffer_ > (lowWaterBuff/2) && playBuffer_ < highWaterBuff) { 
			if ( pause_startTime > 0 )
				pause_cum_Time += Scheduler::instance().clock() - pause_startTime;
			pause_startTime = 0;
			playout_Status = 1;			
		}
			
		if ( playBuffer_ < lowWaterBuff && playout_Status != 2) { 
			printf("rebuffer event, memory low (%f),rebuffer, connection to server at %f\n", playBuffer_, Scheduler::instance().clock());		
			playout_Status = 3;
			clientPlayoutStatus = 1;
		}
		
		if ( playBuffer_ > highWaterBuff ) { 
			printf("rebuffer event, memory full (%f), disconnection at %f\n", playBuffer_, Scheduler::instance().clock());		
			playout_Status = 4;
			clientPlayoutStatus = 2;
		}	

		if ( playBuffer_ < 0 && playout_Status != 2 ) { 
			printf("rebuffer event, pause %f\n", Scheduler::instance().clock());		
			playout_Status = 2;
			clientPlayoutStatus = 1;
			lastPauseTime = Scheduler::instance().clock();
			if ( pause_startTime > 0 )
				pause_cum_Time += Scheduler::instance().clock() - pause_startTime;
			pause_startTime = Scheduler::instance().clock();
		}	
		
		//printf("rebuffer event,  cumulativedWatchContent %f\n", cumulativedWatchContent);	
		if ( cumulativedWatchContent >= videoContent && reportFP == false ) {
		//  if ( Scheduler::instance().clock() >= 25 && reportFP == false ) {
			totalPlayTime = Scheduler::instance().clock() - startup_startTime;
			PCI = 1 - ( pause_cum_Time/totalPlayTime );			
			if ( PCI < minPCI )
				minPCI = PCI;
			if ( avgPCI > 0 ) 
				avgPCI = (avgPCI + PCI) / 2;
			else 
				avgPCI = PCI;
				
			printf("rebuffer event, finish! %f\n", Scheduler::instance().clock());					
			//videoFinish = true;
			reportFP = true;
			skipPCI = true;
			 
			FILE *fp;
			fp = fopen("performance.tr","a");     /* open file pointer */
			fprintf(fp,"%f\t%f\t%f\t%f\n",startup_delay, PCI, minPCI, avgPCI);
			fclose(fp);
		}					
		
		if ( skipPCI == false ) {
			totalPlayTime = Scheduler::instance().clock() - startup_startTime;
			PCI = 1 - ( pause_cum_Time/totalPlayTime );
		
			if ( PCI < minPCI )
				minPCI = PCI;
		
			if ( avgPCI > 0 ) 
				avgPCI = (avgPCI + PCI) / 2;
			else 
				avgPCI = PCI;
		}
	
		printf("rebuffer status %d size\t%f, time %f\t%f\t%f\t%f\t\%f\t%f\n", playout_Status, playBuffer_, Scheduler::instance().clock(), pause_cum_Time, totalPlayTime, PCI, minPCI, avgPCI);
	}
		
	if ( videoFinish == false )
		playTimer.resched(time);
	else 
		playTimer.resched(1000);
}

void MptcpAgent::reset_timer2(double time)
{
	ctimer.resched(time);
	send_control();
}

void StatisticsTimer::expire(Event*)
{
	//  how long dump results?
    //	printf("timer expire!!\n");
	a_->reset_timer(0.01);	 
	//a_->reset_timer(1);	 
}

void CheckForwardingQueueTimer::expire(Event*)
{	
	printf("CheckForwardingQueueTimer expire!!\n");
	a_->reset_timer2(0.01);		
}


void playoutBuffterTimer::expire(Event*)
{			
	a_->checkBufferSatatus(0.1);		
}
