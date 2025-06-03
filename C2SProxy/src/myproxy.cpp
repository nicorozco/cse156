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
#include <regex>
std::mutex log_mutex;
std::string currentTimestamp();
void handleRequest(const std::string& filename,int client_fd,struct sockaddr_in clientAddr);
std::string getClientIP(struct sockaddr_in clientAddr);
std::unordered_set<std::string> loadForbiddenHosts(const std::string& filename);
bool isForbidden(const std::string& hostname, const std::unordered_set<std::string>& forbidden);
void sendHttpResponse(int clientSocket, int statusCode, const std::string& reasonPhrase, const std::string& body);
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
	std::unordered_set<std::string> forbiddenSet = loadForbiddenHosts(forbiddenFile);

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
    	std::thread(handleRequest, forbiddenFile, client_fd,clientAddr).detach();  // non-blocking
	}
	return 0;
}

void handleRequest(const std::string& filename,const std::unordered_set<std::string>& forbiddenSet, int client_fd,struct sockaddr_in clientAddr){
char buffer[1024] = {0};
ssize_t bytes = read(client_fd, buffer, sizeof(buffer));
std::string method,path,version;
std::lock_guard<std::mutex> lock(log_mutex);

	if (bytes < 0) {
    	std::cerr << "Error Recieving from Client" << "\n";
		close(client_fd);
	}
	//open forbiddne li
	//open logfile
	std::ofstream file(filename,std::ios::app);

	if (!file.is_open()) {
		std::cerr << "Failed to open access.log for writing." << std::endl;
		return;
	}
	// parse HTTP message
	std::string request(buffer,bytes);
	size_t headers_end = request.find("\r\n\r\n");
	std::string headers = request.substr(0,headers_end);	
	size_t line_end = headers.find("\r\n");
	//extract client ip 
	int clientIP = std::stoi(getClientIP(clientAddr));
	std::string body = request.substr(headers_end + 4);
	if (line_end == std::string::npos) {
    	std::cerr << "Malformed HTTP request: no line break\n";
    	close(client_fd);
    	return;
	}
	std::string request_line = request.substr(0, line_end);
	// 2. Now safely parse the request line
	std::istringstream request_line_stream(request_line);
	request_line_stream >> method >> path >> version;

	if (headers_end == std::string::npos){
		std::cerr << "Malformed HTTP Request\n";
		close(client_fd);
		return;
	}
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
	file << currentTimestamp() << clientIP << "\n";
	std::regex method_regex(R"(^\s*(GET|HEAD)\s*$)", std::regex_constants::icase);
	std::cout << "Method" << method << "\n";
	if (std::regex_search(method, method_regex)) {
		// Valid method: GET or HEAD
		std::cout << "Request method is GET or HEAD\n";	
		// if GET or HEAD
		//extract host:
		std::string hostname;
		auto it = header_map.find("Host");
		if (it != header_map.end()) {
			hostname = it->second;
			std::cout << "Client requested host: " << hostname << "\n";
		} else {
			std::cerr << "Host header not found!\n";
			// Optionally respond with 400 Bad Request
		}
		if ( 
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
	}else{
		std::cout << "Unsupported HTTP method\n";
		sendHttpResponse(client_fd, 501, "Not Implemented", "501 - Not Implemented\n");
	}
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
void sendHttpResponse(int clientSocket, int statusCode, const std::string& reasonPhrase, const std::string& body) {
    std::string statusLine = "HTTP/1.1 " + std::to_string(statusCode) + " " + reasonPhrase + "\r\n";

    std::string headers;
    headers += "Content-Type: text/plain\r\n";
    headers += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    headers += "Connection: close\r\n";
    headers += "\r\n";

    std::string fullResponse = statusLine + headers + body;

    ssize_t sent = write(clientSocket, fullResponse.c_str(), fullResponse.size());
    if (sent < 0) {
        perror("Failed to send HTTP response");
    }
}
std::unordered_set<std::string> loadForbiddenHosts(const std::string& filename) {
    std::unordered_set<std::string> forbidden;
    std::ifstream infile(filename);
    std::string line;

    while (std::getline(infile, line)) {
        // Remove trailing whitespace or newline
        line.erase(line.find_last_not_of(" \r\n\t") + 1);
        std::transform(line.begin(), line.end(), line.begin(), ::tolower); // lowercase
        forbidden.insert(line);
    }

    return forbidden;
}

bool isForbidden(const std::string& hostname, const std::unordered_set<std::string>& forbidden) {
    std::string lowerHost = hostname;
    std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(), ::tolower);
    return forbidden.find(lowerHost) != forbidden.end();
}
