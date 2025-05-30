#include <iostream>
#include <chrono>
#include <unordered_set>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "client.h"
#include <cstdlib>
#include <ctime>
#include <map>
#include <fstream>
std::string getClientKey(sockaddr_in& addr);
std::unordered_map<std::string,ClientState> clients;//create a map to hold the different clients 
ssize_t sendAck(int serverSocket,uint32_t seqNum,struct sockaddr_in* clientAddr,socklen_t clientLen);
std::string currentTimestamp();
void initRandom();
bool dropPacket(int lossRate);
bool isPortValid(int port);
bool isLossValid(int loss);
int main(int argc, char* argv[]){
	srand(time(0));
	std::string portStr;
	std::string lossRateStr;
	std::string rootFolder;
	int optval = 1;
	int lossRate;
	int port = 0;
	int localPort = 0;
	initRandom(); //seed random generator 
	char buffer[32768];
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	uint32_t seqNum = 0;
	ssize_t bytesRecieved;
	std::map<int, UDPPacket> packetBuffer;
	//b.) create a socket structure for the server 
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY; //have the server listen on all interfaces 
	std::memset(&serverAddr, 0, sizeof(serverAddr));
	//utilize a map to track of the files being written to, enforce file lock 
	std::unordered_set<std::string> activeFiles;
	if (argc < 4){
		std::cerr << "Please provide a port number and packet loss rate for the server";
		return -1;
	} else if (argc == 4) {
		portStr = argv[1];
		lossRateStr = argv[2];
		rootFolder = argv[3];
	}
	port = std::stoi(portStr);
	lossRate = std::stoi(lossRateStr);

	if (isPortValid(port) == false){
		perror("Please enter a valid port");
	}

	if(isLossValid(lossRate) == false){
		std::cerr << "Please enter a valid loss percentage (0-100)\n";
	}
	serverAddr.sin_port = htons(port);
	//1.) create a UDP Socket
	int serverSocket = socket(AF_INET,SOCK_DGRAM,0);
	if (serverSocket < 0){
		perror("Socket creation failed");
	}
	//2 Set the SO_REUSEADDR option
	if(setsockopt(serverSocket,SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval))<0){
		perror("setsockopt failed\n");
		close(serverSocket);
		exit(EXIT_FAILURE);	
	}
	
	//c.) Bind the port to server address structure (associate socket with IP address & Port Number)
	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
		perror("Bind failed");
		close(serverSocket);
	}
	//extract port assigned by os
	sockaddr_in actualAddr{};
    socklen_t addrLen = sizeof(actualAddr);	
	if (getsockname(serverSocket, (struct sockaddr*)&actualAddr, &addrLen) == -1) {
        perror("getsockname() failed");
    } else {
        localPort = ntohs(actualAddr.sin_port);
  	}	
    // Extracting Local Port 

	//implement logic to ensure we dont hang here from recfrom()	
	
	//recieved intial packet
	ssize_t	pathRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);
	filePathPacket* pathPacket = reinterpret_cast<filePathPacket*>(buffer);
	std::string filePath(pathPacket->filepath);
	if (pathRecieved < 0){
		perror("Error receiving filepath");
	}

	//construct full path
	std::filesystem::path fullPath = std::filesystem::path(rootFolder) /  filePath;
	//ensure directories exist
	if(activeFiles.count(fullPath.string()) > 0){
		std::cerr << "File is currently in use: " << fullPath << "\n";
    	const char* errorMsg = "ERROR: File is being written by another client.";
    	sendto(serverSocket, errorMsg, strlen(errorMsg), 0, (struct sockaddr*)&clientAddr, clientLen);
	}
	activeFiles.insert(fullPath.string());//mark file as in-use
	std::filesystem::create_directories(fullPath.parent_path());
	//open file for writing 
	std::ofstream outfile(fullPath,std::ios::binary | std::ios::trunc);
	if(!outfile.is_open()){
		std::cerr << "Failed to open file for writing" << std::strerror(errno) << "\n";
	}	
	char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddr.sin_port);
    std::cout << "Client IP: " << clientIP << "\n";
    std::cout << "Client Port: " << clientPort << "\n";

	while(true){
		//clear buffer 
		
		memset(buffer,0,sizeof(buffer));
		bytesRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);
		if (bytesRecieved < 0){
			std::cout << "Error Recieved Bytes" << "\n";
		}
		
		if(bytesRecieved > 0){		
			std::string clientKey = getClientKey(clientAddr);
			if (clients.find(clientKey) == clients.end()) {
				ClientState newState;
				std::string filename = "client_" + clientKey + ".out";
				newState.fullPath = std::filesystem::path("received/") / filename;
				std::filesystem::create_directories("received");
				newState.outfile.open(newState.fullPath, std::ios::binary);
				if (!newState.outfile.is_open()) {
					std::cerr << "Failed to open file for " << clientKey << std::endl;
					continue;
				}
				clients[clientKey] = std::move(newState);
			}
			ClientState& state = clients[clientKey];
		
			UDPPacket* recievedPacket = (UDPPacket*)buffer;
			uint16_t actualSize = ntohs(recievedPacket->payloadSize); 
			seqNum = ntohl(recievedPacket->sequenceNumber); 
			bool dataDropped = dropPacket(lossRate);	
			if (dataDropped){ //if we random value generate falls within the loss rate it is lost
				std::cout << currentTimestamp()<< ", DROP DATA, " << seqNum << "\n";
			}else{
				std::cout << currentTimestamp() << ", DATA," << seqNum << "\n";
				//________________ Processing Duplicate Packets _______________________________
				if (seqNum < state.expectedSeqNum){
				    std::cout << currentTimestamp() << ", DUPLICATE, " << seqNum << "\n";
					ssize_t sentBytes = sendAck(serverSocket, seqNum, &clientAddr, clientLen);//ack the duplica			
					if (sentBytes < 0) {
					    	perror("Error sending ACK Packet");
						} else {
					    	std::cout << currentTimestamp() <<","<< localPort<<","<< clientIP << ","<< clientPort << ", ACK, " << seqNum << "\n";
						}
					continue;
				}
				//_________________ Processing Out Of Order Packets ________________________________________
				if (seqNum > state.expectedSeqNum){					
					
					size_t totalSize = sizeof(UDPPacket) + actualSize;
					UDPPacket* pktCopy = (UDPPacket*) malloc(totalSize);
					if(!pktCopy){
						std::cerr << "Memory allocation failed for SeqNum" << seqNum << "\n";
						continue;
					}
					pktCopy->sequenceNumber = recievedPacket->sequenceNumber;
					pktCopy->payloadSize = recievedPacket->payloadSize;
					memcpy(pktCopy->data, recievedPacket->data,actualSize);
					
					if(actualSize > 32768){
						std::cerr << "Invalid Payload Size: " << actualSize << " on seqNum " << seqNum << "\n";
						free(pktCopy);
						continue;
					}
					if(!state.packetsRecieved.count(seqNum)){
						state.packetsRecieved[seqNum] = pktCopy;
					}else{
						free(pktCopy);
					}
						
					ssize_t sentBytes = sendAck(serverSocket, seqNum, &clientAddr, clientLen);//ack the duplicate
					if (sentBytes < 0) {
					    perror("Error sending ACK Packet");
					} else {
						std::cout << currentTimestamp() <<","<< localPort<<","<< clientIP << ","<< clientPort << ", ACK, " << seqNum << "\n";
					}
				}

				// ________________________ Processing In Order Packets ______________________
				if(seqNum == state.expectedSeqNum){	
					if (actualSize > 32768){
						std::cerr << "Invalid Payload Size:" << "on seqNum" << seqNum << "\n";
						continue;
					}	
					state.outfile.write(recievedPacket->data, ntohs(recievedPacket->payloadSize));//only write to the file if we have sent the ACK message 
					state.expectedSeqNum++;						
					
					bool ackDropped = dropPacket(lossRate);
					if(ackDropped){
						std::cout << currentTimestamp() <<", DROP ACK, " << seqNum << "\n";
					}else{
						//send an ack packet
						ssize_t sentBytes = sendAck(serverSocket,seqNum,&clientAddr,clientLen);
						if(sentBytes < 0){
							perror("Error sending ACK Packet");
						}else{
					    	std::cout << currentTimestamp() <<","<< localPort<<","<< clientIP << ","<< clientPort << ", ACK, " << seqNum << "\n";
						}
					}
				}

				// __________________ Processing Buffered Packets _______________________
				while(state.packetsRecieved.count(state.expectedSeqNum)){ // check if the next expectedSeqNum has been recieved 
					std::cout << currentTimestamp() << ",DATA, " << state.expectedSeqNum << "\n";
					UDPPacket* pkt = state.packetsRecieved[state.expectedSeqNum];	
					uint16_t dataLen  = ntohs(pkt->payloadSize);
					if (dataLen == 0) {
						std::cerr << "Zero payload in buffered packet at seqNum=" << state.expectedSeqNum << ", skipping\n";
						state.packetsRecieved.erase(state.expectedSeqNum);
						state.expectedSeqNum++;
						continue;
					}	 
					state.outfile.write(pkt->data,dataLen);
					free(pkt);
					state.packetsRecieved.erase(state.expectedSeqNum);
					state.expectedSeqNum++;//increase seqnum
				}
				//_________________________End of File Reached_______________________
				if (seqNum == EOF_SEQ) {
					std::cout << "EOF received from " << clientKey << std::endl;
					while (state.packetsRecieved.count(state.expectedSeqNum)) {
						UDPPacket* bufferedPkt = state.packetsRecieved[state.expectedSeqNum];
						uint16_t size = ntohs(bufferedPkt->payloadSize);
						state.outfile.write(bufferedPkt->data, size);
						free(bufferedPkt);
						state.packetsRecieved.erase(state.expectedSeqNum);
						state.expectedSeqNum++;
					}
					state.outfile.close();
					clients.erase(clientKey);
					continue;
				}

			}
		}
	}
	return 0;
}
std::string getClientKey(sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
    int port = ntohs(addr.sin_port);
    return std::string(ip) + ":" + std::to_string(port);
}

ssize_t sendAck(int serverSocket,uint32_t seqNum,struct sockaddr_in* clientAddr,socklen_t clientLen){

	ACKPacket ackPacket;
	memset(&ackPacket,0,sizeof(ackPacket));
	ackPacket.sequenceNumber = htonl(seqNum); //set the sequence number
	int size = sizeof(uint32_t); //how muhc data to send 
	ssize_t sentBytes = sendto(serverSocket,&ackPacket,size,0,(struct sockaddr*)clientAddr,clientLen);//send an "ACK" message to the client which is just sending the sequence number
	return sentBytes;
}
std::string currentTimestamp(){
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm = *std::gmtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
void initRandom(){
	std::srand(static_cast<unsigned int>(std::time(nullptr)));
}
bool dropPacket(int lossRate){
	double percLossRate = lossRate / 100.0;
	double randVal = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
	return randVal < percLossRate;
}
bool isPortValid(int port){
 if ( port < 1024 || port > 65553){
	return false;
}
	return true;
}	
bool isLossValid(int loss){
if( loss < 0 || loss > 100){
	return false;

}
	return true;
}
