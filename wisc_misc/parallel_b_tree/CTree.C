#include "system_specific.h"
#include "CTree.h"

#include <stdlib.h>
#include <cassert>

using namespace std;

ConcurrentTreeNode::ConcurrentTreeNode() {
    m_p_left = NULL;
    m_p_right = NULL;
    m_key = 0;
    m_data = 0;
}

ConcurrentTreeNode::~ConcurrentTreeNode() {
    if( m_p_left  != NULL ) delete m_p_left;
    if( m_p_right != NULL ) delete m_p_right;
    m_p_left = m_p_right = NULL;
}

int ConcurrentTreeNode::Lookup( int key ) {
    int data = NOT_IN_TREE;

    if( key == m_key ) {
        data = m_data;
    } else if( key < m_key ) {
        if( m_p_left != NULL ) {
            data = m_p_left->Lookup( key );
        }
    } else {
        // key > m_key
        if( m_p_right != NULL ) {
            data = m_p_right->Lookup( key );
        }
    }
    return data;
}

void ConcurrentTreeNode::Remove( int key ) {
    // We know that we will NOT be removing THIS node (but key could equal m_key)

    if( m_p_left == NULL && m_p_right == NULL ) return; // no children
    if( key < m_key && m_p_left  == NULL )      return; // no node exists w/ m_key == key
    if( key > m_key && m_p_right == NULL )      return; // no node exists w/ m_key == key

    if( key < m_key ) {
        if( key == m_p_left->m_key ) {
            /* Deleting the left child */
            if( m_p_left->m_p_left == NULL && m_p_left->m_p_right == NULL ) {
                /* easy case: left child has no children */
                delete m_p_left;
                m_p_left = NULL;
            } else if( m_p_left->m_p_left == NULL ) {
                /* easy case: left child has only a right child */
                ConcurrentTreeNode * p_dead = m_p_left;
                m_p_left = m_p_left->m_p_right;
                p_dead->m_p_right = NULL;
                delete p_dead;
            } else if( m_p_left->m_p_right == NULL ) {
                /* easy case: left child has only a left child */
                ConcurrentTreeNode * p_dead = m_p_left;
                m_p_left = m_p_left->m_p_left;
                p_dead->m_p_left = NULL;
                delete p_dead;
            } else {
                /* Hard case: left child has two children */
                ConcurrentTreeNode * p_new_left_child = m_p_left->m_p_left->MaxKey();
                m_p_left->m_key  = p_new_left_child->m_key;
                m_p_left->m_data = p_new_left_child->m_data;

                /* Don't need this pointer anymore... */
                p_new_left_child = NULL;

                if( m_p_left->m_p_left->m_key == m_p_left->m_key ) {
                    /* Left child had largest key: Therefore, left child has no right children */
                    ConcurrentTreeNode* p_dead = m_p_left->m_p_left;
                    m_p_left->m_p_left = m_p_left->m_p_left->m_p_left;
                    p_dead->m_p_left = NULL;
                    delete p_dead;
                } else {
                    m_p_left->m_p_left->Remove( m_p_left->m_key );
                }

            }
        } else {
            /* Not deleting the left child */
            m_p_left->Remove( key );
        }
    } else {
        if( key == m_p_right->m_key ) {
            /* Deleting the right child */
            if( m_p_right->m_p_left == NULL && m_p_right->m_p_right == NULL ) {
                /* easy case: right child has no children */
                delete m_p_right;
                m_p_right = NULL;
            } else if( m_p_right->m_p_left == NULL ) {
                /* easy case: right child has only a right child */
                ConcurrentTreeNode * p_dead = m_p_right;
                m_p_right = m_p_right->m_p_right;
                p_dead->m_p_right = NULL;
                delete p_dead;
            } else if( m_p_right->m_p_right == NULL ) {
                /* easy case: right child has only a left child */
                ConcurrentTreeNode * p_dead = m_p_right;
                m_p_right = m_p_right->m_p_left;
                p_dead->m_p_left = NULL;
                delete p_dead;
            } else {
                /* Hard case: right child has two children */
                ConcurrentTreeNode * p_new_right_child = m_p_right->m_p_left->MaxKey();
                m_p_right->m_key  = p_new_right_child->m_key;
                m_p_right->m_data = p_new_right_child->m_data;

                /* Don't need this pointer anymore... */
                p_new_right_child = NULL;

                if( m_p_right->m_p_left->m_key == m_p_right->m_key ) {
                    /* Left child had largest key: Therefore, left child has no right children */
                    ConcurrentTreeNode* p_dead = m_p_right->m_p_left;
                    m_p_right->m_p_left = m_p_right->m_p_left->m_p_left;
                    p_dead->m_p_left = NULL;
                    delete p_dead;
                } else {
                    m_p_right->m_p_left->Remove( m_p_right->m_key );
                }

            }
        } else {
            /* Not deleting the right child */
            m_p_right->Remove( key );
        }
    }

}

