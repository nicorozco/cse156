#include <stdlib.h>
#include <string>
#include <iostream>
#include <iostream>        
#include <unistd.h>       
#include <string.h>        
#include <sys/socket.h>    
#include <netinet/in.h>    
#include <arpa/inet.h>     
#include <thread>         
void handleRequest(int client_fd);

int main(int argc, char* argv[]){
std::string listenPort;
std::string forbiddenFile;
std::string logFile;
int port;
struct sockaddr_in client_addr;
socklen_t addrlen = sizeof(client_addr);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-p" && i + 1 < argc) {
            listenPort = argv[++i];
        } else if (arg == "-a" && i + 1 < argc) {
            forbiddenFile = argv[++i];
        } else if (arg == "-l" && i + 1 < argc) {
            logFile = argv[++i];
        } else {
            std::cerr << "[USAGE]: ./myproxy -p listen-port -a forbidden-site-file -l access-log-file-path " << arg << "\n";
            return 1;
        }
    }

    // Debug output to check parsed values
    //std::cout << "Listen Port: " << listenPort << "\n";
   	//std::cout << "Forbidden Sites File: " << forbiddenFile << "\n";
    //std::cout << "Access Log File: " << logFile << "\n";
	port = std::stoi(listenPort);
	if (port < 1 || port > 65535){
		std::cerr << "Error: Invalid port number\n";
		return 6;
	}
	//set up TCP Socket
	int server_fd;
    struct sockaddr_in address;

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket failed");
        return 1;
    }

    // 2. Bind to a port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Accept connections on any IP
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // 3. Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port " << port << "...\n";

	
	while (true) {

		//create fd for connection made 
		int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
		
		if (client_fd < 0) {
			perror("accept failed");
			continue;
		}
		//use thread to handle this request 
    	std::thread(handleRequest, client_fd).detach();  // non-blocking
	}
	return 0;
}

void handleRequest(int client_fd){
char buffer[1024] = {0};
ssize_t bytes = read(client_fd, buffer, sizeof(buffer));
    
	if (bytes < 0) {
    	std::cerr << "Error Recieving from Client" << "\n";
		close(client_fd);
	}
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
