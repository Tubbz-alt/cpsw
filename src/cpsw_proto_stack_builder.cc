 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#define __STDC_FORMAT_MACROS

#include <inttypes.h>

#include <cpsw_proto_mod_depack.h>
#include <cpsw_proto_mod_udp.h>
#include <cpsw_proto_mod_tcp.h>
#include <cpsw_proto_mod_srpmux.h>
#include <cpsw_proto_mod_rssi.h>
#include <cpsw_proto_mod_tdestmux.h>
#include <cpsw_proto_mod_tdestmux2.h>
#include <cpsw_proto_depack.h>
#include <cpsw_stdio.h>

#include <cpsw_yaml.h>
#include <cpsw_debug.h>

#include <socks/libSocks.h>
#include <rssi_bridge/rpcMapService.h>

using cpsw::dynamic_pointer_cast;

#define PSBLDR_DEBUG 0

#ifdef PSBLDR_DEBUG
int cpsw_psbldr_debug = PSBLDR_DEBUG;
#endif

class CProtoStackBuilder : public IProtoStackBuilder {
	public:
		typedef enum TransportProto { NONE = 0,  UDP  = 1,  TCP = 2 } TransportProto;
		typedef enum SRPWriteMode   { UNSP = -1, POST = 0, SYNC = 1 } SRPWriteMode;
	private:
		INetIODev::ProtocolVersion protocolVersion_;
		uint64_t                   SRPTimeoutUS_;
		int                        SRPDynTimeout_;
		unsigned                   SRPRetryCount_;
		SRPWriteMode               SRPDefaultWriteMode_;
		TransportProto             Xprt_;
		unsigned                   XprtPort_;
		unsigned                   XprtOutQueueDepth_;
        int                        UdpThreadPriority_;
		unsigned                   UdpNumRxThreads_;
		int                        UdpPollSecs_;
        int                        TcpThreadPriority_;
		bool                       hasRssi_;
        int                        RssiThreadPriority_;
		int                        hasDepack_;
		DepackProtoVersion         depackProto_;
		unsigned                   DepackOutQueueDepth_;
		unsigned                   DepackLdFrameWinSize_;
		unsigned                   DepackLdFragWinSize_;
		int                        DepackThreadPriority_;
		int                        hasSRPMux_;
		unsigned                   SRPMuxVirtualChannel_;
		unsigned                   SRPMuxOutQueueDepth_;
		int                        SRPMuxThreadPriority_;
		bool                       hasTDestMux_;
		unsigned                   TDestMuxTDEST_;
		int                        TDestMuxStripHeader_;
		unsigned                   TDestMuxOutQueueDepth_;
		unsigned                   TDestMuxInpQueueDepth_;
		int                        TDestMuxThreadPriority_;
		in_addr_t                  IPAddr_;
		struct LibSocksProxy       socksProxy_;
		in_addr_t                  rssiBridgeIPAddr_;
		CRssiConfigParams          rssiConfig_;
		bool                       autoStart_;
	public:
		virtual void reset()
		{
			protocolVersion_        = SRP_UDP_V2;
			SRPTimeoutUS_           = 0;
			SRPDynTimeout_          = -1;
			SRPRetryCount_          = -1;
			SRPDefaultWriteMode_    = UNSP;
			Xprt_                   = UDP;
			XprtPort_               = 8192;
			XprtOutQueueDepth_      = 0;
			UdpThreadPriority_      = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			UdpNumRxThreads_        = 0;
			UdpPollSecs_            = -1;
			TcpThreadPriority_      = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			hasRssi_                = false;
			hasDepack_              = -1;
			depackProto_            = DEPACKETIZER_V0;
			RssiThreadPriority_     = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			DepackOutQueueDepth_    = 0;
			DepackLdFrameWinSize_   = 0;
			DepackLdFragWinSize_    = 0;
			DepackThreadPriority_   = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			hasSRPMux_              = -1;
			SRPMuxOutQueueDepth_    = 0;
			SRPMuxVirtualChannel_   = 0;
			SRPMuxThreadPriority_   = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			hasTDestMux_            = false;
			TDestMuxTDEST_          = 0;
			TDestMuxStripHeader_    = -1;
			TDestMuxOutQueueDepth_  = 0;
			TDestMuxInpQueueDepth_  = 0;
			TDestMuxThreadPriority_ = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			IPAddr_                 = INADDR_NONE;
			rssiBridgeIPAddr_       = INADDR_NONE;
			socksProxy_.version     = SOCKS_VERSION_NONE;
			rssiConfig_             = CRssiConfigParams();
			autoStart_              = true; // legacy behaviour
		}

		CProtoStackBuilder()
		{
			reset();
		}

		CProtoStackBuilder(YamlState &);

		void setIPAddr(in_addr_t peer)
		{
			IPAddr_ = peer;
		}

		in_addr_t getIPAddr()
		{
			return IPAddr_;
		}

