/**********************************
 * FILE NAME: MP2Node.h
 *
 * DESCRIPTION: MP2Node class header file
 **********************************/

#ifndef MP2NODE_H_
#define MP2NODE_H_

/**
 * Header files
 */
#include "stdincludes.h"
#include "EmulNet.h"
#include "Node.h"
#include "HashTable.h"
#include "Log.h"
#include "Params.h"
#include "Message.h"
#include "Queue.h"
#include <list>

#define NUM_REPLICAS 3
#define QUORUM_THD (NUM_REPLICAS/2+1)
#define TIMEOUT_THD 20

/** CLASS NAME: KVStoreMessage
 *
 * DESCRIPTION: This class extends Message to facilitate replica management
 */
class KVStoreMessage : public Message {
  public:
    enum KVStoreMessageType {
        UPDATE,
        QUERY
    };

    static string stripKVHeader(string message) {
        int pos = message.find('@');
        return message.substr(pos+1);
    }

    KVStoreMessage(string message) :
        Message(stripKVHeader(message))
    {
        int header = stoi(message.substr(0, message.find('@')));
        kvMsgType = static_cast<KVStoreMessageType>(header);
    }

    KVStoreMessage(KVStoreMessageType kv_type, string message) :
        Message(message),
        kvMsgType(kv_type)
    { }

    KVStoreMessage(KVStoreMessageType kv_type, const Message& message) :
        Message(message),
        kvMsgType(kv_type)
    { }

    string toString() {
        return to_string(kvMsgType) + '@' + Message::toString();
    }

    KVStoreMessageType kvMsgType;
};

/** CLASS NAME: Transaction
 *
 * DESCRIPTION: This class includes transaction information
 */
class Transaction {
  public:
    int gTransId;           // global transaction id
    int lTimeStamp;         // local timestamp
    int quorum_count;       // required quorum count
    MessageType transType;  //
    string key;
    pair<int, string> val;

    Transaction(int g_id, int l_ts, int x_qc, MessageType x_type, string x_key, string x_val) :
        gTransId(g_id),
        lTimeStamp(l_ts),
        quorum_count(x_qc),
        transType(x_type),
        key(x_key),
        val(make_pair(0, x_val))
    { }
};

/**
 * CLASS NAME: MP2Node
 *
 * DESCRIPTION: This class encapsulates all the key-value store functionality
 * 				including:
 * 				1) Ring
 * 				2) Stabilization Protocol
 * 				3) Server side CRUD APIs
 * 				4) Client side CRUD APIs
 */
class MP2Node {
private:
	// Vector holding the next two neighbors in the ring who have my replicas
	vector<Node> hasMyReplicas;
	// Vector holding the previous two neighbors in the ring whose replicas I have
	vector<Node> haveReplicasOf;
	// Ring
	vector<Node> ring;
	// Hash Table
	HashTable * ht;
	// Member representing this member
	Member *memberNode;
	// Params object
	Params *par;
	// Object of EmulNet
	EmulNet * emulNet;
	// Object of Log
	Log * log;

    list<Transaction> inflightTrans;
    map<string, Entry> keyEntryMap;
    bool initialized;

    // client side message handler
    void handleReadReply(Message msg);
    void handleReply(Message msg);
    // server side message handler
    void handleKeyCreate(Message msg);
    void handleKeyUpdate(Message msg);
    void handleKeyDelete(Message msg);
    void handleKeyRead(Message msg);
    // transactions
    void unicast(KVStoreMessage kvMsg, Address& toAddr);
    void multicast(KVStoreMessage kvMsg, vector<Node>& toNodes);
    void updateInflightTrans();
    // stabilization protocol
    void handleReplicate(ReplicaType repType, Node& toNode);
    void handleReplicateUpdate(Message msg, vector<Node>& toNodes);

public:
	MP2Node(Member *memberNode, Params *par, EmulNet *emulNet, Log *log, Address *addressOfMember);
	Member * getMemberNode() {
		return this->memberNode;
	}

	// ring functionalities
	void updateRing();
	vector<Node> getMembershipList();
	size_t hashFunction(string key);
	void findNeighbors();

	// client side CRUD APIs
	void clientCreate(string key, string value);
	void clientRead(string key);
	void clientUpdate(string key, string value);
	void clientDelete(string key);

	// receive messages from Emulnet
	bool recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);

	// handle messages from receiving queue
	void checkMessages();

	// coordinator dispatches messages to corresponding nodes
	void dispatchMessages(KVStoreMessage kvMsg);

	// find the addresses of nodes that are responsible for a key
	vector<Node> findNodes(string key);

	// server
	bool createKeyValue(string key, string value, ReplicaType replica);
	string readKey(string key);
	bool updateKeyValue(string key, string value, ReplicaType replica);
	bool deletekey(string key);

	// stabilization protocol - handle multiple failures
	void stabilizationProtocol();

	~MP2Node();
};

#endif /* MP2NODE_H_ */
