#include <iostream>
#include <thread>
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
#define TIMEOUT_SEC 30
std::string currentTimestamp();
bool isValidIPv4Format(const std::string& ip);	
int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file, const std::map<uint32_t, std::pair<long,uint16_t>>& metaMap);	
void fileProcessing(const std::string serverIP,int serverPort, int WINDOW_SIZE,int MSS,std::string infilePath,std::string outfilePath);
int main (int argc, char* argv[]){
	std::string windowSize;
	std::string rep;
	std::string mss; 
	std::string serverConf;
	std::string infilePath;
	std::string outfilePath;
	std::string line;
	int MSS = 0;
	int window;
	int repFactor = 0;
	std::vector<std::thread> threads;

	if (argc < 7){
		std::cerr << "Error: not enough arguments.\n";
		std::cerr << "Usage: ./myclient servn servaddr.conf mss winsz infile path outfile path" << "\n";
	}
	
	if (argc == 7){ 
		rep = argv[1];
		serverConf = argv[2];
		mss = argv[3];
		windowSize = argv[4];
		infilePath = argv[5];
		outfilePath = argv[6];
	} else {
		std::cerr << "Error: Invalid Arguments" << "\n";
	}
	std::ifstream serverFile(serverConf);
	// check for error when opening file
	if(!serverFile.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
		return -1;
	}
	serverFile.seekg(0, std::ios::end); // go to end
	if (serverFile.tellg() == 0) {
	    std::cerr << "File is empty.\n";
	    return 0; // or any code you want to indicate "empty file"
	}
	serverFile.clear();
	serverFile.seekg(0,std::ios::beg);//reset to beginning of file
	repFactor = std::stoi(rep);
	MSS = std::stoi(mss);
	window = std::stoi(windowSize);
	if(MSS < 34){
		std::cerr << "Required Minimum MSS is X+1\n";
		return 1;
	}
	//utilize threads to call the packet processin functions 
	int count = 0;
	while(count < repFactor && std::getline(serverFile,line)){
		std::istringstream iss(line);
		std::string serverIP;
		int port;
		if(iss >> serverIP >> port){
			threads.emplace_back([=](){
				fileProcessing(serverIP,port,window,MSS,infilePath,outfilePath);
			});
			count++;
		}
	}
	//join all threads to ensure all instances are finished before exiting main
	for(auto& t: threads){
		t.join();
	}

	std::cout << "Closing Connection" << "\n";
	serverFile.close();
	return 0;
}

