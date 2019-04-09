#ifndef _BARRIER_H_
#define _BARRIER_H_

#include <condition_variable>
#include <mutex>

/* C++ object-oriented barriers -- use Barrier.C */
class PThreadLockCVBarrier {
public:
  PThreadLockCVBarrier( int nThreads ); 
  ~PThreadLockCVBarrier();

  void Arrive();

private:
  int m_nThreads;
  std::mutex m_l_SyncLock;
  std::condition_variable m_cv_SyncCV;
  int m_nSyncCount;
};

#else // #ifdef __cplusplus

/* C function-based barrier -- use Barrier.c */

void Barrier();
void PrepareBarrier(int nProcs);
void DestroyBarrier();

#endif
