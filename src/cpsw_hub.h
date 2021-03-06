 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_HUB_H
#define CPSW_HUB_H

#include <cpsw_api_builder.h>
#include <cpsw_address.h>
#include <cpsw_entry.h>

#include <map>
#include <list>

#include <stdio.h>
#include <string.h>
#include <ctype.h>


using cpsw::weak_ptr;

class   Visitor;

class   CDevImpl;
typedef shared_ptr<CDevImpl>    DevImpl;
typedef weak_ptr<CDevImpl>     WDevImpl;

class CDevImpl : public CEntryImpl, public virtual IDev {
	private:
		int      started_;
	protected:
		typedef  std::map<const char*, AddressImpl, StrCmp> MyChildren;
		typedef  std::list<AddressImpl>                     PrioList;


	private:
		mutable  MyChildren children_;       // only by 'add' and 'startUp' methods
		mutable  PrioList   configPrioList_; // order for dumping configuration fields

	protected:
		virtual void add(AddressImpl a, Field child);

		virtual IAddress::AKey getAKey()  { return IAddress::AKey( getSelfAs<DevImpl>() );       }

		// this must recursively clone all children.
		CDevImpl(const CDevImpl &orig, Key &k);

		virtual int getDefaultConfigPrio() const;

	public:

		typedef MyChildren::const_iterator const_iterator;

		CDevImpl(Key &k, const char *name, uint64_t size= 0);

		CDevImpl(Key &k, YamlState &ypath);

		virtual void dumpYamlPart(YAML::Node &node) const;

		static  const char *_getClassName()       { return "Dev";           }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual ~CDevImpl();

		virtual CDevImpl *clone(Key &k) { return new CDevImpl( *this, k ); }

		virtual void postHook(ConstShObj orig);

		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddress(Field child, unsigned nelms);

		template <typename T>
		shared_ptr<T> doAddAtAddress(Field child, YamlState &ypath)
		{
			IAddress::AKey k = getAKey();
			shared_ptr<T> rval( cpsw::make_shared<T>(k, ypath) );
			add( rval, child->getSelf() );
			return rval;
		}

		virtual void
		addAtAddress(Field child, YamlState &ypath);

		virtual Path findByName(const char *s) const;

		virtual Child getChild(const char *name) const
		{
			return getAddress( name );
		}

		virtual Address getAddress(const char *name) const;

		virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth);

		virtual Children getChildren() const;

		virtual Hub          isHub()          const;

		virtual ConstDevImpl isConstDevImpl() const
		{
			return getSelfAsConst<ConstDevImpl>();
		}

		virtual const_iterator begin() const
		{
			return children_.begin();
		}

		virtual const_iterator end()   const
		{
			return children_.end();
		}

		virtual int isStarted() const
		{
			return started_;
		}

		virtual void startUp();

		virtual uint64_t processYamlConfig(Path p, YAML::Node &, bool) const;
};

#define NULLHUB     Hub( static_cast<IHub *>(NULL) )
#define NULLDEVIMPL DevImpl( static_cast<CDevImpl *>(NULL) )


#endif
