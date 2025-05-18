#include <iostream>
#include <chrono>
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

void echoLoop(int serverSocket,int lossRate){	

	std::unordered_map<std::string,ClientState> clients;//create a map to hold the different clients 
	char buffer[1472];
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	
	uint32_t seqNum = 0;
	ssize_t bytesRecieved;
	std::map<int, UDPPacket> packetBuffer;

	//implement logic to ensure we dont hang here from recfrom()	
	//recieved intial packet
	ssize_t	pathRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);

	filePathPacket* pathPacket = reinterpret_cast<filePathPacket*>(buffer);
	std::string filePath(pathPacket->filepath);
	if (pathRecieved < 0){
		perror("Error receiving filepath");
	}
	//open file path 
	std::ofstream outfile(filePath,std::ios::binary);
	if(!outfile.is_open()){
		std::cerr << "Failed to open file for writing" << std::strerror(errno) << "\n";
	}	
//______________________________________________________________________________________________________________________________________________________
	while(true){
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
			
			UDPPacket* recievedPacket = reinterpret_cast<UDPPacket*>(buffer);
			uint16_t actualSize = ntohs(recievedPacket->payloadSize); 
			seqNum = ntohl(recievedPacket->sequenceNumber); 
			bool dataDropped = dropPacket(lossRate);
			if(actualSize == 0){
				std::cerr << "Zero payload detect at SeqNum= " << seqNum <<				", skipping write\n";
				continue;
			}	
			if (dataDropped){ //if we random value generate falls within the loss rate it is lost
				std::cout << currentTimestamp()<< ", DROP DATA, " << seqNum << "\n";
			}else{
				std::cout << currentTimestamp() << ", DATA," << seqNum << "\n";
				//we utilize the expectedSeqNum to ensure we are recieving the correct packet 	
//______________________________________________________________________________________________________________________________________________________				
				//Case 1: Check for duplicates(retransmission)
				if (seqNum < state.expectedSeqNum){
				    // Already received and written this packet; re-ACK if needed, but don't write again.
				    std::cout << currentTimestamp() << ", DUPLICATE, " << seqNum << "\n";
				   	bool dropAck = dropPacket(lossRate);
					if(dropAck){
						std::cout << currentTimestamp() << ", DROP ACK, " << seqNum << "\n";
					} else {
						ssize_t sentBytes = sendAck(serverSocket, seqNum, &clientAddr, clientLen);//ack the duplica						
						if (sentBytes < 0) {
					    	perror("Error sending ACK Packet");
						} else {
					    	std::cout << currentTimestamp() << ", ACK, " << seqNum << "\n";
						}
					}
				continue;
				}
//________________________________________________________________________________________________________________________________________________________
				// Case 2: Buffer out of order packets 
				if (seqNum > state.expectedSeqNum){					
					std::cout << "Inserting" << seqNum << "into buffer" << "\n";
					UDPPacket pktCopy;
					pktCopy.sequenceNumber = recievedPacket->sequenceNumber;
					pktCopy.payloadSize = recievedPacket->payloadSize;
					memcpy(pktCopy.data, recievedPacket->data,actualSize);
					
					if(actualSize > MSS){
						std::cerr << "Invalid Payload Size: " << actualSize << " on seqNum " << seqNum << "\n";
						continue;
					}
					if(!state.packetsRecieved.count(seqNum)){
						state.packetsRecieved[seqNum] = pktCopy;
					}
					//send acknoledgement
					
						ssize_t sentBytes = sendAck(serverSocket, seqNum, &clientAddr, clientLen);//ack the duplicate
						if (sentBytes < 0) {
					    	perror("Error sending ACK Packet");
						} else {
					    	std::cout << currentTimestamp() << ", ACK, " << seqNum << "\n";
						}
				}

//________________________________________________________________________________________________________
				//Case 3: Recieved the correct seq num 
				if(seqNum == state.expectedSeqNum){	
					if (actualSize > MSS){
						//std::cout << actualSize << "\n";
						std::cerr << "Invalid Payload Size:" << "on seqNum" << seqNum << "\n";
						continue;
					}
					outfile.write(recievedPacket->data,actualSize);//only write to the file if we have sent the ACK message 
					outfile.flush();
					state.expectedSeqNum++;	
					
					bool ackDropped = dropPacket(lossRate);
					//drop ack of expectedSeqNum 
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
					//Process the buffer 
					while(state.packetsRecieved.count(state.expectedSeqNum)){
							//send ack packet
							ssize_t sentBuff = sendAck(serverSocket,state.expectedSeqNum,&clientAddr,clientLen);
							if(sentBuff < 0){
								 std::cerr << "Error Sending ACK Packet\n";
							} else {
								std::cout << "Writing Buffered Data" << "\n";
								std::cout << currentTimestamp() << ", ACK, " << state.expectedSeqNum << "\n";
								UDPPacket& pkt = state.packetsRecieved[state.expectedSeqNum];	
								uint16_t pktSize = ntohs(pkt.payloadSize);
								if (pktSize == 0) {
									std::cerr << "Zero payload in buffered packet at seqNum=" << state.expectedSeqNum << ", skipping\n";
									state.packetsRecieved.erase(state.expectedSeqNum);
									state.expectedSeqNum++;
    								continue;
								}	 
								outfile.write(pkt.data,pktSize);//only write to the file if we have sent the ACK message 
								outfile.flush();
								state.packetsRecieved.erase(state.expectedSeqNum);//erase the seq num from the map
								state.expectedSeqNum++;//increase seqnum
							}
						}
				}
//_________________________________________________________________________________________________________________________
				//Case 4: Reach end of File 
				if(seqNum == EOF_SEQ){
					std::cout << currentTimestamp() << ", EOF RECEIVED\n";
					//process the buffer at the end 
					while (state.packetsRecieved.count(state.expectedSeqNum) && !state.packetsRecieved.empty()) {
							UDPPacket& pkt = state.packetsRecieved[state.expectedSeqNum];
							uint16_t pktSize = ntohs(pkt.payloadSize);
							if (pktSize == 0) {
							    std::cerr << "[EOF SKIP] Zero-length packet at seqNum=" << state.expectedSeqNum << ", skipping\n";
    							state.packetsRecieved.erase(state.expectedSeqNum);
    							state.expectedSeqNum++;
    							continue;
							}
							std::cout << "[EOF WRITE] Seq=" << state.expectedSeqNum << ", Size=" << pktSize << "\n";
							outfile.write(pkt.data, pktSize);
							outfile.flush();
							state.packetsRecieved.erase(state.expectedSeqNum);
							state.expectedSeqNum++;
					}
					std::cout << "Buffer is empty" << "\n";
					outfile.flush();
					break;
				}	
			}
		}
	}
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

int main(int argc, char* argv[]){
	srand(time(0));
	std::string portStr;
	std::string lossRateStr;
	int lossRate;
	int port;
	initRandom(); //seed random generator 
	if (argc < 3){

		std::cerr << "Please provide a port number and packet loss rate for the server";
		return -1;
	} else if (argc == 3) {

		portStr = argv[1];
		lossRateStr = argv[2];

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

	//1.) create a UDP Server
	//a.) create a UDP socket
	int serverSocket = socket(AF_INET,SOCK_DGRAM,0);
	if (serverSocket < 0){
		perror("Socket creation failed");
		return 1;
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
	echoLoop(serverSocket,lossRate);
	std::cout << "Finishing Recieving" << "\n";
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	//d.) recieved a packet
	close(serverSocket);
	return 0;
}
