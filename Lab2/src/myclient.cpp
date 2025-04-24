#include <iostream>
#include <regex>
#include <sstream> // for stream 
#include <cctype> // to test character classification
#include <fstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>      // for close()
#include <cstring>       // for memset()
#include <errno.h>       // for errno
#include <cstdlib> // for stoi()
#include "client.h"
#include <cerrno>
#include <cstring>

bool isValidIPv4Format(const std::string& ip){	
	std::regex ipv4Pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
	return std::regex_match(ip, ipv4Pattern);
}

int main (int argc, char* argv[]) {
	
	std::string serverIP;
	std::string Port;
	std::string infilePath;
	std::string outfilePath;
	char buffer[1472];
	std::string line;

	if (argc < 3){
		std::cerr << "Error: not enough arguments.\n";
		std::cerr << "Usage: ./myclient <sever_ip> <server port> <infile path> <outfile path>" << "\n";
	}
	
	if (argc == 4){ 
		//if we have 4 arguments that means we should have the -h flag in the 3rd		
		serverIP = argv[1];
		Port = argv[2];
		infilePath = argv[3];
		//outfilePath = argv[4];
	} else {
		std::cerr << "Error: Invalid Arguments" << "\n";
	}

	int serverPort = std::stoi(Port);

	//after we have extracted the ip address we check if it's valid
	if (isValidIPv4Format(serverIP) == false){
		std::cout << serverIP << " is not a valid IPv4 format" << "\n";
		return 11;
	}

	// 1.) Create a UDP Socket(file descriptor)	
	int clientSocket = socket(AF_INET,SOCK_DGRAM,0);
	//error has occured creating socket 
	if (clientSocket < 0) {
		std::cerr << "Socket creating failed: " << strerror(errno) << "\n";
		return 1;
	}	
	// 2.) Specify the Server Address we utilize a structure for the address	
	sockaddr_in serverAddress; 
	serverAddress.sin_family = AF_INET;
	
	//checking if port is valid
	if (serverPort < 1 || serverPort > 65535){
		std::cerr << "Error: Invalid port number\n";
		close(clientSocket);
		return 6;
	}
	serverAddress.sin_port = htons(serverPort); 
	
	// 2d.) set the IP Address
	if (inet_pton(AF_INET,serverIP.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
		std::cerr << "Invalid Address" << "\n";
		close(clientSocket);
		return 7;
	}
	
	//3.) Client reads a file from disk & Sends it over UDP Socket
	//a.) Sending Data over a UDP Socket 
	// utilize sendto() Note: each call to sendto() sends a single UDP Diagram
	// open the file 
	//infilePath = "file.txt"; 
	std::ifstream file(infilePath);
	// check for error when opening file
	if(!file.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
		return -1;
	}
	uint32_t sequence = 0;
	//else process data in while loop
	while(file){
		//std::cout << "Sequence:" << sequence << "\n";
		//create an udp packet
		UDPPacket packet;
		packet.sequenceNumber = htons(sequence++);
		//add the data to the packet
		file.read(packet.data,sizeof(packet.data));
		std::streamsize bytesRead = file.gcount();
		//std::cout << "Bytes Read: " << bytesRead << "\n";
		
		//for testing data 
		/*for(int i = 0; i < bytesRead; i++){
			std::cout << packet.data[i];
		}
		std::cout << "\n";
		*/

		//if the bytesRead is less than zero meaning we have data send it 
		if(bytesRead > 0){
			// send the data
			//std::cout << "Sending Data" << "\n";
			ssize_t sentBytes = sendto(clientSocket,&packet, sizeof(uint32_t) + bytesRead, 0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

			if(sentBytes < 0){
				perror("sendto failed");
				close(clientSocket);
				return -1;
			}

		}

	}
	
	file.close(); //close the file after reading
	//b.) Client should be able to detect packet losses
	// utilize sequence numbers

	//4.) Client recieved echo package from the server
	// utilize recvfrom() Note: returns one UDP Packet per call
	// recvfrom() returns the number of bytes recievied from the scoket 
	socklen_t addrlen = sizeof(serverAddress);
	int bytes_recieved;
	while((bytes_recieved = recvfrom(clientSocket,buffer,sizeof(buffer),0, (struct sockaddr*)&serverAddress, &addrlen)) > 0){
	// note: you can get the sender address (helpful when handling multiple sources)-> Server	
	//calculate the true payload recieved
	size_t payloadLenght = bytes_recieved - sizeof(uint32_t);
	// since the data stored in the buffer are raw bytes, we need to cast back into UDP Struct to interpret the data
	UDPPacket* receivedPacket = reinterpret_cast<UDPPacket*>(buffer);
	std::cout << "Sequence: " << ntohl(receivedPacket->sequenceNumber) << "Data: " << std::string(receivedPacket->data,payloadLenght) << "\n";
		//std::cout << "Printing Data Echoed: " << buffer << "\n";  //inet_ntoa(serverAddress.sin_addr) << ":" << ntohs(serverAddress.sin_port) << "\n";
	
	}

	if (bytes_recieved < 0){
		perror("Error Recieving");
		return -1;
	}


	close(clientSocket);
	return 0;
}
