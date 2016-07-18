#define __STDC_FORMAT_MACROS
#include <cpsw_netio_dev.h>
#include <inttypes.h>

#include <cpsw_proto_mod_depack.h>
#include <cpsw_proto_mod_udp.h>
#include <cpsw_proto_mod_srpmux.h>
#include <cpsw_proto_mod_rssi.h>
#include <cpsw_proto_mod_tdestmux.h>

#include <cpsw_mutex.h>

#include <vector>

using boost::dynamic_pointer_cast;

typedef shared_ptr<NetIODevImpl> NetIODevImplP;

typedef uint32_t SRPWord;

struct Mutex {
	CMtx m_;
	Mutex()
	: m_( CMtx::AttrRecursive(), "SRPADDR" )
	{
	}
};

//#define NETIO_DEBUG
//#define TIMEOUT_DEBUG

// if RSSI is used then AFAIK the max. segment size
// (including RSSI header) is 1024 octets.
// Must subtract RSSI (8), packetizer (9), SRP (V3: 20 + 4)
#define MAXWORDS (256 - 11 - 4)

class CNetIODevImpl::CPortBuilder : public INetIODev::IPortBuilder {
	private:
		INetIODev::ProtocolVersion protocolVersion_;
		uint64_t                   SRPTimeoutUS_;
		int                        SRPDynTimeout_;
		unsigned                   SRPRetryCount_;
		bool                       SRPIgnoreMemResp_;
		unsigned                   UdpPort_;
		unsigned                   UdpOutQueueDepth_;
		unsigned                   UdpNumRxThreads_;
		int                        UdpPollSecs_;
		bool                       hasRssi_;
		int                        hasDepack_;
		unsigned                   DepackOutQueueDepth_;
		unsigned                   DepackLdFrameWinSize_;
		unsigned                   DepackLdFragWinSize_;
		int                        hasSRPMux_;
		unsigned                   SRPMuxVirtualChannel_;
		bool                       hasTDestMux_;
		unsigned                   TDestMuxTDEST_;
		int                        TDestMuxStripHeader_;
		unsigned                   TDestMuxOutQueueDepth_;
	public:
		virtual void reset()
		{
			protocolVersion_        = SRP_UDP_V2;
			SRPTimeoutUS_           = 0;
			SRPDynTimeout_          = -1;
			SRPRetryCount_          = -1;
			SRPIgnoreMemResp_       = false;
			UdpPort_                = 8192;
			UdpOutQueueDepth_       = 0;
			UdpNumRxThreads_        = 0;
			UdpPollSecs_            = -1;
			hasRssi_                = false;
			hasDepack_              = -1;
			DepackOutQueueDepth_    = 0;
			DepackLdFrameWinSize_   = 0;
			DepackLdFragWinSize_    = 0;
			hasSRPMux_              = -1;
			SRPMuxVirtualChannel_   = 0;
			hasTDestMux_            = false;
			TDestMuxTDEST_          = 0;
			TDestMuxStripHeader_    = -1;
			TDestMuxOutQueueDepth_  = 0;
		}

		CPortBuilder()
		{
			reset();
		}


		bool hasSRP()
		{
			return SRP_UDP_NONE != protocolVersion_;
		}

		virtual void            setSRPVersion(ProtocolVersion v)
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

		virtual ProtocolVersion getSRPVersion()
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
				return hasRssi() ? 500000 : 10000;
			return SRPTimeoutUS_;
		}

		virtual void            setSRPRetryCount(unsigned v)
		{
			SRPRetryCount_ = v;
		}

		virtual unsigned        getSRPRetryCount()
		{
			if ( (unsigned)-1 == SRPRetryCount_ )
				return 10;
			return SRPRetryCount_;
		}

		virtual void           setSRPIgnoreMemResp(bool v)
		{
			SRPIgnoreMemResp_ = v;
		}

		virtual bool           getSRPIgnoreMemResp()
		{
			return SRPIgnoreMemResp_;
		}

		virtual bool            hasUdp()
		{
			return true;
		}

		virtual void            useSRPDynTimeout(bool v)
		{
			SRPDynTimeout_ = (v ? 1 : 0);
		}

		virtual bool            hasSRPDynTimeout()
		{
			if ( SRPDynTimeout_ < 0 )
				return ! hasTDestMux();
			return SRPDynTimeout_ ? true : false;
		}

		virtual void            setUdpPort(unsigned v)
		{
			if ( v > 65535 || v == 0 )
				throw InvalidArgError("Invalid UDP Port");
			UdpPort_ = v;
		}

		virtual unsigned        getUdpPort()
		{
			return UdpPort_;
		}

		virtual void            setUdpOutQueueDepth(unsigned v)
		{
			UdpOutQueueDepth_ = v;
		}

		virtual unsigned        getUdpOutQueueDepth()
		{
			if ( 0 == UdpOutQueueDepth_ )
				return 10;
			return UdpOutQueueDepth_;
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
				if ( ! hasRssi() && ( ! hasSRP() || hasTDestMux() ) )
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

		virtual void            useDepack(bool v)
		{
			if ( ! (hasDepack_ = (v ? 1:0)) ) {
				setDepackOutQueueDepth( 0 );
				setDepackLdFrameWinSize( 0 );
				setDepackLdFragWinSize( 0 );
			}
		}

		virtual bool            hasDepack()
		{
			if ( hasDepack_ < 0 ) {
				return ! hasSRP() || hasTDestMux();
				
			}
			return hasDepack_ > 0;
		}

		virtual void            setDepackOutQueueDepth(unsigned v)
		{
			DepackOutQueueDepth_ = v;
			useDepack( true );
		}

		virtual unsigned        getDepackOutQueueDepth()
		{
			if ( 0 == DepackOutQueueDepth_ )
				return 50;
			return DepackOutQueueDepth_;
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
				return hasRssi() ? 1 : 5;
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
				return hasRssi() ? 1 : 5;
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
			if ( v > 255 )
				throw InvalidArgError("Requested SRP Mux Virtual Channel out of range");
			useSRPMux( true );
			SRPMuxVirtualChannel_ = v;
		}

		virtual unsigned        getSRPMuxVirtualChannel()
		{
			return SRPMuxVirtualChannel_;
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
			if ( 0 == TDestMuxOutQueueDepth_ )
				return hasSRP() ? 1 : 50;
			return TDestMuxOutQueueDepth_;
		}

		virtual PortBuilder     clone()
		{
			return make_shared<CPortBuilder>( *this );
		}
};

static bool hasRssi(ProtoPort stack)
{
ProtoPortMatchParams cmp;
	cmp.haveRssi_.include();
	return cmp.requestedMatches() == cmp.findMatches( stack );
}

static bool hasDepack(ProtoPort stack)
{
ProtoPortMatchParams cmp;
	cmp.haveDepack_.include();
	return cmp.requestedMatches() == cmp.findMatches( stack );
}