void ConcurrentTreeNode::Set( int key, int data ) {
    if( key == m_key ) {
        /* Make this SET behaviour */
        m_data = data;
    } else if( key < m_key ) {
        if( m_p_left == NULL ) {
            m_p_left = new ConcurrentTreeNode();
            m_p_left->m_key  = key;
            m_p_left->m_data = data;
        } else {
            m_p_left->Set( key, data );
        }
    } else {
        /* key > m_key */
        if( m_p_right == NULL ) {
            m_p_right = new ConcurrentTreeNode();
            m_p_right->m_key  = key;
            m_p_right->m_data = data;
        } else {
            m_p_right->Set( key, data );
        }
    }
}

ConcurrentTreeNode* ConcurrentTreeNode::MaxKey() {
    if( m_p_right == NULL ) return this;
    else                    return m_p_right->MaxKey();
}

void ConcurrentTreeNode::print( ostream &out, int indent ) {
    for(int i=0;i<indent;i++) {
        out << " ";
    }

    out << "<" << m_key << "," << m_data << ">" << endl;
    if( m_p_right == NULL && m_p_left == NULL ) return;
    if( m_p_right != NULL ) {
        m_p_right->print( out, indent+2 );
    } else {
        for(int i=0;i<indent+2;i++) {
            out << " ";
        }
        out << "NULL" << endl;
    }
    if( m_p_left  != NULL ) {
        m_p_left ->print( out, indent+2 );
    } else {
        for(int i=0;i<indent+2;i++) {
            out << " ";
        }
        out << "NULL" << endl;
    }
}

////////////////////////////////////////////////////////////////

ConcurrentTree::ConcurrentTree( int max_threads ) {
    p_root = NULL;
    m_nThreads = max_threads;
    m_nReadLocks = 0;
    m_nWritesRequested = 0;
    m_nNextThreadID = 0;
}

ConcurrentTree::~ConcurrentTree() {
    if( p_root != NULL ) {
        delete p_root;
    }
    p_root = NULL;
}

int ConcurrentTree::Lookup( int key ) {
    if( p_root == NULL ) return NOT_IN_TREE;

    /*
     * Spinning here does not guarantee that m_nWritesRequested is zero after the while loop,
     * but it does help to solve the starving writer problem
     */
    while( m_nWritesRequested )
        // On x86 we require a PAUSE instruction to throttle the spin-wait loop
        // (On SPARC this will compile to a no-op).
        PAUSE;

    /* Acquire a read-lock on the tree */
    AcquireReadLock();

    int val = p_root->Lookup(key);

    /* Release the read-lock */
    ReleaseReadLock();

    return val;
}

