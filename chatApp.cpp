#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <limits>
#include <algorithm>

using namespace std;

#define BACKLOG 10000
#define BUFFER_SIZE 16000
#define CHECKIP 500
#define CHECKPORT 501
#define CHECKIPANDPORT 503
#define INCOMINGMSG 550
#define OUTGOINGMSG 551
#define LOGGEDIN "logged-in"
#define LOGGEDOUT "logged-out"
#define SEND "SEND"
#define BROADCAST "BROADCAST"
#define ADDTOLIST 600
#define REMOVEFROMLIST 601

string port;
int ssocket, csocket;
bool is_server;

struct clientStruct
{
	string ip;
	string port;
	string hostname;
	int	socketnum;
	int outgoing = 0;
	int incoming = 0;
	string onlinestatus = LOGGEDOUT;
	vector<clientStruct> blocklist;
};

vector<clientStruct> clients;

struct messageBuffer
{
	string bufferDestIp;
	string bufferSourceIp;
	string bufferMsg;
	string msgType;
};

vector<messageBuffer> bufferList;

void server(string command_str, vector<string> inputs);
void prompt(bool isserver);

void PORT()
{
	printf("PORT:%d\n", stoi(port));
}

//Utility method for Splitting the string based on delimiter (used for parsing data from string)
vector<string> splitString(string string_msg, char delimiter) 
{
	vector<string> splitted;
	string temp = "";
	for(int i =0; i < string_msg.length() ; i++)
	{	
		if(string_msg[i] == delimiter) 
		{
			splitted.push_back(temp);
			temp = "";
			continue;
		}
		
		temp = temp + string_msg[i];

		if(string_msg[i+1] == '\0')
		{
			splitted.push_back(temp);
		}
	}

	return splitted;
}

string IP() //Used reference from guide provided in the handout document and beej guide
{	
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	getaddrinfo("8.8.8.8", "53", &hints, &res);
	int socketd = socket(res->ai_family, SOCK_DGRAM, 0); //https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#socket
	
	connect(socketd, res->ai_addr, res->ai_addrlen);

	socklen_t addr_len = sizeof(struct sockaddr_in);
	struct sockaddr_in local;
	int status = getsockname(socketd, (struct sockaddr*)&local, &addr_len);

	char ip_addr[16];
	inet_ntop(AF_INET, &local.sin_addr, ip_addr, sizeof(ip_addr)); //https://beej.us/guide/bgnet/html/split/man-pages.html

	return ip_addr;
}

//For checking if port has all digits only
//https://www.geeksforgeeks.org/isdigit-function-in-c-cpp-with-examples/
bool are_digits(string &str) 
{
    for (char c : str) 
	{
        if (!isdigit(c)) return false;
    }
    return true;

}

//Checking if IP and Port are valid for LOGIN
//https://stackoverflow.com/questions/318236/how-do-you-validate-that-a-string-is-a-valid-ipv4-address-in-c
bool isValidIpPort(vector<string>& inputs, int checkIpOrPort)
{
	if(checkIpOrPort == CHECKIP || checkIpOrPort == CHECKIPANDPORT)
	{
		vector<string> ipParts = splitString(inputs[1], '.');

		if(ipParts.size() != 4) 
		{
			return false; 
		}
		
		for(string &ip_segment : ipParts) 
		{
			if( (are_digits(ip_segment) == false) || (ip_segment.empty())) 
			{
				return false; 
			}
			int ip_seg_int = stoi(ip_segment);
			if(ip_seg_int < 0 || ip_seg_int > 255) 
			{
				return false; 
			}
		}
	}
	if(checkIpOrPort == CHECKPORT || checkIpOrPort == CHECKIPANDPORT)
	{
		string port_str = inputs[2];
		if(are_digits(port_str) == false) return false; 
		int port_int = stoi(port_str);
		if(port_int < 0 || port_int > 65535) return false; 
	}

    return true; 
}

bool isValidClient(string _ip)
{
	for(clientStruct mm: clients)
	{
		if(_ip.compare(mm.ip) == 0 && mm.onlinestatus.compare(LOGGEDIN) == 0)
		{
			return true;
		}
	}
	return false;
}

void handleMessageNumbers(string _ip, int messageType)
{
	for(clientStruct &client: clients)
	{
		if(_ip.compare(client.ip) == 0)
		{
			if(messageType == INCOMINGMSG)
			{
				client.incoming = client.incoming + 1;
			}
			else if(messageType == OUTGOINGMSG)
			{
				client.outgoing = client.outgoing + 1;
			}
		}
	}
}

