/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
    for( int i = 0; i < 6; i++ ) {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
        return false;
    }
    else {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
    Queue q;
    return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
    // wangh
//    int id = *(int*)(&memberNode->addr.addr);
//    int port = *(short*)(&memberNode->addr.addr[4]);

    memberNode->bFailed = false;
    memberNode->inited = true;
    memberNode->inGroup = false;
    // node is up!
    memberNode->nnb = 0;
    memberNode->heartbeat = 0;
    memberNode->pingCounter = TFAIL;
    memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
    MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // msg type, addr, 1B, heart beat
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    log->logNodeAdd(&memberNode->addr, &memberNode->addr);
    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode() {
    // wangh
    memberNode->inGroup = false;
    memberNode->inited = false;
    memberNode->heartbeat = 0;
    memberNode->memberList.clear();
    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
        return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
        return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size) {
    // wangh
    // get type, id, and port of the incoming message
    MsgTypes msg_type = ((MessageHdr*)data)->msgType;
    int id;
    short port;
    Address sender;
    memcpy(&(sender.addr), data+sizeof(MsgTypes), sizeof(sender.addr));
    memcpy(&id, data+sizeof(MsgTypes), sizeof(int));
    memcpy(&port, data+sizeof(MsgTypes)+sizeof(int), sizeof(short));

    if (msg_type == JOINREQ) {
        // extract heartbeat
        long heartbeat;
        size_t offset = sizeof(MsgTypes) + sizeof(int) + sizeof(short) + 1;
        memcpy(&heartbeat, data+offset, sizeof(long));
#ifdef DEBUGLOG
        static char s[1024];
        sprintf(s, "Received JOINREQ from id=%d port=%hd", id, port);
        log->LOG(&memberNode->addr, s);
#endif
        // add to membership list
        bool found = false;
        for (auto &entry : memberNode->memberList) {
            if (entry.getid() == id) {
                found = true;
                // update info
                entry.setport(port);
                entry.setheartbeat(heartbeat);
                entry.settimestamp(par->getcurrtime());
                break;
            }
        }
        // add new entry
        if (!found) {
            MemberListEntry newEntry;
            newEntry.setid(id);
            newEntry.setport(port);
            newEntry.setheartbeat(heartbeat);
            newEntry.settimestamp(par->getcurrtime());

            memberNode->memberList.push_back(newEntry);
            log->logNodeAdd(&memberNode->addr, &sender);
        }
        // send a message back
        MessageHdr *msg;
        size_t memberListSize = sizeof(MemberListEntry) * memberNode->memberList.size();
        size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + 1 + memberListSize;
        msg = (MessageHdr*) malloc(msgSize * sizeof(char));
        // type header
        msg->msgType = JOINREP;
        offset = sizeof(MessageHdr);
        // addr
        memcpy((char*)msg + offset, &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        offset += sizeof(memberNode->addr.addr) + 1;
        // member list
        memcpy((char*)msg + offset, serializeMemberList(), memberListSize);
        emulNet->ENsend(&memberNode->addr, &sender, (char*)msg, msgSize);
        free(msg);
    } else if (msg_type == JOINREP) {
        // response from introducer, update membership list
        memberNode->memberList = deserializeMemberList(data, size);
        memberNode->inGroup = true;
#ifdef DEBUGLOG
        static char s[1024];
        sprintf(s, "Received JOINREP from id=%d port=%hd", id, port);
        log->LOG(&memberNode->addr, s);
#endif
        for (auto &x_entry : memberNode->memberList) {
            Address x_addr = makeAddress(x_entry.getid(), x_entry.getport());
            log->logNodeAdd(&memberNode->addr, &x_addr);
        }
    } else if (msg_type == GOSSIP) {
        auto x_memberList = deserializeMemberList(data, size);
        int m_id = *(int*)(&memberNode->addr);
        for (auto &x_entry : x_memberList) {
            if (x_entry.getid() == m_id) {
                // myself
                continue;
            }
            auto m_entry_iter = std::find_if(memberNode->memberList.begin(),
                                        memberNode->memberList.end(),
                                        [&x_entry](MemberListEntry &entry)->bool {
                                            return x_entry.getid() == entry.getid();
                                        });
            if (m_entry_iter != memberNode->memberList.end()) {
                if (m_entry_iter->getheartbeat() < x_entry.getheartbeat()) {
                    m_entry_iter->settimestamp(x_entry.gettimestamp());
                    m_entry_iter->setheartbeat(x_entry.getheartbeat());
                }
            } else {
                // add new entry
                if (par->getcurrtime() - x_entry.gettimestamp() < par->EN_GPSZ*2) {
                    memberNode->memberList.push_back(x_entry);
                    Address x_addr = makeAddress(x_entry.getid(), x_entry.getport());
                    log->logNodeAdd(&memberNode->addr, &x_addr);
                }
            }
        }
    }
    return true;
}

Address MP1Node::makeAddress(int id, short port) const {
    Address ret_addr;
    memcpy(&ret_addr, &id, sizeof(int));
    memcpy((char*)(&ret_addr) + sizeof(int), &port, sizeof(short));
    return ret_addr;
}

char* MP1Node::serializeMemberList() {
    size_t numEntry = memberNode->memberList.size();
    size_t entrySize = sizeof(MemberListEntry);
    MemberListEntry *ret = (MemberListEntry*) malloc(numEntry*entrySize*sizeof(char));
    for (size_t i = 0; i < numEntry; ++i) {
        memcpy((char*)(ret+i), &(memberNode->memberList[i]), entrySize);
    }
    return (char*)ret;
}

vector<MemberListEntry> MP1Node::deserializeMemberList(char *data, size_t size) {
    size_t offset = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + 1;
    int listSize = size - offset;
    vector<MemberListEntry> ret;
    while (listSize > 0) {
        MemberListEntry *entry = (MemberListEntry*)(data + offset);
        ret.push_back(*entry);
        offset += sizeof(MemberListEntry);
        listSize -= sizeof(MemberListEntry);
    }
    return ret;
}

void MP1Node::sendGossip() {
    int randomId = rand() % memberNode->memberList.size();
    auto &m_entry = memberNode->memberList[randomId];
    Address toAddr = makeAddress(m_entry.getid(), m_entry.getport());
    // make a gossip message
    size_t memberListSize = sizeof(MemberListEntry) * memberNode->memberList.size();
    size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + 1 + memberListSize;
    MessageHdr *msg = (MessageHdr*) malloc(msgSize * sizeof(char));
    // type header
    msg->msgType = GOSSIP;
    size_t offset = sizeof(MessageHdr);
    // addr
    memcpy((char*)msg + offset, &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    offset += sizeof(memberNode->addr.addr) + 1;
    // member list
    memcpy((char*)msg + offset, serializeMemberList(), memberListSize);
    emulNet->ENsend(&memberNode->addr, &toAddr, (char*)msg, msgSize);
    free (msg);
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
    // wangh
    // scan for dead node
    for (size_t i = 0; i < memberNode->memberList.size(); ++i) {
        auto entry = memberNode->memberList[i];
        if (entry.getid() == *(int*)(&memberNode->addr)) {
            // myself, remove for now
            memberNode->memberList.erase(memberNode->memberList.begin() + i);
            -- i;
        } else {
            if (par->getcurrtime() - entry.gettimestamp() > par->EN_GPSZ*2+20) {
                Address x_addr = makeAddress(entry.getid(), entry.getport());
                log->logNodeRemove(&memberNode->addr, &x_addr);
                memberNode->memberList.erase(memberNode->memberList.begin() + i);
                -- i;
            }
        }
    }
    // increment heartbeat & add to member list
    ++ memberNode->heartbeat;
    int m_id = *(int*)(&memberNode->addr.addr);
    short m_port = *(short*)(&memberNode->addr.addr[4]);
    MemberListEntry m_entry(m_id, m_port, memberNode->heartbeat, par->getcurrtime());
    memberNode->memberList.push_back(m_entry);
    // pick a neighbor and send gossip
    sendGossip();
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
    return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
    memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
            addr->addr[3], *(short*)&addr->addr[4]);
}
