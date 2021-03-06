 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PATH_H
#define CPSW_PATH_H

#include <list>
#include <vector>
#include <cpsw_api_user.h>

#include <cstdarg>

class IAddress;
typedef shared_ptr<IAddress> Address;

class CDevImpl;
typedef shared_ptr<const CDevImpl> ConstDevImpl;

struct PathEntry {
	Address  c_p_;
	shared_ptr<void> address_pvt_; // address may attach context to be used by read/write
	int      idxf_, idxt_;
	unsigned nelmsLeft_;

	PathEntry(Address a, int idxf = 0, int idxt = -1, unsigned nelmsLeft = 1);
};

typedef std::vector<PathEntry>  PathEntryContainer;

// NOTE: all paths supplied to the constructor(s) must remain valid
// and unmodified (at least from the beginning up to the node which
// was at the tail at the time this iterator was created) while an
// iterator is in use!
class CompositePathIterator : public PathEntryContainer::const_reverse_iterator {
	private:
		bool                                  at_end_;
		std::vector<PathEntryContainer::const_reverse_iterator> l_;
		unsigned                              nelmsRight_;
		unsigned                              nelmsLeft_;

	public:
		// construct from a single path
		CompositePathIterator(ConstPath p);

		unsigned getNelmsRight()
		{
			return nelmsRight_;
		}

		unsigned getNelmsLeft()
		{
			return nelmsLeft_ * (atEnd() ? 1 : (*this)->nelmsLeft_);
		}

		bool atEnd()
		{
			return at_end_;
		}

		// can path 'p' be concatenated with this one, i.e.,
		// is the origin of 'p' identical with the element
		// this iterator points to?
		bool validConcatenation(ConstPath p);

		CompositePathIterator & append(ConstPath p);

		CompositePathIterator & operator++();
		CompositePathIterator   operator++(int);

		// -- not implemented; will throw an exception
		CompositePathIterator &operator--();
		CompositePathIterator operator--(int);

		void dump(FILE *f) const;
};

class IPathImpl : public IPath {
public:
	virtual void         append(Address, int f, int t) = 0;
	virtual PathEntry    tailAsPathEntry() const = 0;
	virtual ConstDevImpl originAsDevImpl() const = 0;
	virtual ConstDevImpl parentAsDevImpl() const = 0;

	static  IPathImpl   *toPathImpl(Path p);

};

class SlicedPathIterator : public CompositePathIterator {
public:
	// 'suffix' (if used) must live for as long as this object is used
	Path suffix;

	void clip(const IndexRange *range)
	{
		if ( range && range->size() > 0 ) {
			int f = (*this)->idxf_;
			int t = (*this)->idxt_;
			if ( range->size() != 1 ) {
				throw InvalidArgError("Currently only 1-level of indices supported, sorry");
			}
			if ( range->getTo() >= 0 )
				t = f + range->getTo();
			if ( range->getFrom() >= 0 )
				f += range->getFrom();
			if ( f < (*this)->idxf_ || t > (*this)->idxt_ || t < f ) {
				throw InvalidArgError("Array indices out of range");
			}
			if ( f != (*this)->idxf_ || t != (*this)->idxt_ ) {
				suffix = IPath::create();
				Address cl = (*this)->c_p_;
				++(*this);
				IPathImpl::toPathImpl( suffix )->append( cl, f, t );
				(*this).append(suffix);
			}
		}
	}

	SlicedPathIterator(ConstPath p, const IndexRange *range = 0)
	:CompositePathIterator( p )
	{
		clip(range);
	}

	SlicedPathIterator(ConstPath p, const IndexRange &range)
	:CompositePathIterator( p )
	{
		clip( &range );
	}

	SlicedPathIterator(const CompositePathIterator &it, const IndexRange &range)
	:CompositePathIterator(it)
	{
		clip( &range );
	}

	SlicedPathIterator(const CompositePathIterator &it, const IndexRange *range = 0)
	:CompositePathIterator(it)
	{
		clip( range );
	}

};


#endif
