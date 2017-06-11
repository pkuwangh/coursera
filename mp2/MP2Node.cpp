/**********************************
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 **********************************/
#include <cassert>
#include "MP2Node.h"

/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
    // need initialize ring
    this->initialized = false;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
}

/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	vector<Node> curMemList;

	/*
	 *  Step 1. Get the current membership list from Membership Protocol / MP1
	 */
	curMemList = getMembershipList();

	/*
	 * Step 2: Construct the ring
	 */
	// Sort the list based on the hashCode
	sort(curMemList.begin(), curMemList.end());

    // wangh
    ring = curMemList;

	/*
	 * Step 3: Run the stabilization protocol IF REQUIRED
	 */
    stabilizationProtocol();
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 *
 * RETURNS:
 * size_t position on the ring
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}

void MP2Node::unicast(KVStoreMessage kvMsg, Address& toAddr) {
    Address* fromAddr = &(this->memberNode->addr);
    char* msgStr = const_cast<char*>(kvMsg.toString().c_str());
    this->emulNet->ENsend(fromAddr, &toAddr, msgStr, strlen(msgStr));
}

void MP2Node::multicast(KVStoreMessage kvMsg, vector<Node>& toNodes) {
    Address* fromAddr = &(this->memberNode->addr);
    for (uint32_t i = 0; i < toNodes.size(); ++i) {
        char* msgStr = const_cast<char*>(kvMsg.toString().c_str());
        this->emulNet->ENsend(fromAddr, &(toNodes[i].nodeAddress), msgStr, strlen(msgStr));
    }
}

void MP2Node::updateInflightTrans() {
    auto iter = inflightTrans.begin();
    while (iter != inflightTrans.end()) {
        if ( (par->getcurrtime() - iter->lTimeStamp) > TIMEOUT_THD ) {
            auto l_addr = this->memberNode->addr;
            auto l_id = iter->gTransId;
            auto l_key = iter->key;
            auto l_val = iter->val.second;
            // timeout
            switch(iter->transType) {
                case MessageType::CREATE: log->logCreateFail(&l_addr, true, l_id, l_key, l_val); break;
                case MessageType::DELETE: log->logDeleteFail(&l_addr, true, l_id, l_key); break;
                case MessageType::READ: log->logReadFail(&l_addr, true, l_id, l_key); break;
                default: break;
            }
            inflightTrans.erase(iter++);
        } else {
            ++iter;
        }
    }
}

