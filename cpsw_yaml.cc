#include <cpsw_yaml.h>
#include <cpsw_entry.h>
#include <cpsw_hub.h>
#include <cpsw_error.h>
#include <cpsw_sval.h>
#include <cpsw_mem_dev.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_netio_dev.h>
#include <cpsw_command.h>

#include <sstream>
#include <fstream>
#include <iostream>

using boost::dynamic_pointer_cast;

// Weird things happen when we do iterative lookup.
// Must be caused by how nodes are implemented and
// reference counted.
// Use RECURSIVE lookup!
#define RECURSIVE_GET_NODE
#ifndef RECURSIVE_GET_NODE
const YAML::Node getNode(const YAML::Node &node, const char *key)
{
YAML::Node n(node);

	do {
std::cout<< "looking for "<<key;
		const YAML::Node & val_node( n[key] );
		if ( val_node ) {
std::cout<< " -> found: " << val_node << "\n";
			return val_node;
		}

std::cout<< " -> not found --> looking for merge node ";

		const YAML::Node & merge_node( n["<<"] );

		// seems we have to be really careful when 
		// manipulating nodes (c++ - we don't really
		// know what's happening under the hood).
		// The 'natural' way to write things would
		// result in crashes:
		//
		//   } while ( (n = n["<<"]) );
		//
		// seems we shouldn't assign to 'n' if n["<<"] is
		// not defined.

		if ( ! merge_node ) {
std::cout<< " -> not found\n";
			return merge_node;
		}
		n = merge_node;
std::cout<< "found - IT - ";

	} while ( 1 );
}
#else
const YAML::Node getNode(const YAML::Node &node, const char *key)
{
	if ( ! node )
		return node;

	const YAML::Node &val( node[key] );

	if ( val )
		return val;

	return getNode( node["<<"], key );
}
#endif

YAML::Node CYamlSupportBase::overrideNode(const YAML::Node &node)
{
	return node;
}


YAML::Node 
CYamlSupportBase::dumpYaml() const
{
YAML::Node node;

	node["class"] = getClassName();

	dumpYamlPart( node );

	return node;
}

CYamlSupportBase::CYamlSupportBase(const YAML::Node &node)
{
}

void
CYamlSupportBase::dumpYamlPart(YAML::Node &node) const
{
}

template <typename T> class CYamlTypeRegistry : public IYamlTypeRegistry<T> {
public:
	struct StrCmp {
		bool operator () (const char *a, const char *b) const {
			return ::strcmp(a,b) < 0 ? true : false;
		}
	};

	typedef std::map<const char *, IYamlFactoryBase<T> *, StrCmp> Map;
	typedef std::pair<const char *, IYamlFactoryBase<T> *>        Member;

private:
	
	Map map_;

public:

	virtual void addFactory(const char *className, IYamlFactoryBase<T> *f)
	{
	std::pair< typename Map::iterator, bool > ret = map_.insert( Member(className, f) );
		if ( ! ret.second )
			throw DuplicateNameError( className );
	}

	virtual void delFactory(const char *className)
	{
		map_.erase( className );
	}

	virtual void extractClassName(std::string *str_p, const YAML::Node &n)
	{
		mustReadNode( n, "class", str_p );
	}

	virtual T makeItem(const YAML::Node &n)
	{
		std::string str;
		extractClassName( &str, n );
		IYamlFactoryBase<T> *fact = map_[ str.c_str() ];
		if ( ! fact )
			throw NotFoundError( str );
		return fact->makeItem(n, this);
	}

	virtual void dumpClasses()
	{
		typename Map::iterator it( map_.begin() );
		while ( it != map_.end() ) {
			std::cout << it->first << "\n";
			++it;
		}
	}
};

void
CYamlFieldFactoryBase::addChildren(CEntryImpl &e, const YAML::Node &node, IYamlTypeRegistry<Field> *registry)
{
}

void
CYamlFieldFactoryBase::addChildren(CDevImpl &d, const YAML::Node &node, IYamlTypeRegistry<Field> *registry)
{
const YAML::Node & children( node["children"] );

	std::cout << "node size " << node.size() << "\n";

	YAML::const_iterator it = node.begin();
	while ( it != node.end() ) {
		std::cout << it->first << "\n";
		++it;
	}

	std::cout<<"adding " << children.size() << " children\n";

	if ( children ) {
		YAML::const_iterator it = children.begin();
		while ( it != children.end() ) {
			Field c = registry->makeItem( *it );
			d.addAtAddress( c, *it );
			++it;
		}
	}
}

Dev
CYamlFieldFactoryBase::dispatchMakeField(const YAML::Node &node, const char *root_name)
{
const YAML::Node &root( root_name ? node[root_name] : node );
	/* Root node must be a Dev */
	return dynamic_pointer_cast<Dev::element_type>( getFieldRegistry()->makeItem( root ) );
}


Dev
CYamlFieldFactoryBase::loadYamlFile(const char *file_name, const char *root_name)
{
	return dispatchMakeField( YAML::LoadFile( file_name ), root_name );
}

Dev
CYamlFieldFactoryBase::loadYamlStream(std::istream &in, const char *root_name)
{
	return dispatchMakeField( YAML::Load( in ), root_name );
}

Dev
CYamlFieldFactoryBase::loadYamlStream(const char *yaml, const char *root_name)
{
std::string  str( yaml );
std::stringstream sstrm( str );
	return loadYamlStream( sstrm, root_name );
}

void
CYamlFieldFactoryBase::dumpYamlFile(Entry top, const char *file_name, const char *root_name)
{
shared_ptr<const EntryImpl::element_type> topi( dynamic_pointer_cast<const EntryImpl::element_type>(top) );

	if ( ! topi ) {
		std::cerr << "WARNING: 'top' not an EntryImpl?\n";
		return;
	}

	const YAML::Node &top_node( topi->dumpYaml() );


	YAML::Emitter emit;

	if ( root_name ) {
		YAML::Node root_node;
		root_node[root_name]=top_node;
		emit << root_node;
	} else {
		emit << top_node;
	}

	if ( file_name ) {
		std::ofstream os( file_name );
		os        << emit.c_str() << "\n";
	} else {
		std::cout << emit.c_str() << "\n";
	}
}

YAML::Emitter& operator << (YAML::Emitter& out, const ScalVal_RO& s) {
    uint64_t u64;
    s->getVal( &u64 );
    out << YAML::BeginMap;
    out << YAML::Key << s->getName();
    out << YAML::Value << u64;
    out << YAML::EndMap;
    return out;
}

YAML::Emitter& operator << (YAML::Emitter& out, const Hub& h) {
    Children ch = h->getChildren();
    for( unsigned i = 0; i < ch->size(); i++ )
    {
//       out << ch[i]; 
    }
    return out;
}

IYamlTypeRegistry<Field> *
CYamlFieldFactoryBase::getFieldRegistry()
{
static CYamlTypeRegistry<Field> the_registry_;
	return &the_registry_;
}

DECLARE_YAML_FIELD_FACTORY(EntryImpl);
DECLARE_YAML_FIELD_FACTORY(DevImpl);
DECLARE_YAML_FIELD_FACTORY(IntEntryImpl);
DECLARE_YAML_FIELD_FACTORY(MemDevImpl);
DECLARE_YAML_FIELD_FACTORY(MMIODevImpl);
DECLARE_YAML_FIELD_FACTORY(NetIODevImpl);
DECLARE_YAML_FIELD_FACTORY(SequenceCommandImpl);