void handleStatus(string _ip, string _status)
{
	for(clientStruct &client: clients)
	{
		if(_ip.compare(client.ip) == 0)
		{
			client.onlinestatus = _status;
		}
	}
}

//Converting the data from vector of clients to string to send the data to the client from server
string getClientsStringList()
{
	string list = "";
	for(clientStruct currentClient: clients)
	{
		if(currentClient.onlinestatus.compare(LOGGEDIN) == 0)
		{
			list = list + currentClient.ip + '|' + currentClient.port + '|' + currentClient.hostname + '|' + currentClient.onlinestatus +  '~'; 
		}
	}
	list.pop_back();
	return list;
}

//Utility method to convert string data received from client to the clients vector from server instance
void pushtoClients(string _ip, string _port, string _hostname, int _sindex)
{
	clientStruct currentClient;
	currentClient.port = _port;
	currentClient.hostname = _hostname;
	currentClient.socketnum = _sindex;
	currentClient.ip = _ip;

	if(clients.size() == 0)
	{
		clients.push_back(currentClient);
		return;
	}

	int index = 0;
	for(clientStruct mm: clients)
	{
		if(stoi(_port) <= stoi(mm.port))
		{
			break;
		}
		index++;
	}

	clients.insert(clients.begin() + index, currentClient);
}

//Method to remove a client from clients vector when it exits the server
void removeClient(int clientport, string _ip)  
{
	int index = 0;
	for(clientStruct mm: clients)
	{
		if(clientport == stoi(mm.port) && _ip.compare(mm.ip) == 0)
		{
			break;
		}
		index++;
	}
	clients.erase(clients.begin() + index);
}

int getSocketFromIP(string _ip)//, string _port)
{
	int index = 0;
	for(clientStruct mm: clients)
	{
		if(_ip.compare(mm.ip) == 0)// && _port.compare(getPort(mm)) == 0)
		{
			return mm.socketnum;
		}
		index++;
	}
	return -1;
}

string getStatus(string _ip)
{
	for(clientStruct mm: clients)
	{
		if(_ip.compare(mm.ip) == 0)
		{
			return mm.onlinestatus;
		}
	}
	return LOGGEDOUT;
}

void handleBuffer(string _ip)
{
	for(auto iter = bufferList.begin(); iter != bufferList.end();) //https://stackoverflow.com/questions/4645705/vector-erase-iterator
	{
		messageBuffer mBuffer = (*iter);
		if(_ip.compare(mBuffer.bufferDestIp) == 0)
		{
			sleep(3);
			string destinationPrint = mBuffer.msgType.compare(SEND) == 0 ? mBuffer.bufferDestIp : "255.255.255.255";
			string _response = mBuffer.bufferSourceIp + "|" + mBuffer.bufferMsg;
			send(getSocketFromIP(_ip), _response.c_str(), _response.length(), 0);
			printf("\n[%s:SUCCESS]\n", "RELAYED");
			printf("msg from:%s, to:%s\n[msg]:%s\n", mBuffer.bufferSourceIp.c_str(), destinationPrint.c_str(), mBuffer.bufferMsg.c_str());
			printf("[%s:END]\n", "RELAYED");
			fflush(stdout);	
			iter = bufferList.erase(iter);
			handleMessageNumbers(_ip, INCOMINGMSG);
		}
		else
		{
			++iter;
		}
	}
}

bool isPresentInBlockList(clientStruct client, string _ip)
{
	for(auto blockedClient: client.blocklist)
	{
		if(blockedClient.ip.compare(_ip) == 0)
		{
			return true;
		}
	}
	return false;
}

void handleBlockList(clientStruct &sourceClient, string blockIp, int addOrRemove)
{
	if(addOrRemove == REMOVEFROMLIST)
	{
		for(vector<clientStruct>::iterator blockedClient = sourceClient.blocklist.begin(); blockedClient != sourceClient.blocklist.end(); blockedClient++)
		{
			if((*blockedClient).ip.compare(blockIp) == 0)
			{
				sourceClient.blocklist.erase(blockedClient);
				return ;
			}
		}
	}

	vector<clientStruct> tempBlockList;
	for(auto client: clients)
	{
		if(sourceClient.ip.compare(client.ip) == 0)
		{
			continue;
		}
		if(client.ip.compare(blockIp) == 0 || isPresentInBlockList(sourceClient, client.ip)) 
		{
			tempBlockList.push_back(client);
		}
	}

	sourceClient.blocklist = tempBlockList;
}