		void setRssiBridgeIPAddr(in_addr_t bridge)
		{
			rssiBridgeIPAddr_ = bridge;
		}

		in_addr_t getRssiBridgeIPAddr()
		{
			return rssiBridgeIPAddr_;
		}

		void setSocksProxy(const LibSocksProxy *proxy)
		{
			socksProxy_ = *proxy;
		}

		const LibSocksProxy *getSocksProxy()
		{
			return &socksProxy_;
		}


		bool hasSRP()
		{
			return SRP_UDP_NONE != protocolVersion_;
		}

		virtual void            setSRPVersion(SRPProtoVersion v)
		{
			if ( SRP_UDP_NONE != v && SRP_UDP_V1 != v && SRP_UDP_V2 != v && SRP_UDP_V3 != v ) {
				throw InvalidArgError("Invalid protocol version");
			}
			protocolVersion_ = v;
			if ( SRP_UDP_NONE == v ) {
				setSRPTimeoutUS( 0 );
				setSRPRetryCount( -1 );
			}
		}

		virtual SRPProtoVersion getSRPVersion()
		{
			return protocolVersion_;
		}

		virtual void            setSRPTimeoutUS(uint64_t v)
		{
			SRPTimeoutUS_ = v;
		}

		virtual uint64_t        getSRPTimeoutUS()
		{
			if ( 0 == SRPTimeoutUS_ )
				return hasRssiAndUdp() ? 500000 : (getTcpPort() > 0 ? 4000000 : 10000);
			return SRPTimeoutUS_;
		}

		virtual void            setSRPDefaultWriteMode(WriteMode mode)
		{
			switch ( mode ) {
				case SYNCHRONOUS:
					SRPDefaultWriteMode_ = SYNC;
				break;
				default:
					SRPDefaultWriteMode_ = POST;
				break;
			}
		}

		virtual WriteMode       getSRPDefaultWriteMode()
		{
			// If this was never set (UNSP) -> default to POSTED
			if ( UNSP == SRPDefaultWriteMode_ )
				return hasRssi() ? POSTED : SYNCHRONOUS;
			return SRPDefaultWriteMode_ == SYNC ? SYNCHRONOUS : POSTED;
		}

		virtual void            setSRPRetryCount(unsigned v)
		{
			SRPRetryCount_ = v;
		}

		virtual unsigned        getSRPRetryCount()
		{
			if ( (unsigned)-1 == SRPRetryCount_ )
				return ( hasRssiAndUdp() || hasTcp() ) ? 0 : 10;
			return SRPRetryCount_;
		}

		virtual bool            hasUdp()
		{
			return getUdpPort() != 0;
		}

		virtual bool            hasTcp()
		{
			return getTcpPort() != 0;
		}

		virtual void            useSRPDynTimeout(bool v)
		{
			SRPDynTimeout_ = (v ? 1 : 0);
		}

		virtual bool            hasSRPDynTimeout()
		{
			if ( hasRssi() || getTcpPort() > 0 )
				return false;
			if ( SRPDynTimeout_ < 0 )
				return ! hasTDestMux() && ! hasRssiAndUdp();
			return SRPDynTimeout_ ? true : false;
		}

		virtual void setXprtPort(unsigned v)
		{
			if ( v > 65535 || v == 0 )
				throw InvalidArgError("Invalid UDP Port");
			XprtPort_ = v;
		}

		virtual bool hasRssiBridge()
		{
			return INADDR_NONE != getRssiBridgeIPAddr();
		}

		virtual TransportProto  getXprt()
		{
			return hasRssiBridge() ? TCP : Xprt_;
		}

		virtual void            setUdpPort(unsigned v)
		{
			setXprtPort(v);
			Xprt_ = UDP;
		}

		virtual void            setTcpPort(unsigned v)
		{
			setXprtPort(v);
			Xprt_ = TCP;
		}

		virtual void            setTcpThreadPriority(int prio)
		{
			TcpThreadPriority_ = prio;
		}

		virtual int             getTcpThreadPriority()
		{
			return TcpThreadPriority_;
		}

		virtual void            setUdpThreadPriority(int prio)
		{
			UdpThreadPriority_ = prio;
		}

		virtual int             getUdpThreadPriority()
		{
			return UdpThreadPriority_;
		}


		virtual unsigned        getUdpPort()
		{
			return getXprt() == UDP ? XprtPort_ : 0;
		}

		virtual unsigned        getTcpPort()
		{
			return getXprt() == TCP ? XprtPort_ : 0;
		}

		virtual void            setUdpOutQueueDepth(unsigned v)
		{
			XprtOutQueueDepth_ = v;
			Xprt_              = UDP;
		}

		virtual void            setTcpOutQueueDepth(unsigned v)
		{
			XprtOutQueueDepth_ = v;
			Xprt_              = TCP;
		}


