#ifndef __BASESYNC_H__
#define __BASESYNC_H__

//////////////////////////////////////////////////////////////////////////
// syncronisation objects
//////////////////////////////////////////////////////////////////////////

#include "basetypes.h"
#include "baseobjects.h"


//////////////////////////////////////////////////////////////////////////
//  Summary of implementation:
//
//  atomic increment/decrement/exchange
//    MSVC/BCC/i386: internal
//    GCC/i386: internal
//    GCC/PowerPC: internal
//    Other: internal
//
//  mutex
//    Win32: Critical section
//    Other: POSIX mutex
//
//  trigger
//    Win32: Event
//    Other: internal, POSIX condvar/mutex
//
//  rwlock:
//    Win32: internal, Event/mutex
//    MacOS: internal, POSIX condvar/mutex
//    Other: POSIX rwlock
//
//  Semaphore:
//    Win32: = SemaphoreTimed
//    MacOS: = SemaphoreTimed
//    Other: POSIX Semaphore
//
//  SemaphoreTimed (with timed waiting):
//    Win32: Semaphore
//    Other: internal, POSIX mutex/condvar
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// definition for a common types
//
// pthread_t      defines the handle to a thread
// pthread_id_t   defines the ID of the thread
//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
  typedef int pthread_id_t;
  typedef int pid_t;
  typedef HANDLE pthread_t;
#else
  typedef pthread_t pthread_id_t;
#endif


