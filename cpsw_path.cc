#include <cpsw_path.h>
#include <cpsw_hub.h>
#include <cpsw_obj_cnt.h>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <iostream>

#undef PATH_DEBUG

using boost::dynamic_pointer_cast;
using boost::static_pointer_cast;
using std::cout;

typedef shared_ptr<const DevImpl::element_type> ConstDevImpl;

DevImpl theRootDev( CShObj::create<DevImpl>("ROOT") );

DECLARE_OBJ_COUNTER( ocnt, "Path", 0 )

Dev IDev::getRootDev()
{
	return theRootDev;
}

class PathImpl : public PathEntryContainer, public IPathImpl {
private:
	ConstDevImpl originDev_;

public:
	PathImpl();
	PathImpl(Hub);
	PathImpl(DevImpl);

	PathImpl(const PathImpl&);

	virtual void clear();
	virtual void clear(Hub);
	virtual void clear(DevImpl);

	virtual void dump(FILE *f) const;

	virtual std::string toString() const;

	virtual int size() const;

	virtual bool empty() const;

	PathImpl::iterator begin();
	PathImpl::const_iterator begin() const;

	virtual Path findByName(const char *name) const;
	virtual Path clone()                      const;

	static bool hasParent(PathImpl::const_reverse_iterator &i);
	static bool hasParent(PathImpl::reverse_iterator &i);

	virtual Child up() 
	{
	Child rval = NULLCHILD;
		if ( ! empty() ) {
			rval = back().c_p_;
			pop_back();
		}
		return rval;
	}

	virtual PathEntry tailAsPathEntry() const
	{
		if ( ! empty() )
			return back();
		
		return PathEntry( NULLADDR, 0, -1 );
	}

	virtual Child tail() const
	{
		PathEntry pe = tailAsPathEntry();
		return pe.c_p_;
	}

	virtual unsigned    getNelms() const
	{
		return tailAsPathEntry().nelmsLeft_;
	}

	virtual bool verifyAtTail(Path p);
	virtual bool verifyAtTail(ConstDevImpl c);

	virtual void append(Path p);
	virtual void append(Address);
	virtual void append(Address, int, int);

	virtual Path concat(Path p) const;

	virtual ConstDevImpl parentAsDevImpl() const;

	virtual Hub parent() const;

	virtual	Hub origin() const
	{
		return originDev_;
	}

	virtual ConstDevImpl originAsDevImpl() const
	{
		return originDev_;
	}

	virtual ~PathImpl();
};

PathEntry::PathEntry(Address a, int idxf, int idxt, unsigned nelmsLeft)
: c_p_(a), idxf_(idxf), idxt_(idxt), nelmsLeft_( nelmsLeft )
{
	if ( idxf < 0 )
		idxf = 0;
	if ( a ) {
		int n = a->getNelms() - 1;
		if ( idxf > n )
			idxf = n;
		if ( idxt < 0 || idxt > n )
			idxt = n;
		if ( idxt < idxf )
			idxt = idxf;
		this->nelmsLeft_ *= idxt - idxf + 1;
	} else {
		idxt = -1;
	}
	idxf_ = idxf;
	idxt_ = idxt;
}

void PathImpl::dump(FILE *f) const
{
std::string s = toString();
	fprintf(f, "%s", s.c_str());
}

static PathImpl *_toPathImpl(Path p)
{
	return static_cast<PathImpl*>( p.get() );
}

IPathImpl * IPathImpl::toPathImpl(Path p)
{
	return _toPathImpl(p);
}

PathImpl::PathImpl()
: PathEntryContainer(), originDev_(theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLADDR) );
	++ocnt();
}

PathImpl::~PathImpl()
{
	--ocnt();
}

PathImpl::PathImpl(const PathImpl &in)
: PathEntryContainer(in),
  originDev_(in.originDev_)
{
	++ocnt();
}

