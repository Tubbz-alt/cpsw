#ifndef CPSW_SHARED_OBJ_H
#define CPSW_SHARED_OBJ_H
// Object referenced via shared pointer

#include <cpsw_obj_cnt.h>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

using boost::shared_ptr;
using boost::weak_ptr;
using boost::static_pointer_cast;

class CShObj {
public:
	class StolenKeyError           {};
	class MustNotAssignError       {};
	class MustNotCopyError         {};
	class CloneNotImplementedError {};

	static CpswObjCounter & sh_ocnt_();

protected:

	typedef shared_ptr<CShObj> ShObj;

	// Key object; only we 
	class Key {
		private:
			bool good_;
			Key():good_(true)
			{
			}
			friend class CShObj;

			Key &operator=(const Key &orig)
			{
				throw MustNotAssignError();
			}

		// subclass of CshObj may copy a key
		// but if they don't hand it to CshObj()'s 
		// constructor they will never be fully
		// constructed (and not able to hand out
		// the stolen key).
		public:
			Key(Key &orig)
			{
				// Enforce move semantics for key;
				good_      = orig.good_;
				orig.good_ = false;
			}

			bool isValid()
			{
				return good_;
			}
	};

	// No subclass must start a thread from a constructor
	// if the thread uses the 'self' pointer because
	// it is not available yet.
	// Instead: create/start all threads from the 'start' method.
	virtual void start()
	{
	}

private:
	weak_ptr<CShObj> self_;

	CShObj & operator= (const CShObj &orig)
	{
		throw MustNotAssignError();
	}

	CShObj(CShObj &orig)
	{
		throw MustNotCopyError();
	}

	CShObj(const CShObj &orig)
	{
		throw MustNotCopyError();
	}

	void startRedirector()
	{
		start();
	}


	template <typename P>
	static shared_ptr<P> setSelf(P *p)
	{
	shared_ptr<P> me( p );
		p->self_ = me;

		// safe to start threads after self_ is set...
		// this starts threads in new and cloned objects!
		p->startRedirector();

		return me;
	}

protected:
	CShObj( const CShObj &orig, Key &k )
	{
		if ( ! k.isValid() ) {
			throw StolenKeyError();
		}
		self_.reset();
		++sh_ocnt_();
	}

	template <typename AS> AS getSelfAs()
	{
		return static_pointer_cast<typename AS::element_type>( ShObj( self_ ) );
	}

	template <typename AS> AS getSelfAsConst() const
	{
		return static_pointer_cast<typename AS::element_type>( ShObj( self_ ) );
	}


public:
	CShObj(Key &k)
	{
		if ( ! k.isValid() ) {
			throw StolenKeyError();
		}
		++sh_ocnt_();
	}

	virtual ~CShObj()
	{
		--sh_ocnt_();
	}

	// EVERY subclass must provide a covariant version of this!
	virtual CShObj *clone(Key &k)
	{
		return new CShObj( *this, k );
	}


	// The typeid check works if we know from
	// what class we clone.
	template <typename T>
	static T clone(T in)
	{
	Key k;
	typename T::element_type *p = in->clone( k );
		if ( typeid( *p ) != typeid( *in ) ) {
			delete p;
			// some subclass of '*in' does not implement virtual clone
			throw CloneNotImplementedError();
		}
		return setSelf( p );
	}

	template <typename T>
	static T cloneUnchecked(T in)
	{
	Key k;
	typename T::element_type *p = in->clone( k );
		return setSelf( p );
	}

	template <typename T>
	static T create()
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k );

		return setSelf( p );
	}

	template <typename T, typename A1>
	static T create(A1 a1)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1 );

		return setSelf( p );
	}

	template <typename T, typename A1, typename A2>
	static T create(A1 a1, A2 a2)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1, a2 );

		return setSelf( p );
	}

	template <typename T, typename A1, typename A2, typename A3>
	static T create(A1 a1, A2 a2, A3 a3)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1, a2, a3 );

		return setSelf( p );
	}

	template <typename T, typename A1, typename A2, typename A3, typename A4>
	static T create(A1 a1, A2 a2, A3 a3, A4 a4)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1, a2, a3, a4 );

		return setSelf( p );
	}

	template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5>
	static T create(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1, a2, a3, a4, a5 );

		return setSelf( p );
	}
	template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
	static T create(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1, a2, a3, a4, a5, a6 );

		return setSelf( p );
	}

	template <typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
	static T create(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7)
	{
	Key k;
	typename T::element_type *p = new typename T::element_type( k, a1, a2, a3, a4, a5, a6, a7 );

		return setSelf( p );
	}

};

#endif