//////////////////////////////////////////////////////////////////////////
// sleep and thread identify
//////////////////////////////////////////////////////////////////////////
extern inline uint sleep(uint milliseconds)
{
#if defined(WIN32)
    Sleep(milliseconds);
#elif defined(__sun__)
    poll(0, 0, milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
	return milliseconds;
}
//////////////////////////////////////////////////////////////////////////
extern inline pthread_id_t this_thread() 
{
#ifdef WIN32
    return (pthread_id_t)GetCurrentThreadId();
#else
    return pthread_self();
#endif
}
//////////////////////////////////////////////////////////////////////////
extern inline bool is_thread(pthread_id_t id)	// NOT the thread handle, but the ID
{
#ifdef WIN32
    return GetCurrentThreadId() == (DWORD)id;
#else
    return pthread_equal(pthread_self(), id);
#endif
}
//////////////////////////////////////////////////////////////////////////

#ifndef WIN32
extern inline pthread_id_t	GetCurrentThreadId()	{ return this_thread(); }
extern inline pid_t			GetCurrentProcessId()	{ return getpid(); }
#endif



#ifdef SINGLETHREAD
//////////////////////////////////////////////////////////////////////////
// define some empty sync classes here
// !!todo!!
// might be better to #ifdef the usage in the following base classes
//////////////////////////////////////////////////////////////////////////
class Mutex: public noncopyable
{
public:
    Mutex()					{}
    ~Mutex()				{}
	bool trylock() const	{ return false; }
    void enter() const		{}
    void leave() const		{}
    void lock() const 		{}
    void unlock() const		{}
};
class ScopeLock: public noncopyable
{
public:
    ScopeLock(const Mutex& imtx)	{}
    ~ScopeLock()					{}
};
class event: public noncopyable
{
public:
    event(bool autoreset=true, bool state=true) {}
    ~event()						{}
    void wait()						{}
    void post()						{}
	void pulse()					{}
    void signal()					{}
    void reset()					{}
};
class Gate : public event
{
public:
	Gate(bool state=false)			{}
	void open()						{}
	void close()					{}
};

class rwlock: protected Mutex
{
public:
    rwlock()						{}
    ~rwlock()						{}
    void rdlock()					{}
    void wrlock()					{}
    void unlock()					{}
    void lock()						{}
};

class rwlockex: protected Mutex
{

	rwlockex()						{}
	~rwlockex()						{}
	void rdlock()					{}
	void wrlock()					{}
	void unlock()					{}
	void lock()						{}
};

class ScopeRead: public noncopyable
{
public:
    ScopeRead(rwlock& irw)			{}
    ~ScopeRead()  					{}
};
//////////////////////////////////////////////////////////////////////////
class ScopeWrite: public noncopyable
{
public:
    ScopeWrite(rwlock& irw)			{}
    ~ScopeWrite()  					{}
};
class Semaphore: public global
{
public:
    Semaphore(ulong initvalue=0)	{}
    virtual ~Semaphore()			{}
    void wait()						{}
    void post()						{}
    void signal()					{}
};
class SemaphoreTimed: public global
{
public:
    SemaphoreTimed(ulong initvalue=0){}
    virtual ~SemaphoreTimed()		{}
    bool wait(ulong msecs = -1)		{ return false; }
    void post()						{}
    void signal()  					{}
};

#else//!SINGLETHREAD
//////////////////////////////////////////////////////////////////////////
// mutex
//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
//////////////////////////////////////////////////////////////////////////
// WIN32 mutex, realized with CRITICAL_SECTION
//////////////////////////////////////////////////////////////////////////
class Mutex: public noncopyable
{
protected:
    CRITICAL_SECTION cs;
public:
    Mutex()				{ InitializeCriticalSection(&cs); }
    ~Mutex()			{ DeleteCriticalSection(&cs); }
	bool trylock() const{ return (TryEnterCriticalSection(const_cast<LPCRITICAL_SECTION >(&cs)) != 0); }
    void enter() const 	{ EnterCriticalSection(const_cast<LPCRITICAL_SECTION >(&cs)); }
    void leave() const 	{ LeaveCriticalSection(const_cast<LPCRITICAL_SECTION >(&cs)); }
    void lock() const 	{ enter(); }
    void unlock() const { leave(); }
};

//////////////////////////////////////////////////////////////////////////
#else
//////////////////////////////////////////////////////////////////////////
// POSIX mutex
//////////////////////////////////////////////////////////////////////////
class Mutex: public noncopyable
{
protected:
    pthread_mutex_t mtx;
public:
    Mutex()		
	{ 
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mtx, &attr); 
		pthread_mutexattr_destroy(&attr);
	}
    ~Mutex()		{ pthread_mutex_destroy(&mtx); }
	bool trylock() const{ return pthread_mutex_trylock(const_cast<pthread_mutex_t*>(&mtx)); }
    void enter() const 	{ pthread_mutex_lock(const_cast<pthread_mutex_t*>(&mtx)); }
    void leave() const 	{ pthread_mutex_unlock(const_cast<pthread_mutex_t*>(&mtx)); }
    void lock()	const 	{ enter(); }
    void unlock() const { leave(); }
};
//////////////////////////////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// ScopeLock
//////////////////////////////////////////////////////////////////////////
class ScopeLock: public noncopyable
{
protected:
    Mutex& mtx;
public:
    ScopeLock(const Mutex& imtx): mtx(const_cast<Mutex&>(imtx))  { mtx.lock(); }
    ~ScopeLock()  { mtx.unlock(); }
};



//////////////////////////////////////////////////////////////////////////
// event
//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
//////////////////////////////////////////////////////////////////////////
// WIN32 trigger, realized with an event handle
//////////////////////////////////////////////////////////////////////////
class event: public noncopyable
{
protected:
    HANDLE handle;      // Event object
public:
	// create autoevent signaled by default
    event(bool autoreset=true, bool state=true) 
	{
		handle = CreateEvent(0, !autoreset, state, 0);
		if (handle == 0)
			throw exception("event failed");
	}
    ~event()            { CloseHandle(handle); }
    void wait()         { WaitForSingleObject(handle, INFINITE); }
    void post()         { SetEvent(handle); }
	void pulse()		{ PulseEvent(handle); }
    void signal()       { post(); }
    void reset()        { ResetEvent(handle); }
};

//////////////////////////////////////////////////////////////////////////
#else
//////////////////////////////////////////////////////////////////////////
// POSIX trigger
//////////////////////////////////////////////////////////////////////////
class event: public noncopyable
{
protected:
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int state;
    bool autoreset;
public:
	// create autoevent signaled by default
	event(bool autoreset=true, bool state=true);
    ~event();
    void wait();
    void post();
	void pulse();
    void signal()  { post(); }
    void reset();
};
//////////////////////////////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Gate
//////////////////////////////////////////////////////////////////////////
class Gate : public event
{
public:
	Gate(bool state=false) : event(false,state)	{}
	void open()		{signal();}
	void close()	{reset();}
};


