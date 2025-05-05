#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "client.h"
#include <cstdlib>
#include <ctime>

void echoLoop(int serverSocket,int lossRate){	
	std::map<uint32_t,UDPPacket> packetsRecieved;
	char buffer[1472];
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	ssize_t dataSize;	
	while(true){

		ssize_t bytesRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);

		if (bytesRecieved < 0){
			perror("Error: Recieving from client");
			continue;
		}
		
		if(bytesRecieved > 0) {//if we are recieving data
			//process the data create a strucutre of the buffer 		

			//simulate packet loss 
			double random = (double)rand() / RAND_MAX; // generates a random number between 0.0 & 1.0
			if (random < lossRate){ //if we random value generate falls within the loss rate it is lost
				std::cout << "Packet Loss\n";
				std::cout << "Loss Rate: " << lossRate << "%\n";
				continue; // by continuing we skip over sending the packet 
			}

			//process the packet
			UDPPacket* recievedPacket = reinterpret_cast<UDPPacket*>(buffer);
			dataSize = bytesRecieved - sizeof(uint32_t);
			uint32_t seq = ntohl(recievedPacket->sequenceNumber);
			std::cout << seq << "\n";
						//insert into map 

			//reorder the packet
		
		}


		if(dataSize > 0){
		//echo it back to the client, meaning just send it back
			sendto(serverSocket, buffer, bytesRecieved,0, (struct sockaddr*)&clientAddr, clientLen);
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

	echoLoop(serverSocket,lossRate);
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	//d.) recieved a packet
	close(serverSocket);
	return 0;
}
