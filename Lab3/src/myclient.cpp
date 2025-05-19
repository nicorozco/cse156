#include <iostream>
#include <chrono>
#include <filesystem>
#include <iterator>
#include <cstdlib>  // for std::system
#include <regex>
#include <sstream> // for stream 
#include <cctype> // to test character classification
#include <fstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>      // for close()
#include <cstring>       // for memset()
#include <errno.h>       // for errno
#include <cstdlib> // for stoi()
#include "client.h"
#include <cerrno>
#include <cstring>
#include <map>
#include <sys/select.h> // for select()
#include <sys/time.h> // for time		
#include <fstream>
#include <unordered_set>
#define WINDOW_SIZE 5
#define TIMEOUT_SEC 30

std::string currentTimestamp(){
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm = *std::gmtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
bool isValidIPv4Format(const std::string& ip){	
	std::regex ipv4Pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
	return std::regex_match(ip, ipv4Pattern);
}
int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file, const std::map<uint32_t, std::pair<long,uint16_t>>& metaMap){
	
	file.clear();

	UDPPacket lostPacket; //create the packet
	lostPacket.sequenceNumber = htonl(expectedSeqNum);//set the sequence number
	
	auto it = metaMap.find(expectedSeqNum);
	if(it == metaMap.end()){
		std::cerr << "Missing metadata for seq=" << expectedSeqNum <<"\n";
		return -1;
	}
	long offset = it->second.first;
	uint16_t dataSize = it->second.second;
	
	file.seekg(0,std::ios::end);
	std::streampos fileSize = file.tellg();	
	
	if (offset > fileSize){
		std::cerr << "Invalid offset: beyond file size. Closing.\n";
		return 2;
	}

	file.seekg(offset, std::ios::beg); //utilize seekg() to point the fd to the data and use SEEK_SET to go from the beginning of the file
	file.read(lostPacket.data,dataSize); //utilize read to read into the data

	std::streamsize bytes_reRead = file.gcount();
	
	if (bytes_reRead <= 0){ // if we read bytes form the file and it's less than 0{
		if(file.eof()){
			//we have reached the end of file
			std::cerr << "EOF Reached, no more data." << "\n";
			return -1;
		}else if(file.fail()){
			//we failed to reach the file
			std::cerr << "Read failed due to logical error" << "\n";
			return -1;
		}else if(file.bad()){
			//server issue
			std::cerr << "Severe read error"<< "\n";
			return -1;
		}else{
			std::cerr <<"Failed to read missing data from file" << "\n";
			return -1;
		}
	}
	 if(bytes_reRead != dataSize) {
			std::cerr << "Warning: Read " << bytes_reRead << " bytes but expected " << dataSize 
					  << " bytes for seq=" << expectedSeqNum << "\n";
			// Continue but use the actual bytes read
    }	
	lostPacket.payloadSize = htons(static_cast<uint16_t>(bytes_reRead));
	int totalSize = sizeof(lostPacket.sequenceNumber) + sizeof(lostPacket.payloadSize) + bytes_reRead;
	auto* ipv4 = (struct sockaddr_in*)serverAddress;
	ssize_t sent = sendto(clientSocket,&lostPacket,totalSize, 0,(struct sockaddr*)ipv4,sizeof(struct sockaddr_in));
	if(sent == -1){
		perror("sendto failed");
	}
	return 0;
}		


