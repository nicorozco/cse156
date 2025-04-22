#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



void echoLoop(int serverSocket){
	char buffer[1024];
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	
	while(true){

		ssize_t bytesRecieved = recvfrom(serverSocket, buffer, sizeof(buffer),0,(struct sockaddr*)&clientAddr, &clientLen);

		if (bytesRecieved < 0){
		perror("Error: Recieving");
	}
		std::cout << "Recieved: " << buffer << "from" << inet_ntoa(clientAddr.sin_addr) << ":"<< ntohs(clientAddr.sin_port) << "\n";	

	//echo it back to the client, meaning just send it back
	sendto(serverSocket, buffer, bytesRecieved,0, (struct sockaddr*)&clientAddr, clientLen);
	
	}
}

bool isPortValid(int port){
 if ( port < 1024 || port > 65553){
	return false;
}
	return true;
}	
int main(int argc, char* argv[]){

	std::string portStr;
	int port;
	
	if (argc < 2){

		std::cerr << "Please provide a port number for the server";
		return -1;
	} else if (argc == 2) {

		portStr = argv[1];

	}
	port = std::stoi(portStr);

	if (isPortValid(port) == false){
		perror("Please enter a valid port");
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

	echoLoop(serverSocket);
	// To continusly listen for packet will need a while loop but for now just doing basic function of recieving packet
	//d.) recieved a packet
	close(serverSocket);
	return 0;
}