#define IGNORE_MEM_RESP 0x4000

CSRPAddressImpl::CSRPAddressImpl(AKey key, INetIODev::PortBuilder bldr, ProtoPort stack)
:CCommAddressImpl(key, stack),
 protoVersion_(bldr->getSRPVersion()),
 usrTimeout_(bldr->getSRPTimeoutUS()),
 dynTimeout_(usrTimeout_),
 useDynTimeout_(bldr->hasSRPDynTimeout()),
 retryCnt_(bldr->getSRPRetryCount() & 0xffff), // undocumented hack to test byte-resolution access
 nRetries_(0),
 nWrites_(0),
 nReads_(0),
 vc_(bldr->getSRPMuxVirtualChannel()),
 tid_(0),
 byteResolution_( bldr->getSRPVersion() >= INetIODev::SRP_UDP_V3 && bldr->getSRPRetryCount() > 65535 ),
 maxWordsRx_( protoVersion_ < INetIODev::SRP_UDP_V3 || !hasDepack( stack) ? MAXWORDS : (1<<28) ),
 maxWordsTx_( MAXWORDS ),
 mutex_( CMtx::AttrRecursive(), "SRPADDR" )
{
ProtoModSRPMux       srpMuxMod( dynamic_pointer_cast<ProtoModSRPMux::element_type>( stack->getProtoMod() ) );
int                  nbits;

	if ( hasRssi( stack ) ) {
		// have RSSI

		// FIXME: should find out dynamically what RSSI's retransmission
		//        timeout is and set the cap to a few times that value
		dynTimeout_.setTimeoutCap( 50000 ); // 50 ms for now
	}

	if ( ! srpMuxMod )
		throw InternalError("No SRP Mux? Found");

	ignoreMemResp_ = bldr->getSRPIgnoreMemResp() ? IGNORE_MEM_RESP : 0;

	tidLsb_ = 1 << srpMuxMod->getTidLsb();
	nbits   = srpMuxMod->getTidNumBits();
	tidMsk_ = (nbits > 31 ? 0xffffffff : ( (1<<nbits) - 1 ) ) << srpMuxMod->getTidLsb();
}

CSRPAddressImpl::CSRPAddressImpl(AKey k, INetIODev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc, bool useRssi, int tDest)
:CCommAddressImpl(k,ProtoPort()),
 protoVersion_(version),
 usrTimeout_(timeoutUs),
 dynTimeout_(usrTimeout_),
 retryCnt_(retryCnt),
 nRetries_(0),
 nWrites_(0),
 nReads_(0),
 vc_(vc),
 tid_(0),
 mutex_( CMtx::AttrRecursive(), "SRPADDR" )
{
ProtoPortMatchParams cmp;
ProtoModSRPMux       srpMuxMod;
ProtoModTDestMux     tDestMuxMod;
ProtoModDepack       depackMod;
ProtoPort            prt;
NetIODevImpl         owner( getOwnerAs<NetIODevImpl>() );
int                  nbits;
int                  foundMatches, foundRefinedMatches;
unsigned             depackQDepth = 32;


	cmp.udpDestPort_  = dport;

	if ( (prt = owner->findProtoPort( &cmp )) ) {

		// existing RSSI configuration must match the requested one
		if ( useRssi )
			cmp.haveRssi_.include();
		else
			cmp.haveRssi_.exclude();

		if ( tDest >= 0 ) {
			cmp.haveDepack_.include();
			cmp.tDest_ = tDest;
		} else {
			cmp.haveDepack_.exclude();
			cmp.tDest_.exclude();
		}

		foundMatches = cmp.findMatches( prt );

		tDestMuxMod  = dynamic_pointer_cast<ProtoModTDestMux::element_type>( cmp.tDest_.handledBy_ );

		switch ( cmp.requestedMatches() - foundMatches ) {
			case 0:
				// either no tdest demuxer or using an existing tdest port
				cmp.srpVersion_     = version;
				cmp.srpVC_          = vc;

				foundRefinedMatches = cmp.findMatches( prt );

				srpMuxMod = dynamic_pointer_cast<ProtoModSRPMux::element_type>( cmp.srpVC_.handledBy_ );

				switch ( cmp.requestedMatches() - foundRefinedMatches ) {
					case 0:
						throw ConfigurationError("SRP VC already in use");
					case 1:
						if ( srpMuxMod && !cmp.srpVC_.matchedBy_ )
							break;
						/* else fall thru */
					default:
						throw ConfigurationError("Cannot create SRP port on top of existing protocol modules");
				}
				break;
				
			case 1:
				if ( tDestMuxMod && ! cmp.tDest_.matchedBy_ ) {
					protoStack_ = tDestMuxMod->createPort( tDest, true, 1 ); // queue depth 1 enough for synchronous operation
					srpMuxMod   = CShObj::create< ProtoModSRPMux >( version );
					protoStack_->addAtPort( srpMuxMod );
					break;
				}
				/* else fall thru */
			default:
				throw ConfigurationError("Cannot create SRP on top of existing protocol modules");
		}


	
	} else {
		// create new
		struct sockaddr_in dst;

		dst.sin_family      = AF_INET;
		dst.sin_port        = htons( dport );
		dst.sin_addr.s_addr = owner->getIpAddress();

		// Note: UDP module MUST have a queue if RSSI is used
		protoStack_ = CShObj::create< ProtoModUdp >( &dst, 10/* queue */, 1 /* nThreads */, 0 /* no poller */ );

		if ( useRssi ) {
			ProtoModRssi rssi = CShObj::create<ProtoModRssi>();
			protoStack_->addAtPort( rssi );
			protoStack_ = rssi;
		}

		if ( tDest >= 0 ) {
			unsigned ldFrameWinSize = useRssi ? 1 : 4;
			unsigned ldFragWinSize  = useRssi ? 1 : 4;

			// since we have RSSI
			depackMod   = CShObj::create< ProtoModDepack >( depackQDepth, ldFrameWinSize, ldFragWinSize, CTimeout(1000000) );
			protoStack_->addAtPort( depackMod );
			protoStack_ = depackMod;

			tDestMuxMod = CShObj::create< ProtoModTDestMux >();
			protoStack_->addAtPort( tDestMuxMod );
			protoStack_ = tDestMuxMod->createPort( tDest, true, 1 );
		}

		srpMuxMod = CShObj::create< ProtoModSRPMux >( version );

		protoStack_->addAtPort( srpMuxMod );
	}

	protoStack_ = srpMuxMod->createPort( vc );

	if ( useRssi ) {
		// FIXME: should find out dynamically what RSSI's retransmission
		//        timeout is and set the cap to a few times that value
		dynTimeout_.setTimeoutCap( 50000 ); // 50 ms for now
	}

	tidLsb_ = 1 << srpMuxMod->getTidLsb();
	nbits   = srpMuxMod->getTidNumBits();
	tidMsk_ = (nbits > 31 ? 0xffffffff : ( (1<<nbits) - 1 ) ) << srpMuxMod->getTidLsb();
}

