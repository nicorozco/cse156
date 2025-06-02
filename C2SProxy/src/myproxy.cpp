#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <iostream>
#include <iostream>        
#include <unistd.h>       
#include <string.h>        
#include <sys/socket.h>    
#include <netinet/in.h>    
#include <arpa/inet.h>     
#include <unordered_map>
#include <thread>         
#include <fstream>
#include <mutex>
std::mutex log_mutex;
void logMessage(const std::string& message, int clientIP);
void handleRequest(int client_fd);
std::string currentTimestamp();
std::string getClientIP(struct sockaddr_in clientAddr);

int main(int argc, char* argv[]){
std::string listenPort;
std::string forbiddenFile;
std::string logFile;
int port;
struct sockaddr_in clientAddr;
socklen_t addrlen = sizeof(clientAddr);

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
	
	//open logfile
	std::ofstream log_file(logFile,std::ios::app);

	if(!log_file.is_open()){
		if (!log_file.is_open()) {
			std::cerr << "Failed to open access.log for writing." << std::endl;
			exit(1);
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
		int client_fd = accept(server_fd, (struct sockaddr*)&clientAddr, &addrlen);
		
		if (client_fd < 0) {
			perror("accept failed");
			continue;
		}
		//use thread to handle this request 
    	std::thread(handleRequest, client_fd,clientAddr).detach();  // non-blocking
	}
	return 0;
}

void handleRequest(int client_fd,struct sockaddr_in clientAddr){
char buffer[1024] = {0};
ssize_t bytes = read(client_fd, buffer, sizeof(buffer));
    
	if (bytes < 0) {
    	std::cerr << "Error Recieving from Client" << "\n";
		close(client_fd);
	}
	// parse HTTP message
	std::string request(buffer,bytes);

	size_t header_end = request.find("\r\n\r\n");
	if (header_end == std::string::npos){
		std::cerr << "Malformed HTTP Request\n";
		close(client_fd);
		return;
	}

	//extract client ip 
	int clientIP = std::stoi(getClientIP(clientAddr));
	std::string headers = request.substr(0,header_end);
	std::string body = request.substr(header_end + 4);


    //extract the request line
	size_t line_end = headers.find("\r\n");
	std::string request_line = headers.substr(0,line_end);
	
	//parse individual header
	std::unordered_map<std::string, std::string> header_map;
	size_t current = line_end + 2;
	while (current < headers.size()) {
		size_t next = headers.find("\r\n", current);
		if (next == std::string::npos) break;

		std::string line = headers.substr(current, next - current);
		size_t colon = line.find(": ");
		if (colon != std::string::npos) {
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 2);
			header_map[key] = value;
		}

		current = next + 2;
	}
	

	//print to log file 
	logMessage(request_line,clientIP);
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

void logMessage(std::string request_line, int clientIP){
	std::string method,path,version;
	std::istringstream request_line_stream(request_line);
	std::lock_guard<std::mutex> lock(log_mutex);
	request_line_stream >> method >> path >> version;
	log_file << currentTimestamp() << clientIP << "\n";
}

std::string currentTimestamp(){
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm = *std::gmtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string getClientIP(struct sockaddr_in clientAddr){
	char ipStr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET,&(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
	return std::string(ipStr);
}
