#include <iostream>
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
struct ACKPacket {
	uint32_t sequenceNumber;
}
void echoLoop(int serverSocket,int lossRate,std::string outfilePath){	
	std::map<uint32_t,UDPPacket> packetsRecieved;
	char buffer[1472];
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	ssize_t dataSize;
	uint32_t seqNum = 0;
	double random;	
	ssize_t bytesRecieved;
	
	//open file to reach 
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
			//process the packet
			UDPPacket* recievedPacket = reinterpret_cast<UDPPacket*>(buffer);
			dataSize = bytesRecieved - sizeof(uint32_t);//size of the data recieved, by removing the sequence number 
			seqNum = ntohl(recievedPacket->sequenceNumber); // the sequence numbers sets the acknolwedgement we should be recieving 
			//simulate packet loss 
			random = (double)rand() / RAND_MAX; // generates a random number between 0.0 & 1.0
			if (random < lossRate){ //if we random value generate falls within the loss rate it is lost
				std::cout << "Packet Loss\n";
				std::cout << "Dropping Packet : " << seqNum << "%\n";
				continue; // by continuing we skip over sending the packet 
			}
			packetsRecieved[seqNum] = *recievedPacket;//if not dropping the packet insert into the map	
			//check if the sequence number is in the map if it is write it to the file and remove it from the map
			
			if (packetsRecieved.count(seqNum)){
				ACKPacket ackPacket;
				memset(&ackPacket,0,sizeof(ackPacket));
				ackPacket.sequenceNumber = htonl(seqNum); //set the sequence number
				int size = sizeof(uint32_t); //how muhc data to send 
				ssize_t sentBytes = sendto(serverSocket,&ackPacket,size,0,(struct sockaddr*)&clientAddr,clientLen);//send an "ACK" message to the client which is just sending the sequence number
				if(sentBytes < 0){
					std::cerr << "Error Sending ACK Packet\n";
				}
				packetsRecieved.erase(seqNum);
				outfile.write(buffer+sizeof(uint32_t),dataSize);			       
				std::cout << "Packet: " << seqNum << " Acknowledged" << "\n";
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
	int port;
	int lossRate;
	
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

	std::string outfile = output.txt;
	echoLoop(serverSocket,lossRate,outfile);
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	//d.) recieved a packet
	close(serverSocket);
	return 0;
}