CSRPAddressImpl::~CSRPAddressImpl()
{
	shutdownProtoStack();
}

CNetIODevImpl::CNetIODevImpl(Key &k, const char *name, const char *ip)
: CDevImpl(k, name), ip_str_(ip ? ip : "ANY")
{
	if ( INADDR_NONE == ( d_ip_ = ip ? inet_addr( ip ) : INADDR_ANY ) ) {
		throw InvalidArgError( ip );
	}
}

void CSRPAddressImpl::setTimeoutUs(unsigned timeoutUs)
{
	this->usrTimeout_.set(timeoutUs);
}

void CSRPAddressImpl::setRetryCount(unsigned retryCnt)
{
	this->retryCnt_ = retryCnt;
}

static void swp32(SRPWord *buf)
{
#ifdef __GNUC__
	*buf = __builtin_bswap32( *buf );
#else
	#error "bswap32 needs to be implemented"
#endif
}

static void swpw(uint8_t *buf)
{
uint32_t v;
	// gcc knows how to optimize this
	memcpy( &v, buf, sizeof(SRPWord) );
#ifdef __GNUC__
	v = __builtin_bswap32( v );
#else
	#error "bswap32 needs to be implemented"
#endif
	memcpy( buf, &v, sizeof(SRPWord) );
}

#ifdef NETIO_DEBUG
static void time_retry(struct timespec *then, int attempt, const char *pre, ProtoPort me)
{
struct timespec now;

	if ( clock_gettime(CLOCK_REALTIME, &now) ) {
		throw IOError("clock_gettime(time_retry) failed", errno);
	}
	if ( attempt > 0 ) {
		CTimeout xx(now);
		xx -= CTimeout(*then);
		printf("%s -- retry (attempt %d) took %"PRId64"us\n", pre, attempt, xx.getUs());
	}
	*then = now;
}
#endif

#define CMD_READ  0x00000000
#define CMD_WRITE 0x40000000

#define CMD_READ_V3  0x000
#define CMD_WRITE_V3 0x100

#define PROTO_VERS_3     3