void startServer() //Used demo code as reference
{
	struct addrinfo hints, *res;
	int ssocket, head_socket, selret, socket_idx, fdaccept = 0;
	struct sockaddr_in client_addr;
	socklen_t caddr_len;
	fd_set master_list, watch_list;
	FD_ZERO(&master_list);
	FD_ZERO(&watch_list);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if(getaddrinfo(NULL, port.c_str(), &hints, &res) != 0)
	{
		printf("%s", "getaddrinfo failed");
	}

	ssocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(ssocket < 0)
	{
		printf("%s", "Socket creation failed");
	}

	if(bind(ssocket, res->ai_addr, res->ai_addrlen) < 0)
	{
		printf("%s", "Bind failed");
	}

	freeaddrinfo(res);

	if(listen(ssocket, BACKLOG) < 0)
	{
		printf("%s", "Error listening to socket");
	}
	
	FD_SET(ssocket, &master_list);
	FD_SET(STDIN_FILENO, &master_list);
	head_socket = ssocket;

	while(true) 
	{
		memcpy(&watch_list, &master_list, sizeof(master_list));

		prompt(true);

		selret = select(head_socket + 1, &watch_list, NULL, NULL, NULL);
		if(selret < 0)
		{
			printf("%s", "select failed");
		}
		if (selret > 0) 
		{
			for(socket_idx = 0; socket_idx <= head_socket; socket_idx+=1)
			{
				if(FD_ISSET(socket_idx, &watch_list))
				{
					if(socket_idx == STDIN_FILENO) 
					{
						char msg[256];
						string string_msg = fgets(msg, sizeof(msg), stdin);
						if (string_msg.length() > 0) string_msg.pop_back();
						vector<string> inputs = splitString(string_msg, ' ');
						string_msg = inputs.front();
						server(string_msg, inputs);
					}
					else if(socket_idx == ssocket) 
					{
						caddr_len = sizeof(client_addr);
						fdaccept = accept(ssocket, (struct sockaddr *)&client_addr, &caddr_len);
						if(fdaccept < 0) 
						{
							printf("Connection Failed!");
						}
						FD_SET(fdaccept,&master_list);
						if(fdaccept > head_socket) 
						{
							head_socket = fdaccept;
						}
					}
					else
					{
						char *buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
						memset(buffer, '\0', BUFFER_SIZE);
						if(recv(socket_idx, buffer, BUFFER_SIZE, 0) <= 0)
						{
							close(socket_idx);
							FD_CLR(socket_idx, &master_list);
						}
						else //Used to respond the client based on the request it got from the string parsing
						{
							vector<string> received = splitString(buffer, '|');
							if(received[0].compare("FIRST") == 0)
							{
								string _ip = received[1];
								string _port = received[2];
								string _hostname = received[3];
								bool contains = false;
								for(auto client: clients)
								{
									if(_ip.compare(client.ip) == 0)
									{
										contains = true;
									}
								}
								if(!contains)
								{
									pushtoClients(_ip, _port, _hostname, fdaccept);
								}
								handleStatus(_ip, LOGGEDIN);
								string _response = getClientsStringList();
								send(socket_idx, _response.c_str(), _response.length(), 0);
								if(bufferList.size() <= 100)
								{
									handleBuffer(_ip);
								}
								fflush(stdout);
							}
							if(received[0].compare("REFRESH") == 0)
							{
								string _response = getClientsStringList();
								send(socket_idx, _response.c_str(), _response.length(), 0);
								fflush(stdout);
							}
							if(received[0].compare("SEND") == 0)
							{
								string _sourceIp = received[1];
								string _destIp = received[2];
								string _response = _sourceIp + "|" + received[3];
								string _status = getStatus(_destIp);
								bool isBlocked = false;
								for(clientStruct client: clients)
								{
									if(client.ip.compare(_destIp) == 0)
									{
										isBlocked = isPresentInBlockList(client, _sourceIp);
										break;
									}
								}
								if(isBlocked)
								{
									free(buffer);
									continue;
								}
								if(_status.compare(LOGGEDOUT) == 0)
								{
									messageBuffer tempBuff;
									tempBuff.bufferSourceIp = _sourceIp;
									tempBuff.bufferDestIp = _destIp;
									tempBuff.bufferMsg = received[3];
									tempBuff.msgType = SEND; 
									bufferList.push_back(tempBuff);
									free(buffer);
									continue;
								}
								int _socket = getSocketFromIP(_destIp);
								send(_socket, _response.c_str(), _response.length(), 0);
								printf("\n[%s:SUCCESS]\n", "RELAYED");
								printf("msg from:%s, to:%s\n[msg]:%s\n", _sourceIp.c_str(), _destIp.c_str(), received[3].c_str());
								printf("[%s:END]\n", "RELAYED");
								handleMessageNumbers(_sourceIp, OUTGOINGMSG);
								handleMessageNumbers(_destIp, INCOMINGMSG);
								fflush(stdout);
							}
							if(received[0].compare("BROADCAST") == 0)
							{
								string _sourceIp = received[1];
								string _destIp = "255.255.255.255";
								string _response = _sourceIp + "|" + received[2];
								handleMessageNumbers(_sourceIp, OUTGOINGMSG);
								bool sent = false;
								for(clientStruct client: clients)
								{
									int _socket = client.socketnum;
									if(isPresentInBlockList(client, _sourceIp) || _socket == getSocketFromIP(_sourceIp))
									{
										continue;
									}
									if(client.onlinestatus.compare(LOGGEDOUT) == 0)
									{
										messageBuffer tempBuff;
										tempBuff.bufferSourceIp = _sourceIp;
										tempBuff.bufferDestIp = client.ip;
										tempBuff.bufferMsg = received[2];
										tempBuff.msgType = BROADCAST; 
										bufferList.push_back(tempBuff);
										continue;
									}
									send(_socket, _response.c_str(), _response.length(), 0);
									handleMessageNumbers(client.ip, INCOMINGMSG);
									sent = true;
								}
								if(sent)
								{
									printf("\n[%s:SUCCESS]\n", "RELAYED");
									printf("msg from:%s, to:%s\n[msg]:%s\n", _sourceIp.c_str(), _destIp.c_str(), received[2].c_str());
									printf("[%s:END]\n", "RELAYED");
									fflush(stdout);
								}
							}
							if(received[0].compare("BLOCK") == 0)
							{
								string _sourceip = received[1];
								string _blockip = received[2];
								bool success = false;
								for(clientStruct &client: clients)
								{
									if(client.ip.compare(_sourceip) == 0)
									{
										bool present = isPresentInBlockList(client, _blockip);
										if(!present)
										{
											handleBlockList(client, _blockip, ADDTOLIST);
											success = true;
										}
										break;
									}
								}
								string _response = success ? "SUCCESS" : "FAILED";
								int _socket = getSocketFromIP(_sourceip);
								send(_socket, _response.c_str(), _response.length(), 0);
							}
							if(received[0].compare("UNBLOCK") == 0)
							{
								string _sourceip = received[1];
								string _unblockip = received[2];
								bool success = false;
								for(clientStruct &client: clients)
								{
									if(client.ip.compare(_sourceip) == 0)
									{
										bool present = isPresentInBlockList(client, _unblockip);
										if(present)
										{
											handleBlockList(client, _unblockip, REMOVEFROMLIST);
											success = true;
										}
										break;
									}
								}
								string _response = success ? "SUCCESS" : "FAILED";
								int _socket = getSocketFromIP(_sourceip);
								send(_socket, _response.c_str(), _response.length(), 0);
							}
							if(received[0].compare("LOGOUT") == 0)
							{
								string _ip = received[1];
								string _port = received[2];
								int _socket = getSocketFromIP(_ip);//, _port);
								close(_socket);
								shutdown(_socket, SHUT_RDWR);
								FD_CLR(_socket, &watch_list);
								FD_CLR(_socket, &master_list);
								handleStatus(_ip, LOGGEDOUT);
							}
							if(received[0].compare("EXIT") == 0)
							{
								string _ip = received[1];
								string _port = received[2];
								int _socket = getSocketFromIP(_ip);//, _port);
								removeClient(stoi(_port), _ip);								
							}
						}
						
						free(buffer);
					}
				}
			}
		}
	}
}

