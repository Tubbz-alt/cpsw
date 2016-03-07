#include <cpsw_api_builder.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <ctype.h>

#include <cpsw_obj_cnt.h>

#include <stdint.h>

static DECLARE_OBJ_COUNTER( ocnt, "EntryImpl", 1 ) // root device

CEntryImpl::CEntryImpl(Key &k, const char *name, uint64_t size)
: CShObj(k),
  name_( name ),
  size_( size),
  cacheable_( UNKNOWN_CACHEABLE ),
  locked_( false )
{
const char *cptr;
	for ( cptr = name_.c_str(); *cptr; cptr++ ) {
		if ( ! isalnum( *cptr )
                     && '_' != *cptr 
                     && '-' != *cptr  )
					throw InvalidIdentError(name_);
	}
	++ocnt();
}

CEntryImpl::CEntryImpl(CEntryImpl &ei, Key &k)
: CShObj(ei,k),
  name_(ei.name_),
  description_(ei.description_),
  size_(ei.size_),
  cacheable_(ei.cacheable_),
  locked_( false )
{
	++ocnt();
}

CEntryImpl::~CEntryImpl()
{
	--ocnt();
}

void CEntryImpl::setDescription(const char *desc)
{
	description_ = desc;
}

void CEntryImpl::setDescription(const std::string &desc)
{
	description_ = desc;
}


void CEntryImpl::setCacheable(Cacheable cacheable)
{
	if ( UNKNOWN_CACHEABLE != getCacheable() && locked_ ) {
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->cacheable_   = cacheable;
}

void CEntryImpl::accept(IVisitor *v, RecursionOrder order, int depth)
{
	v->visit( getSelf() );
}

Field IField::create(const char *name, uint64_t size)
{
	return CShObj::create<EntryImpl>(name, size);
}