uint64_t CSRPAddressImpl::readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const
{
SRPWord  bufh[5];
unsigned hwrds;
uint8_t  buft[sizeof(SRPWord)];
SRPWord  xbuf[5];
SRPWord  header;
SRPWord  status;
unsigned i;
int      j, put;
unsigned headbytes = (byteResolution_ ? 0 : (off & (sizeof(SRPWord)-1)) );
unsigned tailbytes = 0;
int      totbytes;
struct iovec  iov[5];
int      got;
int      nWords;
int      expected = 0;
int      tidoffwrd;
uint32_t tid = getTid();
#ifdef NETIO_DEBUG
struct timespec retry_then;
#endif

	// V1 sends payload in network byte order. We want to transform to restore
	// the standard AXI layout which is little-endian
	bool doSwapV1 = (INetIODev::SRP_UDP_V1 == protoVersion_);
	// header info needs to be swapped to host byte order since we interpret it
	bool doSwap   =	( ( protoVersion_ == INetIODev::SRP_UDP_V1 ? LE : BE ) == hostByteOrder() );

#ifdef NETIO_DEBUG
	fprintf(stderr, "SRP readBlk_unlocked off %"PRIx64"; sbytes %d, swapV1 %d, swap %d protoVersion %d\n", off, sbytes, doSwapV1, doSwap, protoVersion_);
#endif

	if ( sbytes == 0 )
		return 0;

	totbytes = headbytes + sbytes;

	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	put = expected = 0;
	if ( protoVersion_ == INetIODev::SRP_UDP_V1 ) {
		xbuf[put++] = vc_ << 24;
		expected += 1;
	}
	if ( protoVersion_ < INetIODev::SRP_UDP_V3 ) {
		xbuf[put++] = tid;
		xbuf[put++] = (off >> 2) & 0x3fffffff;
		xbuf[put++] = nWords   - 1;
		xbuf[put++] = 0;
		expected += 3;
		hwrds       = 2;
		tidoffwrd   = 0;
	} else {
		xbuf[put++] = CMD_READ_V3 | PROTO_VERS_3 | ignoreMemResp_ ;
		xbuf[put++] = tid;
		xbuf[put++] =   byteResolution_ ? off       : off & ~3ULL;
		xbuf[put++] = off >> 32;
		xbuf[put++] = ( byteResolution_ ? totbytes : (nWords << 2) ) - 1;
		expected += 6;
		hwrds       = 5;
		tidoffwrd   = 1;
	}

	// V2,V3 uses LE, V1 network (aka BE) layout
	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}
	}

	i = 0;
	if ( protoVersion_ == INetIODev::SRP_UDP_V1 ) {
		iov[i].iov_base = &header;
		iov[i].iov_len  = sizeof( header );
		i++;
	}
	iov[i].iov_base = bufh;
	iov[i].iov_len  = hwrds*sizeof(SRPWord) + headbytes;
	i++;

	iov[i].iov_base = dst;
	iov[i].iov_len  = sbytes;
	i++;

	if ( totbytes & (sizeof(SRPWord)-1) ) {
		tailbytes       = sizeof(SRPWord) - (totbytes & (sizeof(SRPWord)-1));
		// padding if not word-aligned
		iov[i].iov_base = buft;
		iov[i].iov_len  = tailbytes;
		i++;
	}

	iov[i].iov_base = &status;
	iov[i].iov_len  = sizeof(SRPWord);
	i++;

	unsigned iovlen  = i;
	unsigned attempt = 0;

	do {
		BufChain xchn = IBufChain::create();
		BufChain rchn;

		xchn->insert( xbuf, 0, sizeof(xbuf[0])*put );

		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		protoStack_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

		do {
			rchn = protoStack_->pop( dynTimeout_.getp(), IProtoPort::REL_TIMEOUT );
			if ( ! rchn ) {
#ifdef NETIO_DEBUG
				time_retry( &retry_then, attempt, "READ", protoStack_ );
#endif
				goto retry;
			}

			if ( clock_gettime(CLOCK_REALTIME, &now) ) {
				throw IOError("clock_gettime(now) failed", errno);
			}

			got = rchn->getSize();

			unsigned bufoff = 0;

			for ( i=0; i<iovlen; i++ ) {
				rchn->extract(iov[i].iov_base, bufoff, iov[i].iov_len);
				bufoff += iov[i].iov_len;
			}

#ifdef NETIO_DEBUG
			printf("got %i bytes, off 0x%"PRIx64", sbytes %i, nWords %i\n", got, off, sbytes, nWords  );
			printf("got %i bytes\n", got);
			for (i=0; i<hwrds; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);
			for ( i=0; i<headbytes; i++ ) {
				printf("headbyte[%i]: %x\n", i, ((uint8_t*)&bufh[hwrds])[i]);
			}

			for ( i=0; (unsigned)i<sbytes; i++ ) {
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
			}

			for ( i=0; i < tailbytes; i++ ) {
				printf("tailbyte[%i]: %x\n", i, buft[i]);
			}
#endif
			if ( doSwap ) {
				swp32( &bufh[tidoffwrd] );
			}
		} while ( (bufh[tidoffwrd] & tidMsk_) != tid );

		if ( useDynTimeout_ )
			dynTimeout_.update( &now, &then );

		if ( got != (int)sizeof(bufh[0])*(nWords + expected) ) {
			if ( got < (int)sizeof(bufh[0])*expected ) {
				printf("got %i, nw %i, exp %i\n", got, nWords, expected);
				throw IOError("Received message (read response) truncated");
			} else {
				rchn->extract( &status, got - sizeof(status), sizeof(status) );
				if ( doSwap )
					swp32( &status );
				throw BadStatusError("SRP Read terminated with bad status", status);
			}
		}

		if ( doSwapV1 ) {
			// switch payload back to LE
			uint8_t  tmp[sizeof(SRPWord)];
			unsigned hoff = 0;
			if ( headbytes ) {
				hoff += sizeof(SRPWord) - headbytes;
				memcpy(tmp, &bufh[2], headbytes);
				if ( hoff > sbytes ) {
					// special case where a single word covers bufh, dst and buft
					memcpy(tmp+headbytes,        dst , sbytes);
					// note: since headbytes < 4 -> hoff < 4
					//       and since sbytes < hoff -> sbytes < 4 -> sbytes == sbytes % 4
					//       then we have: headbytes = off % 4
					//                     totbytes  = off % 4 + sbytes  and totbytes % 4 == (off + sbytes) % 4 == headbytes + sbytes
					//                     tailbytes =  4 - (totbytes % 4)
					//       and thus: headbytes + tailbytes + sbytes  == 4
					memcpy(tmp+headbytes+sbytes, buft, tailbytes);
				} else {
					memcpy(tmp+headbytes, dst, hoff);
				}
#ifdef NETIO_DEBUG
				for (i=0; i< sizeof(SRPWord); i++) printf("headbytes tmp[%i]: %x\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst, tmp+headbytes, hoff);
			}
			int jend =(((int)(sbytes - hoff)) & ~(sizeof(SRPWord)-1));
			for ( j = 0; j < jend; j+=sizeof(SRPWord) ) {
				swpw( dst + hoff + j );
			}
			if ( tailbytes && (hoff <= sbytes) ) { // cover the special case mentioned above
				memcpy(tmp, dst + hoff + j, sizeof(SRPWord) - tailbytes);
				memcpy(tmp + sizeof(SRPWord) - tailbytes, buft, tailbytes);
#ifdef NETIO_DEBUG
				for (i=0; i< sizeof(SRPWord); i++) printf("tailbytes tmp[%i]: %"PRIx8"\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst + hoff + j, tmp, sizeof(SRPWord) - tailbytes);
			}
#ifdef NETIO_DEBUG
			for ( i=0; i< sbytes; i++ ) printf("swapped dst[%i]: %x\n", i, dst[i]);
#endif
		}
		if ( doSwap ) {
			swp32( &status );
			swp32( &header );
		}
		if ( status ) {
			throw BadStatusError("reading SRP", status);
		}

		return sbytes;

retry:
		if ( useDynTimeout_ )
			dynTimeout_.relax();
		nRetries_++;

	} while ( ++attempt <= retryCnt_ );

	if ( useDynTimeout_ )
		dynTimeout_.reset( usrTimeout_ );

	throw IOError("No response -- timeout");
}
	
uint64_t CSRPAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
uint64_t rval            = 0;
unsigned headbytes       = (byteResolution_ ? 0 : (args->off_ & (sizeof(SRPWord)-1)) );
uint64_t off             = args->off_;
uint8_t *dst             = args->dst_;
unsigned sbytes          = args->nbytes_;

unsigned totbytes;
unsigned nWords;

	if ( sbytes == 0 )
		return 0;

	totbytes = headbytes + sbytes;
	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	CMtx::lg GUARD( &mutex_ );

	while ( nWords > maxWordsRx_ ) {
		int nbytes = maxWordsRx_*4 - headbytes;
		rval   += readBlk_unlocked(node, args->cacheable_, dst, off, nbytes);	
		nWords -= maxWordsRx_;
		sbytes -= nbytes;	
		dst    += nbytes;
		off    += nbytes;
		headbytes = 0;
	}

	rval += readBlk_unlocked(node, args->cacheable_, dst, off, sbytes);

	nReads_++;

	return rval;
}

static BufChain assembleXBuf(struct iovec *iov, unsigned iovlen, int iov_pld, int toput)
{
BufChain xchn   = IBufChain::create();

unsigned bufoff = 0, i;
int      j;

	for ( i=0; i<iovlen; i++ ) {
		xchn->insert(iov[i].iov_base, bufoff, iov[i].iov_len);
		bufoff += iov[i].iov_len;
	}

	if ( iov_pld >= 0 ) {
		bufoff = 0;
		for ( j=0; j<iov_pld; j++ )
			bufoff += iov[j].iov_len;
		Buf b = xchn->getHead();
		while ( b && bufoff >= b->getSize() ) {
			bufoff -= b->getSize();
			b = b->getNext();
		}
		j = 0;
		while ( b && j < toput ) {
			if ( bufoff + sizeof(SRPWord) <= b->getSize() ) {
				swpw( b->getPayload() + bufoff );
				bufoff += sizeof(SRPWord);
				j      += sizeof(SRPWord);
			} else {
				bufoff = 0;
				b = b->getNext();
			}
		}
	}

	return xchn;
}

uint64_t CSRPAddressImpl::writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
SRPWord  xbuf[5];
SRPWord  status;
SRPWord  header;
SRPWord  zero = 0;
SRPWord  pad  = 0;
uint8_t  first_word[sizeof(SRPWord)];
uint8_t  last_word[sizeof(SRPWord)];
int      j, put;
unsigned i;
unsigned headbytes       = ( byteResolution_ ? 0 : (off & (sizeof(SRPWord)-1)) );
unsigned totbytes;
struct iovec  iov[5];
int      got;
int      nWords;
uint32_t tid;
int      iov_pld = -1;
#ifdef NETIO_DEBUG
struct timespec retry_then;
#endif
int      expected;
int      tidoff;
int      firstlen = 0, lastlen = 0; // silence compiler warning about un-initialized use

	if ( dbytes == 0 )
		return 0;

	// these look similar but are different...
	bool doSwapV1 =  (protoVersion_ == INetIODev::SRP_UDP_V1);
	bool doSwap   = ((protoVersion_ == INetIODev::SRP_UDP_V1 ? LE : BE) == hostByteOrder() );

	totbytes = headbytes + dbytes;

	nWords = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