PathImpl::PathImpl(Hub h)
: PathEntryContainer()
{
ConstDevImpl c = dynamic_pointer_cast<ConstDevImpl::element_type, Hub::element_type>(h);

	if ( h && ! c )
		throw InternalError("ConstDevImpl not a Hub???");
	originDev_ = c ? c : theRootDev;

	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLADDR) );
	++ocnt();
}

PathImpl::PathImpl(DevImpl c)
: PathEntryContainer(),
  originDev_(c ? c : theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLADDR) );
	++ocnt();
}


int PathImpl::size() const
{
	// not counting the marker element
	return PathEntryContainer::size() - 1;
}

bool PathImpl::empty() const
{
	return size() <= 0;
}

PathImpl::iterator PathImpl::begin()
{
	PathImpl::iterator i = PathEntryContainer::begin();
	return ++i;
}

PathImpl::const_iterator PathImpl::begin() const
{
	PathImpl::const_iterator i = PathEntryContainer::begin();
	return ++i;
}

void PathImpl::clear(Hub h)
{
ConstDevImpl c = dynamic_pointer_cast< ConstDevImpl::element_type, Hub::element_type> ( h );

	if ( ! c && h )
		throw InternalError("Hub is not a DevImpl???");

	clear( c );
}

void PathImpl::clear(DevImpl c)
{
	PathEntryContainer::clear();
	push_back( PathEntry(NULLADDR) );

	originDev_ = c ? c : theRootDev;
}


void PathImpl::clear()
{
	clear( theRootDev );
}

bool PathImpl::hasParent(PathImpl::const_reverse_iterator &i)
{
	return i->c_p_ != NULL;
}

bool PathImpl::hasParent(PathImpl::reverse_iterator &i)
{
	return i->c_p_ != NULL;
}

static void appendNum(std::string *s, int i)
{
div_t d;
	if ( i < 0 ) {
		s->push_back('-');
		i = -i;
	}
	std::string::iterator here = s->end();
	do { 
		d = div(i, 10);
		s->insert(here, d.rem + '0');
		i = d.quot;
	} while ( i );
}

std::string PathImpl::toString() const
{
std::string rval;
	for ( PathImpl::const_iterator it=begin(); it != end(); ++it ) {
		rval.push_back('/');
		rval.append(it->c_p_->getName());
		if ( it->c_p_->getNelms() > 1 ) {
			rval.push_back('[');
			appendNum( &rval, it->idxf_ );
			if ( it->idxt_ != it->idxf_ ) {
				rval.push_back('-');
				appendNum( &rval, it->idxt_ );
			}
			rval.push_back(']');
		}
	}
	return rval;
}

Path IPath::create()
{
	return Path( make_shared<PathImpl>() );
}

Path IPath::create(const char *key)
{
	return Path( make_shared<PathImpl>() )->findByName( key );
}


Path IPath::create(Hub h)
{
	return Path( make_shared<PathImpl>( h ) );
}


// scan a decimal number;
// ASSUMPTIONS: from < to on entry
static inline int getnum(const char *from, const char *to)
{
unsigned long rval;
char         *endp;

	rval = strtoul(from, &endp, 0);

	if ( endp != to || rval > (unsigned long)INT_MAX )
		throw InvalidPathError( from );

	return (int)rval;
}

