#include <cpsw_api_builder.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <getopt.h>

#include <udpsrv_regdefs.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

using boost::mutex;
using boost::lock_guard;

class TestFailed {};

class M {

private:
	unsigned nelms_;
public:

static mutex mtx_;

	typedef uint32_t   ELT;
#define xFMT        "%"PRIx32

	ELT *mem_[3];

	M(unsigned nelms)
	: nelms_(nelms)
	{
	unsigned i;
		for ( i=0; i<sizeof(mem_)/sizeof(mem_[0]); i++ )
			if ( ! (mem_[i] = new ELT[nelms] )) {
				while ( i > 0 ) {
					--i;
					delete [] mem_[i];
				}
				fprintf(stderr,"No Memory\n");
				throw TestFailed();
			}
	}

	unsigned getNelms()
	{
		return nelms_;
	}

	void fill()
	{
	unsigned i,j;
		lock_guard<mutex> GUARD( mtx_ );
		for ( i=0; i<sizeof(mem_)/sizeof(mem_[0]); i++ ) {
			for (j=0; j<getNelms(); j++ )
				mem_[i][j]=mrand48();
		}
	}

	~M()
	{
	unsigned i;
		for ( i=0; i<sizeof(mem_)/sizeof(mem_[0]); i++ )
			delete [] mem_[i];
	}
};

mutex M::mtx_;

static void* test_thread(void* arg)
{
char nm[100];
int loops = 10;
intptr_t vc_idx = reinterpret_cast<intptr_t>(arg);

	sprintf(nm, "comm/mmio_vc_%"PRIdPTR"/val", vc_idx);
	printf("starting test thread %s\n", nm);

	try {

		Path p( IDev::getRootDev()->findByName(nm) );

		ScalVal v = IScalVal::create( p );

		M myData( v->getNelms() );
		myData.fill();


		while ( loops-- > 0 ) {
			try {
				v->setVal( myData.mem_[loops&1], myData.getNelms() );
				v->getVal( myData.mem_[2],       myData.getNelms() );
			} catch (IOError &e) {
				fprintf(stderr,"IO Error in thread %s; is 'udpsrv' running?\n", nm);
				Path comm_addr( p->clone() );
				comm_addr->up();
				comm_addr->tail()->dump();
				return (void*)-1;
			}
			if ( memcmp( myData.mem_[loops&1], myData.mem_[2], myData.getNelms() * sizeof(M::ELT) ) ) {

				lock_guard<mutex> GUARD(M::mtx_);

				fprintf(stderr,"Memory comparison mismatch (%s @loop %d)\n", nm, loops);
				for ( unsigned i=0; i<myData.getNelms(); i++ ) {
					M::ELT got, exp;
					if ( (exp = myData.mem_[loops&1][i]) != (got = myData.mem_[2][i]) ) {
						printf("@[%d == addr 0x%"PRIxPTR"]: got ", i, REGBASE + REG_ARR_OFF + 4*i + (vc_idx - 1)*sizeof(M::ELT));
						printf(xFMT, got);
						printf(", expected ");
						printf(xFMT, exp);
						printf("\n");
						break;
					}
				}
				return (void*)-1;
			}
		}

	} catch (CPSWError &e) {
		fprintf(stderr,"CPSW Error in thread %s: %s\n", nm, e.getInfo().c_str());
		return (void*)-1;
	}

	printf("leaving test thread %s OK\n", nm);
	return (void*)0;
}

int
main(int argc, char **argv)
{
INoSsiDev::ProtocolVersion vers = INoSsiDev::SRP_UDP_V2;
int port = INoSsiDev::SRP_UDP_V2 == vers ? 8192 : 8191;
int vc1  = 81;
int vc2  = 17;
bool have_even = false;
bool have_odd  = false;
bool do_even   = true;
bool do_odd    = true;

	try {
		NoSsiDev comm = INoSsiDev::create("comm", 0);
		MMIODev  mmio_vc_1 = IMMIODev::create("mmio_vc_1",0x10000,BE);
		MMIODev  mmio_vc_2 = IMMIODev::create("mmio_vc_2",0x10000,BE);

		comm->addAtAddress( mmio_vc_1, vers, port, 100000, 5, vc1 );
		comm->addAtAddress( mmio_vc_2, vers, port, 100000, 5, vc2 );

		IDev::getRootDev()->addAtAddress( comm );

		IntField f = IIntField::create("val", 8*sizeof(M::ELT), false, 0);

		mmio_vc_1->addAtAddress( f, REGBASE + REG_ARR_OFF + 0,              REG_ARR_SZ/2, 2*sizeof(M::ELT) );
		mmio_vc_2->addAtAddress( f, REGBASE + REG_ARR_OFF + sizeof(M::ELT), REG_ARR_SZ/2, 2*sizeof(M::ELT) );

		pthread_t even_tid;
		pthread_t odd_tid;

		if ( do_even ) {
			if ( pthread_create( &even_tid, 0, test_thread, (void*)1 ) ) {
				perror("pthread_create failed");
				throw TestFailed();
			}
			have_even = true;
		}

		if ( do_odd ) {
			if ( pthread_create( &odd_tid, 0, test_thread, (void*)2 ) ) {
				perror("pthread_create failed");
				throw TestFailed();
			}
			have_odd = true;
		}

		void *stat;

		if ( have_even ) {
			if ( pthread_join( even_tid, &stat ) ) {
				perror("pthread_join (even)");
				throw TestFailed();
			}
			if ( stat ) {
				fprintf(stderr,"Even-tester returned failure status\n");
				throw TestFailed();
			}
		}

		if ( have_odd ) {
			if ( pthread_join( odd_tid, &stat ) ) {
				perror("pthread_join (even)");
				throw TestFailed();
			}
			if ( stat ) {
				fprintf(stderr,"Even-tester returned failure status\n");
				throw TestFailed();
			}
		}

	} catch (CPSWError &e) {
		fprintf(stderr,"CPSW Error: %s\n", e.getInfo().c_str());
		throw;
	} catch (TestFailed) {
		printf("SRPMUX TEST FAILED\n");
		if ( have_even ) {
		}
		throw;
	}
	printf("SRPMUX TEST PASSED\n");
	return 0;
}
