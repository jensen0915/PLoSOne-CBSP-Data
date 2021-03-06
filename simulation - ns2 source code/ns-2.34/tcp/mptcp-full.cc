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
#include "tcp-full.h"
#include "mptcp-full.h"
#include "flags.h"
#include "random.h"
#include "template.h"
#include "mptcp.h"

#ifndef TRUE
#define	TRUE 	1
#endif

#ifndef FALSE
#define	FALSE 	0
#endif

static class MpFullTcpClass:public TclClass
{
public:
  MpFullTcpClass ():TclClass ("Agent/TCP/FullTcp/Sack/Multipath")
  {
  }
  TclObject *create (int, const char *const *)
  {
    return (new MpFullTcpAgent ());
  }
}

class_mpfull;

void
MpFullTcpAgent::prpkt (Packet * pkt)
{
  hdr_tcp *tcph = hdr_tcp::access (pkt);        // TCP header
  hdr_cmn *th = hdr_cmn::access (pkt);  // common header (size, etc)
  //hdr_flags *fh = hdr_flags::access(pkt);       // flags (CWR, CE, bits)
  hdr_ip *iph = hdr_ip::access (pkt);
  int datalen = th->size () - tcph->hlen ();    // # payload bytes

  fprintf (stdout,
           " [%d:%d.%d>%d.%d] (hlen:%d, dlen:%d, seq:%d, ack:%d, flags:0x%x (%s), salen:%d, reason:0x%x)\n",
           th->uid (), iph->saddr (), iph->sport (), iph->daddr (),
           iph->dport (), tcph->hlen (), datalen, tcph->seqno (),
           tcph->ackno (), tcph->flags (), flagstr (tcph->flags ()),
           tcph->sa_length (), tcph->reason ());
}

/*
 * this function should be virtual so others (e.g. SACK) can override
 */
int
MpFullTcpAgent::headersize ()
{
  int total = tcpip_base_hdr_size_;
  if (total < 1) {
    fprintf (stderr,
             "%f: MpFullTcpAgent(%s): warning: tcpip hdr size is only %d bytes\n",
             now (), name (), tcpip_base_hdr_size_);
  }

  if (ts_option_)
    total += ts_option_size_;

  total += mptcp_option_size_;

  return (total);
}

/*
 * sendpacket: 
 *	allocate a packet, fill in header fields, and send
 *	also keeps stats on # of data pkts, acks, re-xmits, etc
 *
 * fill in packet fields.  Agent::allocpkt() fills
 * in most of the network layer fields for us.
 * So fill in tcp hdr and adjust the packet size.
 *
 * Also, set the size of the tcp header.
 */
