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
#include <map>
#include <sys/select.h> // for select()
#include <sys/time.h> // for time		
#define WINDOW_SIZE 5
bool isValidIPv4Format(const std::string& ip){	
	std::regex ipv4Pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
	return std::regex_match(ip, ipv4Pattern);
}

int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file){
	file.clear();
	UDPPacket lostPacket; //create the packet
	lostPacket.sequenceNumber = htonl(expectedSeqNum);//set the sequence number	
	std::cout << "Resending Sequence Number = " << expectedSeqNum << "\n";
	//recover the data associated with the sequence number from the original file using seekg()
	long offset = expectedSeqNum * 1468;
	//std::cout << "Offset: " << offset << "\n";
	//check if osset is valid before reading
	file.seekg(0,std::ios::end);
	std::streampos fileSize = file.tellg();	
	
	//std::cout << "File Size: " << fileSize << "\n";
	if (offset > fileSize){
		std::cerr << "Invalid offset: beyond file size. Closing.\n";
		return 2;
	}

	file.seekg(offset, std::ios::beg); //utilize seekg() to point the fd to the data and use SEEK_SET to go from the beginning of the file
	file.read(lostPacket.data,1468); //utilize read to read into the data

	//std::cout << "Data:" << lostPacket.data << "\n";	
	std::streamsize bytes_reRead = file.gcount();
	
	if (bytes_reRead <= 0){ // if we read bytes form the file and it's less than 0{
		if(file.eof()){
			//we have reached the end of file
			std::cerr << "EOF Reached, no more data." << "\n";
			return -1;
		}else if(file.fail()){
			//we failed to reach the file
			std::cerr << "Read failed due to logical error" << "\n";
			return -1;
		}else if(file.bad()){
			//server issue
			std::cerr << "Severe read error"<< "\n";
			return -1;
		}else{
			std::cerr <<"Failed to read missing data from file" << "\n";
			return -1;
		}
	}
	//once the data is succesfuly read retransmit the packet and go back to the top of the while loop for recieving
	//std::cout << "Bytes Re-read" << bytes_reRead << "\n";
	auto* ipv4 = (struct sockaddr_in*)serverAddress;
	//std::cout << "Send to" << inet_ntoa(ipv4->sin_addr) << ":" << ntohs(ipv4->sin_port) << " | family=" << ipv4->sin_family << "\n";

	ssize_t sent = sendto(clientSocket,&lostPacket, sizeof(uint32_t) + bytes_reRead, 0,(struct sockaddr*)ipv4,sizeof(struct sockaddr_in));
	if(sent == -1){
		perror("sendto failed");
	}

	//std::cout << "Retransmitted Bytes" << sent << "\n";
	usleep(5000);
	return 0;
}		


int main (int argc, char* argv[]) {
	
	std::string serverIP;
	std::string Port;
	std::string infilePath;
	std::string outfilePath;
	char buffer[1472];
	std::string line;
	std::map<uint32_t,UDPPacket> recievedPackets;
	uint32_t expectedSeqNum = 0;
	uint32_t seqNum;
	int retries = 0;
	const int MAX_RETRIES = 5;
	int bytes_recieved;
	uint16_t sequence;
	
	if (argc < 3){
		std::cerr << "Error: not enough arguments.\n";
		std::cerr << "Usage: ./myclient <sever_ip> <server port> <infile path> <outfile path>" << "\n";
	}
	
	if (argc == 5){ 
		//if we have 4 arguments that means we should have the -h flag in the 3rd		
		serverIP = argv[1];
		Port = argv[2];
		infilePath = argv[3];
		outfilePath = argv[4];
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
	memset(&serverAddress,0,sizeof(serverAddress));
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
	
	std::ifstream file(infilePath,std::ios::binary);
	// check for error when opening file
	if(!file.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
		return -1;
	}

	socklen_t addrlen = sizeof(serverAddress);
	std::ofstream outFile(outfilePath);//open file path for writing
	if(!outFile.is_open()){
		std::cerr << "Failed to open file for writing" << std::strerror(errno) << "\n";
		return -1;
	}
	//reading data & sending packets
	while(true){
		// if there space in the buffer and there is still data to read from the file 
		while(
		//create a packet
		UDPPacket packet;
		packet.sequenceNumber = htonl(sequence++);
		file.read(packet.data,sizeof(packet.data));
		std::streamsize bytesRead = file.gcount();

		//if we have data have data send it 
		if(bytesRead > 0){
			//isend the data
			ssize_t sentBytes = sendto(clientSocket,&packet, sizeof(uint32_t) + bytesRead, 0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));			// insert the packet into the sent packet	
			if(sentBytes < 0){
				perror("sendto failed");
				close(clientSocket);
				return -1;
			}
		}
	}

	fd_set rset; // create socket set
	FD_ZERO(&rset);//clear the socket set
	FD_SET(clientSocket,&rset); //add the clientsocket to the set 
	//set a timer utilize select to prevent hanging forever while waiting to recieved packeyt 
	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	
	int activity = select(clientSocket+1,&rset,NULL,NULL,&timeout);
	if(activity == 0){
		std::cerr << "Cannot detect server\n";
		exit(1);
	}else if (activity < 0){
		std::cerr << "Select Error\n";
		exit(-1);
	}

		if (retries >= MAX_RETRIES){
			std::cerr << "Server Timeout: unable to recieve packets after attempts from server" << "\n";
			return 2;
		}

			//retransmit all the unknockwledge packets
			std::cout << "Sequence Number being retransmitted due to inactivity" << expectedSeqNum << "\n";
			int result = retransmit(expectedSeqNum,clientSocket, (struct sockaddr*)&serverAddress, file);
			std::cout << "Retransmitting Due to Inactivity" << "\n";
		//recieved echoed packets		
		bytes_recieved = recvfrom(clientSocket,buffer,sizeof(buffer),0, (struct sockaddr*)&serverAddress, &addrlen);//call recieved to read the data 			
		
		if(bytes_recieved > 0){ //if we are recieving data
			std::cout << "Expected Sequence Number" << expectedSeqNum << "\n";
			UDPPacket receivedPacket;
			memcpy(&receivedPacket, buffer,sizeof(UDPPacket));
			seqNum = ntohl(receivedPacket.sequenceNumber); //extract the sequence number
			std::cout << "Sequence Number of Recieved Packet" << seqNum << "\n";

//once we acknoweldged the packet slide the window

// if all packets are sent and acknolwedge break

//Final Step: Compare the file outputs
		
	
	std::cout << "Closing Connection" << "\n";
	outFile.close();
	file.close();
	close(clientSocket);
	return 0;
}
