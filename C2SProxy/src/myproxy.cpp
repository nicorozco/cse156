#include <stdlib.h>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <typeinfo>
#include <sstream>
#include <iomanip>
#include <netdb.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <iostream>        
#include <unistd.h>       
#include <string.h>        
#include <sys/socket.h>    
#include <netinet/in.h>    
#include <arpa/inet.h>     
#include <unordered_map>
#include <unordered_set>
#include <thread>         
#include <fstream>
#include <mutex>
#include <regex>
//______________________________________________ Functions Decleration  _________________________________________
void initialize_ssl();
int extractHttpStatusCode(const std::string& httpResponse);
std::string currentTimestamp();
std::string getLocalIP(int connectedSockFD);
void handleRequest(std::string filename,const std::unordered_set<std::string>& forbiddenSet,int client_fd,struct sockaddr_in clientAddr,SSL_CTX* ctx,std::mutex& logMutex);
std::string getClientIP(struct sockaddr_in clientAddr);
std::unordered_set<std::string> loadForbiddenHosts(const std::string& filename);
bool isForbidden(const std::string& hostname, const std::unordered_set<std::string>& forbidden);
void sendHttpResponse(int clientSocket, int statusCode, const std::string& reasonPhrase, const std::string& body);
std::string resolveHostnameToIP(const std::string& hostname);
std::string reverseDNSLookup(const std::string& ip); 
bool isValidHost(const std::string& hostname);
void printForbiddenSet(const std::unordered_set<std::string>& forbiddenSet);
bool isValidIP(const std::string& ip);
int createTCPConnection(const std::string& ip, int port);
//_______________________________________________ Main ____________________________________________

int main(int argc, char* argv[]){
std::mutex logMutex;
std::string listenPort;
std::string forbiddenFile;
std::string logFile;
int port;
initialize_ssl();
struct sockaddr_in clientAddr;
socklen_t addrlen = sizeof(clientAddr);
// Create an SSL context
const SSL_METHOD* method = TLS_client_method();  // for server; use TLS_client_method() for clients
SSL_CTX* ctx = SSL_CTX_new(method);
if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
 	}



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
    	std::thread(
			handleRequest, 
			logFile,
			std::ref(forbiddenSet), 
			client_fd,
			clientAddr,
			ctx,
			std::ref(logMutex)
			)
			.detach();  // non-blocking
	}
	return 0;
}

