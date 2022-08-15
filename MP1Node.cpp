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
    initMemberListTable(this->memberNode);
    srand(time(NULL));
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
	/*
	 * This function is partially implemented and may require changes
	 */

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
	char* msg;
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

        size_t msgsize = sizeof(short) + sizeof(joinaddr->addr) + sizeof(long);

        msg = (char *) malloc(msgsize * sizeof(char));
        
        // create JOINREQ message: format of data is {struct Address myaddr}
        short type = JOINREQ;
        memcpy(msg, &type, sizeof(short));
        memcpy(msg + sizeof(short), memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy(msg + sizeof(short)+ sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));


#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
    return -1;
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

    //increase timestamp:
    memberNode->heartbeat++;

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
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
    short type;
    Address address;
    long heartbeat;

    memcpy(&type, data, sizeof(short));
    memcpy(&address.addr, data + sizeof(short), sizeof(address.addr));
    memcpy(&heartbeat, data + sizeof(short) + sizeof(address.addr), sizeof(long));

    if (type == JOINREQ) {
        handleJoinReq(data+sizeof(type), size-sizeof(type));
    }
    else if (type == JOINREP) {
        memberNode->inGroup = true;
        handleGossip(data, size);
    }
    else if (type == GOSSIP) {
        handleGossip(data, size);
    }

    return true;

}

void MP1Node::handleJoinReq(char *msg, int size) {
    long heartbeat;
    Address addr;

    int id = 0;
    short port;
    memcpy(&id, &msg[0], sizeof(int));
    memcpy(&port, &msg[4], sizeof(short));
    memcpy(&addr, &msg[0], sizeof(memberNode->addr.addr));
    memcpy(&heartbeat, (char *)msg + sizeof(memberNode->addr.addr), sizeof(heartbeat));

    MemberListEntry entry(id, port, heartbeat, memberNode->heartbeat);
    if(!isMember(entry)) {
        memberNode->memberList.push_back(entry);
        memberNode->nnb++;
        log->logNodeAdd(&memberNode->addr, &addr);
        sendJoinRep(addr);
    }
}

