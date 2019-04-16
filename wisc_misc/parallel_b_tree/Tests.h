#ifndef TESTS_H
#define TESTS_H

class PThreadLockCVBarrier;
class ConcurrentTree;

/* 
 * Don't make this bigger than, say, 100000 or the default parallel implementation
 * will take a bloody long time! 
 */
const int NUM_ELEMENTS = 16384; /* Used for torture tests only */
const int NUM_ELEMENTS_THROUGHPUT = NUM_ELEMENTS; 

/*
 * Don't make this bigger than 100 for the same reason
 */
const int NUM_TRANSACTIONS = 500; 

bool testTreeSerial();
bool testTreeParallel( ConcurrentTree * p_tree, int tid, int nThreads, PThreadLockCVBarrier& barrier );
bool testTreeTransactional( ConcurrentTree * p_tree, int tid, int nThreads, PThreadLockCVBarrier& barrier );
bool testTreeThroughput( ConcurrentTree * p_tree, int tid, int nThreads, PThreadLockCVBarrier& barrier );

#endif

