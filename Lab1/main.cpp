#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>      // for close()
#include <cstring>       // for memset()
#include <errno.h>       // for errno
#include <cstdlib> // for stoi()




int main (int argc, char* argv[]) {
	
	std::string hostname;
	std::string url;
	std::string option;
	std::string serverIdentifier;
	std::string ip_address;
	std::string httpRequest;
	bool hOption = false;
	char buffer[4096];
	int bytesrecv;
	std::vector<char> fullData; // vector to hold all the data to pass into file
	if (argc < 2){
			std::cout << "Please enter the correct format for the program: ./main hostname (-H for headers only) IP Adress:Port/path to document" << "\n";
	}
	
	// check if the flag is provided 
	if(std::string(argv[1]) == "-h"){
		if (argc < 4) {
			std::cerr << "Error: Missing hostname or URL.\n";
			return 1;
		}
		hOption = true;
		option = argv[1];
		hostname  = argv[2];
		url = argv[3];
	}else{
		//no -h option given
	 	hostname = argv[1];
	 	url = argv[2];	
	}
		//split the url into multiple parts 
		//1.) IP 2.) Port 3.) Document Path

	        // Step 1: Find ':' (split IP and port)
    	        size_t colonPos = url.find(':');
                std::string ip = url.substr(0, colonPos);

                // Step 2: Find '/' after the colon (split port and path)
                size_t slashPos = url.find('/', colonPos);
		std::string port = url.substr(colonPos + 1, slashPos - colonPos - 1);
                std::string path = url.substr(slashPos); // from slash to end
		int portInt = std::stoi(port);
		
		// Prints for Troubleshooting:
		//std::cout << "Number of Arguments Provided:  "<< argc << "\n";
		//std::cout << "First Command Input: " << argv[0] << "\n";
		//std::cout << "Second Command Input: " << hostname << "\n";
		//std::cout << "Third Command Input: " << url << "\n";
		//std::cout << "Host Name" << hostname << "\n";
		//std::cout << "IP: " << ip << "\n";
                //std::cout << "Port: " << port << "\n";
    	        //std::cout << "Path: " << path << "\n";

		

		//std::cout << "Creating Socket" << "\n";
		// 1.) Create a Socket(file descriptor)	
		int clientSocket = socket(AF_INET,SOCK_STREAM,0);
		//error has occured creating socket 
		if (clientSocket < 0) {
			std::cerr << "Socket creating failed: " <<strerror(errno) << "\n";
			return 1;
		}	
		// 2.) Specify the Server Address we utilize a structure for the address
		sockaddr_in serverAddress; 
		// 2b.) set the IP Family
		serverAddress.sin_family = AF_INET;
		// 2c.) set the port number
		//utilize htons to ensure big endian
	        serverAddress.sin_port = htons(portInt); 
		// 2d.) set the IP Address
		// we utilize (inet_pton) a function to convert the IP address from text form into binary	
		//std::cout << "IP Address: " << ip<< "\n";
		if (inet_pton(AF_INET,ip.c_str(),&serverAddress.sin_addr) < 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
			std::cerr << "Invalid Address / Address not Supported" << "\n";
			return -1;
		}
		// 3.)Intiate a TCP connection to the server (.connect())
		//std::cout << "Server IP Address: " << inet_ntoa(serverAddress.sin_addr) << "\n";
		//std::cout << "Server Port: " << ntohs( serverAddress.sin_port) << "\n";
		// what does .connect() do?
		// connect intiates a tcp connection through the socket to the server address (being the socket structure of the server) 
		//std::cout << "Initiating TCP Connection" << "\n";
		
		if (connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress)) < 0){
		// check for different error:
		// if connections was sucessful connect returns 0, if error return -1
		// error message is stored in errno
			switch(errno) {
				// client tcp connection intiatiation recieves no response 
				case ETIMEDOUT:
					std::cerr << "Error: Connection timed out (ETIMEDOUT)" << "\n";	
					break;
				//server sends reset, refusing to connect to client
				case ECONNREFUSED:
					std::cerr << "Error: Connection refused (ECONNREFUSED)" << "\n";	
					break;
				//unable to reach the host
				case EHOSTUNREACH:
					std::cerr << "Error: Host Unreachable  (EHOSTUNREACH)" << "\n";	
					break;
				// unable to reach the network
				case ENETUNREACH:
					std::cerr << "Error: Network Unreachable (ENETUNREACH)" << "\n";	
					break;
				default:
					std::cerr << "Error: " << strerror(errno) << "\n";
					break;
			}
		}
		// 4.) Send the request (.request())
		// HTTP Request Line Format:
		// 3 Entires
		// 1.) Method: GET/HEAD
		// 2.) URL : url provided by client
		// 3.) Version: we will use HTTP/1.1
		//std::cout << "Connection established" << "\n";
		
		if (hOption){
			//std::cout << "Sending HEAD request" << "\n";
		//  4a.) if the h option is set the http request METHOD TO head
			httpRequest = "HEAD " + path + " HTTP/1.1\r\n" + "Host: " + hostname + "\r\n" + "Connection: close\r\n \r\n";
		}
		
		// 4b.) else use the regular GET METHOD
		//std::cout << "Sending Get Request" << "\n";
		httpRequest = "GET " + path + " HTTP/1.1\r\n" + "Host: " + hostname + "\r\n" + "Connection: close\r\n \r\n";
		// the second argument is a pointer to where you want to store the response
		send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0);
		

		// 5.) recived the request (.recv())
		//std::cout << "Recieving Data" << "\n";	
		while((bytesrecv = recv(clientSocket, buffer, sizeof(buffer),0)) > 0) { 
			//std::cout << "Bytes Recieved" << bytesrecv<< "\n";
			// what does recieved return if succesful the number of bytes actually read into the buffer, if uncessful, returns negative value 
			// while we are recieving write the buffer into the file 
			fullData.insert(fullData.end(), buffer, buffer + bytesrecv);	
			if(hOption){
				std::cout.write(buffer,bytesrecv);
			}
		}
		
		//std::cout << "Printing Out Contents: " << "\n";
		//std::cout << "Vector Data Size" << fullData.size() << "\n";
		/*for(size_t i=0; i < fullData.size(); i++){
			std::cout << fullData[i];
			For testing correct contents in the vector
		}
		*/

		if (bytesrecv < 0){
			std::cerr << "Error recieving" << "\n";
		}
		std::cout << "\n";

		
		if (hOption == false){
			//utilize reverse algorithm to reverse the data
			std::reverse(fullData.begin(), fullData.end());
			//create a file 
			std::ofstream outfile("output.txt");
			//check for error openning file
			if (!outfile) {
				std::cerr << "Failed to open file for writing" << "\n";
			}else{
				//write from the fulldata input into the file
				std::cout << "Writing Final Data to File" << "\n";
				outfile.write(fullData.data(),fullData.size());
				outfile.close();
			}
		}
		//std::cout << "Closing Socket" << "\n";
		close(clientSocket);
	
	return 0;
}