#ifdef NETIO_DEBUG
	fprintf(stderr, "SRP writeBlk_unlocked off %"PRIx64"; dbytes %d, swapV1 %d, swap %d headbytes %i, totbytes %i, nWords %i, msk1 0x%02x, mskn 0x%02x\n", off, dbytes, doSwapV1, doSwap, headbytes, totbytes, nWords, msk1, mskn);
#endif


	bool merge_first = headbytes      || msk1;
	bool merge_last  = ( ! byteResolution_ && (totbytes & (sizeof(SRPWord)-1)) ) || mskn;

	if ( (merge_first || merge_last) && cacheable < IField::WT_CACHEABLE ) {
		throw IOError("Cannot merge bits/bytes to non-cacheable area");
	}

	int toput = dbytes;

	if ( merge_first ) {
		if ( merge_last && dbytes == 1 ) {
			// first and last byte overlap
			msk1 |= mskn;
			merge_last = false;
		}

		int first_byte = headbytes;
		int remaining;

		CReadArgs rargs;
		rargs.cacheable_  = cacheable;
		rargs.dst_        = first_word;

		bool lastbyte_in_firstword =  ( merge_last && totbytes <= (int)sizeof(SRPWord) );

		if ( byteResolution_ ) {
			if ( lastbyte_in_firstword ) {
				rargs.nbytes_ = totbytes; //lastbyte_in_firstword implies totbytes <= sizeof(SRPWord)
			} else {
				rargs.nbytes_ = 1;
			}
			rargs.off_    = off;
		} else {
			rargs.nbytes_ = sizeof(first_word);
			rargs.off_    = off & ~3ULL;
		}
		remaining = (totbytes <= rargs.nbytes_ ? totbytes : rargs.nbytes_) - first_byte - 1;

		firstlen  = rargs.nbytes_;

		read(node, &rargs);

		first_word[ first_byte  ] = (first_word[ first_byte ] & msk1) | (src[0] & ~msk1);

		toput--;

		if ( lastbyte_in_firstword ) {
			// totbytes must be <= sizeof(SRPWord) and > 0 (since last==first was handled above)
			int last_byte = (totbytes-1);
			first_word[ last_byte  ] = (first_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
			remaining--;
			toput--;
			merge_last = false;
		}

		if ( remaining > 0 ) {
			memcpy( first_word + first_byte + 1, src + 1, remaining );
			toput -= remaining;
		}
	}
	if ( merge_last ) {

		CReadArgs rargs;
		int       last_byte;

		rargs.cacheable_ = cacheable;
		rargs.dst_       = last_word;

		if ( byteResolution_ ) {
			rargs.nbytes_    = 1;
			rargs.off_       = off + dbytes - 1;
			last_byte        = 0;
		} else {
			rargs.nbytes_    = sizeof(last_word);
			rargs.off_       = (off + dbytes - 1) & ~3ULL;
			last_byte        = (totbytes-1) & (sizeof(SRPWord)-1);
		}

		lastlen = rargs.nbytes_;

		read(node, &rargs);

		last_word[ last_byte  ] = (last_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
		toput--;

		if ( last_byte > 0 ) {
			memcpy(last_word, &src[dbytes-1-last_byte], last_byte);
			toput -= last_byte;
		}
	}

	put = 0;
	tid = getTid();

	if ( protoVersion_ < INetIODev::SRP_UDP_V3 ) {
		expected    = 3;
		tidoff      = 0;
		if ( protoVersion_ == INetIODev::SRP_UDP_V1 ) {
			xbuf[put++] = vc_ << 24;
			expected++;
			tidoff  = 4;
		}
		xbuf[put++] = tid;
		xbuf[put++] = ((off >> 2) & 0x3fffffff) | CMD_WRITE;
	} else {
		xbuf[put++] = CMD_WRITE_V3 | PROTO_VERS_3;
		xbuf[put++] = tid;
		xbuf[put++] = ( byteResolution_ ? off : (off & ~3ULL) );
		xbuf[put++] = off >> 32;
		xbuf[put++] = ( byteResolution_ ? totbytes : nWords << 2 ) - 1;
		expected    = 6;
		tidoff      = 4;
	}

	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}

		swp32( &zero );
	}

	i = 0;
	iov[i].iov_base = xbuf;
	iov[i].iov_len  = sizeof(xbuf[0])*put;
	i++;

	if ( merge_first ) {
		iov[i].iov_len  = firstlen;
		if ( doSwapV1 )
			swpw( first_word );
		iov[i].iov_base = first_word;
		i++;
	}

	if ( toput > 0 ) {
		iov[i].iov_base = src + (merge_first ? (firstlen - headbytes) : 0);
		iov[i].iov_len  = toput;

		if ( doSwapV1 ) {
/* do NOT swap in the source buffer; defer until copied to
 * the transmit buffer
			for ( j = 0; j<toput; j += sizeof(SRPWord) ) {
				swpw( (uint8_t*)iov[i].iov_base + j );
			}
 */
			iov_pld = i;
		}
		i++;
	}

	if ( merge_last ) {
		if ( doSwapV1 )
			swpw( last_word );
		iov[i].iov_base = last_word;
		iov[i].iov_len  = lastlen;
		i++;
	}

	if ( protoVersion_ < INetIODev::SRP_UDP_V3 ) {
		iov[i].iov_base = &zero;
		iov[i].iov_len  = sizeof(SRPWord);
		i++;
	} else if ( byteResolution_ && (totbytes & 3) ) {
		iov[i].iov_base = &pad;
		iov[i].iov_len  = sizeof(SRPWord) - (totbytes & 3);
		i++;
	}

	unsigned attempt = 0;
	uint32_t got_tid;
	unsigned iovlen  = i;

	do {

		BufChain xchn = assembleXBuf(iov, iovlen, iov_pld, toput);

		BufChain rchn;
		uint8_t *rbuf;
		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		protoStack_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

		do {
			rchn = protoStack_->pop( dynTimeout_.getp(), IProtoPort::REL_TIMEOUT );
			if ( ! rchn ) {
#ifdef NETIO_DEBUG
				time_retry( &retry_then, attempt, "WRITE", protoStack_ );
#endif
				goto retry;
			}

			if ( clock_gettime(CLOCK_REALTIME, &now) ) {
				throw IOError("clock_gettime(now) failed", errno);
			}

			got = rchn->getSize();
			if ( rchn->getLen() > 1 )
				throw InternalError("Assume received payload fits into a single buffer");
			rbuf = rchn->getHead()->getPayload();
#if 0
			printf("got %i bytes, dbytes %i, nWords %i\n", got, dbytes, nWords);
			printf("got %i bytes\n", got);
			for (i=0; i<2; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);

			for ( i=0; i<dbytes; i++ )
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
#endif
			memcpy( &got_tid, rbuf + tidoff, sizeof(SRPWord) );
			if ( doSwap ) {
				swp32( &got_tid );
			}
		} while ( tid != (got_tid & tidMsk_) );

		if ( useDynTimeout_ )
			dynTimeout_.update( &now, &then );

		if ( got != (int)sizeof(SRPWord)*(nWords + expected) ) {
			if ( got < (int)sizeof(SRPWord)*expected ) {
				printf("got %i, nw %i, exp %i\n", got, nWords, expected);
				throw IOError("Received message (write response) truncated");
			} else {
				rchn->extract( &status, got - sizeof(status), sizeof(status) );
				if ( doSwap )
					swp32( &status );
				throw BadStatusError("SRP Write terminated with bad status", status);
			}
		}

		if ( protoVersion_ == INetIODev::SRP_UDP_V1 ) {
			if ( LE == hostByteOrder() ) {
				swpw( rbuf );
			}
			memcpy( &header, rbuf,  sizeof(SRPWord) );
		} else {
			header = 0;
		}
		if ( doSwap ) {
			swpw( rbuf + got - sizeof(SRPWord) );
		}
		memcpy( &status, rbuf + got - sizeof(SRPWord), sizeof(SRPWord) );
		if ( status )
			throw BadStatusError("writing SRP", status);
		return dbytes;
retry:
		if ( useDynTimeout_ )
			dynTimeout_.relax();
		nRetries_++;
	} while ( ++attempt <= retryCnt_ );

	if ( useDynTimeout_ )
		dynTimeout_.reset( usrTimeout_ );

	throw IOError("Too many retries");
}