//////////////////////////////////////////////////////////////////////////
// Switch
//////////////////////////////////////////////////////////////////////////
class Switch : public Mutex
{
	int value;
public:
	Switch():value(0)		{}
	bool isopen()		
	{	
		Mutex::lock(); 
		bool ret = (value==0);
		if(ret) value = 1;
		Mutex::unlock(); 
		return ret;
	}
	void close()		
	{
		Mutex::lock(); 
		value = 0;
		Mutex::unlock(); 
	}
};


//////////////////////////////////////////////////////////////////////////
// read/write lock
//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(__DARWIN__) || defined(__bsdi__)

#define __MUTEX_RWLOCK__

#elif defined(linux)
   // on Linux rwlocks are included only with -D_GNU_SOURCE.
   // programs that don't use rwlocks, do not need to define
   // _GNU_SOURCE either.
#if defined(_GNU_SOURCE) || defined(__USE_UNIX98)
#define __POSIX_RWLOCK__
#endif
#else
#define __POSIX_RWLOCK__
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef __MUTEX_RWLOCK__
//////////////////////////////////////////////////////////////////////////
// read/write lock with mutex
//////////////////////////////////////////////////////////////////////////

class rwlock: protected Mutex
{
protected:
#ifdef WIN32
    HANDLE  reading;    // Event object
    HANDLE  finished;   // Event object
    int     readcnt;
    int     writecnt;
#else
    pthread_mutex_t mtx;
    pthread_cond_t readcond;
    pthread_cond_t writecond;
    int locks;
    int writers;
    int readers;
#endif
public:
    rwlock();
    ~rwlock();
    void rdlock();
    void wrlock();
    void unlock();
    void lock()     { wrlock(); }
};

//////////////////////////////////////////////////////////////////////////
#elif defined(__POSIX_RWLOCK__)
//////////////////////////////////////////////////////////////////////////
// POSIX read/write lock
//////////////////////////////////////////////////////////////////////////
class rwlock: public noncopyable
{
protected:
    pthread_rwlock_t rw;
public:
    rwlock()		{ if(0 != pthread_rwlock_init(&rw, 0)) throw exception("rwlock failed"); }
    ~rwlock()       { pthread_rwlock_destroy(&rw); }
    void rdlock()   { pthread_rwlock_rdlock(&rw); }
    void wrlock()   { pthread_rwlock_wrlock(&rw); }
    void unlock()   { pthread_rwlock_unlock(&rw); }
    void lock()     { wrlock(); }
};
//////////////////////////////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////////////////////////////