Path PathImpl::findByName(const char *s) const
{
Address      found;
ConstDevImpl h;
Path         rval = make_shared<PathImpl>( *this );
PathImpl    *p    = _toPathImpl( rval );
const char  *sl;

shared_ptr<const EntryImpl::element_type> tailp;

#ifdef PATH_DEBUG
cout<<"checking: "<< s <<"\n";
#endif

	if ( empty() ) {
		if ( (tailp = originAsDevImpl()) ) {
#ifdef PATH_DEBUG
cout<<"using origin\n";
#endif
		} else {
			throw InternalError();
		} 
	} else {
		tailp = back().c_p_->getEntryImpl();
#ifdef PATH_DEBUG
cout<<"starting at: "<<found->getName() << "\n";
#endif
	}

	do {

		int      idxf  = -1;
		int      idxt  = -1;

		// find substring
		while ( '/' == *s )
			s++;

		sl = strchr(s,'/');

		// Check for '..'
		if ( s[0] == '.' && s[1] == '.' && ( 0 == s[2] || ( sl && 2 == sl - s ) ) ) {

			/* implies s[0] != 0 , s[1] != 0 */
			if ( p->empty() ) {
				throw InvalidPathError(s);
			}
			p->pop_back();
			found = p->back().c_p_;

		} else {

			const char *op = strchr(s,'[');
			const char *cl = op ? strchr(op,']') : 0;
			const char *mi = op ? strchr(op,'-') : 0;

			if ( ! (h = dynamic_pointer_cast<const DevImpl::element_type>( tailp )) ) {
				throw NotDevError( found->getName() );
			}


			if ( sl ) {
				if ( op && op > sl )
					op = 0;
				if ( cl && cl > sl )
					cl = 0;
				if ( mi && mi > sl )
					mi = 0;
			}

			if ( op ) {
				if (   !cl
						|| (  cl <= op + 1)
						|| (  sl && (sl < cl || sl != cl+1))
						|| ( !sl && *(cl+1) )
						|| (  mi && (mi >= cl-1 || mi <= op + 1) ) 
				   ) {
					throw InvalidPathError( s );
				}

				idxf = getnum( op+1, mi ? mi : cl );
				if ( mi ) {
					idxt = getnum( mi+1, cl );
				}
			}

			std::string key(s, (op ? op - s : ( sl ? sl - s : strlen(s) ) ) );

#ifdef PATH_DEBUG
			cout<<"looking for: " << key << " in: " << h->getName() << "\n";
#endif

			found = h->getAddress( key.c_str() );

			if ( ! found ) {
				throw NotFoundError( key.c_str() );
			}

			if ( idxt < 0 )
				idxt = idxf < 0 ? found->getNelms() - 1 : idxf;
			if ( idxf < 0 )
				idxf = 0;

			if ( idxf > idxt || idxt >= (int)found->getNelms() ) {
				throw InvalidPathError( s );
			}

			p->push_back( PathEntry(found, idxf, idxt, p->getNelms()) );

		}

		tailp = found->getEntryImpl();

	} while ( (s = sl) != NULL );

	return rval;
}

ConstDevImpl PathImpl::parentAsDevImpl() const
{
PathImpl::const_reverse_iterator it = rend();
	++it; // rend points after last el
	++it; // if empty this points at the NULL marker element
	return hasParent( it ) ? boost::static_pointer_cast<const CDevImpl, CEntryImpl>(it->c_p_->getEntryImpl()) : NULLDEVIMPL;
}

Hub PathImpl::parent() const
{
	return parentAsDevImpl();
}

bool PathImpl::verifyAtTail(Path p)
{
PathImpl *pi = _toPathImpl( p );
	return verifyAtTail( pi->originAsDevImpl() );
}

bool PathImpl::verifyAtTail(ConstDevImpl h)
{
	if ( empty() ) {
		originDev_ = h;
		return true;
	} 
	return (    static_pointer_cast<Entry::element_type, ConstDevImpl::element_type>( h )
             == static_pointer_cast<Entry::element_type, EntryImpl::element_type>( back().c_p_->getEntryImpl() ) );
}


static void append2(PathImpl *h, PathImpl *t)
{
	if ( ! h->verifyAtTail( t->originAsDevImpl() ) )
		throw InvalidPathError( t->toString() );

	PathImpl::iterator it = t->begin();

	unsigned nelmsLeft = h->getNelms();
	for ( ;  it != t->end(); ++it ) {
		PathEntry e = *it;
		e.nelmsLeft_ *= nelmsLeft;
		h->push_back( e );
	}
}

Path PathImpl::clone() const
{
	return make_shared<PathImpl>( *this );
}

