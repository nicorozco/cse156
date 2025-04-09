#include <iostream>
#include <vector> 

int main (int argc, char* argv[]) {
	
	if (argc < 2){
			std::cout << "Please enter the correct format for the program: ./main hostname (-H for headers only) IP Adress:Port/path to document" << "\n";
	}else{ 
		std::cout << "Number of Arguments Provided:  "<< argc << "\n";
		std::cout << "First Command Input: " << argv[0] << "\n";
		std::cout << "Second Command Input: " << argv[1] << "\n";
		std::cout << "Third Command Input: " << argv[2] << "\n";

	// 1.) Create a Socket(file descriptor)
	// 2.) Specify the Server Address
	// 3.) Connect to the server (.connect())
	// 4.) Send the request (.request())
	// 5.) recived the request (.recv())
	// 6.) Close the socket (.close())
	}
	return 0;
}
