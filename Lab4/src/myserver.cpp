#include <iostream>
#include <chrono>
#include <thread>
#include <mutex> 
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

ssize_t sendAck(int serverSocket,uint32_t seqNum,struct sockaddr_in* clientAddr,socklen_t clientLen);
std::string currentTimestamp();
void initRandom();
bool dropPacket(int lossRate);
bool isPortValid(int port);
bool isLossValid(int loss);
void handleClient(int serverSocket,int lossRate,std::string rootFolder,std::unordered_set<std::string>& activeFiles,std::unordered_map<std::string, ClientState>& clients,std::mutex& clientsMutex);	

int main(int argc, char* argv[]){
	srand(time(0));
	std::string portStr;
	std::string lossRateStr;
	std::string folderPath;
	int optval = 1;
	int lossRate;
	int port;
	initRandom(); //seed random generator 
	//utilize a map to track of the files being written to, enforce file lock 
	std::unordered_set<std::string> activeFiles;
	std::unordered_map<std::string,ClientState> clients;//create a map to hold the different clients 
	std::unordered_map<std::string, std::thread> clientThreads;//map to handle threads
	std::mutex clientsMutex;
    struct sockaddr_in clientAddr;
	if (argc < 4){
		std::cerr << "Please provide a port number and packet loss rate for the server";
		return -1;
	} else if (argc == 4) {
		portStr = argv[1];
		lossRateStr = argv[2];
		folderPath = argv[3];
	}
	port = std::stoi(portStr);
	lossRate = std::stoi(lossRateStr);

	if (isPortValid(port) == false){
		perror("Please enter a valid port");
		return -1;
	}

	if(isLossValid(lossRate) == false){
		std::cerr << "Please enter a valid loss percentage (0-100)\n";
		return -1;
	}

	//1.) create a UDP Socket
	int serverSocket = socket(AF_INET,SOCK_DGRAM,0);
	if (serverSocket < 0){
		perror("Socket creation failed");
		return 1;
	}
	//2 Set the SO_REUSEADDR option
	if(setsockopt(serverSocket,SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval))<0){
		perror("setsockopt failed\n");
		close(serverSocket);
		exit(EXIT_FAILURE);	
	}
	//b.) create a socket structure for the server 
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY; //have the server listen on all interfaces 
	serverAddr.sin_port = htons(port);
	
	//c.) Bind the port to server address structure (associate socket with IP address & Port Number)
	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
		perror("Bind failed");
		close(serverSocket);
		return 1;
	}

	while(true){
		std::string key = std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + std::to_string(ntohs(clientAddr.sin_port));

		// First packet indicates new client
		if (clients.find(key) == clients.end()) {
			clients[key] = ClientState();  // set up client state
			
			//use thread to handle multiple clients 
			std::thread t(handleClient, serverSocket, lossRate, folderPath, std::ref(activeFiles), std::ref(clients), std::ref(clientsMutex));
			t.detach();
		}

	}

	std::cout << "Finishing Recieving" << "\n";
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	//d.) recieved a packet
	close(serverSocket);
	return 0;
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
void handleClient(int serverSocket,int lossRate,std::string rootFolder, std::unordered_set<std::string>& activeFiles,std::unordered_map<std::string, ClientState>& clients,std::mutex& clientsMutex){	
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	uint32_t seqNum = 0;
	ssize_t bytesRecieved;
	std::map<int, UDPPacket> packetBuffer;
	char buffer[32768];
	//implement logic to ensure we dont hang here from recfrom()	
	//recieved intial packet
	ssize_t	pathRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);
	filePathPacket* pathPacket = reinterpret_cast<filePathPacket*>(buffer);
	
	std::string filePath(pathPacket->filepath);
	
	if (pathRecieved < 0){
		perror("Error receiving filepath");
	}
    if(filePath.empty()){
		std::cerr << "ERROR: Received empty file path!" << std::endl;
		std::cout << "Received raw file path: [" << pathPacket->filepath << "]" << std::endl;
		return;
	}
	//construct full path
	std::filesystem::path fullPath = std::filesystem::path(rootFolder) /  filePath;
	//ensure directories exist
	if(activeFiles.count(fullPath.string()) > 0){
		std::cerr << "File is currently in use: " << fullPath << "\n";
    	const char* errorMsg = "ERROR: File is being written by another client.";
    	sendto(serverSocket, errorMsg, strlen(errorMsg), 0, (struct sockaddr*)&clientAddr, clientLen);
    	return; // Skip handling this client
	}
	activeFiles.insert(fullPath.string());//mark file as in-use
	std::filesystem::create_directories(fullPath.parent_path());
	//open file for writing 
	std::cout << "Attempting to open file: " << fullPath << std::endl;	
	std::ofstream outfile(fullPath,std::ios::binary | std::ios::trunc);
	if(!outfile.is_open()){
		std::cerr << "Failed to open file for writing" << std::strerror(errno) << "\n";
	}	
	while(true){
		//clear buffer 
		memset(buffer,0,sizeof(buffer));
		bytesRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);
		if (bytesRecieved < 0){
			std::cout << "Error Recieved Bytes" << "\n";
		}
		if(bytesRecieved > 0){		
			std::string key = std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + std::to_string(ntohs(clientAddr.sin_port));
			
			if(clients.find(key) == clients.end()){
				clients[key] = ClientState{}; 
			}
			ClientState& state = clients[key];
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
					    	std::cout << currentTimestamp() << ", ACK, " << seqNum << "\n";
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
					    std::cout << currentTimestamp() << ", ACK, " << seqNum << "\n";
					}
				}

				// ________________________ Processing In Order Packets ______________________
				if(seqNum == state.expectedSeqNum){	
					if (actualSize > 32768){
						std::cerr << "Invalid Payload Size:" << "on seqNum" << seqNum << "\n";
						continue;
					}	
					outfile.write(recievedPacket->data, ntohs(recievedPacket->payloadSize));//only write to the file if we have sent the ACK message 
					outfile.flush();
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
							std::cout << currentTimestamp() << ", ACK, " << seqNum << "\n";	
						}
					}
				}

				// __________________ Processing Buffered Packets _______________________
				while(state.packetsRecieved.count(state.expectedSeqNum)){ // check if the next expectedSeqNum has been recieved 
					std::cout << "Processing Buffered Packets" << "\n";
					std::cout << currentTimestamp() << ",DATA, " << state.expectedSeqNum << "\n";
					UDPPacket* pkt = state.packetsRecieved[state.expectedSeqNum];	
					uint16_t dataLen  = ntohs(pkt->payloadSize);
					if (dataLen == 0) {
						std::cerr << "Zero payload in buffered packet at seqNum=" << state.expectedSeqNum << ", skipping\n";
						state.packetsRecieved.erase(state.expectedSeqNum);
						state.expectedSeqNum++;
						continue;
					}	 
					outfile.write(pkt->data,dataLen);
					outfile.flush();
					free(pkt);
					state.packetsRecieved.erase(state.expectedSeqNum);
					state.expectedSeqNum++;//increase seqnum
				}
				//_________________________End of File Reached_______________________
				if(seqNum == EOF_SEQ){
					std::cout << currentTimestamp() << ", EOF RECEIVED\n";
					//process the buffer at the end 
					while (!state.packetsRecieved.empty()) {
						if(state.packetsRecieved.count(state.expectedSeqNum)){	
							UDPPacket* pkt = state.packetsRecieved[state.expectedSeqNum];
							uint16_t pktSize = ntohs(pkt->payloadSize);
							std::cout << "[EOF WRITE] Seq=" << state.expectedSeqNum << ", Size=" << pktSize << "\n";
							outfile.write(pkt->data, pktSize);
							outfile.flush();
							state.packetsRecieved.erase(state.expectedSeqNum);
							free(pkt);
							state.expectedSeqNum++;
						}
					}
					std::cout << "Buffer is empty or processed all available packets\n";
					outfile.close(); //close the file when done 
					state.expectedSeqNum = 0; //reset SeqNUm
					state.packetsRecieved.clear(); //reset buffer
					clients.erase(key); //drop state for the finished client
					activeFiles.erase(fullPath.string());//remove file from active after done writing 
				}
			}
		}
	}
}
