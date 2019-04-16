#ifndef CTREE_H
#define CTREE_H

#include <iostream>
#include <map>
#include <mutex>

const int NOT_IN_TREE = -2147483647-1;
class ConcurrentTree;

class ConcurrentTreeNode {
  public:
    ConcurrentTreeNode();
    ~ConcurrentTreeNode();

    int  Lookup( int key );
    void Remove( int key );
    void Set( int key, int data );

    void print( std::ostream &out, int indent );

    /* To facilitate deletion */
    ConcurrentTreeNode* MaxKey();

  private:
    friend class ConcurrentTree;

    ConcurrentTreeNode *m_p_left, *m_p_right;
    int m_key, m_data;

    /* Add any data members you want here */

};

class ConcurrentTree {
  public:
    ConcurrentTree( int max_threads );
    ~ConcurrentTree();

    int  Lookup( int key );
    void Remove( int key );
    void Set( int key, int data );

    void print( std::ostream &out );

    void InitiateTransaction();
    void CommitTransaction();
    void TransactionAborted();

    bool TransactionalLookup( int &data, int key );
    bool TransactionalRemove( int key );
    bool TransactionalSet( int key, int data );

  private:

    void AcquireReadLock();
    void ReleaseReadLock();

    void AcquireWriteLock();
    void ReleaseWriteLock();

    void AcquireTransactionalLock();
    void ReleaseTransactionalLock();

    ConcurrentTreeNode * p_root;

    /* Add any data members you want here */
    std::mutex m_l_writeLock;
    int m_nReadLocks;
    int m_nWritesRequested;

    int m_nNextThreadID, m_nThreads;
    std::mutex m_l_transLock;

};

#endif // #ifndef CTREE_H
