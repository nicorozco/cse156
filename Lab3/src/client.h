// H file for the client side
#include <cstdint>
#include <iostream> 
#pragma pack(push,1)

const uint32_t EOF_SEQ = UINT32_MAX;
//structure for UDP packet
struct __attribute__((packed)) UDPPacket {
	uint32_t sequenceNumber;
	uint16_t payloadSize;
	char data[1466];
};
#pragma pack(pop)
constexpr size_t MSS = sizeof(UDPPacket{}.data);
struct filePathPacket{
	char filepath[256];
};

struct ACKPacket{
	uint32_t sequenceNumber;
};

bool isValidIPv4Format(const std::string& ip);

int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file);