void MP1Node::sendJoinRep(Address addr) {
    char *msg;
    short type = JOINREP;
    int msgSize = sizeof(type) + sizeof(memberNode->addr.addr) + sizeof(long);
    msg = (char *) malloc(msgSize * sizeof(char));
    memcpy(msg, &type, sizeof(short));
    memcpy(msg + sizeof(short), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy(msg + sizeof(short) + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

    emulNet->ENsend(&memberNode->addr, &addr, (char *)msg, msgSize);
    free(msg);
}

void MP1Node::sendGossip(Address address) {

    size_t initSize = sizeof(short) + sizeof(address.addr) + sizeof(long);

    char* msg = (char *) malloc(initSize * sizeof(char));
    short type = GOSSIP;

    memcpy(msg, &type, sizeof(short));
    memcpy(msg + sizeof(short), &memberNode->addr.addr, sizeof(address.addr));
    memcpy(msg + sizeof(short)+ sizeof(address.addr), &(memberNode->heartbeat), sizeof(long));
   
    size_t messageSize = initSize;
    for (int i = 0 ; i < memberNode->memberList.size(); i++) {
        if (memberNode->heartbeat - memberNode->memberList[i].gettimestamp() > TFAIL)
            continue;

        Address memberAddr;
        memcpy(&memberAddr.addr[0], &memberNode->memberList[i].id, sizeof(int));
        memcpy(&memberAddr.addr[4], &memberNode->memberList[i].port, sizeof(short));
        
        if (strcmp(memberAddr.addr, address.addr) != 0) {

            size_t newAddedSize = sizeof(memberAddr.addr) + sizeof(long);
            msg = (char*) realloc(msg, messageSize + newAddedSize);
            if(msg == NULL) {
                free(msg);
                return;
            }
            memcpy(msg+messageSize, &(memberAddr.addr), sizeof(memberAddr.addr));
            memcpy(msg+messageSize+sizeof(memberAddr.addr), &(memberNode->memberList[i].heartbeat), sizeof(long));
            messageSize += newAddedSize;
            
        }
    }
    emulNet->ENsend(&memberNode->addr, &address, (char *)msg, messageSize);
    free(msg);
}

void MP1Node::handleGossip(char* msg, int msgSize) {
    vector<MemberListEntry> msgMemberList = getMemberListFromMsg(msg, msgSize);

    for (int i = 0 ; i < msgMemberList.size(); i++) {
        if (!isMember(msgMemberList[i])) {
            memberNode->memberList.push_back(msgMemberList[i]);
            memberNode->nnb++;
            Address address;
            memcpy(&address.addr[0], &msgMemberList[i].id, sizeof(int));
		    memcpy(&address.addr[4], &msgMemberList[i].port, sizeof(short));
            log->logNodeAdd(&memberNode->addr, &address);
        }
        else {
            int j = findMemberEntry(msgMemberList[i]);
            if (memberNode->memberList[j].getheartbeat() < msgMemberList[i].getheartbeat() 
                    &&  memberNode->heartbeat - memberNode->memberList[j].gettimestamp() <= TFAIL) {
                memberNode->memberList[j].setheartbeat(msgMemberList[i].getheartbeat());
                memberNode->memberList[j].settimestamp(memberNode->heartbeat);
            }
        }
    }
}

vector<MemberListEntry> MP1Node::getMemberListFromMsg(char* msg, int msgSize) {
    vector<MemberListEntry> memberList;

    size_t headerSize = sizeof(short);

    for( size_t i = headerSize ; i+sizeof(memberNode->addr.addr)+sizeof(memberNode->heartbeat) <= msgSize ; i += sizeof(memberNode->addr.addr)+sizeof(memberNode->heartbeat) ) {
        int id;
        short port;
        long heartbeat;
        memcpy(&id, msg+i, sizeof(int));
        memcpy(&port, msg+i+sizeof(int), sizeof(short));
        memcpy(&heartbeat, msg+i+sizeof(int)+sizeof(short), sizeof(long));
        MemberListEntry entry(id, port, heartbeat, memberNode->heartbeat);

        memberList.push_back(entry);
    }
    return memberList;
}

bool MP1Node::isMember(MemberListEntry entry) {
   for(auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        if(it->id == entry.id) {
            return true;
        }
    }
    return false;
}

int MP1Node::findMemberEntry(MemberListEntry entry) {
    for(int i = 0 ; i < memberNode->memberList.size(); i++) {
        if (memberNode->memberList[i].id == entry.id && memberNode->memberList[i].port == entry.port) 
            return i;
    }
    return -1;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

    // Node Timeout
    int size = memberNode->memberList.size();
    for (int i = 0 ; i < size; i++) {
        if(memberNode->heartbeat - memberNode->memberList[i].gettimestamp() >= TREMOVE) {
            Address memberAddr;
            memcpy(&memberAddr.addr[0], &memberNode->memberList[i].id, sizeof(int));
            memcpy(&memberAddr.addr[4], &memberNode->memberList[i].port, sizeof(short));

            memberNode->memberList.erase(memberNode->memberList.begin() + i);
            i--;
            memberNode->nnb--;
            size--;

            log->logNodeRemove(&memberNode->addr, &memberAddr);          
        }
    }


    // Gossip Protocol
    vector<int> kRandomNodes;
    while(kRandomNodes.size() != GOSSIP_FANOUT && kRandomNodes.size() != memberNode->memberList.size()) {
        int number = rand() % memberNode->memberList.size();
        if (!count(kRandomNodes.begin(), kRandomNodes.end(), number)) {
            kRandomNodes.push_back(number);
        }
    }

    for (int i = 0 ; i < kRandomNodes.size(); i++) {
        MemberListEntry entry =  memberNode->memberList[kRandomNodes[i]];
        Address memberAddr;
        memcpy(&memberAddr.addr[0], &entry.id, sizeof(int));
        memcpy(&memberAddr.addr[4], &entry.port, sizeof(short));
        sendGossip(memberAddr);
    }
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
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
