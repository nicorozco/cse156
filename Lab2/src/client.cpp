#include <iostream>
#include <regex>
#include <sstream> // for stream 
#include <cctype> // to test character classification
#include <fstream>
#include <string>
#include <algorithm>
#include <vector> // for vector
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>      // for close()
#include <cstring>       // for memset()
#include <errno.h>       // for errno
#include <cstdlib> // for stoi()

//Object for Packet

int main (int argc, char* argv[]) {
	
	std::string serverIP;
	std::string serverPort:
	std::string infilePath
	std::string outfilePath;
	
	if (argc < 3){
		std::cerr << "Error: not enough arguments.\n";
		std::cerr << "Usage: ./myclient <sever_ip> <server port> <infile path> <outfile path>" << "\n";
	}
	
	if (argc == 5){ 
		//if we have 4 arguments that means we should have the -h flag in the 3rd
		
		serverIP= argv[1];
		serverPort = arv[2];
		infilePath = argv[3];
		outfilePath = argv[4];
	} else {
		std::cerr << "Error: Invalid Arguments" << "\n";
	}

	
	//after we have extracted the ip address we check if it's valid
	if (isValidIPv4Format(ip) == false){
		std::cout << ip << " is not a valid IPv4 format" << "\n";
		return 11;
	}
	if (isValidHost(hostname) == false){

		std::cout << hostname << " is not a valid host name format" << "\n";	
		return 11;
	}
	if (isValidPath(path) == false){

		std::cout << path << " is not a valid path format" << "\n";
		return 11;
	}
	// 1.) Create a UDP Socket(file descriptor)	
	int clientSocket = socket(AF_INET,SOCK_STREAM,0);
	//error has occured creating socket 
	if (clientSocket < 0) {
		std::cerr << "Socket creating failed: " << strerror(errno) << "\n";
		return 1;
	}	
	// 2.) Specify the Server Address we utilize a structure for the address	
	sockaddr_in serverAddress; 
	serverAddress.sin_family = AF_INET;
	
	//checking if port is valid
	if (port < 1 || port > 65535){
		std::cerr << "Error: Invalid port number\n";
		close(clientSocket);
		return 6;
	}
	serverAddress.sin_port = htons(port); 
	// 2d.) set the IP Address
	if (inet_pton(AF_INET,ip.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
		std::cerr << "Invalid Address" << "\n";
		close(clientSocket);
		return 7;
	}
	
	close(clientSocket);
	return 0;
}