//______________________________________________ Functions Definition  _________________________________________
void handleRequest(std::string filename,const std::unordered_set<std::string>& forbiddenSet, int client_fd,struct sockaddr_in clientAddr,SSL_CTX* ctx,std::mutex& logMutex){
char buffer[1024] = {0};
ssize_t bytes = read(client_fd, buffer, sizeof(buffer));
std::string method,path,version;
int destPort = 443;
std::string hostIP;
std::string hostname;
std::string hostHeader;
size_t totalBytesSent = 0;
std::string httpVersion;
int statusCode;
std::string statusMessage;
SSL* ssl = SSL_new(ctx);


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
	
	
	std::regex method_regex(R"(^\s*(GET|HEAD)\s*$)", std::regex_constants::icase);
	std::cout << "Method " << method << "\n";
	if (std::regex_search(method, method_regex)) {
		// Valid method: GET or HEAD
		std::cout << "Request method is GET or HEAD\n";	
		// if GET or HEAD
		//extract host:
		if (header_map.find("Host") != header_map.end()) {
			hostHeader = header_map["Host"];
		} else {
			std::cerr << "Host header not found!\n";
			sendHttpResponse(client_fd, 400, "Bad Request", "400 Bad Request: Host header missing.\n");
			close(client_fd);
			return;
		}

		// Extract hostname and optional port
		size_t colonPos = hostHeader.find(':');
		if (colonPos != std::string::npos) {
			hostname = hostHeader.substr(0, colonPos);
			std::string portStr = hostHeader.substr(colonPos + 1);
			try {
				destPort = std::stoi(portStr);
			} catch (...) {
				std::cerr << "Invalid port in Host header: " << portStr << "\n";
				sendHttpResponse(client_fd, 400, "Bad Request", "400 Bad Request: Invalid port.\n");
				close(client_fd);
				return;
			}
		} else {
			hostname = hostHeader; // no port specified
		}

		std::cout << "Hostname: " << hostname << "\n";
		std::cout << "Port: " << destPort << "\n";	
		
		// 	check if the destination is within the fordbidden list 
		// note host name can be an IP address, if it's an IP Address make sure it's a valid IP 
		// make sure the hostname is valid before checking if it's in the fordbidden list 
		//std::cout << "Valid HostName " << isValidHost(hostname) << "\n";
		//std::cout << "Valid Host IP "<< isValidIP(hostname) << "\n";
		if(isValidHost(hostname) == true){
			//if within the forbiden list 
			std::cout << "Web Host is Fordbidden "<< isForbidden(hostname, forbiddenSet) << "\n";
			if(isForbidden(hostname, forbiddenSet)){
				// send 403 forbidden message
				std::cout << "Web Host is Fordbidden " << "\n";
				sendHttpResponse(client_fd, 403, "Forbidden", "403 Forbidden: Access Denied.\n");
				return;
			}else{
				//if the host name is a string resolve to get IP
				if(!isValidIP(hostname)){
					hostIP = resolveHostnameToIP(hostname);
					if(hostIP.empty()){ 	
					// if unable to resolve the domain name:
						sendHttpResponse(client_fd, 502, "Bad Gateway", "502 - Bad Gateway.\n");
						return;
					}
					std::cout << "Resolved Host to IP " << hostIP << "\n"; 
				}else{
					//it's an IP and we need to resolve the hostname
					std::string hostResolved= reverseDNSLookup(hostname);
					// if unable to resolve:
					if(hostResolved.empty()){
						sendHttpResponse(client_fd, 502, "Bad Gateway", "502 - Bad Gateway.\n");
                        return;
					}
				}
				//now that we have extracted the ip, create a TCP Connection with the server 
				int server_fd = createTCPConnection(hostIP, destPort);
				//if unable to connec to server
				if(server_fd < 0){
					//send error message to client
					sendHttpResponse(client_fd, 504, "Gateway Timeout", "504- Gateway Timedout.\n");
					return;
				}
				std::string proxyIP = getLocalIP(server_fd);
				if(proxyIP.empty()){
					std::cerr << "Error retrieving server IP" << "\n";
				}
				std::string XHeader = "X-Forwarded-For: " + clientIP + std::string(", ") + proxyIP + "\r\n";
				//connection is set up 
				// Setting up SSL
				// 1.) Create SSL Object for specific connection
				//Error Handling:
				if(!ssl){
					ERR_print_errors_fp(stderr);
					close(client_fd);
					return;
				}

				//2.) Bind SSL Object to socket
				SSL_set_fd(ssl,server_fd);
				//3.) Perform SSL Handshake
				if(SSL_connect(ssl) <= 0){
					ERR_print_errors_fp(stderr);
					close(server_fd);
					SSL_free(ssl);
					return;
				}
				//write http request to server
				std::cout << "SSL Connection Established" << "\n";
				//start reading & writing in SSL
				//SSL_write() securely send data from server
				//SSL_read() securely recieved data from server
				
				std::ostringstream request;

				// Request line
				request << "GET " << path << " HTTP/1.1\r\n";
				// Required headers
				request << "Host: " << hostname << "\r\n";
				request << "User-Agent: ProxyClient/1.0\r\n";
				request << "Connection: close\r\n";
				// Add the X-Forwarded-For header
				request << "X-Forwarded-For: " << clientIP << ", " << proxyIP << "\r\n";
				// End of headers
				request << "\r\n";

				// Convert to string
				std::string requestStr = request.str();

				// Send it via SSL
				int bytesSent = SSL_write(ssl, requestStr.c_str(), requestStr.length());
				
				if (bytesSent <= 0) {
					ERR_print_errors_fp(stderr);
					std::cerr << "Failed to send request\n";
				} else {
					std::cout << "Sent " << bytesSent << " bytes to server.\n";
				}
				char buffer[4096];
				std::string partialBuffer;
				bool statusParsed = false;
				//read response from server
				while (true) {
					int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);
					if (bytesRead > 0){
						buffer[bytesRead] = '\0';  // Null-terminate
						if (!statusParsed) {
							partialBuffer += std::string(buffer, bytesRead);

							size_t pos = partialBuffer.find("\r\n");
							if (pos != std::string::npos) {
								std::string statusLine = partialBuffer.substr(0, pos);
								std::istringstream statusStream(statusLine);
								statusStream >> httpVersion >> statusCode;
								std::getline(statusStream >> std::ws, statusMessage);

								std::cout << "Status Code: " << statusCode << "\n";

								// Mark as parsed
								statusParsed = true;
							}
						}
						ssize_t sent = send(client_fd, buffer, bytesRead, 0);
						if (sent < 0) {
							perror("Failed to send data back to client");
							break;
						}
						totalBytesSent += sent;
						std::lock_guard<std::mutex> lock(logMutex);
						//log into file 
						file << currentTimestamp() << " " 
						<< getClientIP(clientAddr) << " "
						<< "\"" << method << " " << hostname << " " << httpVersion << "\" "
						<< statusCode << " " 
					 	<< totalBytesSent << "\n";
						} else if (bytesRead == 0) {
							std::cout << "\n[Server closed the connection]\n";
							break;
						} else {
							int err = SSL_get_error(ssl, bytesRead);
							if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
								continue;  // Retry
							} else {
								std::cerr << "SSL_read error: " << ERR_reason_error_string(ERR_get_error()) << "\n";
								break;
							}
						}
					}		
				}
			}
		}else{
			std::cout << "Unsupported HTTP method\n";
			sendHttpResponse(client_fd, 501, "Not Implemented", "501 - Not Implemented\n");
		}
		
		//SSL Connection CleanUp 
		SSL_shutdown(ssl); //Graceful shutdown
		SSL_free(ssl); //Free SSL Structure
		close(client_fd); // close the client socket
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
        // Trim trailing whitespace/newlines
        size_t end = line.find_last_not_of(" \r\n\t");
        if (end != std::string::npos) {
            line.erase(end + 1);
        } else {
            continue; // skip empty/invalid lines
        }

        // Convert to lowercase
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);

        // Insert clean hostname or IP
        forbidden.insert(line);
    }

    return forbidden;
}
bool isForbidden(const std::string& hostname, const std::unordered_set<std::string>& forbidden) {
    std::string lowerHost = hostname;
    std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(), ::tolower);
    return forbidden.find(lowerHost) != forbidden.end();
}