/*
//////////////////////////////////////////////////////////////////////////
// extended rw-lock
// a thread that readlocked the thing can make it writelocked 
// without closing the readlock first
// in a normal rwlock the thread would deadlock itself
// when it tries to writelock after readlocking
//////////////////////////////////////////////////////////////////////////
class rwlockex: protected Mutex
{
	//////////////////////////////////////////////////////////////////////
	// class data
	//////////////////////////////////////////////////////////////////////
	Mutex					cWLock;		// exclusice write lock
	event					cEvent;		// event for writer
	pthread_id_t			cW;			// writer
	TSLISTDST<pthread_id_t>	cRList;		// list of reading threads
public:
	//////////////////////////////////////////////////////////////////////
	// constructor/destructor
	// create the event as autoreset/signaled
	//////////////////////////////////////////////////////////////////////
	rwlockex():cEvent(true,true),cW(0)	{}
	~rwlockex()							{}
	//////////////////////////////////////////////////////////////////////
	// read locking
	//////////////////////////////////////////////////////////////////////
    void rdlock()   
	{
		// check for a writer
		cWLock.lock();

		// close this for exclusive access
		Mutex::lock();
		// add the current thread id to the list
		cRList.Insert( this_thread() );
		// close the event for writers
		cEvent.reset();
		// open this
		Mutex::unlock();

		// unlock the writer so other threads can follow
		cWLock.unlock();
	}
	//////////////////////////////////////////////////////////////////////
	// write locking
	//////////////////////////////////////////////////////////////////////
    void wrlock()   
	{	
		pthread_id_t safeid = 0;

		// close this for exclusive access
		Mutex::lock();
		// check if we have got a readlock before
		if( cRList.Find(this_thread()) )
		{	// remove the readlock but notice it
			unlock();
			safeid = this_thread();
		}
		// open this
		Mutex::unlock();

		// close writers 
		// this will serialize readers and writers
		// order of procession is order of aquiring this mutex
		cWLock.lock();
		// store the safeid globaly
		cW = safeid;
		// wait for a start event
		cEvent.wait();
	}
	//////////////////////////////////////////////////////////////////////
	// unlock the last locking
	//////////////////////////////////////////////////////////////////////
    void unlock()   
	{	// close this for exclusive access
		Mutex::lock();
		// if we have readers in the list the incoming is a read thread
		if( cRList.Count() > 0 )
		{	// remove from list
#ifdef CHECK_LOCKS
			if( !cRList.Remove( this_thread() ) )
				throw exception("rwlockex: unlocking from unknown thread");
#else
			cRList.Remove( this_thread() );
#endif	
		}
		else
		{	// otherwise it is a write thread
			// so check if we have to regain read access
			// and unlock the write mutex 
			if(cW != 0)
			{
				cRList.Insert( cW );
				cW = 0;
			}
			cWLock.unlock();
		}
		// and post the event
		// so the reader can go on/writer can check the current status
		if( 0==cRList.Count() )
			cEvent.post();
		// open this
		Mutex::unlock();
	}
	//////////////////////////////////////////////////////////////////////
	// alias
	//////////////////////////////////////////////////////////////////////
    void lock()     { wrlock(); }
};

*/






#if defined(__MUTEX_RWLOCK__) || defined(__POSIX_RWLOCK__)
//////////////////////////////////////////////////////////////////////////
// scoperead & scopewrite
//////////////////////////////////////////////////////////////////////////
class ScopeRead: public noncopyable
{	// prevent allocation
	void* operator new(size_t s);
	void* operator new[](size_t s);
protected:
    rwlock* rw;
public:
    ScopeRead(rwlock& irw): rw(&irw)  { rw->rdlock(); }
    ~ScopeRead()  { rw->unlock(); }
};
//////////////////////////////////////////////////////////////////////////
class ScopeWrite: public noncopyable
{	// prevent allocation
	void* operator new(size_t s);
	void* operator new[](size_t s);
protected:
    rwlock* rw;
public:
    ScopeWrite(rwlock& irw): rw(&irw)  { rw->wrlock(); }
    ~ScopeWrite()  { rw->unlock(); }
};
//////////////////////////////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// semaphores
//////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(__DARWIN__) || defined(__bsdi__)
//////////////////////////////////////////////////////////////////////////
// these systems have a common implementation for timed and untimes sema
// so map ordinary Semaphore to timed Semaphore
//////////////////////////////////////////////////////////////////////////
class SemaphoreTimed;
typedef SemaphoreTimed Semaphore;
//////////////////////////////////////////////////////////////////////////
#else
//////////////////////////////////////////////////////////////////////////
// POSIX Semaphore
//////////////////////////////////////////////////////////////////////////
class Semaphore: public global
{
protected:
    sem_t handle;
public:
    Semaphore(ulong initvalue=0);
    virtual ~Semaphore();

    void wait();
    void post(size_t amount=1);
    void signal(size_t amount=1)  { post(amount); }
};
//////////////////////////////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// timed Semaphore
//////////////////////////////////////////////////////////////////////////
class SemaphoreTimed: public global
{
protected:
#ifdef WIN32
    HANDLE handle;
#else
    int count;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
#endif
public:
    SemaphoreTimed(ulong initvalue=0);
    virtual ~SemaphoreTimed();
    bool wait(ulong msecs = -1);
    void post(size_t amount=1);
    void signal(size_t amount=1)  { post(amount); }
};
//////////////////////////////////////////////////////////////////////////


#endif//!SINGLETHREAD

#endif//__BASESYNC_H__

