#include <stdlib.h>
#include <string>
//create function to handle request 
void handleRequest(){
// establish the HTTP 
// parse HTTP message
// check if GET/HEAD
// if GET or HEAD
// 	check if the destination is within the fordbidden list 
	// if within the forbiden list 
	// send 403 forbidden message

	// if not in the fodbiden list 
		// resolve domain name utilize dns function 
			// if unable to resolve the domain name:
				// "Return 502 Bad Gateway Message back to client 
			// if able to resolve:
				// send HTTP Request Message through SSL
					// add header to HTTP Request
					// 	

}
int main(){
std::string listenPort;
std::string forbiddenFile;
std::string logFilePath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-p" && i + 1 < argc) {
            listenPort = argv[++i];
        } else if (arg == "-a" && i + 1 < argc) {
            forbiddenFile = argv[++i];
        } else if (arg == "-l" && i + 1 < argc) {
            logFilePath = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            return 1;
        }
    }

    // Debug output to check parsed values
    std::cout << "Listen Port: " << listen_port << "\n";
    std::cout << "Forbidden Sites File: " << forbidden_sites_file << "\n";
    std::cout << "Access Log File: " << access_log_file << "\n";

//set up TCP Socket
//lister for connections
//connection made
// use thread to handle this request 

	return 0;
}