void send(string message)
{
	if(send(csocket, message.c_str(), strlen(message.c_str()), 0) == strlen(message.c_str())) //https://beej.us/guide/bgnet/html/#sendrecv and demo code
	fflush(stdout);
}

string receive()
{
	char *buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
	memset(buffer, '\0', BUFFER_SIZE);
	if(recv(csocket, buffer, BUFFER_SIZE, 0) >= 0){ //https://beej.us/guide/bgnet/html/#sendrecv and demo code
		fflush(stdout);
	}

	string response = buffer;
	free(buffer);

	return response;
}

//Utility method to add/update the clients vector based on latest response from client/server
void addResponseToClients(string response)
{
	clients.clear();
	vector<string> responseClients = splitString(response, '~');
	for(string clientdet: responseClients) //vector<map<string, vector<string>>> clients;
	{
		clientStruct clientFull;
		vector<string> clientDetails = splitString(clientdet, '|');
		clientFull.ip = clientDetails[0];
		clientFull.port = clientDetails[1];
		clientFull.hostname = clientDetails[2];
		clientFull.onlinestatus = clientDetails[3];
		clients.push_back(clientFull);
	}	
}

//For LIST command
void LIST()
{
	int count = 1;
	for(clientStruct client: clients)
	{
		if(client.onlinestatus.compare(LOGGEDIN) == 0)
		{
			printf("%-5d%-35s%-20s%-8d\n", count, client.hostname.c_str(), client.ip.c_str(), stoi(client.port));
			fflush(stdout);
			count++;
		}
	}
}