bool isValidIPv4Format(const std::string& ip){	
	std::regex ipv4Pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
	return std::regex_match(ip, ipv4Pattern);
}
std::string currentTimestamp(){
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm = *std::gmtime(&now_c);

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void fileProcessing(const std::string serverIP,int serverPort, int WINDOW_SIZE,int MSS,std::string infilePath,std::string outfilePath){
	std::map<uint32_t, std::pair<long, uint16_t>> sentPacketMeta;	
	uint32_t nextSeqNum = 0;
	uint32_t baseSeqNum = 0;
	uint32_t seqNum = 0;
	uint32_t baseWindow = 0;
	std::unordered_map<uint32_t, int> unackedPackets;
	std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> sentTimes; //map to track individual packet to retrasnmit 
	char buffer[1472];
	const int MAX_RETRIES = 5;
	int bytes_recieved;
	const int TIMEOUT_MS = 3000;
	int clientSocket = socket(AF_INET,SOCK_DGRAM,0);	
	bool firstRecieved = false;
	std::ifstream file(infilePath,std::ios::binary);
	// check for error when opening file
	if(!file.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
	}
	std::filesystem::path outPath(outfilePath);
   	std::filesystem::path parentDir = outPath.parent_path();

    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        try {
            std::filesystem::create_directories(parentDir);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create directory: " << e.what() << "\n";
        }
	}

	file.seekg(0, std::ios::end); // go to end
	
	if(file.tellg() == 0) {
	    std::cerr << "File is empty.\n";
	}
	file.clear();
	file.seekg(0,std::ios::beg);//reset to beginning of file
	//error has occured creating socket 
	if (clientSocket < 0) {
		std::cerr << "Socket creating failed: " << strerror(errno) << "\n";
	}
		
	// 2.) Specify the Server Address we utilize a structure for the address	
	sockaddr_in serverAddress; 
	memset(&serverAddress,0,sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	
	//checking if port is valid
	if (serverPort < 1 || serverPort > 65535){
		std::cerr << "Error: Invalid port number\n";
		close(clientSocket);
	}
	serverAddress.sin_port = htons(serverPort); 
	// 2d.) set the IP Address
	if (inet_pton(AF_INET,serverIP.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
		std::cerr << "Invalid Address" << "\n";
		close(clientSocket);
	}
	socklen_t addrlen = sizeof(serverAddress);
	//reading data & sending packets

	//___________________first send path file to server_____________________________  
	filePathPacket pathPacket;
	memset(&pathPacket,0,sizeof(pathPacket));
	strncpy(pathPacket.filepath, outPath.c_str(),sizeof(pathPacket.filepath)-1);
	pathPacket.filepath[sizeof(pathPacket.filepath)-1] = '\0';
	//send to server
	ssize_t pathSent = sendto(clientSocket,&pathPacket, sizeof(pathPacket),0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));

	if(pathSent < 0){
		perror("Error Sending Path to Client");
		close(clientSocket);
	}	
	
	fd_set rset; // create socket set
	FD_ZERO(&rset);//clear the socket set
	FD_SET(clientSocket,&rset); //add the clientsocket to the set 
	
	//set a timer utilize select to prevent hanging forever while waiting to recieved packeyt 
	struct timeval timeout;
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;
	int activity = select(clientSocket+1,&rset,NULL,NULL,&timeout);
	if (activity == 0){
 		std::cerr << "Timeout: No response from server within 30 seconds. Exiting.\n";
    	close(clientSocket);
	}else if (activity < 0){
		std::cerr << "Select Error\n";
		close(clientSocket);
	}
	
	char response[128] = {};
	socklen_t serverLen = sizeof(serverAddress);
	ssize_t bytes = recvfrom(clientSocket, response, sizeof(response), 0,
                         (struct sockaddr*)&serverAddress, &serverLen);

	if (bytes < 0) {
		perror("Timeout or error waiting for ACK from server");
		close(clientSocket);
		return;
	}
	std::cout << "Server ACK: " << response << "\n";
	size_t totalSize = sizeof(UDPPacket) + MSS;

	//_____________Start Processing Packets_____________________
	while(true){
		//sending packets within window size 
		//__________________________________________________________________________
		while(nextSeqNum < baseSeqNum + WINDOW_SIZE && !file.eof()){
			
			UDPPacket* packet = (UDPPacket*)malloc(totalSize); //allocate data
			if(!packet){
				perror("malloc failed");
			}
			memset(packet,0,totalSize);
			file.read(packet->data,MSS);
			std::streamsize bytesRead = file.gcount();
			if(bytesRead == 0 && file.eof()){
				free(packet); //free packet before break
				break;
			}

			long offset = file.tellg() - bytesRead;
			sentPacketMeta[nextSeqNum] = std::make_pair(offset,static_cast<uint16_t>(bytesRead));
			
			packet->payloadSize = htons(static_cast<uint16_t>(bytesRead));
			packet->sequenceNumber = htonl(nextSeqNum);	
			ssize_t sentBytes = sendto(clientSocket,packet,totalSize, 0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));				
			std::cout << "Sending Packets" << "\n";
			if(sentBytes < 0){
				perror("sendto failed");
				close(clientSocket);
				free(packet);
			}
			if(!unackedPackets.count(nextSeqNum)){
				unackedPackets[nextSeqNum] = 0;
				sentTimes[nextSeqNum] = std::chrono::steady_clock::now();//input the time sent 
			}
			nextSeqNum++;
			free(packet);
		}
		//_____________________________________________________________________________________________________________
		//process packets from server 
		//If we recieved no activity from the socket within 30 seconds server timed out 
		// ________________________________________________________________________
		if(activity > 0 && FD_ISSET(clientSocket, &rset)){
			
			bytes_recieved = recvfrom(clientSocket,buffer,sizeof(buffer),0, (struct sockaddr*)&serverAddress, &addrlen);//call recieved to read the data 				
			//extract rip and rport
			char rip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(serverAddress.sin_addr),rip,INET_ADDRSTRLEN);
			int rport = ntohs(serverAddress.sin_port);
			
			//extracting lport
			struct sockaddr_in localAddr;
			socklen_t localLen = sizeof(localAddr);
			getsockname(clientSocket, (struct sockaddr*)&localAddr, &localLen);
			int lport = ntohs(localAddr.sin_port);
	
			if(bytes_recieved > 0){
				std::cout << "Recieving Bytes" << "\n";
				uint32_t net_seq;
				memcpy(&net_seq,buffer,sizeof(uint32_t));
				seqNum = ntohl(net_seq); //extract the sequence number
				//if the sequence number in the ack packet is sent by the server, we remove it from the unackedpacket and advance the window
				if(unackedPackets.count(seqNum)){
					baseWindow = baseSeqNum + WINDOW_SIZE;
					std::cout << currentTimestamp() <<"," << lport << "," << rip << "," << rport << " ACK, "<< seqNum <<"," << baseSeqNum << "," << nextSeqNum <<"," << baseWindow << "\n"; 		
					unackedPackets.erase(seqNum);//if the sequence number is found remove it
					sentTimes.erase(seqNum);
					while(!unackedPackets.count(baseSeqNum) && baseSeqNum < nextSeqNum) { //if we reach the ending of the unacked window
							baseSeqNum++;//slide the baseSeqNum to slide the window
					}
				}
			}else if (bytes_recieved < 0){
				perror("recvfrom failed");
			}	
		}

		//check if the lowest unacked packet, the base is still in the unackedPacket, increase the number of retries 
		if(unackedPackets.count(baseSeqNum) && sentTimes.count(baseSeqNum)){
			auto now = std::chrono::steady_clock::now();
			auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTimes[baseSeqNum]).count();
			if (waitTime >= TIMEOUT_MS && firstRecieved == false){
				std::cout << "Packet Loss Detected" << "\n";
				unackedPackets[baseSeqNum]++;	
				//check if we hit max retries 	
				if (unackedPackets[baseSeqNum] >= MAX_RETRIES){
					std::cerr << "Reached max retransmission limit\n";
					std::cout << unackedPackets[baseSeqNum] << "\n";
					std::cout << MAX_RETRIES << "\n";
				}
					
				int retrans = retransmit(baseSeqNum,clientSocket, (struct sockaddr*)&serverAddress, file, sentPacketMeta);

				if(retrans == -1){
					std::cerr << "Error Transmitting Seq=" << baseSeqNum << "\n";
				}else{
					std::cout << "Retransmitting seq= " << baseSeqNum << ", attempt " << unackedPackets[baseSeqNum] << "\n";
					sentTimes[baseSeqNum] = now;
				}
			}
		}
	
		if(file.eof() && unackedPackets.empty()){ //if we reach the end of file and there are no packets in the map break out
			//send EOF Packet
			std::cout << currentTimestamp() << ", EOF TRIGGERED — all packets sent and ACKed\n";
			size_t eofSize = sizeof(UDPPacket);
			UDPPacket* eofPacket = (UDPPacket*)malloc(eofSize);
			if(!eofPacket){
				perror("malloc for EOF packet failed");
			}
			memset(eofPacket,0,eofSize);
			eofPacket->sequenceNumber = htonl(EOF_SEQ);
			eofPacket->payloadSize = htons(0);
			ssize_t sent = sendto(clientSocket, eofPacket, eofSize, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
			if(sent < 0){
				perror("sendto for EOF packet Failed");
				free(eofPacket);
			}
			free(eofPacket);
			std::cout << "All Packet send and no more data to send" << "\n";
			break;
		}
	}

	
	std::cout << "Closing Connection" << "\n";
	file.close();
	close(clientSocket);
}