Path PathImpl::concat(Path p) const
{
Path rval = clone();

	if ( ! p->empty() ) {
		PathImpl *h = _toPathImpl(rval);
		PathImpl *t = _toPathImpl(p);

		append2(h, t);
	}

	return rval;
}

void PathImpl::append(Path p)
{
	if ( ! p->empty() )
		append2(this, _toPathImpl(p));
}

void PathImpl::append(Address a, int f, int t)
{
	if ( ! verifyAtTail( a->getOwnerAsDevImpl() ) ) {
		throw InvalidPathError( this->toString() );
	}
	push_back( PathEntry(a, f, t, getNelms()) );
}

void PathImpl::append(Address a)
{
	append(a, 0, -1);
}


bool CompositePathIterator::validConcatenation(Path p)
{
	if ( atEnd() )
		return true;
	if ( p->empty() )
		return false;
	return (    static_pointer_cast<Entry::element_type, Hub::element_type      >( p->origin() )
             == static_pointer_cast<Entry::element_type, EntryImpl::element_type>( (*this)->c_p_->getEntryImpl() ) );
}

static PathImpl::reverse_iterator rbegin(Path p)
{
	return _toPathImpl(p)->rbegin();
}


void CompositePathIterator::append(Path p)
{
	if ( p->empty() )
		return;
	if ( ! validConcatenation( p ) ) {
		throw InvalidPathError( p->toString() );
	}
	if ( ! atEnd() ) {
		l_.push_back( *this );
		nelmsLeft_ *= (*this)->nelmsLeft_;
	} else {
		if ( nelmsLeft_ != 1 )
			throw InternalError("assertion failed: nelmsLeft should == 1 at this point");
	}
	PathImpl::reverse_iterator::operator=( rbegin(p) );
	at_end_     = false;
	nelmsRight_ = 1;
}

CompositePathIterator::CompositePathIterator(Path *p0, Path *p, ...)
{
	va_list ap;
	va_start(ap, p);
	nelmsLeft_ = 1;
	if ( ! (at_end_ = (*p0)->empty()) ) {
		PathImpl::reverse_iterator::operator=( rbegin(*p0) );
	}
	while ( p ) {
		append( *p );
		p = va_arg(ap, Path*);
	}
	va_end(ap);
	nelmsRight_ = 1;
}

CompositePathIterator::CompositePathIterator(Path *p)
{
	nelmsLeft_  = 1;
	if ( ! (at_end_ = (*p)->empty()) ) {
		PathImpl::reverse_iterator::operator=( rbegin(*p) );
	}
	nelmsRight_ = 1;
}

CompositePathIterator & CompositePathIterator::operator++()
{
	nelmsRight_ *= ((*this)->idxt_ - (*this)->idxf_) + 1;
	PathImpl::reverse_iterator::operator++();
	if ( ! PathImpl::hasParent(*this) ) {
		if ( ! l_.empty() ) {
			PathImpl::reverse_iterator::operator=(l_.back());
			l_.pop_back();
			nelmsLeft_ /= (*this)->nelmsLeft_;
		} else {
			at_end_ = true;
			if ( nelmsLeft_ != 1 ) {
				throw InternalError("assertion failed: nelmsLeft should equal 1");
			}
		}
	}
	return *this;
}

CompositePathIterator CompositePathIterator::operator++(int)
{
	CompositePathIterator tmp = *this;
	operator++();
	return tmp;
}

CompositePathIterator & CompositePathIterator::operator--()
{
	throw InternalError();
}

CompositePathIterator CompositePathIterator::operator--(int)
{
	throw InternalError();
}

void CompositePathIterator::dump(FILE *f) const
{
CompositePathIterator tmp = *this;
	while ( ! tmp.atEnd() ) {
		fprintf(f, "%s[%i-%i]<", tmp->c_p_->getName(), tmp->idxf_, tmp->idxt_);
		++tmp;
	}
	fprintf(f, "\n");
}
