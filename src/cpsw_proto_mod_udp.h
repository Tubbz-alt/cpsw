 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_UDP_H
#define CPSW_PROTO_MOD_UDP_H

#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>
#include <cpsw_thread.h>
#include <cpsw_sock.h>
#include <cpsw_compat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <vector>

using cpsw::atomic;

class CProtoModUdp;
typedef shared_ptr<CProtoModUdp> ProtoModUdp;

class CUdpHandlerThread : public CRunnable {
protected:
	CSockSd        sd_;

public:
	virtual void getMyAddr(struct sockaddr_in *addr_p)
	{
		sd_.getMyAddr( addr_p );
	}

	CUdpHandlerThread(const char *name, int threadPriority, struct sockaddr_in *dest, struct sockaddr_in *me_p = NULL);
	CUdpHandlerThread(CUdpHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me_p);

	virtual ~CUdpHandlerThread() {}
};

class CUdpPeerPollerThread : public CUdpHandlerThread {
private:
	unsigned pollSecs_;

protected:

	virtual void* threadBody();

public:
	CUdpPeerPollerThread(const char *name, struct sockaddr_in *dest, struct sockaddr_in *me = NULL, unsigned pollSecs = 60);
	CUdpPeerPollerThread(CUdpPeerPollerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me);

	virtual unsigned getPollSecs() const
	{
		return pollSecs_;
	}

	virtual ~CUdpPeerPollerThread() { threadStop(); }
};

class CProtoModUdp : public CProtoMod {
protected:

	class CUdpRxHandlerThread : public CUdpHandlerThread {
		private:
			atomic<uint64_t> nOctets_;
			atomic<uint64_t> nDgrams_;
			atomic<uint64_t> nRxDrop_;
		public:
			// cannot use smart pointer here because CProtoModUdp's
			// constructor creates the threads (and a smart ptr is
			// not available yet).
			// In any case - the thread objects are only used
			// internally...
			CProtoModUdp   *owner_;

		protected:

			virtual void* threadBody();

		public:
			CUdpRxHandlerThread(const char *name, int threadPriority, struct sockaddr_in *dest, struct sockaddr_in *me, CProtoModUdp *owner);
			CUdpRxHandlerThread(CUdpRxHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me, CProtoModUdp *owner);

			virtual uint64_t getNumOctets() { return nOctets_.load( cpsw::memory_order_relaxed ); }
			virtual uint64_t getNumDgrams() { return nDgrams_.load( cpsw::memory_order_relaxed ); }
			virtual uint64_t getNumRxDrop() { return nRxDrop_.load( cpsw::memory_order_relaxed ); }

			virtual ~CUdpRxHandlerThread() { threadStop(); }
	};

private:
	struct sockaddr_in dest_;
	CSockSd            tx_;
	atomic<uint64_t>   nTxOctets_;
	atomic<uint64_t>   nTxDgrams_;
	int                threadPriority_;
protected:
	std::vector< CUdpRxHandlerThread * > rxHandlers_;
	CUdpPeerPollerThread                 *poller_;

	void createThreads(unsigned nRxThreads, int pollSeconds);

	virtual bool doPush(BufChain bc, bool wait, const CTimeout *timeout, bool abs_timeout);

	virtual bool push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
	{
		return doPush(bc, true, timeout, abs_timeout);
	}

	virtual bool tryPush(BufChain bc)
	{
		return doPush(bc, false, NULL, true);
	}

	virtual int iMatch(ProtoPortMatchParams *cmp);

public:
	// negative or zero 'pollSecs' avoids creating a poller thread
	CProtoModUdp(Key &k, struct sockaddr_in *dest, unsigned depth, int threadPriority, unsigned nRxThreads = 1, int pollSecs = 4);

	CProtoModUdp(CProtoModUdp &orig, Key &k);

	virtual const char *getName()  const
	{
		return "UDP";
	}

	virtual unsigned getDestPort() const
	{
		return ntohs( dest_.sin_port );
	}

	virtual void dumpInfo(FILE *f);

	virtual CProtoModUdp *clone(Key &k)
	{
		return new CProtoModUdp( *this, k );
	}

	virtual uint64_t getNumTxOctets() { return nTxOctets_.load( cpsw::memory_order_relaxed ); }
	virtual uint64_t getNumTxDgrams() { return nTxDgrams_.load( cpsw::memory_order_relaxed ); }
	virtual uint64_t getNumRxOctets();
	virtual uint64_t getNumRxDgrams();
	virtual uint64_t getNumRxDrops();
	virtual void modStartup();
	virtual void modShutdown();

	virtual unsigned getMTU();

	virtual void dumpYaml(YAML::Node &) const;

	virtual ~CProtoModUdp();

};

#endif