void
MpFullTcpAgent::sendpacket (int seqno, int ackno, int pflags, int datalen,
                            int reason, Packet * p)
{
//#ifdef MPTCP_debug  
  printf("at %f subflows_[%d] MpFullTcpAgent::sendpacket(TCP seqno %d, ackno %d, pflags %d, datalen %d, reason %d)\n", now(), mptcpfull_id, seqno, ackno, pflags, datalen, reason);
//#endif  
  
  printf("dumpSubflow%dsentSeq\t%f\t%d\n", (mptcpfull_id+1), now(), seqno);
  
  if (!p)
    p = allocpkt ();
  hdr_tcp *tcph = hdr_tcp::access (p);
  hdr_flags *fh = hdr_flags::access (p);
  hdr_cmn *ch = hdr_cmn::access (p);
  
  /* build basic header w/options */

  tcph->seqno () = seqno;
  tcph->ackno () = ackno;
  
  // [important!] Jensen 2014-05-07=8, if the sending size is varying, this ackno might not correct 
  // leading to packet missing without packet retransmission.
  
  tcph->flags () = pflags;
  tcph->reason () |= reason;    // make tcph->reason look like ns1 pkt->flags?
  tcph->sa_length () = 0;       // may be increased by build_options()
  tcph->hlen () = tcpip_base_hdr_size_;
  tcph->hlen () += build_options (tcph);

  /*
   * Explicit Congestion Notification (ECN) related:
   * Bits in header:
   *      ECT (EC Capable Transport),
   *      ECNECHO (ECHO of ECN Notification generated at router),
   *      CWR (Congestion Window Reduced from RFC 2481)
   * States in TCP:
   *      ecn_: I am supposed to do ECN if my peer does
   *      ect_: I am doing ECN (ecn_ should be T and peer does ECN)
   */

  if (datalen > 0 && ecn_) {
    // set ect on data packets 
    fh->ect () = ect_;          // on after mutual agreement on ECT
  }
  else if (ecn_ && ecn_syn_ && ecn_syn_next_ && (pflags & TH_SYN)
           && (pflags & TH_ACK)) {
    // set ect on syn/ack packet, if syn packet was negotiating ECT
    fh->ect () = ect_;
  }
  else {
    /* Set ect() to 0.  -M. Weigle 1/19/05 */
    fh->ect () = 0;
  }
  if (ecn_ && ect_ && recent_ce_) {
    // This is needed here for the ACK in a SYN, SYN/ACK, ACK
    // sequence.
    pflags |= TH_ECE;
  }
  // fill in CWR and ECE bits which don't actually sit in
  // the tcp_flags but in hdr_flags
  if (pflags & TH_ECE) {
    fh->ecnecho () = 1;
  }
  else {
    fh->ecnecho () = 0;
  }
  if (pflags & TH_CWR) {
    fh->cong_action () = 1;
  }
  else {
    /* Set cong_action() to 0  -M. Weigle 1/19/05 */
    fh->cong_action () = 0;
  }

  /*
   * mptcp processing
   */
  mptcp_option_size_ = 0;
  if (pflags & TH_SYN) {
    if (!(pflags & TH_ACK)) {
      if (mptcp_is_primary ()) {
        tcph->mp_capable () = 1;
        mptcp_option_size_ += MPTCP_CAPABLEOPTION_SIZE;
#ifdef MPTCP_debug		
        printf(" ---MPTCP_CAPABLEOPTION_SIZE 0\n");
#endif		
      }
      else {
        tcph->mp_join () = 1;
        mptcp_option_size_ += MPTCP_JOINOPTION_SIZE;
#ifdef MPTCP_debug		
        printf(" ---MPTCP_JOINOPTION_SIZE 0\n");
#endif		
      }
    }
    else {
      if (mpcapable_) {
        tcph->mp_capable () = 1;
        mptcp_option_size_ += MPTCP_CAPABLEOPTION_SIZE;
#ifdef MPTCP_debug		
        printf(" ---MPTCP_CAPABLEOPTION_SIZE 1\n");
#endif		
      }
      if (mpjoin_) {
        tcph->mp_join () = 1;
        mptcp_option_size_ += MPTCP_JOINOPTION_SIZE;
#ifdef MPTCP_debug		
        printf(" ---MPTCP_JOINOPTION_SIZE 1\n");
#endif		
      }
    }
  }
  else {
    tcph->mp_ack () = mptcp_recv_getack (ackno);
    if (tcph->mp_ack ()){
      mptcp_option_size_ += MPTCP_ACKOPTION_SIZE;
#ifdef MPTCP_debug	  
      printf(" ---MPTCP_ACKOPTION_SIZE!, MPTCP DNS = %d\n", tcph->mp_ack ());
#endif	  
    }
  }

  if (datalen && !mptcp_dsnmap_.empty ()) {
    vector < dsn_mapping >::iterator it;
    for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
      struct dsn_mapping *p = &*it;

      /* 
         check if this is the mapping for this packet 
         if so, attach the mapping info to the packet 
       */
      if (seqno >= p->sseqnum && seqno < p->sseqnum + p->length) {
        /* 
           if this is first transssion for this mapping or
           if this is retransmited packet, attach mapping 
         */
        if (!p->sentseq || p->sentseq == seqno) {
          tcph->mp_dsn () = p->dseqnum;
          tcph->mp_subseq () = p->sseqnum;
          tcph->mp_dsnlen () = p->length;
          p->sentseq = seqno;
          mptcp_option_size_ += MPTCP_DATAOPTION_SIZE;
#ifdef MPTCP_debug		  
          printf("dsn_mapping, seqno = %d, p->dseqnum = %d, p->sseqnum = %d, p->length = %d\n", seqno, p->dseqnum, p->sseqnum, p->length);
#endif		  
          break;
        }
      }
    }
  }
  tcph->hlen () += mptcp_option_size_;
   
  /* actual size is data length plus header length */


  ch->size () = datalen + tcph->hlen ();
  
  if ( ch->size () != tcph->hlen () ) { 
	//ch->timestamp() = Scheduler::instance().clock();
	
  }
   
  //printf("updated 1 tcph->ts_ %f\n", tcph->ts_);
  //tcph->ts_ = Scheduler::instance().clock();
  //printf("updated 2 tcph->ts_ %f\n", tcph->ts_);
  
  //printf("Time %f sendpacket_length = %d, header length = %d, packet timestamp = %f\n", now(), (datalen + tcph->hlen ()),  tcph->hlen (), ch->timestamp());
  
  if (datalen <= 0)
    ++nackpack_;
  else { 
    ++ndatapack_;
    ndatabytes_ += datalen;
    last_send_time_ = now ();   // time of last data
  }
  if (reason == REASON_TIMEOUT || reason == REASON_DUPACK
      || reason == REASON_SACK) {
    ++nrexmitpack_;
    nrexmitbytes_ += datalen;
  }

  last_ack_sent_ = ackno;

