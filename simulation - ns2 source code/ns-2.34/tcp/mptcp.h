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
#ifndef mptcp_h
#define	mptcp_h

#include "tcp-full.h"
#include "mptcp-full.h"
#include "timer-handler.h"
#include "agent.h"
#include "node.h"
#include "packet.h"
#include <vector>

#define MAX_SUBFLOW 100
#define MAX_ARRAY_SIZE 500000
//#define MPTCP_debug

class MptcpAgent;

class StatisticsTimer : public TimerHandler {
public: 
	StatisticsTimer(MptcpAgent *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	MptcpAgent *a_;
};

class CheckForwardingQueueTimer : public TimerHandler {
public: 
	CheckForwardingQueueTimer(MptcpAgent *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	MptcpAgent *a_;
};

class playoutBuffterTimer : public TimerHandler {
public: 
	playoutBuffterTimer(MptcpAgent *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	MptcpAgent *a_;
};

struct subflow
{
  subflow ():used (false), status(false), addr_ (0), port_ (0), daddr_ (-1), dport_ (-1),
    link_ (NULL), target_ (NULL), tcp_ (NULL), scwnd_ (0), price_ (0), outstand_pkt_ (0), rtt_ (0), numReceivedPackets_(0), lossRate_ (0), outstand_pkt_recover_(0), updateOutstandPktFlag_(false), schQuota_(0), disable_ (false), lastSentTime_(0), lastReceivedAckTime_(0), sch_seq(0)
  {
  };
  bool used;
  bool status;
  int addr_;
  int port_;
  int daddr_;
  int dport_;
  NsObject *link_;
  NsObject *target_;
  MpFullTcpAgent *tcp_;
  int dstid_;  
  double scwnd_;
  double price_;
  int outstand_pkt_;
  double rtt_;
  double numReceivedPackets_;
  double lossRate_;
  int outstand_pkt_recover_;
  bool updateOutstandPktFlag_;
  int schQuota_;
  bool disable_;
  double lastSentTime_;
  double lastReceivedAckTime_;
  int sch_seq;
};

struct dstinfo
{
  dstinfo ():addr_ (-1), port_ (-1), active_ (false)
  {
  };
  int addr_;
  int port_;
  bool active_;
};

struct dack_mapping
{
  int ackno;
  int length;
};

struct reordering_buffer
{
    reordering_buffer (int seq, int subflowID, double time, Packet * pkt, Handler * h)
    {
        re_pktseq = seq;
		re_subflowID = subflowID;
		receivedTime = time;
        re_pkt = pkt;
        re_h = h;
    }
    int re_pktseq;
	int re_subflowID;
	double receivedTime; 
    Packet * re_pkt;
    Handler * re_h;
};

class MptcpAgent:public Agent
{
  friend class StatisticsTimer;
  friend class CheckForwardingQueueTimer;
  friend class playoutBuffterTimer;
  friend class XcpEndsys;
  
  virtual void sendmsg (int nbytes, const char *flags = 0);
public:
    MptcpAgent ();
   ~MptcpAgent ()
  {
  };
  void delay_bind_init_all ();
  void recv (Packet * pkt, Handler *);
  void set_dataack (int ackno, int length);
  int get_dataack ()
  {
    return mackno_;
  }
  double get_alpha ()
  {
    return alpha_;
  }
  double get_totalcwnd ()
  {
    totalcwnd_ = 0;
    for (int i = 0; i < sub_num_; i++) {
       totalcwnd_ += subflows_[i].tcp_->mptcp_get_cwnd ();
    }
    return totalcwnd_;
  }
  int command (int argc, const char *const *argv);
  void calculate_alpha ();
  TracedInt curseq_;
protected:
  int get_subnum ();
  int find_subflow (int addr, int port);
  int find_subflow (int addr);
  void send_control ();
  void add_destination (int addr, int port);
  bool check_routable (int sid, int addr, int port);
  void reset_timer(double time);
  void reset_timer2(double time);
  void checkBufferSatatus(double time);
  int uniform_integer(int start,int end);
  void enSchQueue(int subflowIndex, int mss, int DSN);
  int deSchQueue(int subflowIndex);
  void enSendQueue(int subflowIndex, int bytes);

  //-------------------ETOM---------------------
  int real_mcurseq_;
  int real_total_bytes_;
  int out_of_order_num_;
  int total_packet_num_;
  int noterase; 
  
  int scheduled_queue_global_seq[MAX_SUBFLOW][MAX_ARRAY_SIZE];
  int scheduled_queue_packet_length[MAX_SUBFLOW][MAX_ARRAY_SIZE];
  //scheduled_queue_packet::scheduled_queue_packet(int a, int b): global_seq(a), packet_length(b) {}

  int scheduled_queue_num[MAX_SUBFLOW];  
  int scheduled_queue_index[MAX_SUBFLOW];

  //struct sended_queue_packet sended_queue[3][100];
  int sended_queue_global_seq[MAX_SUBFLOW][MAX_ARRAY_SIZE];		//DSN
  int sended_queue_local_seq[MAX_SUBFLOW][MAX_ARRAY_SIZE];		//TCP seq
  int sended_queue_pkt_length[MAX_SUBFLOW][MAX_ARRAY_SIZE];		
  double sended_queue_arrive_time[MAX_SUBFLOW][MAX_ARRAY_SIZE];  
  double sended_queue_estimated_transmission_time[MAX_SUBFLOW][MAX_ARRAY_SIZE];  
  bool sended_queue_use_for_scheduled[MAX_SUBFLOW][MAX_ARRAY_SIZE];  
  bool temp_scheduled_flag[MAX_SUBFLOW][MAX_ARRAY_SIZE];  
  int sended_queue_num[MAX_SUBFLOW];    		//指資料的總比數
  int sended_queue_index[MAX_SUBFLOW];		//指資料起始段的index 
  
  double sub_bandwidth[MAX_SUBFLOW];
  //-------------------ETOM---------------------

  Classifier *core_;
  int sub_num_;
  int dst_num_;
  int total_bytes_;
  int mcurseq_;
  int mackno_;
  double totalcwnd_;
  double alpha_;
  
  //--------------------------------------------
  long expect_seq;  
  int mptcp_id;
  vector < reordering_buffer > reordering_buffer_;
  int curPath;
  int lastPath;
  StatisticsTimer  stimer;
  CheckForwardingQueueTimer ctimer; 
  playoutBuffterTimer playTimer; 
  bool isSender;  
  bool enablePtimer;
  double playBuffer_;
  //--------------------------------------------
  struct subflow subflows_[MAX_SUBFLOW];
  struct dstinfo dsts_[MAX_SUBFLOW];
  vector < dack_mapping > dackmap_;    
  vector <int> sendQueue_;				// transmission queue, put DNS
  vector <int> reschedulingQueue_DSN;	
  vector <int> reschedulingQueue_len;	
  
  vector <int> transmissionQueue_;		// put DNS
  vector <vector<int> > transmissionQueue2D_;		// put DNS
};
#endif
