// H file for the client side
//structure for UDP packet
struct UDPPacket {
	uint32_t sequenceNumber;
	char data[1468];
};


bool isValidIPv4Format(const std::string& ip);

int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file);