//if (state_ != TCPS_ESTABLISHED) {
//printf("MP %f(%s)[state:%s]: sending pkt ", now(), name(), statestr(state_));
//prpkt(p);
//}

  send (p, 0);

  return;
}

void
MpFullTcpAgent::mptcp_set_core (MptcpAgent * core)
{
  mptcp_core_ = core;
}

void
MpFullTcpAgent::mptcp_add_mapping (int dseqnum, int length)
{  
  int sseqnum = (int) curseq_ + 1;
#ifdef MPTCP_debug  
  printf("subflows_[%d] sender sent data! MpFullTcpAgent::mptcp_add_mapping(MPTCP DSN %d, TCP SN %d, data length %d)\n", mptcpfull_id, dseqnum, sseqnum, length);  
  printf("[before add] mptcp_dsnmap_ size = %ld  ", mptcp_dsnmap_.size());  
#endif  
  struct dsn_mapping tmp_map (dseqnum, sseqnum, length);
  mptcp_dsnmap_.push_back (tmp_map);
#ifdef MPTCP_debug  
  printf("[after add] mptcp_dsnmap_ size = %ld\n", mptcp_dsnmap_.size());  
#endif  
}

void
MpFullTcpAgent::mptcp_recv_add_mapping (int dseqnum, int sseqnum, int length)
{
#ifdef MPTCP_debug
  printf("subflows_[%d] receiver got data! mptcp_recv_add_mapping(MPTCP DSN %d, TCP SN %d, data length %d)\n", mptcpfull_id, dseqnum, sseqnum, length);  
  printf("[before add] mptcp_recv_dsnmap_ size = %ld  ", mptcp_recv_dsnmap_.size());  
#endif  
  struct dsn_mapping tmp_map (dseqnum, sseqnum, length);
  mptcp_recv_dsnmap_.push_back (tmp_map);
#ifdef MPTCP_debug  
  printf("[after add] mptcp_recv_dsnmap_ size = %ld\n", mptcp_recv_dsnmap_.size());  
#endif
}

/*
 * return dsn for mptcp from subseqnum
 */