		virtual unsigned        getUdpOutQueueDepth()
		{
			if ( 0 == XprtOutQueueDepth_ )
				return 10;
			return XprtOutQueueDepth_;
		}

		virtual unsigned        getTcpOutQueueDepth()
		{
			return getUdpOutQueueDepth();
		}


		virtual void            setUdpNumRxThreads(unsigned v)
		{
			if ( v > 100 )
				throw InvalidArgError("Too many UDP RX threads");
			UdpNumRxThreads_ = v;
		}

		virtual unsigned        getUdpNumRxThreads()
		{
			if ( 0 == UdpNumRxThreads_ )
				return 1;
			return UdpNumRxThreads_;
		}

		virtual void            setUdpPollSecs(int v)
		{
			UdpPollSecs_ = v;
		}

		virtual int             getUdpPollSecs()
		{
			if ( 0 >  UdpPollSecs_ ) {
				if ( ! hasRssiAndUdp() && ( ! hasSRP() || hasTDestMux() ) )
					return 60;
			}
			return UdpPollSecs_;
		}

		virtual void            useRssi(bool v)
		{
			hasRssi_ = v;
		}

		virtual bool            hasRssi()
		{
			return hasRssi_;
		}

		virtual bool            hasRssiAndUdp()
		{
			return hasRssi_ && hasUdp();
		}

		virtual void            setRssiThreadPriority(int prio)
		{
			if ( prio != IProtoStackBuilder::NO_THREAD_PRIORITY ) {
				useRssi( true );
			}
			RssiThreadPriority_ = prio;
		}

		virtual int             getRssiThreadPriority()
		{
			return hasRssiAndUdp() ? RssiThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
		}

		virtual void            useDepack(bool v)
		{
			hasDepack_ = !! v;
		}

		virtual bool            hasDepack()
		{
			if ( hasDepack_ < 0 ) {
				return hasTDestMux();
			}
			return hasDepack_ > 0;
		}

		virtual void            setDepackOutQueueDepth(unsigned v)
		{
			DepackOutQueueDepth_ = v;
			useDepack( true );
		}

		virtual void            setDepackVersion(DepackProtoVersion vers)
		{
			depackProto_ = vers;
		}

		virtual DepackProtoVersion getDepackVersion()
		{
			return depackProto_;
		}

		virtual unsigned        getDepackOutQueueDepth()
		{
			if ( 0 == DepackOutQueueDepth_ )
				return 50;
			return DepackOutQueueDepth_;
		}

		virtual void            setDepackThreadPriority(int prio)
		{
			DepackThreadPriority_ = prio;
			useDepack( true );
		}

		virtual int             getDepackThreadPriority()
		{
			return hasDepack() ? DepackThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
		}

		virtual void            setDepackLdFrameWinSize(unsigned v)
		{
			if ( v > 10 )
				throw InvalidArgError("Requested depacketizer frame window too large");
			DepackLdFrameWinSize_ = v;
			useDepack( true );
		}

		virtual unsigned        getDepackLdFrameWinSize()
		{
			if ( 0 == DepackLdFrameWinSize_ )
				return hasRssiAndUdp() ? 1 : 5;
			return DepackLdFrameWinSize_;
		}

		virtual void            setDepackLdFragWinSize(unsigned v)
		{
			if ( v > 10 )
				throw InvalidArgError("Requested depacketizer frame window too large");
			DepackLdFragWinSize_ = v;
			useDepack( true );
		}

		virtual unsigned        getDepackLdFragWinSize()
		{
			if ( 0 == DepackLdFragWinSize_ )
				return hasRssiAndUdp() ? 1 : 5;
			return DepackLdFragWinSize_;
		}

		virtual void            useSRPMux(bool v)
		{
			hasSRPMux_ = (v ? 1 : 0);
		}

		virtual bool            hasSRPMux()
		{
			if ( hasSRPMux_ < 0 )
				return hasSRP();
			return hasSRPMux_ > 0;
		}

		virtual void            setSRPMuxVirtualChannel(unsigned v)
		{
			if ( v > 127 )
				throw InvalidArgError("Requested SRP Mux Virtual Channel out of range");
			useSRPMux( true );
			SRPMuxVirtualChannel_ = v;
		}

		virtual unsigned        getSRPMuxVirtualChannel()
		{
			return SRPMuxVirtualChannel_;
		}

		virtual void            setSRPMuxOutQueueDepth(unsigned v)
		{
			SRPMuxOutQueueDepth_ = v;
			useSRPMux( true );
		}

		virtual unsigned        getSRPMuxOutQueueDepth()
		{
			if ( 0 == SRPMuxOutQueueDepth_ )
				return 50;
			return SRPMuxOutQueueDepth_;
		}

		virtual void            setSRPMuxThreadPriority(int prio)
		{
			if ( prio != IProtoStackBuilder::NO_THREAD_PRIORITY ) {
				useSRPMux( true  );
			}
			SRPMuxThreadPriority_ = prio;
		}

