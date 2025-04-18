#include <iostream>
#include <regex>
#include <sstream> // for stream 
#include <cctype> // to test character classification
#include <fstream>
#include <string>
#include <algorithm>
#include <vector> // for vector
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>      // for close()
#include <cstring>       // for memset()
#include <errno.h>       // for errno
#include <cstdlib> // for stoi()

bool isValidIPv4Format(const std::string& ip){

	std::regex ipv4Pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
	return std::regex_match(ip, ipv4Pattern); 
}
bool isValidHost(const std::string& hostname){
	//function to allow DNS valid hostnames
	if (hostname.length() > 253){
		return false;
	}

	std::regex pattern(R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)");
	return std::regex_match(hostname, pattern);
}

bool isValidPath(const std::string& path){
	if(path.length() > 2000){
		return false;
	}	
	std::regex pattern(R"(^\/[a-zA-Z0-9\-._~\/]*$)");
	return std::regex_match(path,pattern);
}

int main (int argc, char* argv[]) {
	
	std::string hostname;
	std::string url;
	std::string option;
	std::string serverIdentifier;
	std::string ip_address;
	std::string httpRequest;
	bool hOption = false;
	char buffer[4096];
	std::string ip, path;
	path = "/";
	int port = 80;
	int bytesrecv;
	std::vector<char> fullData; //vector to hold all the data to pass into file
	bool headerParse = false;
	int contentLenght;
	std::string httpVersion;
	int statusCode;
	std::string statusMessage;
	std::string statusLine; //holder for the first line of HTTP response
	size_t headerEndIndex;// grab the index where the headers end
	size_t colonPos;
	size_t slashPos;
	std::vector<char> body; //vector to hold the body message
	
	if (argc < 3){
		std::cerr << "Error: not enough arguments.\n";
		std::cerr << "Usage: ./main <hostname> <IP Address:Port Number(Optional)> <-h (for headers only)>" << "\n";
		return 11;
	}
	
	// check if the flag is provided 
	if (argc == 4){ 
		//if we have 4 arguments that means we should have the -h flag in the 3rd
		
		std::string flag = argv[3];
		std::transform(flag.begin(),flag.end(),flag.begin(), ::tolower);
		
		if (flag  == "-h") {
			hOption = true;
			hostname  = argv[1];
			url = argv[2];
		}else{
			std::cerr << "Error: '-h' flag must be in the correct position (the last argument)."<< "\n";
			return 11;

		}
	}else if (argc == 3){
		//no -h option given
	 	hostname = argv[1];
	 	url = argv[2];	
	} else {
		std::cerr << "Error: Invalid Arguments" << "\n";
		return 11;
	}

	//split the url into multiple parts 
    	colonPos = url.find(':');
        slashPos = url.find('/');
	
	if (slashPos == std::string::npos){
		path = "/";
	}else {
		path = url.substr(slashPos); // from slash to end
	}

	if(colonPos != std::string::npos && colonPos < slashPos){
		//Format is: ip:port/path
		ip = url.substr(0, colonPos);
		std::string portStr = url.substr(colonPos+1,slashPos - colonPos - 1);
		if(!std::all_of(portStr.begin(),portStr.end(),::isdigit)){

			std::cerr << "Error: Port containts non-numeric characters\n";
			return 6;
		}
		port = std::stoi(portStr);

	} else {
		//Format is: ip/path
		ip = url.substr(0,slashPos);

	}
	
	//after we have extracted the ip address we check if it's valid
	if (isValidIPv4Format(ip) == false){
		std::cout << ip << " is not a valid IPv4 format" << "\n";
		return 11;
	}
	if (isValidHost(hostname) == false){

		std::cout << hostname << " is not a valid host name format" << "\n";	
		return 11;
	}
	if (isValidPath(path) == false){

		std::cout << path << " is not a valid path format" << "\n";
		return 11;
	}
	// 1.) Create a Socket(file descriptor)	
	int clientSocket = socket(AF_INET,SOCK_STREAM,0);
	//error has occured creating socket 
	if (clientSocket < 0) {
		std::cerr << "Socket creating failed: " << strerror(errno) << "\n";
		return 1;
	}	
	// 2.) Specify the Server Address we utilize a structure for the address
	sockaddr_in serverAddress; 
	serverAddress.sin_family = AF_INET;
	//checking if port is valid
	if (port < 1 || port > 65535){
		std::cerr << "Error: Invalid port number\n";
		close(clientSocket);
		return 6;
	}
	serverAddress.sin_port = htons(port); 
	// 2d.) set the IP Address
	if (inet_pton(AF_INET,ip.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
		std::cerr << "Invalid Address" << "\n";
		close(clientSocket);
		return 7;
	}
	
	// 3.)Intiate a TCP connection to the server (.connect())	
	if (connect(clientSocket,(struct sockaddr*)&serverAddress,sizeof(serverAddress)) < 0){
		// check for different error:
		// if connections was sucessful connect returns 0, if error return -1
		close(clientSocket);
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
				return 7;
				// unable to reach the network
			case ENETUNREACH:
				std::cerr << "Error: Network Unreachable (ENETUNREACH)" << "\n";	
				break;
			default:
				std::cerr << "Error: " << strerror(errno) << "\n";
				break;
			}
		}
		
		if (hOption){
			//std::cout << "Sending HEAD request" << "\n";
		//  4a.) if the h option is set the http request METHOD TO head
			httpRequest = "HEAD " + path + " HTTP/1.1\r\n" + "Host: " + hostname + "\r\n" + "X-UCSC-Student-ID: norozco6 \r\n" + "Connection: close\r\n" +"\r\n";
			//std::cout << httpRequest << "\n";
		}else{
		
			// 4b.) else use the regular GET METHOD
			//std::cout << "Sending Get Request" << "\n";
			httpRequest = "GET " + path + " HTTP/1.1\r\n" + "Host: " + hostname + "\r\n" + "X-UCSC-Student-ID: norozco6 \r\n" + "Connection: close\r\n" + "\r\n";
			//std::cout << httpRequest << "\n";
		}
		// the second argument is a pointer to where you want to store the response
		if ((send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0)) < 1){
			std::cerr << "Error Sending HTTP Request" << "\n";
			close(clientSocket);
		}
		
		// 5.) recived the request (.recv())
		//Step1: we are going to recieved until headers are complete headers are complete when we find \r\n\r\n
		while((bytesrecv = recv(clientSocket, buffer, sizeof(buffer),0)) > 0) { 
			//std::cout << "Recieving Data" << "\n";	
			// insert chunked data into the vector	
			
			if(hOption){
				//h option write out to terminal
				//std::cout << "Writing Content to Buffer \n";
				std::cout.write(buffer,bytesrecv);
			}
			
			fullData.insert(fullData.end(), buffer, buffer + bytesrecv);	
			//when do we know when to stop reading bytes, utilize the content lenght 
			// 1.) First we need to for the end of header in the vector

			if (!headerParse){
				//std::cout << "Reading Headers \n";
				std::string delimiter = "\r\n\r\n";	
		        	auto it = std::search(fullData.begin(),fullData.end(),delimiter.begin(),delimiter.end());	
			
				//2.) We need to confirm the end of header marker was found
				if (it != fullData.end()){ // if it == fullData.end means we didn't find the header
					headerEndIndex = std::distance(fullData.begin(), it) + delimiter.length();
					headerParse = true;

					//std::cout << "Extracting Header\n";
					//extract the headers
					std::string headers(fullData.begin(), fullData.begin() + headerEndIndex);

					//Parse the Content-Lenght:
					size_t pos = headers.find("Content-Length:");
					if (pos != std::string::npos){
						std::string lenStr = headers.substr(pos + 15);
						lenStr = lenStr.substr(0, lenStr.find("\r\n"));
						contentLenght = std::stoi(lenStr);
					}
					if (headers.find("Transfer-Enconding: Chunked") != std::string::npos){
						std::cerr << "Error: 'Transfer-Encoding: Chunked' Not supported\n";
						close(clientSocket);
						return 11;
					}


					//calculate how much of the body we have
					int fullDataBytes = fullData.size() - headerEndIndex;

					if (fullDataBytes >= contentLenght){
						std::cout << "No longer recieving data\n";
					       	std::cout << "Total Bytes Read: " << fullDataBytes << "\n";	
						break;
					}
				}else{
					//Headers already parsed, just recieved the body
					//std::cout << "Header already pasred, just recieving the body\n"; 
					size_t headerEndIndex = std::string(fullData.begin(), fullData.end()).find("\r\n\r\n") + 4;
					int bodySize = fullData.size() - headerEndIndex;
					//stop recieving data
					if (bodySize >= contentLenght){
						break;
					}

				}
			}
			
		}
		

		if(bytesrecv == 0){
			std::cout << "Connection closed by server (EOF reached)\n";
		}else if (bytesrecv < 0){
			std::cerr << "Error recieving" << "\n";
			close(clientSocket);
		}

		std::string response(fullData.begin(), fullData.end()); // create an input stream from the response string in order to read line by line
		std::istringstream responseStream(response); 
		std::getline(responseStream,statusLine); // utilize the get line reads up to \n
		std::istringstream statusLineStream(statusLine);

		if (statusLineStream >> httpVersion >> statusCode) {

			if(statusCode >= 400){
				return 9;
			}
		}else {
			std::cerr << "Failed to parse HTTP status line.\n";
		}
			
		if (hOption == false){
			body.insert(body.end(),fullData.begin() + headerEndIndex, fullData.begin() + headerEndIndex + contentLenght); 
			std::reverse(body.begin(), body.end());
			std::ofstream outfile("slug_download_norozco6.dat",std::ios::binary);
			if (!outfile) {
				std::cerr << "Failed to open file for writing" << "\n";
				close(clientSocket);
				return 10;
			}else{
				outfile.write(body.data(),body.size());
				outfile.close();
			}
		}
		close(clientSocket);
	
	return 0;
}