int
MpFullTcpAgent::mptcp_recv_getack (int ackno)
{
#ifdef MPTCP_debug
	printf("subflows_[%d] receiver send ack to sender! MpFullTcpAgent::mptcp_recv_getack(TCP ackno = %d), mptcp_recv_dsnmap_ size = %ld\n", mptcpfull_id, ackno, mptcp_recv_dsnmap_.size());
#endif	

  if (ackno == 1 || ackno == -1)
    return 0;                   /* we don't receive any data */
	
  if ( mptcp_recv_dsnmap_.size() == 0 )
	return mptcp_core_->get_dataack ();	
	
  vector < dsn_mapping >::iterator it = mptcp_recv_dsnmap_.begin ();
  while (it != mptcp_recv_dsnmap_.end ()) {
    struct dsn_mapping *p = &*it;
 
#ifdef MPTCP_debug	
	printf("ackno = %d, p->sseqnum = %d, p->length = %d\n", ackno, p->sseqnum, p->length);	
#endif	
    /* check if this is the mapping for this packet */
    if (ackno >= p->sseqnum && ackno <= p->sseqnum + p->length) {
	//if (ackno <= p->sseqnum + p->length) {	//直接看如果還沒ACKed的資料 就不earse
      int offset = ackno - p->sseqnum;
#ifdef MPTCP_debug	  
	  printf("offset = %d, ackno = %d, p->length = %d, p->dseqnum = %d\n", offset, ackno, p->sseqnum, p->dseqnum);	
#endif	  
      //use reordering buffer to set mackno_ (原本作者的程式沒辦法指出目前正確的dsn應該是多少,因為少了out of order的觀念, 另外receiver應該回報目前最新的dsn, 這邊送ack回去就把mptcp_recv_dsnmap_的資料移除, 如果發生ack loss, sender都沒收到receiver的ack該如何處理?) 
	  //mptcp_core_->set_dataack (p->dseqnum, offset);
      return mptcp_core_->get_dataack ();
    }
    if (ackno > p->sseqnum + p->length) {
#ifdef MPTCP_debug      
	  printf("[erase] since ackno = %d > p->sseqnum + p->length = %d\n", ackno, (p->sseqnum + p->length));		
#endif	  
      it = mptcp_recv_dsnmap_.erase (it);
    }
    else
      ++it;
	  
	if ( mptcp_recv_dsnmap_.size() == 0 )
		return mptcp_core_->get_dataack ();
  }
  	
  fprintf (stderr, "fatal. [id %d] no mapping info was found. ackno %d, mptcp_recv_dsnmap_ size = %d\n", mptcpfull_id, ackno, (int)mptcp_recv_dsnmap_.size());
  abort ();
}

void
MpFullTcpAgent::mptcp_remove_mapping (int seqnum)
{
	//subflow outstanding pkt 在這邊減
#ifdef MPTCP_debug	
  printf("subflows_[%d] sender received ack from receiver! mptcp_remove_mapping(%d)\n", mptcpfull_id, seqnum);
  printf("[before remove] mptcp_dsnmap_ size = %ld  \n", mptcp_dsnmap_.size() );  
#endif
  
  vector < dsn_mapping >::iterator it;
  for (it = mptcp_dsnmap_.begin (); it != mptcp_dsnmap_.end (); ++it) {
    struct dsn_mapping *p = &*it;

    if (seqnum > p->dseqnum + p->length) {
#ifdef MPTCP_debug	  
	  printf("  [erase] seqnum = %d > p->dseqnum + p->length = %d  ", seqnum, (p->dseqnum + p->length));		
#endif	  
      it = mptcp_dsnmap_.erase (it);
#ifdef MPTCP_debug	  
	  printf("[after remove] mptcp_dsnmap_ size = %ld\n", mptcp_dsnmap_.size());  
#endif	  
      return;
    }
  }  
}

/*
 * open up the congestion window based on linked increase algorithm
 * in draft-ietf-mptcp-congestion-05

   The logic in the draft is: 
     For each ack received on subflow i, increase cwnd_i by min
     (alpha*bytes_acked*mss_i/tot_cwnd , bytes_acked*mss_i/cwnd_i )
   
   Since ns-2's congestion control logic is packet base, the logic 
   in here is rather simplified. Please note the following difference from
   the original one.
     o we don't use bytes_acked. use 1 packet size instead.
     o we don't use mss_i. use 1 packet size instead.
 *
 */
