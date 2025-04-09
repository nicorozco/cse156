#include <iostream>
#include <vector> 
#include <string>
int main (int argc, char* argv[]) {
	
	std::string hostname;
	std::string url;
	int port;
	std::string serverIdentifier;
	std::string ip_address
	

	if (argc < 2){
			std::cout << "Please enter the correct format for the program: ./main hostname (-H for headers only) IP Adress:Port/path to document" << "\n";
	}else{ 
		hostname  = argv[1];
		//split this into multiple parts 
		//1.) IP
		//2.) Port
		//3.) Document Path
		url = argv[2];
		// Prints for Troubleshooting:
		//std::cout << "Number of Arguments Provided:  "<< argc << "\n";
		//std::cout << "First Command Input: " << argv[0] << "\n";
		//std::cout << "Second Command Input: " << hostname << "\n";
		//std::cout << "Third Command Input: " << url << "\n";
	
		// 1.) Create a Socket(file descriptor)	
		int clientSocket = socket(AF_INET,SOCK.STREAM);
		//error has occured creating socket 
		if (clientSocket < 0) {
			std::cerr << "Socket creating failed: " <<strerror(errno) << "\n";
			return 1;
		}	
		// 2.) Specify the Server Address
		//we utilize a structure for the address
		// 2a.) declare the structure
		sockadrr_in serverAddress;
		// 2b.) set the IP Family
		serverAddress.sin_family = AF_INET;
		// 2c.) set the port number
		//	utilize htons to ensure big endian
		serverAdderss.sin_port = htons(port); 
		// 2d.) set the IP Address
		// we utilize (inet_pton) a function to convert the IP address from text form into binary
		// since we are utilizing a function we are going to pass the ip variable by reference of the structure
		inet_port(AF_INET,ip_address,&serverAddress.sin_adr);
		
		// 3.) Connect to the server (.connect())
		// 4.) Send the request (.request())
		// 5.) recived the request (.recv())
		//  5a.) reverse the respose and write into a file
		// 6.) Close the socket (.close())
	}
	return 0;
}