		virtual int             getSRPMuxThreadPriority()
		{
			return hasSRPMux() ? SRPMuxThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
		}


		virtual void            useTDestMux(bool v)
		{
			hasTDestMux_ = v;
		}

		virtual bool            hasTDestMux()
		{
			return hasTDestMux_;
		}

		virtual void            setTDestMuxTDEST(unsigned v)
		{
			if ( v > 255 )
				throw InvalidArgError("Requested TDEST out of range");
			useTDestMux( true );
			TDestMuxTDEST_ = v;
		}

		virtual unsigned        getTDestMuxTDEST()
		{
			return TDestMuxTDEST_;
		}

		virtual void            setTDestMuxStripHeader(bool v)
		{
			TDestMuxStripHeader_    = (v ? 1:0);
			useTDestMux( true );
		}

		virtual bool            getTDestMuxStripHeader()
		{
			if ( 0 > TDestMuxStripHeader_ ) {
				return hasSRP();
			}
			return TDestMuxStripHeader_ > 0;
		}

		virtual void            setTDestMuxOutQueueDepth(unsigned v)
		{
			TDestMuxOutQueueDepth_ = v;
			useTDestMux( true );
		}

		virtual unsigned        getTDestMuxOutQueueDepth()
		{
			// output queue important with async SRP
			if ( 0 == TDestMuxOutQueueDepth_ )
				return 50;
			return TDestMuxOutQueueDepth_;
		}

		virtual void            setTDestMuxInpQueueDepth(unsigned v)
		{
			TDestMuxInpQueueDepth_ = v;
			useTDestMux( true );
		}

		virtual unsigned        getTDestMuxInpQueueDepth()
		{
			// input queue important with async SRP
			if ( 0 == TDestMuxInpQueueDepth_ )
				return 50;
			return TDestMuxInpQueueDepth_;
		}


		virtual void            setTDestMuxThreadPriority(int prio)
		{
			if ( prio != IProtoStackBuilder::NO_THREAD_PRIORITY ) {
				useTDestMux( true  );
			}
			TDestMuxThreadPriority_ = prio;
		}

		virtual int             getTDestMuxThreadPriority()
		{
			return hasTDestMux() ? TDestMuxThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
		}

		virtual shared_ptr<CProtoStackBuilder> cloneInternal()
		{
			return cpsw::make_shared<CProtoStackBuilder>( *this );
		}

		virtual ProtoStackBuilder clone()
		{
			return cloneInternal();
		}

		virtual const CRssiConfigParams *getRssiConfigParams()
		{
			return &rssiConfig_;
		}

		virtual bool getAutoStart()
		{
			return autoStart_;
		}

		virtual void setAutoStart(bool v)
		{
			autoStart_ = v;
		}

		virtual ProtoPort build( std::vector<ProtoPort>& );

		static ProtoPort findProtoPort( ProtoPortMatchParams *, std::vector<ProtoPort>& );

		static shared_ptr<CProtoStackBuilder>  create(YamlState &ypath);
		static shared_ptr<CProtoStackBuilder>  create();
};

shared_ptr<CProtoStackBuilder>
CProtoStackBuilder::create(YamlState &node)
{
	return cpsw::make_shared<CProtoStackBuilder>( node );
}