void
MpFullTcpAgent::opencwnd ()
{
  // calculate alpha here
  mptcp_core_->calculate_alpha ();
#ifdef MPTCP_debug  
  printf(" %f opencwnd mptcpfull_id %d, ssthresh_ %d\n", Scheduler::instance().clock(), mptcpfull_id, (int)ssthresh_); 
#endif  
  
  double increment;  
  if (cwnd_ < ssthresh_ && mptcp_allow_slowstart_) {
    /* slow-start (exponential) */
    cwnd_ += 1;		
  }
  else {  
#if 1
    double alpha = mptcp_core_->get_alpha ();
    double totalcwnd = mptcp_core_->get_totalcwnd ();
    if (totalcwnd > 0.1) {

      // original increase logic 
      double oincrement = increase_num_ / cwnd_;
      /*
        Subflow i will increase by alpha*cwnd_i/tot_cwnd segments per RTT.
      */
      increment = increase_num_ / cwnd_ * (cwnd_ / totalcwnd) * alpha;
      //increment = increase_num_ / cwnd_ * (cwnd_ / totalcwnd) * 0.5;

      /*
        we ensure that any multipath subflow cannot be more aggressive
        than a TCP flow in the same circumstances 
      */
      if (oincrement < increment) 
         increment = oincrement;
		 
		//printf(" %f opencwnd 2 increment %f\n", Scheduler::instance().clock(), increment); 	 
    } else
#endif
    // original increase logic
    increment = increase_num_ / cwnd_;
	//printf(" %f  increment 1 %f\n", Scheduler::instance().clock(), increment);  
    if ((last_cwnd_action_ == 0 || last_cwnd_action_ == CWND_ACTION_TIMEOUT)
        && max_ssthresh_ > 0) {
      increment = limited_slow_start (cwnd_, max_ssthresh_, increment);
    }
	printf("opencwnd\t%f increment %f \n", Scheduler::instance().clock(), increment);  
    cwnd_ += increment;
  }
  // if maxcwnd_ is set (nonzero), make it the cwnd limit
  if (maxcwnd_ && (int (cwnd_) > maxcwnd_))
    cwnd_ = maxcwnd_;

  return;
}

//----------------ETOM
double
MpFullTcpAgent::esti_cwnd ()
{
  // calculate alpha here
  mptcp_core_->calculate_alpha ();
#ifdef MPTCP_debug  
  printf(" %f esti_opencwnd mptcpfull_id %d, ssthresh_ %d\n", Scheduler::instance().clock(), mptcpfull_id); 
#endif  
  
  double increment;  
  if (esti_cwnd_ < ssthresh_ && mptcp_allow_slowstart_) {
    /* slow-start (exponential) */
    esti_cwnd_ += 1;		
  }
  else {  
#if 1
    double alpha = mptcp_core_->get_alpha ();
    double totalcwnd = mptcp_core_->get_totalcwnd ();
    if (totalcwnd > 0.1) {

      // original increase logic 
      double oincrement = increase_num_ / esti_cwnd_;
      /*
        Subflow i will increase by alpha*cwnd_i/tot_cwnd segments per RTT.
      */
      increment = increase_num_ / esti_cwnd_ * (esti_cwnd_ / totalcwnd) * alpha;
      //increment = increase_num_ / esti_cwnd_ * (esti_cwnd_ / totalcwnd) * 0.5;

      /*
        we ensure that any multipath subflow cannot be more aggressive
        than a TCP flow in the same circumstances 
      */
      if (oincrement < increment) 
         increment = oincrement;
		 
		//printf(" %f esti_opencwnd 2 increment %f\n", Scheduler::instance().clock(), increment); 	 
    } else
#endif
    // original increase logic
    increment = increase_num_ / esti_cwnd_;
	//printf(" %f  increment 1 %f\n", Scheduler::instance().clock(), increment);  
    if ((last_cwnd_action_ == 0 || last_cwnd_action_ == CWND_ACTION_TIMEOUT)
        && max_ssthresh_ > 0) {
      increment = limited_slow_start (esti_cwnd_, max_ssthresh_, increment);
    }
	//printf(" %f increment 2 %f \n", Scheduler::instance().clock(), increment);  
    esti_cwnd_ += increment;
  }
  // if maxcwnd_ is set (nonzero), make it the cwnd limit
  if (maxcwnd_ && (int (esti_cwnd_) > maxcwnd_))
    esti_cwnd_ = maxcwnd_;

  return esti_cwnd_;
}
//----------------ETOM

void
MpFullTcpAgent::timeout_action()
{
  SackFullTcpAgent::timeout_action();

  // calculate alpha here
  mptcp_core_->calculate_alpha ();
}

void
MpFullTcpAgent::dupack_action()
{
  SackFullTcpAgent::dupack_action();

  // calculate alpha here
  mptcp_core_->calculate_alpha ();
}
