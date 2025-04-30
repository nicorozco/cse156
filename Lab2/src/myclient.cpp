#include <iostream>
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

bool isValidIPv4Format(const std::string& ip){	
	std::regex ipv4Pattern(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
	return std::regex_match(ip, ipv4Pattern);
}

int main (int argc, char* argv[]) {
	
	std::string serverIP;
	std::string Port;
	std::string infilePath;
	std::string outfilePath;
	char buffer[1472];
	std::string line;
	std::map<uint32_t,UDPPacket> recievedPackets;
	uint32_t expectedSeqNum = 0;
	uint32_t seqNum;
	bool serverActive = false;

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

	int serverPort = std::stoi(Port);

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
	serverAddress.sin_family = AF_INET;
	
	//checking if port is valid
	if (serverPort < 1 || serverPort > 65535){
		std::cerr << "Error: Invalid port number\n";
		close(clientSocket);
		return 6;
	}
	serverAddress.sin_port = htons(serverPort); 
	
	// 2d.) set the IP Address
	if (inet_pton(AF_INET,serverIP.c_str(),&serverAddress.sin_addr) <= 0 ){
		// check for errors: 1 means address was set succesfuly, anything less than 1 means error
		std::cerr << "Invalid Address" << "\n";
		close(clientSocket);
		return 7;
	}
	
	//3.) Client reads a file from disk & Sends it over UDP Socket
	//a.) Sending Data over a UDP Socket 
	// utilize sendto() Note: each call to sendto() sends a single UDP Diagram
	// open the file 
	//infilePath = "file.txt"; 
	std::ifstream file(infilePath,std::ios::binary);
	// check for error when opening file
	if(!file.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
		return -1;
	}

	uint32_t sequence = 0;
	
	//reading data & sending packets
	while(file){
		//std::cout << "Reading File" << "\n";
		//std::cout << "Sequence:" << sequence << "\n";
		//create an udp packet
		UDPPacket packet;
		packet.sequenceNumber = htons(sequence++);
		//std::cout << "Packet Sequence Number"<< htons(packet.sequenceNumber) <<"\n"; 
		//add the data to the packet

		file.read(packet.data,sizeof(packet.data));
		std::streamsize bytesRead = file.gcount();
		//std::cout << "Bytes Read: " << bytesRead << "\n";
		
		//for testing data 
		/*for(int i = 0; i < bytesRead; i++){
			std::cout << packet.data[i];
		}
		std::cout << "\n";
		*/

		//if the bytesRead is less than zero meaning we have data send it 
		if(bytesRead > 0){
			// send the data
			//std::cout << "Sending Data" << "\n";
			ssize_t sentBytes = sendto(clientSocket,&packet, sizeof(uint32_t) + bytesRead, 0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));			// insert the packet into the sent packet	
			if(sentBytes < 0){
				perror("sendto failed");
				close(clientSocket);
				return -1;
			}

			usleep(10000);
		}
	}

	//std::cout << "Finished Reading File" << "\n";
	file.close(); //close the file until we are finished with everything as we still need it to retransmit lost data
	//b.) Client should be able to detect packet losses
	// utilize sequence numbers

	//4.) Client recieved echo package from the server
	// utilize recvfrom() Note: returns one UDP Packet per call
	// recvfrom() returns the number of bytes recievied from the scoket 
	socklen_t addrlen = sizeof(serverAddress);
	int bytes_recieved;

	std::ofstream outFile(outfilePath);//open file path for writing
	if(!outFile.is_open()){
		std::cerr << "Failed to open file for writing" << std::strerror(errno) << "\n";
		return -1;
	}

	file.open(infilePath,std::ios::binary); //reopen the file for packet retransmission
	// check for error when opening file
	if(!file.is_open()){
		std::cerr << "Error: " << std::strerror(errno) << "\n";
		return -1;
	}

	//hanging is going on here 	
	//Recieving echoed packets
	while(true){
		// since the data stored in the buffer are raw bytes, we need to cast back into UDP Struct to interpret the data
		//std::cout << "Recieving for Echo Server" << "\n";
		// call select() to check if the socket is reading, if the socket is ready call recvfrom()
		

		//utilize select() to know when to socket is ready for reading	
		fd_set rset; // create socket set
		FD_ZERO(&rset);//clear the socket set
		FD_SET(clientSocket,&rset); //add the clientsocket to the set 
		//set a timer utilize select to prevent hanging forever while waiting to recieved packeyt 
		
		struct timeval timeout;
		timeout.tv_sec = 60;
		timeout.tv_usec = 0;
			
		
		int active = select(clientSocket+1,&rset,NULL,NULL,&timeout);

		// if the port isn't active 
		if(active == 0){
			//time out due to the server not being detected
			if(!serverActive){
				std::cerr << "Timeout Occured, Cannot detect server"<<"\n";
				return -1;
			}
			break;
		}else if (active < 0){
			std::cerr << "Select Error Occured" << "\n";
			return -1;
		}else if(FD_ISSET(clientSocket, &rset)){ //clientSocket is ready, meaning we are ready to recieve data -> call recvfrom()	
			std::cout << "Client Socket is Ready" << "\n";
			bytes_recieved = recvfrom(clientSocket,buffer,sizeof(buffer),0, (struct sockaddr*)&serverAddress, &addrlen);//call recieved to read the data 			

				//if we are recieving data
			if(bytes_recieved > 0){

				std::cout << "recieving Data" <<"\n";
				serverActive = true; // this means the server is active and we set the flag
				//process the packet
				UDPPacket* receivedPacket = reinterpret_cast<UDPPacket*>(buffer);
				seqNum = ntohs(receivedPacket->sequenceNumber); //extract the sequence number
				recievedPackets[seqNum] = *receivedPacket;//instert the pair in the map
		
				//while the sequence number is found in the map
				if(recievedPackets.count(expectedSeqNum)){
					std::cout << "Packet Found" << "\n";
					UDPPacket& pkt = recievedPackets[expectedSeqNum];
					std::cout << "Packet: " << expectedSeqNum << "\n";
					outFile << pkt.data; //write into the oufile
					recievedPackets.erase(expectedSeqNum); //erase from the recieved map
					expectedSeqNum++; // increase the sequence nubmer
					usleep(5000);
				}else if(recievedPackets.size() >= 3){//if the packet isn't detected within 5 packets detect packet loss & retransmitt	
					std::cerr << "Packet " << expectedSeqNum << " Loss Detected" << "\n";
					UDPPacket lostPacket; //create the packet
					lostPacket.sequenceNumber = htons(expectedSeqNum);//set the sequence number	
				
						//recover the data associated with the sequence number from the original file using seekg()
					long offset = expectedSeqNum * 1468;
					//std::cout << "Offset: " << offset << "\n";
					//check if osset is valid before reading
					file.seekg(0,std::ios::end);
					std::streampos fileSize = file.tellg();	
					
					//std::cout << "File Size: " << fileSize << "\n";
					if (offset > fileSize){
						std::cerr << "Invalid offset: beyond file size. Closing.\n";
						return 2;
					}

					file.seekg(offset, std::ios::beg); //utilize seekg() to point the fd to the data and use SEEK_SET to go from the beginning of the file
					file.read(lostPacket.data,1468); //utilize read to read into the data
					
					std::streamsize bytes_reRead = file.gcount();
					
					if (bytes_reRead <= 0){ // if we read bytes form the file and it's less than 0{
						if(file.eof()){
							//we have reached the end of file
							std::cerr << "EOF Reached, no more data." << "\n";
							break;
						}else if(file.fail()){
							//we failed to reach the file
							std::cerr << "Read failed due to logical error" << "\n";
							break;
						} else if(file.bad()){
							//server issue
							std::cerr << "Severe read error"<< "\n";
							break;
						}else{
							std::cerr <<"Failed to read missing data from file" << "\n";
							break;
						}
					}
					//once the data is succesfuly read retransmit the packet and go back to the top of the while loop for recieving
					std::cout << "Retransmitting Packet" << "\n";
					sendto(clientSocket,&lostPacket, sizeof(uint32_t) + bytes_reRead, 0,(struct sockaddr*)&serverAddress,sizeof(serverAddress));
					continue;
				}		
			} else if (bytes_recieved == 0){
				//means we are no longer recieving data
				std::cerr << "End of File Reached, no longer recieving data" << "\n";
				return 2;
			}
		}
	std::cout << "looping"<< "\n";
	}
	
	
	//compare the file outputs
		
	
	std::cout << "Closing Connection" << "\n";
	outFile.close();
	file.close();
	close(clientSocket);
	return 0;
}