uint64_t CSRPAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
uint64_t rval            = 0;
unsigned headbytes       = (byteResolution_ ? 0 : (args->off_ & (sizeof(SRPWord)-1)) );
unsigned dbytes          = args->nbytes_;
uint64_t off             = args->off_;
uint8_t *src             = args->src_;
uint8_t  msk1            = args->msk1_;

unsigned totbytes;
unsigned nWords;

	if ( dbytes == 0 )
		return 0;

	totbytes = headbytes + dbytes;
	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	CMtx::lg GUARD( &mutex_ );

	while ( nWords > maxWordsTx_ ) {
		int nbytes = maxWordsTx_*4 - headbytes;
		rval += writeBlk_unlocked(node, args->cacheable_, src, off, nbytes, msk1, 0);	
		nWords -= maxWordsTx_;
		dbytes -= nbytes;	
		src    += nbytes;
		off    += nbytes;
		headbytes = 0;
		msk1      = 0;
	}

	rval += writeBlk_unlocked(node, args->cacheable_, src, off, dbytes, msk1, args->mskn_);

	nWrites_++;
	return rval;

}

void CCommAddressImpl::dump(FILE *f) const
{
	CAddressImpl::dump(f);
	fprintf(f,"\nPeer: %s\n", getOwnerAs<NetIODevImpl>()->getIpAddressString());
	fprintf(f,"\nProtocol Modules:\n");
	if ( protoStack_ ) {
		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			m->dumpInfo( f );
		}
	}
}

void CSRPAddressImpl::dump(FILE *f) const
{
	fprintf(f,"CSRPAddressImpl:\n");
	CCommAddressImpl::dump(f);
	fprintf(f,"SRP Info:\n");
	fprintf(f,"  Protocol Version  : %8u\n",   protoVersion_);
	fprintf(f,"  Timeout (user)    : %8"PRIu64"us\n", usrTimeout_.getUs());
	fprintf(f,"  Timeout %s : %8"PRIu64"us\n", useDynTimeout_ ? "(dynamic)" : "(capped) ", dynTimeout_.get().getUs());
	if ( useDynTimeout_ )
	{
	fprintf(f,"  max Roundtrip time: %8"PRIu64"us\n", dynTimeout_.getMaxRndTrip().getUs());
	fprintf(f,"  avg Roundtrip time: %8"PRIu64"us\n", dynTimeout_.getAvgRndTrip().getUs());
	}
	fprintf(f,"  Retry Limit       : %8u\n",   retryCnt_);
	fprintf(f,"  # of retried ops  : %8u\n",   nRetries_);
	fprintf(f,"  # of writes (OK)  : %8u\n",   nWrites_);
	fprintf(f,"  # of reads  (OK)  : %8u\n",   nReads_);
	fprintf(f,"  Virtual Channel   : %8u\n",   vc_);
}

bool CNetIODevImpl::portInUse(unsigned port)
{
ProtoPortMatchParams cmp;

	cmp.udpDestPort_=port;

	return findProtoPort( &cmp ) != 0;
}

void CNetIODevImpl::addAtAddress(Field child, PortBuilder bldr)
{
IAddress::AKey key  = getAKey();
ProtoPort      port = makeProtoStack( bldr );

shared_ptr<CCommAddressImpl> addr;

	switch( bldr->getSRPVersion() ) {
		case SRP_UDP_NONE:
#ifdef NETIO_DEBUG
			fprintf(stderr,"Creating CCommAddress\n");
#endif
			addr = make_shared<CCommAddressImpl>(key, port);
		break;

		case SRP_UDP_V1:
		case SRP_UDP_V2:
		case SRP_UDP_V3:
#ifdef NETIO_DEBUG
			fprintf(stderr,"Creating SRP address (V %d)\n", bldr->getSRPVersion());
#endif
			addr = make_shared<CSRPAddressImpl>(key, bldr, port);
		break;

		default:
			throw InternalError("Unknown Communication Protocol");
	}

	add(addr, child);
	addr->startProtoStack();
}

void CNetIODevImpl::addAtAddress(Field child, INetIODev::ProtocolVersion version, unsigned dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc, bool useRssi, int tDest)
{
PortBuilder bldr( INetIODev::createPortBuilder() );

	bldr->setSRPVersion( version );
	bldr->setSRPTimeoutUS( timeoutUs );
	bldr->setSRPRetryCount( retryCnt );
	bldr->useRssi( useRssi );
	bldr->setSRPMuxVirtualChannel( vc );
	bldr->setUdpPort( dport );
	if ( tDest >= 0 ) {
		bldr->setTDestMuxTDEST( tDest );
	}

	addAtAddress(child, bldr);
}

void CNetIODevImpl::addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi, int tDest)
{
PortBuilder bldr( INetIODev::createPortBuilder() );

	bldr->setSRPVersion( SRP_UDP_NONE );
	bldr->setUdpPort( dport );
	// ignore depacketizer (input) timeout -- no builder support because not necessary
	bldr->setUdpOutQueueDepth( inQDepth );
	bldr->setUdpNumRxThreads( nUdpThreads );
	bldr->useRssi( useRssi );
	bldr->setDepackOutQueueDepth( inQDepth );
	bldr->setDepackLdFrameWinSize( ldFrameWinSize );
	bldr->setDepackLdFragWinSize( ldFragWinSize );
	if ( tDest >= 0 ) {
		bldr->setTDestMuxTDEST( tDest );
	}

	addAtAddress(child, bldr);
}


