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

void echoLoop(int serverSocket,int lossRate,std::string outfilePath){	
	uint32_t expectedSeqNum = 0;
	std::map<uint32_t,UDPPacket> packetsRecieved;
	char buffer[1472];
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	struct sockaddr_in clientAddr;
	ssize_t dataSize;
	socklen_t clientLen = sizeof(clientAddr);
	uint32_t seqNum = 0;
	ssize_t bytesRecieved;
	std::map<int, UDPPacket> packetBuffer;

	//implement logic to ensure we dont hang here from recfrom()	
	//recieved intial packet
	ssize_t	pathRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);

	filePathPacket* pathPacket = reinterpret_cast<filePathPacket*>(buffer);
	std::string filePath(pathPacket->filepath);
	std::cout << "File Path recieved:" << filePath << "\n";
	if (pathRecieved < 0){
		perror("Error receiving filepath");
	}else {
		std::string filePath(pathPacket->filepath);
		std::cout << "File path recieved: "<< filePath << "\n";
	}
	//open file path 
	std::ofstream outfile(outfilePath,std::ios::binary);
	if(!outfile.is_open()){
		std::cerr << "Failed to open file for writing" << std::strerror(errno) << "\n";
	}	

	while(true){
		bytesRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);
		if (bytesRecieved < 0){
			perror("Error: Recieving from client");
			continue;
		}
		if(bytesRecieved > 0){//if we are recieving data	
			
			
			UDPPacket* recievedPacket = reinterpret_cast<UDPPacket*>(buffer);
			uint16_t actualSize = ntohs(recievedPacket->payloadSize);//size of the data recieved, by removing the sequence number 
			seqNum = ntohl(recievedPacket->sequenceNumber); // the sequence numbers sets the acknolwedgement we should be recieving 
			
			//simulate packet loss 
			if (dropPacket(lossRate)){ //if we random value generate falls within the loss rate it is lost
				std::cout << currentTimestamp()<< ", DROP DATA, " << seqNum << "\n";
				continue; // by continuing we skip over sending the packet 
			}
			
			std::cout << currentTimestamp() << ", DATA," << seqNum << "\n";
			
			//-------------work on this part-------------
			
			//we utilize the expectedSeqNum to ensure we are recieving the correct packet 	
			if(seqNum == EOF_SEQ){
				std::cout << currentTimestamp() << ", EOF RECEIVED\n";
    				break;
			}
			
			if (seqNum == expectedSeqNum){
				if(dropPacket(lossRate)){
					std::cout << currentTimestamp() <<", DROP ACK, " << seqNum << "\n";
					continue; //drop packet if true
				}

				//send an ack packet
				ssize_t sentBytes = sendAck(serverSocket,seqNum,&clientAddr,clientLen);
				if(sentBytes < 0){
					perror("Error sending ACK Packet");
				}
				std::cout << currentTimestamp() << ", ACK, " << seqNum << "\n";
				
				if(packetsRecieved.count(seqNum)){
					UDPPacket& pkt = packetsRecieved[seqNum];
					uint16_t bufferedSize = ntohs(pkt.payloadSize);
					outfile.write(pkt.data, bufferedSize);
					packetsRecieved.erase(seqNum);
					std::cout << "Writing" << bufferedSize << "Bytes" << "\n";
				}else{
					outfile.write(recievedPacket->data,actualSize);//only write to the file if we have sent the ACK message 
				}
					expectedSeqNum++;
				
				while(packetsRecieved.count(expectedSeqNum)){
					//possibility of dropping as well
					std::cout << "Writing Buffered Packets" << "\n";	
					if(dropPacket(lossRate)){
						std::cout << currentTimestamp() <<", DROP ACK, " << seqNum << "\n";
						continue; //drop packet if true
					}

					//send ack packet
					 ssize_t sentBuff = sendAck(serverSocket,expectedSeqNum,&clientAddr,clientLen);
					 if(sentBuff < 0){
						 std::cerr << "Error Sending ACK Packet\n";
						 continue;
					 }
					 
					UDPPacket& pkt = packetsRecieved[expectedSeqNum];
					//this is wrong, size 
					uint16_t actualSize = ntohs(pkt.payloadSize);
					outfile.write(pkt.data,actualSize);//only write to the file if we have sent the ACK message 
					std::cout << "Writing" << actualSize << "Bytes" << "\n";
					packetsRecieved.erase(expectedSeqNum);//erase the seq num from the map
					expectedSeqNum++;//increase seqnum
				}

			}else {
				if (!packetsRecieved.count(seqNum)){//buffer the packet that have arrived but not in correct order 
					//insert into map 
					packetsRecieved[seqNum] = *recievedPacket;
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
	std::string outfile = "output.txt";
	echoLoop(serverSocket,lossRate,outfile);
	std::cout << "Finishing Recieving" << "\n";
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	//d.) recieved a packet
	close(serverSocket);
	return 0;
}
