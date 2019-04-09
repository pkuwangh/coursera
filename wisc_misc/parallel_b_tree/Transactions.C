#include <cassert>
#include <stdlib.h>
#include <map>
#include "Transactions.h"
#include "CTree.h"
#include "Tests.h"
#include "Stats.h"

//
// Matt says:
// the way rand was called in this file was not thread-safe
// so we'll patch it up here
//
static __thread unsigned int seed;

void initSeed (int thread_id)
{
	seed = (unsigned int) thread_id;
}

char randomTransaction() {
	//int c = rand() % WEIGHT_SUM;
	int c = rand_r (&seed) % WEIGHT_SUM;
  if( c < SCAN_WEIGHT ) return SCAN;
  c -= SCAN_WEIGHT;
  if( c < UPDATE_WEIGHT ) return UPDATE;
  c -= UPDATE_WEIGHT;
  if( c < LOOKUP_WEIGHT ) return LOOKUP;
  c -= LOOKUP_WEIGHT;
  if( c < CONDITIONAL_ADD_WEIGHT ) return CONDITIONAL_ADD; 
  return CONDITIONAL_REMOVE;
}

int doTortureScan( ConcurrentTree * p_tree ) {
  p_tree->InitiateTransaction();

  int sum = 0;
  for( int key = 0; key < NUM_ELEMENTS; key ++ ) {
    assert(key >= 0 && key < NUM_ELEMENTS );
    int data;

    if( p_tree->TransactionalLookup( data, key ) ) {
      /* TRUE -> Need to abort */
      sum = 0;
      key = -1;
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nScanAborts );
      p_tree->InitiateTransaction();
    } else {
      /* FALSE -> Transactional Lookup didn't lead to an abort */
		// NOT_IN_TREE bug fix
		if (data != NOT_IN_TREE)
			sum += data;
    }
  }

  p_tree->CommitTransaction();
  return sum;
}

void doTortureUpdate( ConcurrentTree * p_tree ) {
  int keys[SMALL_TRANSACTION_SIZE];
  
  p_tree->InitiateTransaction();

  map<int,int> log;
  map<int,int>::iterator iter;

  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {
    keys[i] = rand_r(&seed) % NUM_ELEMENTS;
  }

  int data, newdata;

  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {
    if( !p_tree->TransactionalLookup( data, keys[i] ) ) {
      /* FALSE: Don't abort... yet */
		// NOT_IN_TREE bug fix
		if (data == NOT_IN_TREE) data = 0;

      if( i % 2 ) {
        newdata = data + 1;
      } else {
        newdata = data - 1;
      }

      if( !p_tree->TransactionalSet( keys[i], newdata ) ) {
        /* FALSE: Don't abort on this iteration, but log this access */

        iter = log.find( keys[i] );
        if( iter == log.end() ) {
          log[keys[i]] = data; // log the old value
        }

        continue; // don't abort
        
      }
    } 
    
    /* at least one was true... Abort... */

    for( iter = log.begin(); iter != log.end(); iter++ ) {
      int key = iter->first;
      int orig_data = iter->second;
      if( p_tree->TransactionalSet( key, orig_data ) ) {
        /* SHOULD NEVER happen, since we haven't released isolation yet */
        cout << "Explicit violation! Failed to acquire isolation on previously-isolated data!" << endl; 
        assert(0);
      }
    }

    log.clear();

    i = -1;

    p_tree->TransactionAborted();
    INCREMENT_STAT( nAborts );
    INCREMENT_STAT( nUpdateAborts );
    p_tree->InitiateTransaction();
  }

  p_tree->CommitTransaction();
}

