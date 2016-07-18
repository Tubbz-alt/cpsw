#include <udpsrv_util.h>
#include <udpsrv_port.h>
#include <udpsrv_rssi_port.h>

struct UdpPrt_ {
	UdpPort   udp_;
	ProtoPort top_;
	CMtx      mtx_;
	UdpPrt_(const char *mtxnm):mtx_(mtxnm){}
};

UdpPrt udpPrtCreate(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler, int withRssi)
{
ProtoPort  prt;
UdpPrt     p = new UdpPrt_("udp_port");

	p->udp_ = IUdpPort::create(ina, port, simLossPercent, ldScrambler);
	if ( withRssi ) {
		p->top_ = CRssiPort::create( true );
		p->top_->attach( p->udp_ );
	} else {
		p->top_ = p->udp_;
	}
	for ( prt = p->top_; prt; prt = prt->getUpstreamPort() )
		prt->start();

	return p;
}

void udpPrtDestroy(UdpPrt p)
{
ProtoPort  prt;

	if ( ! p )
		return;

	for ( prt = p->top_; prt; prt = prt->getUpstreamPort() )
		prt->stop();

	p->top_.reset();
	p->udp_.reset();
	delete p;
}

int xtrct(BufChain bc, void *buf, unsigned size)
{
unsigned bufsz = bc->getSize();

	if ( size > bufsz )
		size = bufsz;

	bc->extract(buf, 0, size);

	return size;
}

int udpPrtRecv(UdpPrt p, void *buf, unsigned size)
{
CMtx::lg( &p->mtx_ );

BufChain bc = p->top_->pop( NULL );
	if ( ! bc )
		return -1;

	return xtrct(bc, buf, size);
}

int udpPrtIsConn(UdpPrt p)
{
CMtx::lg( &p->mtx_ );
	return p->udp_->isConnected();
}

static BufChain fill(void *buf, unsigned size)
{
BufChain bc = IBufChain::create();
Buf      b  = bc->createAtHead( IBuf::CAPA_ETH_BIG );

	bc->insert(buf, 0, size);

	return bc;
}

int udpPrtSend(UdpPrt p, void *buf, unsigned size)
{
CMtx::lg( &p->mtx_ );

BufChain bc = fill(buf, size);

	return p->top_->push(bc, NULL) ? size : -1;
}

struct UdpQue_ {
	BufQueue q_;
};

UdpQue udpQueCreate(unsigned depth)
{
UdpQue rval = new UdpQue_();
	rval->q_ = IBufQueue::create(depth);
	return rval;
}

void   udpQueDestroy(UdpQue q)
{
	if ( q ) {
		q->q_.reset();
		delete(q);
	}
}

int udpQueTryRecv(UdpQue q, void *buf, unsigned size)
{
BufChain bc = q->q_->tryPop();
	if ( ! bc )
		return -1;

	return xtrct(bc, buf, size);
}

int udpQueRecv(UdpQue q, void *buf, unsigned size, struct timespec *abs_timeout)
{
CTimeout to( *abs_timeout );

BufChain bc = q->q_->pop( to.isIndefinite() ? NULL : &to );

	if ( ! bc )
		return -1;

	return xtrct(bc, buf, size);
}

int udpQueTrySend(UdpQue q, void *buf, unsigned size)
{
BufChain bc = fill(buf, size);
	return q->q_->tryPush( bc ) ? size : -1;
}