/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientCreate(string key, string value) {
    // wangh
    ++ g_transID;
    // find replicas
    vector<Node> nodes = findNodes(key);
    assert(nodes.size() == NUM_REPLICAS);
    // construct and send messages
    Address* fromAddr = &(this->memberNode->addr);
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        KVStoreMessage newMsg (
                KVStoreMessage::QUERY,
                Message(g_transID, *fromAddr, MessageType::CREATE, key, value,
                        static_cast<ReplicaType>(i)) );
        unicast(newMsg, nodes[i].nodeAddress);
    }
    // logging
    Transaction tran(g_transID, par->getcurrtime(), QUORUM_THD, MessageType::CREATE, key, value);
    inflightTrans.push_front(tran);
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
    // wangh
    ++ g_transID;
    vector<Node> nodes = findNodes(key);
    assert(nodes.size() == NUM_REPLICAS);
    KVStoreMessage newMsg (
            KVStoreMessage::QUERY,
            Message(g_transID, this->memberNode->addr, MessageType::READ, key) );
    multicast(newMsg, nodes);
    // logging
    Transaction tran(g_transID, par->getcurrtime(), QUORUM_THD, MessageType::READ, key, "");
    inflightTrans.push_front(tran);
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
    // wangh
    ++ g_transID;
    vector<Node> nodes = findNodes(key);
    assert(nodes.size() == NUM_REPLICAS);
    Address* fromAddr = &(this->memberNode->addr);
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        KVStoreMessage newMsg (
                KVStoreMessage::QUERY,
                Message(g_transID, *fromAddr, MessageType::UPDATE, key, value,
                        static_cast<ReplicaType>(i)) );
        unicast(newMsg, nodes[i].nodeAddress);
    }
    // logging
    Transaction tran(g_transID, par->getcurrtime(), QUORUM_THD, MessageType::UPDATE, key, value);
    inflightTrans.push_front(tran);
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key) {
    // wangh
    ++ g_transID;
    vector<Node> nodes = findNodes(key);
    assert(nodes.size() == NUM_REPLICAS);
    KVStoreMessage newMsg (
            KVStoreMessage::QUERY,
            Message(g_transID, this->memberNode->addr, MessageType::DELETE, key) );
    multicast(newMsg, nodes);
    // logging
    Transaction tran(g_transID, par->getcurrtime(), QUORUM_THD, MessageType::DELETE, key, "");
    inflightTrans.push_front(tran);
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, ReplicaType replica) {
    // wangh
	// Insert key, value, replicaType into the hash table
    Entry newEntry(value, par->getcurrtime(), replica);
    keyEntryMap.insert(make_pair(key, newEntry));
    return true;
}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key) {
    // wangh
	// Read key from local hash table and return value
    auto iter = keyEntryMap.find(key);
    if (iter != keyEntryMap.end()) {
        return iter->second.convertToString();
    } else {
        return "";
    }
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, ReplicaType replica) {
	/*
	 * Implement this
	 */
	// Update key in local hash table and return true or false
    return true;
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key) {
    // wangh
	// Delete the key from the local hash table
    if (keyEntryMap.erase(key))
        return true;
    else
        return false;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {
	char * data;
	int size;
	// dequeue all messages and handle them
	while ( !memberNode->mp2q.empty() ) {
		/*
		 * Pop a message from the queue
		 */
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();

		string message(data, data + size);

        // wangh
        KVStoreMessage newMsg(message);
        dispatchMessages(newMsg);
	}

	/*
	 * This function should also ensure all READ and UPDATE operation
	 * get QUORUM replies
	 */
}

/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (unsigned int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
        // check timeout-ed transaction
        updateInflightTrans();
        // receiving messages
    	return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

void MP2Node::dispatchMessages(KVStoreMessage kvMsg) {
    // wangh
    if (kvMsg.kvMsgType == KVStoreMessage::QUERY) {
        switch(kvMsg.type) {
            // server side
            case MessageType::CREATE: handleKeyCreate(kvMsg); break;
            case MessageType::DELETE: handleKeyDelete(kvMsg); break;
            case MessageType::READ: handleKeyRead(kvMsg); break;
            // client side
            case MessageType::REPLY: handleReply(kvMsg); break;
            case MessageType::READREPLY: handleReadReply(kvMsg); break;
            default: break;
        }
    } else if (kvMsg.kvMsgType == KVStoreMessage::UPDATE) {
        handleReplicateUpdate(kvMsg);
    } else {
        // corrupted packet
        return;
    }
}

void MP2Node::handleReply(Message msg) {
    auto l_id = msg.transID;
    list<Transaction>::iterator iter;
    for (iter = inflightTrans.begin(); iter != inflightTrans.end(); ++iter) {
        if (iter->gTransId == l_id) {
            break;
        }
    }
    if (iter == inflightTrans.end()) {
        // transaction has been dropped due to timeout
        return;
    } else if (!msg.success) {
        // operation failed
        return;
    } else if (--(iter->quorum_count) == 0) {
        switch(iter->transType) {
            case MessageType::CREATE: log->logCreateSuccess(&memberNode->addr, true, l_id, iter->key, iter->val.second); break;
            case MessageType::DELETE: log->logDeleteSuccess(&memberNode->addr, true, l_id, iter->key); break;
            default: break;
        }
        inflightTrans.erase(iter);
        return;
    } else {
        return;
    }
}

void MP2Node::handleReadReply(Message msg) {
    // extract message
    string value = msg.value;
    if (value.empty()) return;
    vector<string> tuple;
    int start = 0, pos = 0;
    while ((pos = value.find(":", start)) != (int)string::npos) {
        string token = value.substr(start, pos-start);
        tuple.push_back(token);
        start = pos + 1;
    }
    tuple.push_back(value.substr(start));
    assert(tuple.size() == 3);
    string retVal = tuple[0];
    int timestamp = stoi(tuple[1]);
    int l_id = msg.transID;
    auto iter = inflightTrans.begin();
    for ( ; iter != inflightTrans.end(); ++iter) {
        if (iter->gTransId == l_id) break;
    }

    if (iter == inflightTrans.end()) {
        // transaction has been dropped due to timeout
        return;
    } else if (--(iter->quorum_count) == 0) {
        log->logReadSuccess(&memberNode->addr, true, l_id, iter->key, iter->val.second);
        inflightTrans.erase(iter);
    } else {
        if (timestamp >= iter->val.first) {
            iter->val = make_pair(timestamp, retVal);
        }
    }
}

void MP2Node::handleKeyCreate(Message msg) {
    auto l_id = msg.transID;
    auto l_addr = this->memberNode->addr;
    auto l_key = msg.key;
    auto l_value = msg.value;
    KVStoreMessage retMsg (
            KVStoreMessage::QUERY, Message(l_id, l_addr, MessageType::REPLY, false) );

    if (createKeyValue(l_key, l_value, msg.replica)) {
        retMsg.success = true;
        log->logCreateSuccess(&l_addr, false, l_id, l_key, l_value);
    } else {
        retMsg.success = false;
        log->logCreateFail(&l_addr, false, l_id, l_key, l_value);
    }
    unicast(retMsg, msg.fromAddr);
}

void MP2Node::handleKeyDelete(Message msg) {
    auto l_id = msg.transID;
    auto l_addr = this->memberNode->addr;
    auto l_key = msg.key;
    KVStoreMessage retMsg (
            KVStoreMessage::QUERY, Message(l_id, l_addr, MessageType::REPLY, false) );

    if (deletekey(l_key)) {
        retMsg.success = true;
        log->logDeleteSuccess(&l_addr, false, l_id, l_key);
    } else {
        retMsg.success = false;
        log->logDeleteFail(&l_addr, false, l_id, l_key);
    }
    unicast(retMsg, msg.fromAddr);
}

void MP2Node::handleKeyRead(Message msg) {
    auto l_id = msg.transID;
    auto l_addr = this->memberNode->addr;
    auto l_key = msg.key;
    // read
    string retVal = readKey(l_key);
    if (retVal.empty()) {
        log->logReadFail(&l_addr, false, l_id, l_key);
    } else {
        log->logReadSuccess(&l_addr, false, l_id, l_key, retVal);
    }
    KVStoreMessage retMsg(KVStoreMessage::QUERY, Message(l_id, l_addr, retVal));
    unicast(retMsg, msg.fromAddr);
}

void MP2Node::handleReplicate(ReplicaType repType, Node& toNode) {
    for (auto iter = keyEntryMap.begin(); iter != keyEntryMap.end(); ++iter) {
        if (iter->second.replica == repType) {
            KVStoreMessage newMsg (
                    KVStoreMessage::UPDATE,
                    Message(-1, memberNode->addr, MessageType::CREATE, iter->first,
                        iter->second.value, ReplicaType::TERTIARY) );
            unicast(newMsg, toNode.nodeAddress);
        }
    }
}

void MP2Node::handleReplicateUpdate(Message msg) {
    createKeyValue(msg.key, msg.value, msg.replica);
}

/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol() {
    // wangh
    unsigned int idx = 0;
    // find myself in the ring
    for (idx = 0; idx < ring.size(); ++idx) {
        if (ring[idx].nodeAddress == this->memberNode->addr) {
            break;
        }
    }
    // initialize neighbors
    int p_2 = (idx+ring.size()-2) % ring.size();
    int p_1 = (idx+ring.size()-1) % ring.size();
    int n_1 = (idx+1) % ring.size();
    int n_2 = (idx+2) % ring.size();

    if (!initialized) {
        haveReplicasOf.push_back(ring[p_2]);
        haveReplicasOf.push_back(ring[p_1]);
        hasMyReplicas.push_back(ring[n_1]);
        hasMyReplicas.push_back(ring[n_2]);
        initialized = true;
    } else {
        Node n1 = ring[n_1];
        Node n2 = ring[n_2];
        Node n3 = ring[p_1];
        if (!(n1.nodeAddress == hasMyReplicas[0].nodeAddress)) {
            // next node in the ring has failed
            handleReplicate(ReplicaType::PRIMARY, n2);
        } else if (!(n2.nodeAddress == hasMyReplicas[1].nodeAddress)) {
            // next next node in the ring has failed
            handleReplicate(ReplicaType::PRIMARY, n2);
        } else if (!(n3.nodeAddress == haveReplicasOf[0].nodeAddress)) {
            // previous node in the ring has failed
            handleReplicate(ReplicaType::SECONDARY, n2);
        } else {
        }
        // update tables
        haveReplicasOf.clear();
        hasMyReplicas.clear();
        haveReplicasOf.push_back(ring[p_2]);
        haveReplicasOf.push_back(ring[p_1]);
        hasMyReplicas.push_back(ring[n_1]);
        hasMyReplicas.push_back(ring[n_2]);
    }
}