void ConcurrentTree::Remove( int key ) {
    if( p_root == NULL ) return;

    /* Acquire a write-lock */
    AcquireWriteLock();

    /* Write-lock acquired */

    if( p_root->m_key == key ) {
        /* Removing the root */
        if( p_root->m_p_left == NULL && p_root->m_p_right == NULL ) {
            /* Easy case: root has no children */
            delete p_root;
            p_root = NULL;
        } else if( p_root->m_p_left == NULL ) {
            /* Easy case: root has only a right child */
            ConcurrentTreeNode * p_dead = p_root;
            p_root = p_root->m_p_right;
            p_dead->m_p_right = NULL;
            delete p_dead;
        } else if( p_root->m_p_right == NULL ) {
            /* Easy case: root has only a left child */
            ConcurrentTreeNode * p_dead = p_root;
            p_root = p_root->m_p_left;
            p_dead->m_p_left = NULL;
            delete p_dead;
        } else {
            /* Not an easy case: Root has two children */
            ConcurrentTreeNode * p_new_root = p_root->m_p_left->MaxKey();
            p_root->m_key  = p_new_root->m_key;
            p_root->m_data = p_new_root->m_data;

            /* Don't need this pointer anymore... */
            p_new_root = NULL;

            if( p_root->m_p_left->m_key == p_root->m_key ) {
                /* Left child had largest key: Therefore, left child has no right children */
                ConcurrentTreeNode* p_dead = p_root->m_p_left;
                p_root->m_p_left = p_root->m_p_left->m_p_left;
                p_dead->m_p_left = NULL;
                delete p_dead;
            } else {
                p_root->m_p_left->Remove( p_root->m_key );
            }
        }
    } else {
        /* Not removing the root */
        p_root->Remove( key );
    }

    /* Now release the write-lock */
    ReleaseWriteLock();

}

void ConcurrentTree::Set( int key, int data ) {

    AcquireWriteLock();

    if( p_root == NULL ) {
        p_root = new ConcurrentTreeNode();
        p_root->m_key = key;
        p_root->m_data = data;
    } else {
        p_root->Set(key,data);
    }

    ReleaseWriteLock();
}

void ConcurrentTree::print( ostream &out ) {
    if( p_root == NULL ) out << "NULL" << endl;
    else                 p_root->print( out, 0 );
}

////////////////////////////////////////////////////////////////

void ConcurrentTree::InitiateTransaction() {
    AcquireTransactionalLock();
}

void ConcurrentTree::CommitTransaction() {
    ReleaseTransactionalLock();
}

void ConcurrentTree::TransactionAborted() {
    ReleaseTransactionalLock();
}

bool ConcurrentTree::TransactionalLookup( int &data, int key ) {
    data = Lookup( key );
    return false;
}

bool ConcurrentTree::TransactionalRemove( int key ) {
    Remove( key );
    return false;
}

bool ConcurrentTree::TransactionalSet( int key, int data ) {
    Set( key, data );
    return false;
}

void ConcurrentTree::AcquireReadLock() {
    m_l_writeLock.lock();
    m_nReadLocks++;
    m_l_writeLock.unlock();
}

void ConcurrentTree::AcquireWriteLock() {
    m_l_writeLock.lock();
    m_nWritesRequested++;
    m_l_writeLock.unlock();

    while( 1 ) {
        while( m_nReadLocks ) ;

        if( m_l_writeLock.try_lock()) {
            if( m_nReadLocks == 0 ) return;
            else m_l_writeLock.unlock();

        } else {
            continue;
        }
    }

}

void ConcurrentTree::AcquireTransactionalLock() {
    m_l_transLock.lock();
}

void ConcurrentTree::ReleaseReadLock() {
    m_l_writeLock.lock();
    m_nReadLocks--;
    m_l_writeLock.unlock();
}

void ConcurrentTree::ReleaseWriteLock() {
    m_nWritesRequested--;
    m_l_writeLock.unlock();
}

void ConcurrentTree::ReleaseTransactionalLock() {
    m_l_transLock.unlock();
}