CProtoStackBuilder::CProtoStackBuilder(YamlState &node)
{
unsigned                   u;
uint64_t                   u64;
bool                       b;
SRPProtoVersion            proto_vers;
int                        i;
WriteMode                  writeMode;

	reset();

	{
		YamlState nn( &node, YAML_KEY_SRP );
		if ( !!nn && nn.IsMap() )
		{
			// initialize proto_vers to silence rhel compiler warning
			// about potentially un-initialized 'proto_vers'
			proto_vers = getSRPVersion();
			if ( readNode(nn, YAML_KEY_protocolVersion, &proto_vers) )
				setSRPVersion( proto_vers );
			// initialize u64 to silence rhel compiler warning
			// about potentially un-initialized 'u64'
			u64 = getSRPTimeoutUS();
			if ( readNode(nn, YAML_KEY_timeoutUS, &u64) )
				setSRPTimeoutUS( u64 );
			if ( readNode(nn, YAML_KEY_dynTimeout, &b) )
				useSRPDynTimeout( b );
			if ( readNode(nn, YAML_KEY_retryCount, &u) )
				setSRPRetryCount( u );
			if ( readNode(nn, YAML_KEY_defaultWriteMode, &writeMode) )
			{
				if ( hasSRPMux_ < 0 || UNSP == SRPDefaultWriteMode_ ) {
					/* SRPMux'es setting overrides what we find here.. */
					setSRPDefaultWriteMode( writeMode );
				}
			}
		}
	}
	{
		YamlState nn( &node, YAML_KEY_TCP );
		if ( !!nn && nn.IsMap() )
		{
			if ( ! readNode(nn, YAML_KEY_instantiate, &b) || b ) {
				if ( readNode(nn, YAML_KEY_port, &u) )
					setTcpPort( u );
				if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
					setTcpOutQueueDepth( u );
				if ( readNode(nn, YAML_KEY_threadPriority, &i) )
					setTcpThreadPriority( i );
			}
		}
	}
	{
		YamlState nn( &node, YAML_KEY_UDP );
		if ( !!nn && nn.IsMap() )
		{
			if ( ! readNode(nn, YAML_KEY_instantiate, &b) || b ) {
				if ( readNode(nn, YAML_KEY_port, &u) )
					setUdpPort( u );
				if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
					setUdpOutQueueDepth( u );
				if ( readNode(nn, YAML_KEY_numRxThreads, &u) )
					setUdpNumRxThreads( u );
				// initialize i to silence rhel compiler warning
				// about potentially un-initialized 'i'
				i = getUdpPollSecs();
				if ( readNode(nn, YAML_KEY_pollSecs, &i) )
					setUdpPollSecs( i );
				if ( readNode(nn, YAML_KEY_threadPriority, &i) )
					setUdpThreadPriority( i );
			}
		}
	}
	{
        YamlState nn( &node, YAML_KEY_RSSI );

        useRssi( !!nn );

		if ( !!nn && nn.IsMap() ) {
			if ( readNode(nn, YAML_KEY_threadPriority, &i) ) {
				setRssiThreadPriority( i );
			}
			if ( readNode(nn, YAML_KEY_instantiate, &b) ) {
				useRssi( b );
			}
			/* Some of the members of 'rssiConfig' are uint8_t and yaml-cpp will try
			 * to read as 'char'; i.e., number like '2' in YAML will be interpreted
			 * as '2' and yield 52 (numerical ascii code of '2').
			 */
			if ( readNode(nn, YAML_KEY_ldMaxUnackedSegs,        &u) ) {
				rssiConfig_.ldMaxUnackedSegs_  = u;
			}
			if ( readNode(nn, YAML_KEY_outQueueDepth,           &u) ) {
				rssiConfig_.outQueueDepth_     = u;
			}
			if ( readNode(nn, YAML_KEY_inpQueueDepth,           &u) ) {
				rssiConfig_.inpQueueDepth_     = u;
			}
			if ( readNode(nn, YAML_KEY_retransmissionTimeoutUS, &u) ) {
				rssiConfig_.rexTimeoutUS_      = u;
			}
			if ( readNode(nn, YAML_KEY_cumulativeAckTimeoutUS,  &u) ) {
				rssiConfig_.cumAckTimeoutUS_   = u;
			}
			if ( readNode(nn, YAML_KEY_nullTimeoutUS,           &u) ) {
				rssiConfig_.nulTimeoutUS_      = u;
			}
			if ( readNode(nn, YAML_KEY_maxRetransmissions,      &u) ) {
				rssiConfig_.rexMax_            = u;
			}
			if ( readNode(nn, YAML_KEY_maxCumulativeAcks,       &u) ) {
				rssiConfig_.cumAckMax_         = u;
			}
			if ( readNode(nn, YAML_KEY_maxSegmentSize,          &u) ) {
				rssiConfig_.forcedSegsMax_     = u;
			}
		}
	}
	{
		YamlState nn( &node, YAML_KEY_depack );
		if ( !!nn && nn.IsMap() )
		{
		DepackProtoVersion proto_vers = DEPACKETIZER_V0; // silence rhel g++ warning about un-initialized use (??)
			useDepack( true );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				setDepackOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_protocolVersion, &proto_vers) )
				setDepackVersion( proto_vers );
			if ( readNode(nn, YAML_KEY_ldFrameWinSize, &u) )
				setDepackLdFrameWinSize( u );
			if ( readNode(nn, YAML_KEY_ldFragWinSize, &u) )
				setDepackLdFragWinSize( u );
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				setDepackThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				useDepack( b );
		}
	}
	{
		YamlState nn( &node, YAML_KEY_SRPMux );
		if ( !!nn && nn.IsMap() )
		{
			useSRPMux( true );
			if ( readNode(nn, YAML_KEY_virtualChannel, &u) )
				setSRPMuxVirtualChannel( u );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				setSRPMuxOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				setSRPMuxThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				useSRPMux( b );
			if ( readNode(nn, YAML_KEY_defaultWriteMode, &writeMode) )
			{
				// SRPMux overrides any setting SRP might have made
				setSRPDefaultWriteMode( writeMode );
			}
		}
	}
	{
		YamlState nn( &node, YAML_KEY_TDESTMux );
		if ( !!nn && nn.IsMap() )
		{
			useTDestMux( true );
			if ( readNode(nn, YAML_KEY_TDEST, &u) )
				setTDestMuxTDEST( u );
			if ( readNode(nn, YAML_KEY_stripHeader, &b) )
				setTDestMuxStripHeader( b );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				setTDestMuxOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_inpQueueDepth, &u) )
				setTDestMuxInpQueueDepth( u );
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				setTDestMuxThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				useTDestMux( b );
		}
	}
}

