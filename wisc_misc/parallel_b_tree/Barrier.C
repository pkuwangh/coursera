#include <sys/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "Barrier.h"
#include "fatals.h"

PThreadLockCVBarrier::PThreadLockCVBarrier( int nThreads ) {
    if( nThreads < 1 ) {
        fatal("Invalid number of threads for barrier: %i\n",nThreads );
    }

    m_nThreads = nThreads;
    m_nSyncCount = 0;

}

PThreadLockCVBarrier::~PThreadLockCVBarrier() {
}

void PThreadLockCVBarrier::Arrive() {
    // a typical barrier based on conditional variable
    // acquire a mutex
    std::unique_lock<std::mutex> lk(m_l_SyncLock);
    m_nSyncCount++;

    // check the condition
    if (m_nSyncCount == m_nThreads) {
        // wakeup all
        m_cv_SyncCV.notify_all();
        m_nSyncCount = 0;
    } else {
        // wait & yield the lock
        m_cv_SyncCV.wait(lk);
    }
}