int retransmit(int expectedSeqNum,int clientSocket,const struct sockaddr* serverAddress,std::ifstream& file, const std::map<uint32_t, std::pair<long,uint16_t>>& metaMap){
	file.clear();
	auto it = metaMap.find(expectedSeqNum);
	if(it == metaMap.end()){
		std::cerr << "Missing metadata for seq=" << expectedSeqNum <<"\n";
		return -1;
	}
	long offset = it->second.first;
	uint16_t dataSize = it->second.second;
	
	file.seekg(0,std::ios::end);
	if (offset > file.tellg()){
		std::cerr << "Invalid offset: beyond file size. Closing.\n";
		return 2;
	}

	file.seekg(offset, std::ios::beg); //utilize seekg() to point the fd to the data and use SEEK_SET to go from the beginning of the file
	size_t totalSize = sizeof(UDPPacket) + dataSize;
	UDPPacket* lostPacket = (UDPPacket*)malloc(totalSize);
	if(!lostPacket){
		perror("malloc failed");
		return -1;
	}
	memset(lostPacket,0,totalSize);
	lostPacket->sequenceNumber = htonl(expectedSeqNum);
	file.read(lostPacket->data,dataSize);

	std::streamsize bytesRead = file.gcount();

	if (bytesRead <= 0){ // if we read bytes form the file and it's less than 0{
		std::cerr << "Failed to re-read file at offset " << offset << "\n";
        free(lostPacket);
        return -1;
	}
	if(bytesRead != dataSize) {
			std::cerr << "Warning: Read " << bytesRead << " bytes but expected " << dataSize 
					  << " bytes for seq=" << expectedSeqNum << "\n";
			// Continue but use the actual bytes read
    }	
	lostPacket->payloadSize = htons(static_cast<uint16_t>(bytesRead));
	ssize_t sent = sendto(clientSocket,lostPacket,totalSize, 0,serverAddress,sizeof(struct sockaddr_in));
	if(sent == -1){
		perror("sendto failed");
		free(lostPacket);
		return -1;
	}
	free(lostPacket);
	return 0;
}
