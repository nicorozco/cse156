#include <iostream>
#include <string>
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
	char buffer[1024];
	int bytesRecieved;
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
		//1.) IP
		//2.) Port
		//3.) Document Path

	        // Step 1: Find ':' (split IP and port)
    	        size_t colonPos = url.find(':');
                std::string ip = url.substr(0, colonPos);

                // Step 2: Find '/' after the colon (split port and path)
                size_t slashPos = url.find('/', colonPos);
		std::string port = url.substr(colonPos + 1, slashPos - colonPos - 1);
                std::string path = url.substr(slashPos); // from slash to end
		int portInt = std::stoi(port);
		
		// Prints for Troubleshooting:
		std::cout << "Number of Arguments Provided:  "<< argc << "\n";
		std::cout << "First Command Input: " << argv[0] << "\n";
		std::cout << "Second Command Input: " << hostname << "\n";
		std::cout << "Third Command Input: " << url << "\n";
    	        std::cout << "IP: " << ip << "\n";
                std::cout << "Port: " << port << "\n";
    	       // std::cout << "Path: " << path << "\n";

		

		std::cout << "Creating Socket" << "\n";
		// 1.) Create a Socket(file descriptor)	
		int clientSocket = socket(AF_INET,SOCK_STREAM,0);
		//error has occured creating socket 
		if (clientSocket < 0) {
			std::cerr << "Socket creating failed: " <<strerror(errno) << "\n";
			return 1;
		}	
		// 2.) Specify the Server Address
		//we utilize a structure for the address
		// 2a.) declare the structure
		sockaddr_in serverAddress;
		// 2b.) set the IP Family
		serverAddress.sin_family = AF_INET;
		// 2c.) set the port number
		//	utilize htons to ensure big endian
	        serverAddress.sin_port = htons(portInt); 
		// 2d.) set the IP Address
		// we utilize (inet_pton) a function to convert the IP address from text form into binary
		// since we are utilizing a function we are going to pass the ip variable by reference of the structure
		//inet expects a constant string type, type cast the string in to a pointer
		
		if (inet_pton(AF_INET,ip_address.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
			std::cerr << "Invalid Address / Address not Supported" << "\n";
			return -1;
		}
		// 3.)Intiate a TCP connection to the server (.connect())
		// what does .connect() do?
		// connect intiates a tcp connection through the socket to the server address (being the socket structure of the server) 
		std::cout << "Initiating TCP Connection" << "\n";
		
		if (connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress)) < 0){
		// check for different error:
		// if connections was sucessful connect returns 0, if error return -1
		// error message is stored in errno
			switch(errno) {
				case ETIMEDOUT:
				// client tcp connection intiatiation recieves no response 
					std::cerr << "Error: Connection timed out (ETIMEDOUT)" << "\n";	
					break;
				case ECONNREFUSED:
					std::cerr << "Error: Connection refused (ECONNREFUSED)" << "\n";	
					break;
				//server sends reset, refusing to connect to client
				case EHOSTUNREACH:
				//unable to reach the host
					std::cerr << "Error: Host Unreachable  (EHOSTUNREACH)" << "\n";	
					break;
				case ENETUNREACH:
				// unable to reach the network
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
		if (hOption){
		//  4a.) if the h option is set the http request METHOD TO head
			httpRequest = "HEAD" + path + "HTTP/1,1" + "\n";
			//std::cout << httpRequest << "\n";
		}	
		// 4b.) else use the regualar GET METHOD
		httpRequest = "GET" + path + "HTTP/1,1" + "\n";
		// the second argument is a pointer to where you want to store the response
		send(clientSocket, &httpRequest, sizeof(httpRequest), 0);

		// 5.) recived the request (.recv())
		if ( bytesRecieved = recv(clientSocket, &buffer, sizeof(buffer),0) < 1 ){ 
		// what does recieved return if succesful the number of bytes actually read into the buffer, if uncessful, returns negative value 
			std::cerr << "Error recieving" << "\n";
		}
		if (hOption == false){
			//create a file 
			std::ofstream outfile("output.txt);
			// read from the buffer and input into the file
			// Note reverse the output 
	
		}
		//  5b.) else reverse the respose and write into a file
		// 5b.)
		// 6.) Close the socket (.close())
		clientSocket.close();
	return 0;
}