int doTortureLookup( ConcurrentTree * p_tree ) {
  int keys[SMALL_TRANSACTION_SIZE];
  p_tree->InitiateTransaction();

  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {
    keys[i] = rand_r(&seed) % NUM_ELEMENTS;
  }

  int sum = 0;
  int data;
  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {

    if( p_tree->TransactionalLookup( data, keys[i] ) ) {
      /* TRUE: Abort this transaction */
      sum = 0;
      i = -1;
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nLookupAborts );
      p_tree->InitiateTransaction();
    } else {
      /* FALSE: Don't abort */
		// NOT_IN_TREE bug fix
		if (data != NOT_IN_TREE)
			sum += data;
    }
  }

  p_tree->CommitTransaction();
  return sum;
}

void doTortureConditionalAdd( ConcurrentTree * p_tree ) {
  p_tree->InitiateTransaction();
  int key = rand_r(&seed) % NUM_ELEMENTS;
  int data;
  while( 1 ) {
    if( p_tree->TransactionalLookup( data, key ) ) {
      /* TRUE - abort */
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nCAddAborts );
      p_tree->InitiateTransaction();
      continue;
    }  

    if( data == NOT_IN_TREE ) {
      if( p_tree->TransactionalSet( key, 0 ) ) {
        /* TRUE - abort */
        p_tree->TransactionAborted();
        INCREMENT_STAT( nAborts );
        INCREMENT_STAT( nCAddAborts );
        p_tree->InitiateTransaction();
        continue;
      } else {
        break;
      } 
    } else {
      break; // done
    }
  }

  p_tree->CommitTransaction();
}

void doTortureConditionalRemove( ConcurrentTree * p_tree ) {
  p_tree->InitiateTransaction();
  int key = rand_r(&seed) % NUM_ELEMENTS;
  int data;
  while( 1 ) {
    if( p_tree->TransactionalLookup( data, key ) ) {
      /* TRUE - abort */
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nCRemoveAborts );
      p_tree->InitiateTransaction();
      continue;
    }  

    if( data == 0 ) {
      if( p_tree->TransactionalRemove( key ) ) {
        /* TRUE - abort */
        p_tree->TransactionAborted();
        INCREMENT_STAT( nAborts );
        INCREMENT_STAT( nCRemoveAborts );
        p_tree->InitiateTransaction();
        continue;
      } else {
        break;
      } 
    } else {
      break; // done
    }
  }
  p_tree->CommitTransaction();
}

int doScan( ConcurrentTree * p_tree ) {
  p_tree->InitiateTransaction();

  int sum = 0;
  for( int key = 0; key < NUM_ELEMENTS; key ++ ) {
    assert(key >= 0 && key < NUM_ELEMENTS );
    int data;

    if( p_tree->TransactionalLookup( data, key ) ) {
      /* TRUE -> Need to abort */
      sum = 0;
      key = -1;
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nScanAborts );
      p_tree->InitiateTransaction();
    } else {
      /* FALSE -> Transactional Lookup didn't lead to an abort */
		// NOT_IN_TREE bug fix
		if (data != NOT_IN_TREE)
			sum += data;
    }
  }

  p_tree->CommitTransaction();
  return sum;
}

void doUpdate( ConcurrentTree * p_tree ) {
  int keys[SMALL_TRANSACTION_SIZE];
  
  p_tree->InitiateTransaction();

  map<int,int> log;
  map<int,int>::iterator iter;

  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {
    keys[i] = rand_r(&seed) % NUM_ELEMENTS;
  }

  int data, newdata;

  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {

    usleep( INTER_ATOMIC_SLEEP_TIME );

    if( !p_tree->TransactionalLookup( data, keys[i] ) ) {
      /* FALSE: Don't abort... yet */
		// NOT_IN_TREE bug fix
		if (data == NOT_IN_TREE)
			data = 0;

      if( i % 2 ) {
        newdata = data + 1;
      } else {
        newdata = data - 1;
      }

      usleep( INTER_ATOMIC_SLEEP_TIME );

      if( !p_tree->TransactionalSet( keys[i], newdata ) ) {
        /* FALSE: Don't abort on this iteration, but log this access */

        iter = log.find( keys[i] );
        if( iter == log.end() ) {
          log[keys[i]] = data; // log the old value
        }

        continue; // don't abort
        
      }
    } 
    
    /* at least one was true... Abort... */

    for( iter = log.begin(); iter != log.end(); iter++ ) {
      int key = iter->first;
      int orig_data = iter->second;
      if( p_tree->TransactionalSet( key, orig_data ) ) {
        /* SHOULD NEVER happen, since we haven't released isolation yet */
        cout << "Explicit violation! Failed to acquire isolation on previously-isolated data!" << endl; 
        assert(0);
      }
    }

    log.clear();

    i = -1;

    p_tree->TransactionAborted();
    INCREMENT_STAT( nAborts );
    INCREMENT_STAT( nUpdateAborts );
    p_tree->InitiateTransaction();
  }

  p_tree->CommitTransaction();
}