std::string resolveHostnameToIP(const std::string& hostname) {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;  // IPv4 only. Use AF_UNSPEC for IPv4/IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    int result = getaddrinfo(hostname.c_str(), nullptr, &hints, &res);
    if (result != 0) {
        std::cerr << "DNS resolution failed: " << gai_strerror(result) << std::endl;
        return 0;
    }

    char ipStr[INET_ADDRSTRLEN];
    void* addrPtr = &((struct sockaddr_in*)res->ai_addr)->sin_addr;

    inet_ntop(AF_INET, addrPtr, ipStr, sizeof(ipStr));
    freeaddrinfo(res);  // Clean up

    return std::string(ipStr);
}
std::string reverseDNSLookup(const std::string& ip) {
    struct sockaddr_in sa;
    char host[NI_MAXHOST];

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

    int result = getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                             host, sizeof(host),
                             nullptr, 0, NI_NAMEREQD);

    if (result != 0) {
        std::cerr << "Reverse DNS lookup failed: " << gai_strerror(result) << std::endl;
        return "";
    }

    return std::string(host);
}
bool isValidHost(const std::string& host) {
    // IPv4 address regex
    std::regex ipv4Regex(R"(^(\d{1,3}\.){3}\d{1,3}$)");

    // Hostname regex (e.g., www.example.com)
    std::regex hostnameRegex(R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)");

    return std::regex_match(host, ipv4Regex) || std::regex_match(host, hostnameRegex);
}
void printForbiddenSet(const std::unordered_set<std::string>& forbiddenSet) {
    std::cout << "Forbidden Set Contents:\n";
    for (const std::string& entry : forbiddenSet) {
        std::cout << " - " << entry << '\n';
    }
}
bool isValidIP(const std::string& ip) {
    std::regex ipv4Regex(R"(^(\d{1,3}\.){3}\d{1,3}$)");
    return std::regex_match(ip, ipv4Regex);
}
void initialize_ssl() {
    SSL_library_init();              // Initializes OpenSSL
    SSL_load_error_strings();       // Error strings for error messages
    OpenSSL_add_all_algorithms();   // Load encryption algorithms
}
int createTCPConnection(const std::string& ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);  // TCP socket
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);  // Convert port to network byte order

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection to server failed");
        close(sockfd);
        return -1;
    }

    std::cout << "Connected to " << ip << ":" << port << "\n";
    return sockfd;
}
std::string getLocalIP(int connectedSockFD) {
    struct sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    char ipStr[INET_ADDRSTRLEN];

    if (getsockname(connectedSockFD, (struct sockaddr*)&localAddr, &addrLen) == 0) {
        inet_ntop(AF_INET, &localAddr.sin_addr, ipStr, sizeof(ipStr));
        return std::string(ipStr);
    } else {
        perror("getsockname failed");
        return "";
    }
}