//For LOGIN command, to connect with server and send/receive the client details as well
void LOGIN(const char *server_ip, const char *server_port) //
{
	int client_socketfd;
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(server_ip, server_port, &hints, &res) != 0) //https://beej.us/guide/bgnet/html/#getaddrinfo
	{
		throw string("Socket Creation Failed 1");
	}

	client_socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); //https://beej.us/guide/bgnet/html/#socket

	if(client_socketfd < 0 )
	{
		throw string("Socket Creation Failed 2");
	}

	if(connect(client_socketfd, res->ai_addr, res->ai_addrlen) < 0) //https://beej.us/guide/bgnet/html/#connect
	{
		throw string("Socket Creation Failed 3");
	} 

	csocket = client_socketfd;

	char hostname[512];
	gethostname(hostname, 512); //https://stackoverflow.com/questions/27914311/get-computer-name-and-logged-user-name#
	string firstMessage = "FIRST|" + string(IP()) + "|" + string(port) + "|" + string(hostname);
	send(firstMessage);
	addResponseToClients(receive());
}

//Common method to call for both server and client
void common(string command_str)
{
	if (command_str.compare("LIST") == 0)
	{
			printf("[%s:SUCCESS]\n", command_str.c_str());
			LIST();
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
	}
	else if(command_str.compare("IP") == 0)
	{
		try
		{
			string ip_addr = IP();
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("IP:%s\n", ip_addr.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		catch(exception e)
		{
			printf("[%s:ERROR]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
	}
	else if(command_str.compare("PORT") == 0)
	{
		printf("[%s:SUCCESS]\n", command_str.c_str());
		PORT();
		printf("[%s:END]\n", command_str.c_str());
		fflush(stdout);
	}

	fflush(stdout);
}

//Server function to call only server related commands
void server(string command_str, vector<string> inputs)
{
	try
	{
		if(command_str.compare("STATISTICS") == 0)
		{
			int count = 1;
			printf("[%s:SUCCESS]\n", command_str.c_str());
			for(clientStruct client: clients)
			{
				printf("%-5d%-35s%-8d%-8d%-8s\n", count, client.hostname.c_str(), client.outgoing, client.incoming, client.onlinestatus.c_str());
				count++;
			}
			printf("[%s:END]\n", command_str.c_str());
		}
		else if(command_str.compare("BLOCKED") == 0)
		{
			int count = 1;
			if(inputs.size() == 0 || !isValidIpPort(inputs, CHECKIP) || !isValidClient(inputs[1]))
			{
				throw string("Not valid IP");
			}

			string clientIp = inputs[1];
			printf("[%s:SUCCESS]\n", command_str.c_str());
			for(clientStruct client: clients)
			{
				if(client.ip.compare(clientIp) == 0)
				{
					for(clientStruct blockedClient: client.blocklist)
					{
						printf("%-5d%-35s%-20s%-8d\n", count, blockedClient.hostname.c_str(), blockedClient.ip.c_str(), stoi(blockedClient.port));
						fflush(stdout);
						count++;
					}
				}
			}
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else
		{
			common(command_str);
		}	
	}
	catch(const string &e)
	{
		printf("[%s:ERROR]\n", command_str.c_str());
		printf("[%s:END]\n", command_str.c_str());
		fflush(stdout);
	}
}

//client function to call only client related commands
void client(string command_str, vector<string> inputs)
{
	try
	{
		if(command_str.compare("LOGIN") == 0)
		{
			if(!isValidIpPort(inputs, CHECKIPANDPORT))
			{
				throw string("Not valid Login");
			}
			LOGIN(inputs[1].c_str(), inputs[2].c_str());
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("REFRESH") == 0)
		{
			send("REFRESH");
			addResponseToClients(receive());
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("SEND") == 0)
		{
			string _ip = inputs[1];
			string _msg = "";
			for(int i = 2; i < inputs.size(); i++)
			{
				_msg += inputs[i];
				if(i != inputs.size() - 1)
				{
					_msg = _msg + " ";
				}
			}

			if(!isValidIpPort(inputs, CHECKIP) || !isValidClient(_ip))
			{
				throw string("Not valid IP");
			}

			string _send = "SEND|" + IP() + "|" + _ip + "|" + _msg + "|";// + "|" + _port;
			send(_send);
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("BROADCAST") == 0)
		{
			string _msg = "";
			for(int i = 1; i < inputs.size(); i++)
			{
				_msg += inputs[i];
				if(i != inputs.size() - 1)
				{
					_msg = _msg + " ";
				}
			}

			string _send = "BROADCAST|" + IP() + "|" + _msg + "|";// + "|" + _port;
			send(_send);
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("BLOCK") == 0)
		{
			string _ip = inputs[1];

			if(!isValidIpPort(inputs, CHECKIP) || !isValidClient(_ip))
			{
				throw string("Not valid IP");
			}

			string _send = "BLOCK|" + IP() + "|" + _ip + "|";
			send(_send);
			string outcome = receive();
			if(outcome.compare("FAILED") == 0)
			{
				throw string("FAILED");
			}
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("UNBLOCK") == 0)
		{
			string _ip = inputs[1];

			if(!isValidIpPort(inputs, CHECKIP) || !isValidClient(_ip))
			{
				throw string("Not valid IP");
			}

			string _send = "UNBLOCK|" + IP() + "|" + _ip + "|";
			send(_send);
			string outcome = receive();
			if(outcome.compare("FAILED") == 0)
			{
				throw string("FAILED");
			}
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("LOGOUT") == 0)
		{
			string _logout = "LOGOUT|" + string(IP()) + "|" + string(port) + "|";
			send(_logout);
			close(csocket);
			shutdown(csocket, SHUT_RDWR);
			csocket = 0;
			clients.clear();
			printf("[%s:SUCCESS]\n", command_str.c_str());
			printf("[%s:END]\n", command_str.c_str());
			fflush(stdout);
		}
		else if(command_str.compare("EXIT") == 0)
		{
			string _exit = "EXIT|" + string(IP()) + "|" + string(port) + "|";
			send(_exit);
			exit(0);
		}
		else
		{
			common(command_str);
		}
	}
	catch(const string &e)
	{
		printf("[%s:ERROR]\n", command_str.c_str());
		printf("[%s:END]\n", command_str.c_str());
		fflush(stdout);
	}
}

void prompt(bool server)
{
	if(server)
	{
		printf("\n[Server]$ ");
	}
	else
	{
		printf("\n[Client]$ ");
	}
	fflush(stdout);
}

int main(int argc, char **argv)
{
	string option1 = argv[1] ? argv[1] : "";
	is_server = option1.compare("s") == 0;
	port = argv[2] ? argv[2] : "";

	if(is_server)
	{
		startServer();
	}

	while(true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(csocket, &readfds);
		FD_SET(STDIN_FILENO, &readfds);
		prompt(is_server);		
    	int num_ready = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if(num_ready == -1) 
		{
			printf("Error at Select");
		}
		else if(num_ready > 0) 
		{
			if(FD_ISSET(csocket, &readfds)) 
			{
				string _msg = receive();
				if(!_msg.empty())
				{
					vector<string> received = splitString(_msg, '|');
					printf("\n[%s:SUCCESS]\n", "RECEIVED");
					printf("msg from:%s\n", received[0].c_str());
					printf("[msg]:%s\n", received[1].c_str());
					printf("[%s:END]\n", "RECEIVED");
				}
			}
			if(FD_ISSET(STDIN_FILENO, &readfds)) 
			{
				char msg[512];
				string string_msg = fgets(msg, sizeof(msg), stdin);
				if (string_msg.length() > 0) 
				{
					string_msg.pop_back();
				}
				vector<string> inputs = splitString(string_msg, ' ');
				string_msg = inputs.front();
				
				client(string_msg, inputs);
				if(inputs[0].compare("LOGOUT") == 0)
				{
					FD_CLR(csocket, &readfds);
				}
			}		
		}
		fflush(stdout);
	}
		
	return 0;
}