ProtoStackBuilder
IProtoStackBuilder::create(YamlState &node)
{
	return CProtoStackBuilder::create( node );
}

shared_ptr<CProtoStackBuilder>
CProtoStackBuilder::create()
{
	return cpsw::make_shared<CProtoStackBuilder>();
}

ProtoStackBuilder
IProtoStackBuilder::create()
{
	return CProtoStackBuilder::create();
}

ProtoPort CProtoStackBuilder::findProtoPort(ProtoPortMatchParams *cmp, std::vector<ProtoPort> &existingPorts )
{
std::vector<ProtoPort>::const_iterator it;
int                                    requestedMatches = cmp->requestedMatches();

	for ( it = existingPorts.begin(); it != existingPorts.end(); ++it )
	{
	int found;
		// 'match' modifies the parameters (storing results)
		cmp->reset();
		if ( requestedMatches == (found = cmp->findMatches( *it )) )
			return *it;
#ifdef PSBLDR_DEBUG
		if ( cpsw_psbldr_debug > 1 ) {
			fprintf(CPSW::fDbg(), "%s; requested %d, found %d\n", (*it)->getProtoMod()->getName(), requestedMatches, found);
		}
#endif
	}
	return ProtoPort();
}

ProtoPort CProtoStackBuilder::build( std::vector<ProtoPort> &existingPorts )
{
ProtoPort                      rval;
ProtoPort                      foundTDestPort;
ProtoPortMatchParams           cmp;
ProtoMod                       tDestMuxMod;
ProtoModSRPMux                 srpMuxMod;
shared_ptr<CProtoStackBuilder> bldr   = cloneInternal();
bool                           hasSRP = IProtoStackBuilder::SRP_UDP_NONE != bldr->getSRPVersion();

#ifdef PSBLDR_DEBUG
	if ( cpsw_psbldr_debug > 1 ) {
		fprintf(CPSW::fDbg(), "makeProtoPort...entering\n");
	}
#endif

	// sanity checks
	if ( ( ! bldr->hasUdp() && ! bldr->hasTcp() ) || (INADDR_NONE == bldr->getIPAddr()) )
		throw ConfigurationError("Currently only UDP or TCP transport supported\n");

	if ( ! hasSRP && bldr->hasSRPMux() ) {
		throw ConfigurationError("Cannot configure SRP Demuxer w/o SRP protocol version");
	}

	if ( bldr->hasUdp() )
		cmp.udpDestPort_ = bldr->getUdpPort();
	else
		cmp.tcpDestPort_ = bldr->getTcpPort();

#ifdef PSBLDR_DEBUG
	if ( cpsw_psbldr_debug > 0 ) {
		fprintf(CPSW::fDbg(), "makeProtoPort for %s port %d\n",
			bldr->hasRssiBridge() ? "UDP (via bridge/TCP)" : (bldr->hasUdp() ? "UDP" : "TCP"),
			bldr->hasUdp()        ? bldr->getUdpPort()     : bldr->getTcpPort()
		);
	}
#endif

	if ( findProtoPort( &cmp, existingPorts ) ) {

		if ( ! bldr->hasTDestMux() && ( !hasSRP || !bldr->hasSRPMux() ) ) {
			throw ConfigurationError("Some kind of demuxer must be used when sharing a UDP port");
		}

#ifdef PSBLDR_DEBUG
		if ( cpsw_psbldr_debug > 0 ) {
			fprintf(CPSW::fDbg(), "        found %s port %d\n",
				bldr->hasRssiBridge() ? "UDP (via bridge/TCP)" : (bldr->hasUdp() ? "UDP" : "TCP"),
				bldr->hasUdp()        ? bldr->getUdpPort()     : bldr->getTcpPort()
			);
		}
#endif

		// existing RSSI configuration must match the requested one
		if ( bldr->hasRssiAndUdp() ) {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  including RSSI\n");
			}
#endif
			cmp.haveRssi_.include();
		} else {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  excluding RSSI\n");
			}
#endif
			cmp.haveRssi_.exclude();
		}

		// existing DEPACK configuration must match the requested one
		if ( bldr->hasDepack() ) {
			cmp.haveDepack_.include();
			cmp.depackVersion_ = bldr->getDepackVersion();
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  including depack\n");
			}
#endif
		} else {
			cmp.haveDepack_.exclude();
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  excluding depack\n");
			}
#endif
		}

		if ( bldr->hasTDestMux() ) {
			cmp.tDest_         = bldr->getTDestMuxTDEST();
			cmp.depackVersion_ = bldr->getDepackVersion();
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  tdest %d\n", bldr->getTDestMuxTDEST());
			}
