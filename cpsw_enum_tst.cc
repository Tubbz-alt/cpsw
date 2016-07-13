#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <cpsw_api_builder.h>
#include <cpsw_obj_cnt.h>

#ifdef WITH_YAML
#include <cpsw_yaml.h>
#endif

using std::string;

class TestFailed {
public:
	string msg_;
	TestFailed(const char *msg):msg_(msg){}
};


#define SZ 128

int
main(int argc, char **argv)
{
int opt;
const char *use_yaml = 0;
const char *dmp_yaml = 0;

	while ( (opt = getopt(argc, argv, "y:Y:")) > 0 ) {
		switch (opt) {
#ifdef WITH_YAML
			case 'Y': use_yaml = optarg; break;
			case 'y': dmp_yaml = optarg; break;
#else
			case 'Y':
			case 'y':
				fprintf(stderr,"YAML support not compiled in\n");
				throw TestFailed("YAMLS support not compiled in");
#endif
		}
	}
try {

{
	Dev root;

	if ( use_yaml ) {
#ifdef WITH_YAML
		root = CYamlFieldFactoryBase::loadYamlFile( use_yaml, "root" );
#endif
	} else {
		MemDev  memio = IMemDev::create("mem", SZ);
		MMIODev mmio  = IMMIODev::create("mmio", SZ, LE);

		memio->addAtAddress( mmio );

		IIntField::Builder b = IIntField::IBuilder::create();

		b->sizeBits(4);
		b->setEnum( enumBool );

		mmio->addAtAddress( b->build("bool"), 0 );

		Enum menu = IMutableEnum::create(NULL, "apples" , 2, "oranges", 0, "kiwi",    3, "mango",  13, NULL);

		b->setEnum( menu );

		menu.reset();

		mmio->addAtAddress( b->build("menu"), 0 );

		root = memio;
	}

	ScalVal boolVal = IScalVal::create( root->findByName("mmio/bool") );
	ScalVal menuVal = IScalVal::create( root->findByName("mmio/menu") );

	menuVal->setVal( 13 );

	CString  str;
	uint32_t u32;

	menuVal->getVal( &str );

	if ( ::strcmp(str->c_str(), "mango" ) ) {
		throw TestFailed("reading initial value of 'mango'");
	}


	boolVal->getVal( &str );
	if ( ::strcmp(str->c_str(), "True" ) ) {
		throw TestFailed("reading initial value of 'mango' (as bool)");
	}

std::cout << "YYY\n";
	IEnum::iterator itbeg = menuVal->getEnum()->begin();
	IEnum::iterator itend = menuVal->getEnum()->end();
	IEnum::iterator it;

	unsigned nitems = 0;

	// try string->num
	for ( it=itbeg; it != itend; ++it ) {
		const char *nm = (*it).first->c_str();
		menuVal->setVal( &nm );
		menuVal->getVal( &u32 );
		if ( u32 != (*it).second ) {
			throw TestFailed("Readback of menu val (string->num) failed");
		}
		boolVal->getVal( &str );
		if ( strcmp( str->c_str(), u32 ? "True" : "False" ) ) {
			throw TestFailed("Boolean Readback (string->num) Failed");
		}
		nitems++;
	}


	// try num->string
	for ( it=itbeg; it != itend; ++it ) {
		const char *nm = (*it).first->c_str();
		uint64_t    vl = (*it).second;
		menuVal->setVal( vl );
		menuVal->getVal( &str );
		if ( ::strcmp( str->c_str(), nm ) ) {
			throw TestFailed("Readback of menu val (num->string) failed");
		}
		boolVal->getVal( &str );
		if ( strcmp( str->c_str(), vl ? "True" : "False" ) ) {
			throw TestFailed("Boolean Readback (num->string) Failed");
		}
		nitems++;
	}


	boolVal->setVal("True");
	try {

		menuVal->getVal( &str );

		throw TestFailed("Conversion should have failed");

	} catch (ConversionError e) {
		// expected
	}

	if ( nitems != 8 || nitems != 2*menuVal->getEnum()->getNelms() ) {
		throw TestFailed("Unexpected number of enum entries");
	}

#ifdef WITH_YAML
	if ( dmp_yaml ) {
		CYamlFieldFactoryBase::dumpYamlFile( root, dmp_yaml, "root" );
	}
#endif

}

	if ( CpswObjCounter::report() ) {
		throw TestFailed("Unexpected object count (LEAK)\n");
	}

} catch (CPSWError err) {
	fprintf(stderr,"Test '%s' FAILED: CPSW Error: %s\n", argv[0], err.getInfo().c_str());
	return 1;
} catch (TestFailed err) {
	fprintf(stderr,"Test '%s' FAILED: %s\n", argv[0], err.msg_.c_str());
	return 1;
}

	fprintf(stderr,"Test '%s' PASSED\n", argv[0]);

	return 0;
}