int main (int argc, char* argv[]) {
	std::map<uint32_t, std::pair<long, uint16_t>> sentPacketMeta;	
	std::string serverIP;
	std::string Port;
	std::string infilePath;
	std::string outfilePath;
	std::unordered_map<uint32_t, int> unackedPackets;	
	char buffer[1472];
	std::string line;
	uint32_t nextSeqNum = 0;
	uint32_t baseSeqNum = 0;
	uint32_t seqNum = 0;
	uint32_t baseWindow = 0;
	int retries = 0;
	const int MAX_RETRIES = 5;
	int bytes_recieved;
	
	if (argc < 3){
		std::cerr << "Error: not enough arguments.\n";
		std::cerr << "Usage: ./myclient <sever_ip> <server port> <infile path> <outfile path>" << "\n";
	}
	
	if (argc == 5){ 
		//if we have 4 arguments that means we should have the -h flag in the 3rd		
		serverIP = argv[1];
		Port = argv[2];
		infilePath = argv[3];
		outfilePath = argv[4];
	} else {
		std::cerr << "Error: Invalid Arguments" << "\n";
	}
	std::filesystem::path outPath(outfilePath);
   	std::filesystem::path parentDir = outPath.parent_path();

    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        try {
            std::filesystem::create_directories(parentDir);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create directory: " << e.what() << "\n";
            return 1;
        }
    }
	int serverPort = std::stoi(argv[2]);
	//after we have extracted the ip address we check if it's valid
	if (isValidIPv4Format(serverIP) == false){
		std::cout << serverIP << " is not a valid IPv4 format" << "\n";
		return 11;
	}

	// 1.) Create a UDP Socket(file descriptor)	
	int clientSocket = socket(AF_INET,SOCK_DGRAM,0);
	//error has occured creating socket 
	if (clientSocket < 0) {
		std::cerr << "Socket creating failed: " << strerror(errno) << "\n";
		return 1;
	}	
	// 2.) Specify the Server Address we utilize a structure for the address	
	sockaddr_in serverAddress; 
	memset(&serverAddress,0,sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	
	//checking if port is valid
	if (serverPort < 1 || serverPort > 65535){
		std::cerr << "Error: Invalid port number\n";
		close(clientSocket);
		return -1;
	}
	serverAddress.sin_port = htons(serverPort); 
	
	// 2d.) set the IP Address
	if (inet_pton(AF_INET,serverIP.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
		std::cerr << "Invalid Address" << "\n";
		close(clientSocket);
		return -1;
	}
	std::ifstream file(infilePath,std::ios::binary);
	// check for error when opening file
	if(!file.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
		close(clientSocket);
		return -1;
	}
	file.seekg(0, std::ios::end); // go to end
	if (file.tellg() == 0) {
	    std::cerr << "File is empty.\n";
	    return 0; // or any code you want to indicate "empty file"
	}
	file.seekg(0, std::ios::beg); // rewind to beginning if not empty

	socklen_t addrlen = sizeof(serverAddress);
	//reading data & sending packets
	auto startTime = std::chrono::steady_clock::now();

	//utilize a simple packet 
	filePathPacket pathPacket;
	memset(&pathPacket,0,sizeof(pathPacket));
	strncpy(pathPacket.filepath, outPath.c_str(),sizeof(pathPacket.filepath)-1);
	pathPacket.filepath[sizeof(pathPacket.filepath)-1] = '\0';
	//send to server

	ssize_t pathSent = sendto(clientSocket,&pathPacket, sizeof(pathPacket),0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

	if(pathSent < 0){
		perror("Error Sending Path to Client");
	}

	while(true){
		while(nextSeqNum < baseSeqNum + WINDOW_SIZE){
			UDPPacket packet;
			memset(&packet,0,sizeof(packet));
			file.read(packet.data,MSS);
			std::streamsize bytesRead = file.gcount();
			if(bytesRead <= 0){
				break;
			}else if (bytesRead < 34){
				std::cerr << "Required Minimum MSS is X+1\n";
				return 1;
			}
			long offset = file.tellg() - bytesRead;
			sentPacketMeta[nextSeqNum] = std::make_pair(offset,static_cast<uint16_t>(bytesRead));
			
			packet.payloadSize = htons(static_cast<uint16_t>(bytesRead));
			packet.sequenceNumber = htonl(nextSeqNum);	
			int totalSize = sizeof(packet.sequenceNumber) + sizeof(packet.payloadSize) + bytesRead;
			ssize_t sentBytes = sendto(clientSocket,&packet,totalSize, 0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));				
			if(sentBytes < 0){
				perror("sendto failed");
				close(clientSocket);
				return -1;
			}
			
			if(!unackedPackets.count(nextSeqNum)){
				unackedPackets[nextSeqNum] = 0;
			}
			nextSeqNum++;
		}
		
		fd_set rset; // create socket set
		FD_ZERO(&rset);//clear the socket set
		FD_SET(clientSocket,&rset); //add the clientsocket to the set 
		//set a timer utilize select to prevent hanging forever while waiting to recieved packeyt 
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		
		int activity = select(clientSocket+1,&rset,NULL,NULL,&timeout);
		
		if(activity == 0){
			std::cerr << "Timeout waiting for echo. Retry #" << retries << "\n";
			
			auto currentTime = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
			if (elapsed >= 30) {
					std::cerr << "Cannot detect server\n";
					close(clientSocket);
					return 5;
			}
			retries++;
				//check if the lowest sequence number not acknlowedge
			for(uint32_t seq = baseSeqNum; seq < nextSeqNum; seq++){ 	
				if(unackedPackets.count(seq)){
					unackedPackets[seq]++;

					if (unackedPackets[seq] >= MAX_RETRIES){
						std::cerr << "Reached max retransmission limit\n";
						return 4;
					}
					int retrans = retransmit(seq,clientSocket, (struct sockaddr*)&serverAddress, file, sentPacketMeta);
					if(retrans == -1){
						std::cerr << "Error Transmitting Seq=" << seq << "\n";
					}else{
						std::cout << "Retransmitting seq= " << seq << ", attempt " << unackedPackets[seq] << "\n";
					}
				}
			}
		}

		if (activity < 0){
			std::cerr << "Select Error\n";
			return -1;
		}
		//processing ACKED Packets		
		bytes_recieved = recvfrom(clientSocket,buffer,sizeof(buffer),0, (struct sockaddr*)&serverAddress, &addrlen);//call recieved to read the data 			
		if(bytes_recieved > 0){
			uint32_t net_seq;
			memcpy(&net_seq,buffer,sizeof(uint32_t));
			seqNum = ntohl(net_seq); //extract the sequence number
			startTime = std::chrono::steady_clock::now();
			if(unackedPackets.count(seqNum)){
				baseWindow = baseSeqNum + WINDOW_SIZE;
				std::cout << currentTimestamp() <<", ACK, "<< seqNum <<"," << baseSeqNum << "," << nextSeqNum <<"," << baseWindow << "\n"; 
				
				unackedPackets.erase(seqNum);//if the sequence number is found remove it
				while(!unackedPackets.count(baseSeqNum) && baseSeqNum < nextSeqNum) { //if we reach the ending of the unacked window
						baseSeqNum++;//slide the baseSeqNum to slide the window
				}
				retries = 0;
			}
		}else if (bytes_recieved < 0){
			perror("recvfrom failed");
			return -1;
		}
		if(file.eof() && unackedPackets.empty()){ //if we reach the end of file and there are no packets in the map break out
			//send EOF Packet
			UDPPacket eofPacket;
			memset(&eofPacket,0,sizeof(eofPacket));
			eofPacket.sequenceNumber = htonl(EOF_SEQ);
			eofPacket.payloadSize = htons(0);
			int eofSize = sizeof(eofPacket.sequenceNumber) + sizeof(eofPacket.payloadSize);
			sendto(clientSocket, &eofPacket, eofSize, 0,
       (struct sockaddr*)&serverAddress, sizeof(serverAddress));
			sleep(1);
			std::cout << "All Packet send and no more data to send" << "\n";
			break;
		}
	}
	
	std::cout << "Closing Connection" << "\n";
	file.close();
	close(clientSocket);
	return 0;
}