#endif
		} else {
			cmp.tDest_.exclude();
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  tdest excluded\n");
			}
#endif
		}

		if ( (foundTDestPort = findProtoPort( &cmp, existingPorts )) ) {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  tdest port FOUND\n");
			}
#endif

			// either no tdest demuxer or using an existing tdest port
			if ( ! hasSRP ) {
				throw ConfigurationError("Cannot share TDEST w/o SRP demuxer");
			}

			cmp.srpVersion_     = bldr->getSRPVersion();
			cmp.srpVC_          = bldr->getSRPMuxVirtualChannel();

			if ( findProtoPort( &cmp, existingPorts ) ) {
				throw ConfigurationError("SRP VC already in use");
			}

			cmp.srpVC_.wildcard();

			if ( ! findProtoPort( &cmp, existingPorts ) ) {
				throw ConfigurationError("No SRP Demultiplexer found -- cannot create SRP port on top of existing protocol modules");
			}

			srpMuxMod = dynamic_pointer_cast<ProtoModSRPMux::element_type>( cmp.srpVC_.handledBy_ );

			if ( ! srpMuxMod ) {
				throw InternalError("No SRP Demultiplexer - but there should be one");
			}

		} else {
			// possibilities here are
			//   asked for no tdest demux but there is one present
			//   asked for tdest demux on non-existing port (OK)
			//   other mismatches
			if ( bldr->hasTDestMux() ) {
				cmp.tDest_.wildcard();
				if ( ! findProtoPort( &cmp, existingPorts ) ) {
					cmp.dump( CPSW::fErr() );
					throw ConfigurationError("No TDEST Demultiplexer found");
				}
				tDestMuxMod  = cmp.tDest_.handledBy_;

				if ( ! tDestMuxMod ) {
					throw InternalError("No TDEST Demultiplexer (base-class pointer) - but there should be one");
				}
#ifdef PSBLDR_DEBUG
				if ( cpsw_psbldr_debug > 0 ) {
					fprintf(CPSW::fDbg(), "  using (existing) tdest MUX\n");
				}
#endif
			} else {
				throw ConfigurationError("Unable to create new port on existing protocol modules");
			}
		}
	} else {
		// create new
		struct sockaddr_in dst;

		dst.sin_family      = AF_INET;
		dst.sin_port        = htons( bldr->hasUdp() ? bldr->getUdpPort() : bldr->getTcpPort() );
		dst.sin_addr.s_addr = bldr->getIPAddr();

		if ( bldr->hasUdp() ) {
			// Note: transport module MUST have a queue if RSSI is used
			rval = CShObj::create< ProtoModUdp >( &dst,
			                                       bldr->getUdpOutQueueDepth(),
			                                       bldr->getUdpThreadPriority(),
			                                       bldr->getUdpNumRxThreads(),
			                                       bldr->getUdpPollSecs()
			);
		} else {
			struct sockaddr_in via = dst;

			if ( INADDR_NONE != bldr->getRssiBridgeIPAddr() ) {
				/* lookup the TCP address via the RPC mapping service of the bridge */
				PortMap        map[1];
				unsigned       nMaps = sizeof(map)/sizeof(map[0]);
				unsigned long  timeoutUS = 4000000; /* hardcoded for now */
				CSockSd        sock( SOCK_STREAM, bldr->getSocksProxy() );

				via.sin_addr.s_addr = bldr->getRssiBridgeIPAddr();
				via.sin_port        = htons( rpcRelayServerPort() );

				map[0].reqPort      = bldr->getTcpPort();
				map[0].flags        = 0;

				if ( bldr->hasRssi() ) {
					map[0].flags |= MAP_PORT_DESC_FLG_RSSI;
				}

				sock.init( &via, 0, false );

				if ( rpcMapLookup( &via, dup( sock.getSd() ), dst.sin_addr.s_addr, map, nMaps, timeoutUS ) ) {
					fprintf(CPSW::fErr(), "RPC Lookup failed for port %u @ %s -- maybe no rssi_bridge running?\n", map[0].reqPort, inet_ntoa( via.sin_addr ) );
					throw NotFoundError("RPC Lookup failed");
				}

				if ( 0 == map[0].actPort ) {
					fprintf(CPSW::fErr(), "RPC Lookup for port %u @ %s produced no mapping (missing -p/-u on bridge?)\n", map[0].reqPort, inet_ntoa( via.sin_addr ) );
					throw NotFoundError("RPC Lookup produced no mapping");
				}

				via.sin_port = htons( map[0].actPort );
			}

			rval = CShObj::create< ProtoModTcp >( &dst,
			                                      bldr->getTcpOutQueueDepth(),
			                                      bldr->getTcpThreadPriority(),
			                                      bldr->getSocksProxy(),
			                                      &via
			);
		}

		if ( bldr->hasRssiAndUdp() ) {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  creating RSSI\n");
			}
#endif
			ProtoModRssi rssi = CShObj::create<ProtoModRssi>(
			                                bldr->getRssiThreadPriority(),
			                                bldr->getRssiConfigParams()
			);
			rval->addAtPort( rssi );
			rval = rssi;
		}

		if ( bldr->hasDepack() && bldr->getDepackVersion() == DEPACKETIZER_V0 ) {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  creating depack\n");
			}
