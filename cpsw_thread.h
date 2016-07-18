#ifndef CPSW_THREAD_HELPER_H
#define CPSW_THREAD_HELPER_H

#include <pthread.h>
#include <string>

class CRunnable {
private:
	pthread_t     tid_;	
	bool          started_;
	std::string   name_;

	static void*  wrapper(void*);

protected:
	// to be implemented by subclasses

	virtual void* threadBody()    = 0; 

	pthread_t     getTid() { return tid_ ; }



public:
	CRunnable(const char *name) : started_(false), name_(name) {}

	CRunnable(const CRunnable &orig) : started_(false), name_(orig.name_) {}

	const std::string& getName() { return name_; }

	virtual void  threadStart();
	virtual void* threadJoin();
	virtual void  threadCancel();

	// returns 'false' when the thread hadn't been
	// started yet.

	// note that the 'started_' flag is manipulated
	// by the 'start', 'stop' and 'join' methods
	// in the context of their caller.
	virtual bool threadStop(void **joinval_p);
	virtual bool threadStop() { return threadStop(NULL); }

	// Destructor of subclass should call stop -- cannot
	// rely on base class to do so because subclass 
	// data is already torn down!
	virtual ~CRunnable();
};

#endif
