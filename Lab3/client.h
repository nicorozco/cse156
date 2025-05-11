// H file for the client side
#include <cstdint>
const uint32_t EOF_SEQ = UINT32_MAX;
//structure for UDP packet
struct UDPPacket {
	uint32_t sequenceNumber;
	uint16_t payloadSize;
	char data[1466];
};

struct filePathPacket{
	char filepath[256];
};

struct ACKPacket{
	uint32_t sequenceNumber;
};

bool isValidIPv4Format(const std::string& ip);

int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file);