#endif
			ProtoModDepack depackMod  = CShObj::create< ProtoModDepack > (
			                                bldr->getDepackOutQueueDepth(),
			                                bldr->getDepackLdFrameWinSize(),
			                                bldr->getDepackLdFragWinSize(),
			                                CTimeout(),
			                                bldr->getDepackThreadPriority());
			rval->addAtPort( depackMod );
			rval = depackMod;
		}
	}

	if ( bldr->hasTDestMux()  && ! foundTDestPort ) {
#ifdef PSBLDR_DEBUG
		if ( cpsw_psbldr_debug > 0 ) {
			fprintf(CPSW::fDbg(), "  creating tdest port\n");
		}
#endif
		ProtoModTDestMux  v0;
		ProtoModTDestMux2 v2;

		if ( ! tDestMuxMod ) {
			if ( bldr->getDepackVersion() == DEPACKETIZER_V2 ) {
#ifdef PSBLDR_DEBUG
				if ( cpsw_psbldr_debug > 0 ) {
					fprintf(CPSW::fDbg(), "    creating new TDest Mux V2 module\n");
				}
#endif
				v2 = CShObj::create< ProtoModTDestMux2 >( bldr->getTDestMuxThreadPriority() );
				rval->addAtPort( v2 );
			} else {
#ifdef PSBLDR_DEBUG
				if ( cpsw_psbldr_debug > 0 ) {
					fprintf(CPSW::fDbg(), "    creating new TDest Mux V0 module\n");
				}
#endif
				v0 = CShObj::create< ProtoModTDestMux  >( bldr->getTDestMuxThreadPriority() );
				rval->addAtPort( v0 );
			}
		} else {
			if ( bldr->getDepackVersion() == DEPACKETIZER_V2 ) {
#ifdef PSBLDR_DEBUG
				if ( cpsw_psbldr_debug > 0 ) {
					fprintf(CPSW::fDbg(), "    looking for existing TDest Mux V2 module\n");
				}
#endif
				v2 = dynamic_pointer_cast<ProtoModTDestMux2::element_type>( tDestMuxMod );
				if ( ! v2 ) {
					throw InternalError("No TDEST V2 Demultiplexer - but there should be one");
				}
			} else {
#ifdef PSBLDR_DEBUG
				if ( cpsw_psbldr_debug > 0 ) {
					fprintf(CPSW::fDbg(), "    looking for existing TDest Mux V0 module\n");
				}
#endif
				v0 = dynamic_pointer_cast<ProtoModTDestMux::element_type>( tDestMuxMod );
				if ( ! v0 ) {
					throw InternalError("No TDEST V0 Demultiplexer - but there should be one");
				}
			}
		}
		if ( v2 ) {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  creating TDest Mux V2 port\n");
			}
#endif
			rval = v2->createPort(
			                       bldr->getTDestMuxTDEST(),
			                       bldr->getTDestMuxStripHeader(),
			                       bldr->getTDestMuxOutQueueDepth(),
			                       bldr->getTDestMuxInpQueueDepth()
			                     );
		} else {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  creating TDest Mux V0 port\n");
			}
#endif
			rval = v0->createPort(
			                       bldr->getTDestMuxTDEST(),
			                       bldr->getTDestMuxStripHeader(),
			                       bldr->getTDestMuxOutQueueDepth()
			                     );
		}
	}

	if ( bldr->hasSRPMux() ) {
		if ( ! srpMuxMod ) {
#ifdef PSBLDR_DEBUG
			if ( cpsw_psbldr_debug > 0 ) {
				fprintf(CPSW::fDbg(), "  creating SRP mux module\n");
			}
#endif
			srpMuxMod   = CShObj::create< ProtoModSRPMux >( bldr->getSRPVersion(), bldr->getSRPMuxThreadPriority() );
			rval->addAtPort( srpMuxMod );
		}
		// reserve enough queue depth - must potentially hold replies to synchronous retries
		// until the synchronous reader comes along for the next time!
		unsigned retryCount = bldr->getSRPRetryCount() & 0xffff; // undocumented hack to test byte-resolution access
		rval = srpMuxMod->createPort( bldr->getSRPMuxVirtualChannel(), 2 * (retryCount + 1) );
#ifdef PSBLDR_DEBUG
		if ( cpsw_psbldr_debug > 0 ) {
			fprintf(CPSW::fDbg(), "  creating SRP mux port\n");
		}
#endif
	}

	return rval;
}
