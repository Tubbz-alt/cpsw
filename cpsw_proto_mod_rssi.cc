#include <cpsw_proto_mod_rssi.h>

BufChain
CProtoModRssi::pop(const CTimeout *timeout, bool abs_timeout)
{
	if ( ! timeout || timeout->isIndefinite() ) {
		return outQ_->pop( NULL );
	} else if ( timeout->isNone() ) {
		return outQ_->tryPop();
	} else if ( abs_timeout ) {
		return outQ_->pop( timeout );
	} else {
		CTimeout abst( outQ_->getAbsTimeoutPop( timeout ) );
		return outQ_->pop( &abst );
	}	
}

bool
CProtoModRssi::push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
{
	if ( ! timeout || timeout->isIndefinite() ) {
		return inpQ_->push( bc, NULL );
	} else if ( timeout->isNone() ) {
		return inpQ_->tryPush( bc );
	} else if ( abs_timeout ) {
		return inpQ_->push( bc, timeout );
	} else {
		CTimeout abst( inpQ_->getAbsTimeoutPush( timeout ) );
		return inpQ_->pop( &abst );
	}	
}

bool
CProtoModRssi::pushDown(BufChain bc, const CTimeout *rel_timeout)
{
	if ( ! rel_timeout || rel_timeout->isIndefinite() ) {
		return outQ_->push( bc, NULL );
	} else if ( rel_timeout->isNone() ) {
		return outQ_->tryPush( bc );
	} else {
		CTimeout abst( outQ_->getAbsTimeoutPush( rel_timeout ) );
		return outQ_->push( bc, &abst );
	}	
}

BufChain
CProtoModRssi::tryPop()
{
	return outQ_->tryPop();
}

bool
CProtoModRssi::tryPush(BufChain bc)
{
	return inpQ_->tryPush( bc );
}

ProtoMod
CProtoModRssi::getProtoMod()
{
	return getSelfAs<ProtoModRssi>();
}

ProtoMod
CProtoModRssi::getUpstreamProtoMod()
{
	return upstream_ ? upstream_->getProtoMod() : ProtoMod();
}

ProtoPort
CProtoModRssi::getUpstreamPort()
{
	return upstream_;
}

IEventSource *
CProtoModRssi::getReadEventSource()
{
	return outQ_->getReadEventSource();
}
	
CTimeout
CProtoModRssi::getAbsTimeoutPop(const CTimeout *rel_timeout)
{
	return outQ_->getAbsTimeoutPop( rel_timeout );
}

CTimeout
CProtoModRssi::getAbsTimeoutPush(const CTimeout *rel_timeout)
{
	return inpQ_->getAbsTimeoutPush( rel_timeout );
}

int
CProtoModRssi::match(ProtoPortMatchParams *cmp)
{
ProtoPort up( getUpstreamPort() );
	return up ? up->match( cmp ) : 0;
}

void
CProtoModRssi::attach(ProtoPort upstream)
{
IEventSource *src = upstream->getReadEventSource();
	upstream_ = upstream;
	if ( ! src )
		printf("Upstream: %s has no event source\n", upstream->getProtoMod()->getName());
	CRssi::attach( upstream->getReadEventSource() );
}

void
CProtoModRssi::addAtPort(ProtoMod downstreamMod)
{
	downstreamMod->attach( getSelfAs<ProtoModRssi>() );
}

void
CProtoModRssi::dumpInfo(FILE *f)
{
	CRssi::dumpStats( f );
}

const char *
CProtoModRssi::getName() const
{
	return "RSSI";
}

void
CProtoModRssi::modStartup()
{
	threadStart();
}

BufChain
CProtoModRssi::tryPopUpstream()
{
	return getUpstreamPort()->tryPop();
}

CProtoModRssi::~CProtoModRssi()
{
}

bool
CProtoModRssi::tryPushUpstream(BufChain bc)
{
	return getUpstreamPort()->tryPush( bc );
}


void
CProtoModRssi::modShutdown()
{
	threadStop();
}

ProtoModRssi
CProtoModRssi::create()
{
	return CShObj::create<ProtoModRssi>();
}