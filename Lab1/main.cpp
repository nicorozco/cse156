#include <iostream>
#include <vector> 
#include <string>
int main (int argc, char* argv[]) {
	
	std::string hostname;
	std::string url;
	

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
		// 3.) Connect to the server (.connect())
		// 4.) Send the request (.request())
		// 5.) recived the request (.recv())
		//  5a.) reverse the respose and write into a file
		// 6.) Close the socket (.close())
	}
	return 0;
}