int doLookup( ConcurrentTree * p_tree ) {
  int keys[SMALL_TRANSACTION_SIZE];
  p_tree->InitiateTransaction();

  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {
    keys[i] = rand_r(&seed) % NUM_ELEMENTS;
  }

  int sum = 0;
  int data;
  for( int i=0;i<SMALL_TRANSACTION_SIZE;i++ ) {

    usleep( INTER_ATOMIC_SLEEP_TIME );

    if( p_tree->TransactionalLookup( data, keys[i] ) ) {
      /* TRUE: Abort this transaction */
      sum = 0;
      i = -1;
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nLookupAborts );
      p_tree->InitiateTransaction();
    } else {
      /* FALSE: Don't abort */
		// NOT_IN_TREE bug fix
		if (data != NOT_IN_TREE)
			sum += data;
    }
  }

  p_tree->CommitTransaction();
  return sum;
}

void doConditionalAdd( ConcurrentTree * p_tree ) {
  p_tree->InitiateTransaction();
  int key = rand_r(&seed) % NUM_ELEMENTS;
  int data;
  while( 1 ) {

    usleep( INTER_ATOMIC_SLEEP_TIME );

    if( p_tree->TransactionalLookup( data, key ) ) {
      /* TRUE - abort */
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nCAddAborts );
      p_tree->InitiateTransaction();
      continue;
    }  

    usleep( INTER_ATOMIC_SLEEP_TIME );

    if( data == NOT_IN_TREE ) {
      if( p_tree->TransactionalSet( key, 0 ) ) {
        /* TRUE - abort */
        p_tree->TransactionAborted();
        INCREMENT_STAT( nAborts );
        INCREMENT_STAT( nCAddAborts );
        p_tree->InitiateTransaction();
        continue;
      } else {
        break;
      } 
    } else {
      break; // done
    }
  }

  p_tree->CommitTransaction();
}

void doConditionalRemove( ConcurrentTree * p_tree ) {
  p_tree->InitiateTransaction();
  int key = rand_r(&seed) % NUM_ELEMENTS;
  int data;
  while( 1 ) {
    usleep( INTER_ATOMIC_SLEEP_TIME );

    if( p_tree->TransactionalLookup( data, key ) ) {
      /* TRUE - abort */
      p_tree->TransactionAborted();
      INCREMENT_STAT( nAborts );
      INCREMENT_STAT( nCRemoveAborts );
      p_tree->InitiateTransaction();
      continue;
    }  

    usleep( INTER_ATOMIC_SLEEP_TIME );

    if( data == 0 ) {
      if( p_tree->TransactionalRemove( key ) ) {
        /* TRUE - abort */
        p_tree->TransactionAborted();
        INCREMENT_STAT( nAborts );
        INCREMENT_STAT( nCRemoveAborts );
        p_tree->InitiateTransaction();
        continue;
      } else {
        break;
      } 
    } else {
      break; // done
    }
  }
  p_tree->CommitTransaction();
}