NetIODev INetIODev::create(const char *name, const char *ipaddr)
{
	return CShObj::create<NetIODevImpl>(name, ipaddr);
}

INetIODev::PortBuilder INetIODev::createPortBuilder()
{
	return make_shared<CNetIODevImpl::CPortBuilder>();
}

void CNetIODevImpl::setLocked()
{
	if ( getLocked() ) {
		throw InternalError("Cannot attach this type of device multiple times -- need to create a new instance");
	}
	CDevImpl::setLocked();
}


ProtoPort CNetIODevImpl::findProtoPort(ProtoPortMatchParams *cmp_p)
{
Children myChildren = getChildren();

Children::element_type::iterator it;
int                              requestedMatches = cmp_p->requestedMatches();

	for ( it = myChildren->begin(); it != myChildren->end(); ++it ) {
		shared_ptr<const CCommAddressImpl> child = static_pointer_cast<const CCommAddressImpl>( *it );
		// 'match' modifies the parameters (storing results)
		cmp_p->reset();
		if ( requestedMatches == cmp_p->findMatches( child->getProtoStack() ) ) {
			return child->getProtoStack();
		}
	}
	return ProtoPort();
}

ProtoPort CNetIODevImpl::makeProtoStack(PortBuilder bldr_in)
{
ProtoPort            rval;
ProtoPort            foundTDestPort;
ProtoPortMatchParams cmp;
ProtoModTDestMux     tDestMuxMod;
ProtoModSRPMux       srpMuxMod;
PortBuilder          bldr   = bldr_in->clone();
bool                 hasSRP = SRP_UDP_NONE != bldr->getSRPVersion();

	// sanity checks
	if ( ! bldr->hasUdp() || 0 == bldr->getUdpPort() )
		throw ConfigurationError("Currently only UDP transport supported\n");

	if ( ! hasSRP && bldr->hasSRPMux() ) {
		throw ConfigurationError("Cannot configure SRP Demuxer w/o SRP protocol version");
	}

	cmp.udpDestPort_ = bldr->getUdpPort();

#ifdef NETIO_DEBUG
	printf("makeProtoPort for port %d\n", bldr->getUdpPort());
#endif

	if ( findProtoPort( &cmp ) ) {

		if ( ! bldr->hasTDestMux() && ( !hasSRP || !bldr->hasSRPMux() ) ) {
			throw ConfigurationError("Some kind of demuxer must be used when sharing a UDP port");
		}

#ifdef NETIO_DEBUG
	printf("makeProtoPort port %d found\n", bldr->getUdpPort());
#endif

		// existing RSSI configuration must match the requested one
		if ( bldr->hasRssi() ) {
#ifdef NETIO_DEBUG
	printf("  including RSSI\n");
#endif
			cmp.haveRssi_.include();
		} else {
#ifdef NETIO_DEBUG
	printf("  excluding RSSI\n");
#endif
			cmp.haveRssi_.exclude();
		}

		// existing DEPACK configuration must match the requested one
		if ( bldr->hasDepack() ) {
			cmp.haveDepack_.include();
#ifdef NETIO_DEBUG
	printf("  including depack\n");
#endif
		} else {
			cmp.haveDepack_.exclude();
#ifdef NETIO_DEBUG
	printf("  excluding depack\n");
#endif
		}

		if ( bldr->hasTDestMux() ) {
			cmp.tDest_ = bldr->getTDestMuxTDEST();
#ifdef NETIO_DEBUG
	printf("  tdest %d\n", bldr->getTDestMuxTDEST());
#endif
		} else {
			cmp.tDest_.exclude();
#ifdef NETIO_DEBUG
	printf("  tdest excluded\n");
#endif
		}

		if ( (foundTDestPort = findProtoPort( &cmp )) ) {
#ifdef NETIO_DEBUG
	printf("  tdest port FOUND\n");
#endif

			// either no tdest demuxer or using an existing tdest port
			if ( ! hasSRP ) {
				throw ConfigurationError("Cannot share TDEST w/o SRP demuxer");
			}

			cmp.srpVersion_     = bldr->getSRPVersion();
			cmp.srpVC_          = bldr->getSRPMuxVirtualChannel();

			if ( findProtoPort( &cmp ) ) {
				throw ConfigurationError("SRP VC already in use");
			}

			cmp.srpVC_.wildcard();

			if ( ! findProtoPort( &cmp ) ) {
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
				if ( ! findProtoPort( &cmp ) ) {
					throw ConfigurationError("No TDEST Demultiplexer found");
				}
				tDestMuxMod  = dynamic_pointer_cast<ProtoModTDestMux::element_type>( cmp.tDest_.handledBy_ );

				if ( ! tDestMuxMod ) {
					throw InternalError("No TDEST Demultiplexer - but there should be one");
				}
#ifdef NETIO_DEBUG
	printf("  using (existing) tdest MUX\n");
#endif
			} else {
				throw ConfigurationError("Unable to create new port on existing protocol modules");
			}
		}
	} else {
		// create new
		struct sockaddr_in dst;

		dst.sin_family      = AF_INET;
		dst.sin_port        = htons( bldr->getUdpPort() );
		dst.sin_addr.s_addr = getIpAddress();

		// Note: UDP module MUST have a queue if RSSI is used
		rval = CShObj::create< ProtoModUdp >( &dst, bldr->getUdpOutQueueDepth(), bldr->getUdpNumRxThreads(), bldr->getUdpPollSecs() );

		if ( bldr->hasRssi() ) {
#ifdef NETIO_DEBUG
	printf("  creating RSSI\n");
#endif
			ProtoModRssi rssi = CShObj::create<ProtoModRssi>();
			rval->addAtPort( rssi );
			rval = rssi;
		}

		if ( bldr->hasDepack() ) {
#ifdef NETIO_DEBUG
	printf("  creating depack\n");
#endif
			ProtoModDepack depackMod  = CShObj::create< ProtoModDepack > (
			                                bldr->getDepackOutQueueDepth(),
			                                bldr->getDepackLdFrameWinSize(),
			                                bldr->getDepackLdFragWinSize(),
			                                CTimeout() );
			rval->addAtPort( depackMod );
			rval = depackMod;
		}
	}

	if ( bldr->hasTDestMux()  && ! foundTDestPort ) {
#ifdef NETIO_DEBUG
	printf("  creating tdest port\n");
#endif
		if ( ! tDestMuxMod ) {
			tDestMuxMod = CShObj::create< ProtoModTDestMux >();
			rval->addAtPort( tDestMuxMod );
		}
		rval = tDestMuxMod->createPort( bldr->getTDestMuxTDEST(), bldr->getTDestMuxStripHeader(), bldr->getTDestMuxOutQueueDepth() );
	}

	if ( bldr->hasSRPMux() ) {
		if ( ! srpMuxMod ) {
#ifdef NETIO_DEBUG
	printf("  creating SRP mux module\n");
#endif
			srpMuxMod   = CShObj::create< ProtoModSRPMux >( bldr->getSRPVersion() );
			rval->addAtPort( srpMuxMod );
		}
		rval = srpMuxMod->createPort( bldr->getSRPMuxVirtualChannel() );
#ifdef NETIO_DEBUG
	printf("  creating SRP mux port\n");
#endif
	}

	return rval;
}

