#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]){

	std::string portStr;
	int port;
	char buffer[1024];
	
	if (argc < 2){

		std::cerr << "Please provide a port number for the server";

	} else if (argc == 2) {

		portStr = argv[1];

	}
	port = std::stoi(portStr);

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
	// To continusly listen for packet will need a while loop but for not just doing basic function of recieving packet
	




	close(serverSocket);
	return 0;
}
