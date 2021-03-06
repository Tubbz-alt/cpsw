 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <cpsw_compat.h>
#include <cpsw_mmio_dev.h>
#include <yaml-cpp/yaml.h>

#include <string.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <getopt.h>
#include <iostream>

#include <vector>

#include <pthread.h>

#define VLEN 123
#define ADCL 10

#define SYNC_LIMIT 10

using cpsw::atomic;
using cpsw::memory_order_acquire;
using cpsw::memory_order_release;

using std::vector;

class TestFailed {};

class   IAXIVers;
typedef shared_ptr<IAXIVers> AXIVers;

class CAXIVersImpl;
typedef shared_ptr<CAXIVersImpl> AXIVersImpl;

class IAXIVers : public virtual IMMIODev {
public:
	static AXIVers create(const char *name);
};

class CAXIVersImpl : public CMMIODevImpl, public virtual IAXIVers {
protected:
	CAXIVersImpl(const CAXIVersImpl &orig, Key &k)
	: CMMIODevImpl(orig, k)
	{
	}

public:
	CAXIVersImpl(Key &k, const char *name);

	virtual CAXIVersImpl *clone(Key &k)
	{
		return new CAXIVersImpl( *this, k );
	}
};

AXIVers IAXIVers::create(const char *name)
{
AXIVersImpl v = CShObj::create<AXIVersImpl>(name);
Field f;
	f = IIntField::create("FpgaVersion", 32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("DeviceDna", 64, false, 0, IIntField::RO, 4);
	v->CMMIODevImpl::addAtAddress( f , 0x08 );
	f = IIntField::create("FdSerial", 64, false, 0, IIntField::RO, 4);
	v->CMMIODevImpl::addAtAddress( f, 0x10 );
	f = IIntField::create("UpTimeCnt",  32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f, 0x24 );
	f = IIntField::create("BuildStamp",  8, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f, 0x800, VLEN  );
	f = IIntField::create("ScratchPad",32,true,  0);
	v->CMMIODevImpl::addAtAddress( f,   4 );
	f = IIntField::create("Bits",22,true, 4);
	v->CMMIODevImpl::addAtAddress( f,   4 );
	return v;	
}

CAXIVersImpl::CAXIVersImpl(Key &key, const char *name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}

class   IPRBS;
typedef shared_ptr<IPRBS> PRBS;

class CPRBSImpl;
typedef shared_ptr<CPRBSImpl> PRBSImpl;

class IPRBS : public virtual IMMIODev {
public:
	static PRBS create(const char *name);
};

class CPRBSImpl : public CMMIODevImpl, public virtual IPRBS {
protected:
	CPRBSImpl(const CPRBSImpl &orig, Key &k)
	: CMMIODevImpl(orig, k)
	{
	}

public:
	CPRBSImpl(Key &k, const char *name);

	virtual CPRBSImpl *clone(Key &k)
	{
		return new CPRBSImpl( *this, k );
	}
};

PRBS IPRBS::create(const char *name)
{
PRBSImpl v = CShObj::create<PRBSImpl>(name);
Field f;
	f = IIntField::create("AxiEn",        1, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("TxEn",         1, false, 1);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("Busy",         1, false, 2, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("Overflow",     1, false, 3, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("OneShot",      1, false, 4);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("OacketLength",32, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x04 );
	f = IIntField::create("tDest",        8, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x08 );
	f = IIntField::create("tId",          8, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x09 );
	f = IIntField::create("DataCount",   32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x0c );
	f = IIntField::create("EventCount",  32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x10 );
	f = IIntField::create("RandomData",  32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x14 );

	return v;
}

CPRBSImpl::CPRBSImpl(Key &key, const char *name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}

static ScalVal_RO vpb(vector<ScalVal_RO> *v, ScalVal_RO x)
{
	v->push_back( x );
	return x;
}

static ScalVal vpb(vector<ScalVal_RO> *v, ScalVal x)
{
	v->push_back( x );
	return x;
}

class ThreadArg {
private:
	atomic<int> firstFrame_;
	atomic<int> nFrames_;
	uint64_t    size_;
	int         shots_;
	bool        cont_;
	ScalVal     trig_;
	Hub         root_;
public:
	CTimeout trigTime;

	ThreadArg(uint64_t size, int shots, ScalVal trig, Hub root)
	:firstFrame_(-1),
	 nFrames_(0),
	 size_(size),
	 shots_(shots),
	 trig_(trig),
	 root_(root)
	{
	}

	void gotFrame(int frameNo)
	{
		int expected = -1;
		firstFrame_.compare_exchange_strong( expected, frameNo );
		nFrames_.fetch_add(1, memory_order_release);
	}

	int firstFrame()
	{
		return firstFrame_.load( memory_order_acquire );
	}

	int nFrames()
	{
		return nFrames_.load( memory_order_acquire );
	}

	uint64_t getSize()
	{
		return size_;
	}

	int getShots()
	{
		return shots_;
	}

	bool isCont()
	{
		return !!trig_;
	}

	void trigOff()
	{
		trig_->setVal( (uint64_t)0 );
	}

	Hub getRoot()
	{
		return root_;
	}
};


static void *rxThread(void *arg)
{
uint8_t  buf[16];
int64_t  got;
ThreadArg *stats = static_cast<ThreadArg*>(arg);
CTimeout now;
Stream strm = IStream::create( stats->getRoot()->findByName("dataSource") );


	while ( stats->nFrames() < stats->getShots() ) {
		got = strm->read( buf, sizeof(buf), CTimeout(20000000) );
		if ( ! got ) {
			fprintf(stderr,"RX thread timed out\n");
			exit (1);
		}
		clock_gettime( CLOCK_MONOTONIC, &now.tv_ );
		unsigned frameNo;
		if ( got > 2 ) {
			frameNo = (buf[1]<<4) | (buf[0] >> 4);
			stats->gotFrame( frameNo );
			if ( ! stats->isCont() ) {
				now -= stats->trigTime;
				printf("Received frame # %d Data rate: %g MB/s\n", frameNo, (double)stats->getSize() / (double)now.getUs()  );
			}
		} else {
			fprintf(stderr,"Received frame too small!\n");
		}
	}
	if ( stats->isCont() ) {
		now -= stats->trigTime;
		double sz = stats->getSize();
		printf("Received %d frames (size %gMB) Data rate: %g MB/s\n", stats->getShots(), sz/1000000.,  sz * stats->getShots() / (double)now.getUs()  );
		uint64_t to = (uint64_t)stats->getSize() / 10 /*MB/s*/ ;
		if ( to < 10000 )
			to = 10000;
		while ( strm->read(buf, sizeof(buf), CTimeout(to) ) > 0 ) {
			printf("Dumped frame # %d\n", (buf[1]<<4) | (buf[0] << 4));
		}
	}

	return 0;
}

static void testBram(ScalVal bram)
{
unsigned nelms = bram->getNelms();
uint8_t oval[nelms];
uint8_t ival[nelms];
uint64_t offs[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
uint64_t lens[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
unsigned o,l,i;

	memset(oval, 0xaa, sizeof(oval[0])*nelms );
	memset(ival, 0x00, sizeof(ival[0])*nelms );
	bram->setVal( oval, nelms );
	bram->getVal( ival, nelms );
	if ( memcmp(oval, ival, sizeof(oval[0])*nelms) ) {
		fprintf(stderr,"Readback of BRAM (0xaa) FAILED\n");
		throw TestFailed();
	}

	for ( i=0; i<nelms; i++ ) {
		oval[i] = (uint8_t)(rand()>>16);
	}
	memset(ival, 0x00, sizeof(ival[0])*nelms );
	bram->setVal( oval, nelms );
	bram->getVal( ival, nelms );
	if ( memcmp(oval, ival, sizeof(oval[0])*nelms) ) {
		fprintf(stderr,"Readback of BRAM (random-data) FAILED\n");
		throw TestFailed();
	}



	for ( o=0; o<sizeof(offs)/sizeof(offs[0]); o++ ) {
		for ( l=0; l<sizeof(lens)/sizeof(lens[0]); l++ ) {
			IndexRange r(offs[o], offs[o]+lens[l] - 1);
			// fill target 
			bram->setVal( 0xaa );
			// fill 'expected' array
			memset(oval, 0xaa, sizeof(oval[0])*nelms );
			// zero readback array
			memset(ival, 0x00, sizeof(ival[0])*nelms );

			// merge random data into 'expected' at offset/len
			for (i=0; i<lens[l]; i++ )
				oval[ offs[o] + i ] = (uint8_t)(rand()>>16); 

			// merge on target
			bram->setVal(oval + offs[o], lens[l], &r);

			// read everything back
			bram->getVal(ival, nelms);

			// compare
			if ( memcmp(oval, ival, sizeof(oval[0])*nelms) ) {
				fprintf(stderr,"BRAM write-test: Readback (offset %" PRId64 ", len %" PRId64 ") FAILED\n", offs[o], lens[l]);
				throw TestFailed();
#ifdef DEBUG
			} else {
				printf("BRAM write-test (offset %" PRId64 ", len %" PRId64 ") PASSED\n", offs[o], lens[l]);
#endif
			}

			// clear
			memset(ival, 0xaa, nelms);

			// readback subset only
			bram->getVal(ival+offs[o], lens[l], &r);

			// compare
			if ( memcmp(oval, ival, sizeof(oval[0])*nelms) ) {
				fprintf(stderr,"BRAM read-test: (offset %" PRId64 ", len %" PRId64 ") FAILED\n", offs[o], lens[l]);
				throw TestFailed();
#ifdef DEBUG
			} else {
				printf("BRAM read-test (offset %" PRId64 ", len %" PRId64 ") PASSED\n", offs[o], lens[l]);
#endif
			}

		}
	}

	printf("BRAM Test PASSED\n");
}

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-a <ip_addr>[:<port>[:<stream_port>]]] [-mhRrs] [-V <version>] [-S <length>] [-n <shots>] [-p <period>] [-d tdest] [-D tdest] [-M tdest] [-T timeout]\n", nm);
	fprintf(stderr,"       -a <ip_addr>:  destination IP\n");
	fprintf(stderr,"       -V <version>:  SRP version (1..3)\n");
	fprintf(stderr,"       -m          :  use 'fake' memory image instead\n");
	fprintf(stderr,"                      of real device and UDP\n");
	fprintf(stderr,"       -s          :  test system monitor ADCs\n");
	fprintf(stderr,"       -S <length> :  test streaming interface\n");
	fprintf(stderr,"                      with frames of 'length'.\n");
	fprintf(stderr,"                      'length' must be > 0 to enable streaming\n");
	fprintf(stderr,"       -n <shots>  :  stream 'shots' fragments\n");
	fprintf(stderr,"                      (defaults to 10).\n");
	fprintf(stderr,"       -p <period> :  trigger a fragment every <period> ms\n");
	fprintf(stderr,"                      (defaults to 1000).\n");
	fprintf(stderr,"       -R          :  use RSSI (SRP)\n");
	fprintf(stderr,"       -r          :  use RSSI (stream)\n");
	fprintf(stderr,"       -D <tdest>  :  use tdest demuxer (SRP)\n");
	fprintf(stderr,"       -d <tdest>  :  use tdest demuxer (stream)\n");
	fprintf(stderr,"       -M <tdest>  :  use tdest demuxer (AXI4 memory)\n");
	fprintf(stderr,"       -T <us>     :  set SRP timeout\n");
	fprintf(stderr,"       -h          :  print this message\n\n\n");
	fprintf(stderr,"Base addresses of AxiVersion, Sysmon and PRBS\n");
	fprintf(stderr,"may be changed by defining VERS_BASE, SYSM_BASE\n");
	fprintf(stderr,"and PRBS_BASE env-vars, respectively\n");
}

static void dmp(YAML::Node n)
{
YAML::Emitter e;
	e<<n;
	std::cout << e.c_str() << "\n";
}

class MyFixup : public IYamlFixup {
public:
	virtual void operator()(YAML::Node &root, YAML::Node &top)
	{
        YAML::Node axiVersion = IYamlFixup::findByName(root,"children/mmio/children/AxiVersion/children");
		YAML::Node off        = IYamlFixup::findByName(axiVersion,"ScratchPad/at/offset");
#if 1
		if ( !! axiVersion ) {
			printf("AXI FOunc\n");
		}
		if ( !! off ) {
			dmp(off);			
			printf("OFF FOunc\n");
		}
        YAML::Node bits;
			bits["class"]    = "IntField";
			bits["lsBit"]    = "4";
			bits["sizeBits"] = "22";

		// retrieve offset
        YAML::Node addr(YAML::NodeType::Map);
            addr["offset"]    = off.as<std::string>();

            bits["at"]       = addr;

        axiVersion["Bits"]    = bits;

#endif

		if ( 0) {
		YAML::Emitter e;
			e << root;

		std::cout << e.c_str() << "\n";
		}
		if ( 1) {
		YAML::Emitter e;
			e << axiVersion;

		std::cout << e.c_str() << "\n";
		}
	}
};

int
main(int argc, char **argv)
{
int         rval    = 0;
const char *ip_addr = "192.168.2.10";
bool        use_mem = false;
int        *i_p;
int         vers    = 2;
int         srpTo   = 0;
int         length  = 0;
int         shots   = 10;
int         period  = 1000; // ms
unsigned    port    = 8192;
unsigned    sport   = 8193;
char        cbuf[100];
const char *col1    = NULL;
bool        srpRssi = false;
bool        strRssi = false;
int         tDestSRP  = -1;
int         tDestSTRM = -1;
int         tDestMEM  = -1;
bool        sysmon    = false;
uint32_t    vers_base = 0x00000000;
uint32_t    sysm_base = 0x00010000;
uint32_t    prbs_base = 0x00030000;
unsigned    byteResHack = 0x00000;
const char *str;
bool        useTcp    = false;

const char *use_yaml  = 0;
const char *dmp_yaml  = 0;

	for ( int opt; (opt = getopt(argc, argv, "a:mV:S:hn:p:rRd:D:stT:M:by:Y:")) > 0; ) {
		i_p = 0;
		switch ( opt ) {
			case 'a': ip_addr = optarg;     break;
			case 'm': use_mem = true;       break;
			case 's': sysmon  = true;       break;
			case 'V': i_p     = &vers;      break;
			case 'S': i_p     = &length;    break;
			case 'n': i_p     = &shots;     break;
			case 'p': i_p     = &period;    break;
			case 'h': usage(argv[0]);      
				return 0;
			case 'r': strRssi = true;       break;
			case 'R': srpRssi = true;       break;
			case 'd': i_p     = &tDestSTRM; break;
			case 'D': i_p     = &tDestSRP;  break;
			case 'M': i_p     = &tDestMEM;  break;
			case 't': useTcp  = true;       break;
			case 'T': i_p     = &srpTo;     break;
			case 'b': byteResHack = 0x10000;break;
			case 'Y': use_yaml    = optarg; break;
			case 'y': dmp_yaml    = optarg; break;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				usage(argv[0]);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr,"Unable to scan argument to option '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( (str = getenv("VERS_BASE")) && 1 != sscanf(str,"%" SCNi32 , &vers_base) ) {
		fprintf(stderr,"Unable to scan VERS_BASE envvar\n");
		throw TestFailed();
	}
	if ( (str = getenv("SYSM_BASE")) && 1 != sscanf(str,"%" SCNi32 , &sysm_base) ) {
		fprintf(stderr,"Unable to scan SYSM_BASE envvar\n");
		throw TestFailed();
	}
	if ( (str = getenv("PRBS_BASE")) && 1 != sscanf(str,"%" SCNi32 , &prbs_base) ) {
		fprintf(stderr,"Unable to scan PRBS_BASE envvar\n");
		throw TestFailed();
	}
	

	if ( vers != 1 && vers != 2 && vers != 3 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1..3\n", vers);
		throw TestFailed();
	}

#if 0
	if ( length > 0 && (port == sport) ) {
		if ( tDestSRP < 0 || tDestSTRM < 0 ) {
			fprintf(stderr,"When running STREAM and SRP on the same port both must use (different) TDEST (-d/-D)\n");
			throw TestFailed();
		}
	}
#endif

	if ( (col1 = strchr(ip_addr,':')) ) {
		unsigned len = col1 - ip_addr;
		if ( len >= sizeof(cbuf) ) {
			fprintf(stderr,"IP-address string too long\n");
			throw TestFailed();
		}
		strncpy(cbuf, ip_addr, len);
		cbuf[len]=0;
		if ( strchr(col1+1,':') ) {
			if ( 2 != sscanf(col1+1,"%d:%d", &port, &sport) ) {
				fprintf(stderr,"Unable to scan ip-address (+ 2 ports)\n");
				throw TestFailed();
			}
		} else {
			if ( 1 != sscanf(col1+1,"%d", &port) ) {
				fprintf(stderr,"Unable to scan ip-address (+ 1 port)\n");
				throw TestFailed();
			}
		}
		ip_addr = cbuf;
	}

try {

Hub         root;
std::vector<uint8_t> str;
int16_t     adcv[ADCL];
uint64_t    u64;
uint32_t    u32;
uint16_t    u16;
MyFixup     fix;

	if ( use_mem )
		length = 0;

	if ( use_yaml ) {
		root = IPath::loadYamlFile( use_yaml, "root", NULL, &fix )->origin();
	} else {
		NetIODev  comm = INetIODev::create("fpga", ip_addr);
		MMIODev   mmio = IMMIODev::create ("mmio",0x10000000);
		AXIVers   axiv = IAXIVers::create ("AxiVersion");
		MMIODev   sysm = IMMIODev::create ("sysm",    0x1000, LE);
		MMIODev   axi4 = IMMIODev::create ("axi4",   0x10000, LE);
		MemDev    rmem = IMemDev::create  ("rmem",  0x100000);
		PRBS      prbs = IPRBS::create    ("SsiPrbsTx");

		if ( use_mem )
			root = rmem;
		else
			root = comm;

		rmem->addAtAddress( mmio );

		uint8_t *buf = rmem->getBufp();
		for (int i=16; i<24; i++ )
			buf[i]=i-16;


		sysm->addAtAddress( IIntField::create("adcs", 16, true, 0), 0x400, ADCL, 4 );

		mmio->addAtAddress( axiv, vers_base );
		mmio->addAtAddress( sysm, sysm_base );
		mmio->addAtAddress( prbs, prbs_base );

		{
			ProtoStackBuilder bldr = IProtoStackBuilder::create();
			IProtoStackBuilder::SRPProtoVersion protoVers;
			switch ( vers ) {
				default: throw TestFailed();
				case 1: protoVers = IProtoStackBuilder::SRP_UDP_V1; break;
				case 2: protoVers = IProtoStackBuilder::SRP_UDP_V2; break;
				case 3: protoVers = IProtoStackBuilder::SRP_UDP_V3; break;
			}
			bldr->setSRPVersion              (             protoVers );
			if ( useTcp )
				bldr->setTcpPort             (                  port );
			else
				bldr->setUdpPort             (                  port );
			if ( srpTo > 0 ) {
				u64 = srpTo;
			} else {
				u64 = length;
				u64/= 10; /* MB/s */
				if ( u64 < 90000 )
					u64 = 90000;
			}
			bldr->setSRPTimeoutUS            (                   u64 );
			bldr->setSRPRetryCount           (                     5 );
			bldr->setSRPMuxVirtualChannel    (                     0 );
			bldr->useRssi                    (   ! useTcp && srpRssi );
			if ( tDestSRP >= 0 ) {
				bldr->setTDestMuxTDEST       (              tDestSRP );
			}

			comm->addAtAddress( mmio, bldr );

			if ( vers >= 3 && tDestMEM >= 0 ) {
				bldr->setTDestMuxTDEST       (              tDestMEM );
				bldr->setSRPRetryCount       (       byteResHack | 4 ); // enable byte-resolution access
				axi4->addAtAddress( IIntField::create("bram", 8, false, 0), 0x00000000, 0x10000 );
				comm->addAtAddress( axi4, bldr );
			}

		}

		if ( length > 0 ) {
			ProtoStackBuilder bldr = IProtoStackBuilder::create();
			bldr->setSRPVersion          ( IProtoStackBuilder::SRP_UDP_NONE );
			if ( useTcp ) {
				bldr->setTcpPort         ( sport                            );
				bldr->setTcpOutQueueDepth(                               32 );
			} else {
				bldr->setUdpPort         ( sport                            );
				bldr->setUdpOutQueueDepth(                               32 );
				bldr->setUdpNumRxThreads (                                2 );
			}
			bldr->setDepackOutQueueDepth (                               16 );
			bldr->setDepackLdFrameWinSize(                                4 );
			bldr->setDepackLdFragWinSize (                                4 );
			bldr->useRssi                (              ! useTcp && strRssi );
			if ( tDestSTRM >= 0 )
				bldr->setTDestMuxTDEST   (                        tDestSTRM );
			comm->addAtAddress( IField::create("dataSource"), bldr );
		}
	}

	if ( dmp_yaml ) {
		IYamlSupport::dumpYamlFile( root, dmp_yaml, "root" );
	}

	// can use raw memory for testing instead of UDP
	Path pre = IPath::create( root );

	ScalVal_RO fpgaVers = IScalVal_RO::create( pre->findByName("mmio/AxiVersion/FpgaVersion") );

	ScalVal_RO bldStamp = IScalVal_RO::create( pre->findByName("mmio/AxiVersion/BuildStamp") );
	ScalVal_RO fdSerial = IScalVal_RO::create( pre->findByName("mmio/AxiVersion/FdSerial") );
	ScalVal_RO dnaValue = IScalVal_RO::create( pre->findByName("mmio/AxiVersion/DeviceDna") );
	ScalVal_RO counter  = IScalVal_RO::create( pre->findByName("mmio/AxiVersion/UpTimeCnt" ) );
	ScalVal  scratchPad = IScalVal::create   ( pre->findByName("mmio/AxiVersion/ScratchPad") );
	ScalVal        bits = IScalVal::create   ( pre->findByName("mmio/AxiVersion/Bits") );

	vector<ScalVal_RO> vals;

	ScalVal    axiEn        = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/SsiPrbsTx/AxiEn") ));
	ScalVal    trig         = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/SsiPrbsTx/TxEn") ));
	ScalVal_RO busy         = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/SsiPrbsTx/Busy") ));
	ScalVal_RO overflow     = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/SsiPrbsTx/Overflow") ));
	ScalVal    oneShot      = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/SsiPrbsTx/OneShot") ));
	ScalVal    packetLength = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/SsiPrbsTx/PacketLength") ));
	if ( tDestSTRM < 0 ) {
	// tDest from this register is unused if there is a tdest demuxer in FW
	ScalVal    tDest        = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/SsiPrbsTx/tDest") ));
	}
	ScalVal    tId          = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/SsiPrbsTx/tId") ));
	ScalVal_RO dataCnt      = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/SsiPrbsTx/DataCount") ));
	ScalVal_RO eventCnt     = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/SsiPrbsTx/EventCount") ));
	ScalVal_RO randomData   = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/SsiPrbsTx/RandomData") ));


	ScalVal_RO adcs;

	if ( sysmon )
		adcs = IScalVal_RO::create( pre->findByName("mmio/sysm/adcs") );

	printf("AxiVersion:\n");
	fpgaVers->getVal( &u64 );
	printf("FpgaVersion: %" PRIu64 "\n", u64);
	unsigned nelms = bldStamp->getNelms();
	str.reserve( nelms );
	bldStamp->getVal( &str[0], nelms );
	printf("Build String:\n%s\n", &str[0]);

	fdSerial->getVal( &u64, 1 );
	printf("Serial #: 0x%" PRIx64 "\n", u64);
	dnaValue->getVal( &u64, 1 );
	printf("DNA    #: 0x%" PRIx64 "\n", u64);
	counter->getVal( &u32, 1 );
	printf("Counter : 0x%" PRIx32 "\n", u32);
	counter->getVal( &u32, 1 );
	printf("Counter : 0x%" PRIx32 "\n", u32);

	u16=0x6765;
	u32=0xffffffff;
	scratchPad->setVal( &u32, 1 );
	bits->setVal( &u16, 1 );
	scratchPad->getVal( &u32, 1 );
	printf("ScratchPad: 0x%" PRIx32 "\n", u32);

	if ( u32 == 0xfc06765f ) {
		printf("Readback of merged bits (expected 0xfc06765f) PASSED\n");
	} else {
		printf("Readback of merged bits (expected 0xfc06765f) FAILED\n");
		throw TestFailed();	
	}

	if ( sysmon ) {
		adcs->getVal( (uint16_t*)adcv, sizeof(adcv)/sizeof(adcv[0]) );
		printf("\n\nADC Values:\n");
		for ( int i=0; i<ADCL; i++ ) {
			printf("  %6hd\n", adcv[i]);
		}
	}

	if ( length > 0 ) {
		uint32_t v;
		v = 1;
		axiEn->setVal( &v, 1 );
		v = length;
		packetLength->setVal( &v, 1 );

		trig->setVal( (uint64_t)0 );
		oneShot->setVal( (uint64_t)0 );

		printf("PRBS Regs:\n");
		for (unsigned i=0; i<vals.size(); i++ ) {
			vals[i]->getVal( &v, 1 );
			printf("%14s: %d\n", vals[i]->getName(), v );
		}

		pthread_t tid;
		void     *ign;
		struct timespec p, dly;
		ThreadArg arg( (uint64_t)4*(uint64_t)length, shots, period == 0 ? trig : ScalVal(), root );
		if ( pthread_create( &tid, 0, rxThread, &arg ) ) {
			perror("pthread_create");
		}
		if ( period > 0 ) {
			p.tv_sec  = period/1000;
			p.tv_nsec = (period % 1000) * 1000000;
			// wait for the thread to come up (hack!)
			dly = p;
			nanosleep( &dly, 0 );
			for (int i = 0; i<shots; i++) {
				dly = p;
				clock_gettime( CLOCK_MONOTONIC, &arg.trigTime.tv_ );
				oneShot->setVal( 1 );
				nanosleep( &dly, 0 );
				// not truly thread safe but
				// a correct algorithm would
				// be much more complex.
				if ( arg.firstFrame() < 0 ) {
					//not yet synchronized
					if ( i > SYNC_LIMIT ) {
						fprintf(stderr,"Stream unable to synchronize: FAILED\n");
						rval = 1;
						pthread_cancel( tid );
						break;
					}
					shots++;
				}
			}
		} else {
			// if we assume a (conservative) data rate of 40MB/s
			// and give 10 attempts to synchronize then this translates
			// to 10*length/(40B/us) = 10 * length/B * 1000/40 ns
			uint64_t p_ns = (uint64_t)length * (uint64_t) 25;
			p.tv_sec  = p_ns/1000000000;
			p.tv_nsec = p_ns%1000000000;
			int lfrm  = -1;
			int     i = 0;
			int  frm;

			clock_gettime( CLOCK_MONOTONIC, &arg.trigTime.tv_ );
			trig->setVal( 1 );

			while ( arg.nFrames() < shots ) {
				dly = p;
				nanosleep( &dly, 0 );
				frm  = arg.firstFrame();
				if ( 0 == (frm - lfrm) ) {
					if ( ++i > SYNC_LIMIT ) {
						fprintf(stderr,"Stream unable to synchronize: FAILED\n");
						rval = 1;
						pthread_cancel( tid );
						break;
					}
				} else {
					lfrm = frm;
					i    = 0;
				}
			}
			trig->setVal( (uint64_t)0 );
		}
		pthread_join( tid, &ign );
		printf("%d shots were fired; %d frames received\n", shots, arg.nFrames());
		printf("(Difference to requested shots are due to synchronization)\n");
	}

	// try to get better statistics for the average timeout
	for (int i=0; i<1000; i++ )
		counter->getVal( &u32, 1 );


	root->findByName("mmio")->tail()->dump(stdout);

	if ( tDestMEM >= 0 ) {
		ScalVal bram( IScalVal::create( root->findByName("axi4/bram") ) );
		testBram( bram );
	}

} catch (CPSWError &e) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}

	return rval;
}