CCommAddressImpl::CCommAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi, int tDest)
:CAddressImpl(key),
 running_(false)
{
NetIODevImpl         owner( getOwnerAs<NetIODevImpl>() );
ProtoPort            prt;
struct sockaddr_in   dst;
ProtoPortMatchParams cmp;
ProtoModTDestMux     tdm;
bool                 stripHeader = false;
unsigned             qDepth      = 5;

	cmp.udpDestPort_  = dport;

	if ( (prt = owner->findProtoPort( &cmp )) ) {
		// if this UDP port is already in use then
		// there must be a tdest demuxer and we must use it
		if ( tDest < 0 )
			throw ConfigurationError("If stream is to share a UDP port then it must use a TDEST demuxer");

		// existing RSSI configuration must match the requested one
		if ( useRssi )
			cmp.haveRssi_.include();
		else
			cmp.haveRssi_.exclude();

		// there must be a depacketizer
		cmp.haveDepack_.include();

		cmp.tDest_ = tDest;

		int foundMatches = cmp.findMatches( prt );

		tdm = dynamic_pointer_cast<ProtoModTDestMux::element_type>( cmp.tDest_.handledBy_ );

		switch ( cmp.requestedMatches() - foundMatches ) {
			case 0:
				throw ConfigurationError("TDEST already in use");
			case 1:
				if ( tdm && ! cmp.tDest_.matchedBy_ )
					break;
				/* else fall thru */
			default:
				throw ConfigurationError("Cannot create stream on top of existing protocol modules");
		}


	} else {
		dst.sin_family      = AF_INET;
		dst.sin_port        = htons( dport );
		dst.sin_addr.s_addr = owner->getIpAddress();

		ProtoModUdp    udpMod     = CShObj::create< ProtoModUdp >( &dst, inQDepth, nUdpThreads );

		ProtoModDepack depackMod  = CShObj::create< ProtoModDepack >( outQDepth, ldFrameWinSize, ldFragWinSize, CTimeout(timeoutUs) );

		if ( useRssi ) {
			ProtoModRssi   rssiMod    = CShObj::create<ProtoModRssi>();
			udpMod->addAtPort( rssiMod );
			rssiMod->addAtPort( depackMod );
		} else {
			udpMod->addAtPort( depackMod );
		}

		protoStack_ = depackMod;

		if ( tDest >= 0 ) {
			tdm = CShObj::create< ProtoModTDestMux >();
			depackMod->addAtPort( tdm );
		}
	}

	if ( tdm )
		protoStack_ = tdm->createPort( tDest, stripHeader, qDepth );
}

uint64_t CCommAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
BufChain bch;

	bch = protoStack_->pop( &args->timeout_, IProtoPort::REL_TIMEOUT  );

	if ( ! bch )
		return 0;

	return bch->extract( args->dst_, args->off_, args->nbytes_ );
}

uint64_t CCommAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
BufChain bch = IBufChain::create();
uint64_t rval;

	bch->insert( args->src_, args->off_, args->nbytes_ );

	rval = bch->getSize();

	if ( ! protoStack_->push( bch, &args->timeout_, IProtoPort::REL_TIMEOUT ) )
		return 0;
	return rval;
}

void CCommAddressImpl::startProtoStack()
{
	// start in reverse order
	if ( protoStack_  && ! running_) {
		std::vector<ProtoMod> mods;
		int                      i;

		running_ = true;

		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			mods.push_back( m );
		}
		for ( i = mods.size() - 1; i >= 0; i-- ) {
			mods[i]->modStartup();
		}
	}
}

void CCommAddressImpl::shutdownProtoStack()
{
	if ( protoStack_ && running_ ) {
		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			m->modShutdown();
		}
		running_ = false;
	}
}

CCommAddressImpl::~CCommAddressImpl()
{
	shutdownProtoStack();
}

DynTimeout::DynTimeout(const CTimeout &iniv)
: timeoutCap_( CAP_US )
{
	reset( iniv );
}

void DynTimeout::setLastUpdate()
{
	if ( clock_gettime( CLOCK_MONOTONIC, &lastUpdate_.tv_ ) ) {
		throw IOError("clock_gettime failed! :", errno);
	}

	// when the timeout becomes too small then I experienced
	// some kind of scheduling problem where packets
	// arrive but not all threads processing them upstream
	// seem to get CPU time quickly enough.
	// We mitigate by capping the timeout if that happens.

	uint64_t new_timeout = avgRndTrip_ >> (AVG_SHFT-MARG_SHFT);
	if ( new_timeout < timeoutCap_ )
		new_timeout = timeoutCap_;
	dynTimeout_.set( new_timeout );
	nSinceLast_ = 0;
}

const CTimeout
DynTimeout::getAvgRndTrip() const
{
	return CTimeout( avgRndTrip_ >> AVG_SHFT );
}

void DynTimeout::reset(const CTimeout &iniv)
{
	maxRndTrip_.set(0);
	avgRndTrip_ = iniv.getUs() << (AVG_SHFT - MARG_SHFT);
	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	printf("dynTimeout reset to %"PRId64"\n", dynTimeout_.getUs());
#endif
}

void DynTimeout::relax()
{

	// increase
	avgRndTrip_ += (dynTimeout_.getUs()<<1) - (avgRndTrip_ >> AVG_SHFT);

	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	printf("RETRY (timeout %"PRId64")\n", dynTimeout_.getUs());
#endif
}

void DynTimeout::update(const struct timespec *now, const struct timespec *then)
{
CTimeout diff(*now);
int64_t  diffus;

	diff -= CTimeout( *then );
	
	if ( maxRndTrip_ < diff )
		maxRndTrip_ = diff;

	avgRndTrip_ += (diffus = diff.getUs() - (avgRndTrip_ >> AVG_SHFT));

	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	printf("dynTimeout update to %"PRId64" (rnd %"PRId64" -- diff %"PRId64", avg %"PRId64" = 128*%"PRId64")\n",
		dynTimeout_.getUs(),
		diff.getUs(),
		diffus,
		avgRndTrip_,
		avgRndTrip_>>AVG_SHFT);
#endif
}
