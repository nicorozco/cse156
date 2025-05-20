// H file for the client side
#include <map>
#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <chrono> 
#pragma pack(push,1)

const uint32_t EOF_SEQ = 99999999;
//structure for UDP packet
struct __attribute__((packed)) UDPPacket {
	uint32_t sequenceNumber;
	uint16_t payloadSize;
	char data[];//utilize flexible array member 
};
#pragma pack(pop)
struct filePathPacket{
	char filepath[256];
};

struct ACKPacket{
	uint32_t sequenceNumber;
};
struct ClientState {
	uint32_t expectedSeqNum;//each client will have it's own expectedSeqNum
	std::unordered_map<int,UDPPacket> packetsRecieved; //and it's own buffred packets 
	std::chrono::steady_clock::time_point lastHeard;
};

bool isValidIPv4Format(const std::string& ip);

int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file,const std::map<uint32_t, std::pair<long, uint16_t>>& metaMap);

