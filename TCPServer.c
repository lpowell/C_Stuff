// Includes

#include <stdio.h>
#include <WinSock2.h>
#include <time.h>

// pragma winsock lib
#pragma comment(lib, "ws2_32.lib")

int main() {
	// Struct to hold Windows Socket init
	WSADATA wsa;
	SOCKET server_socket, client_socket;
	
	// struct to hold server and client address
	struct sockaddr_in server, client;

	// store client and received data size
	int client_size, recv_size; 
	// buffer
	char client_message[500];
	
	// client IP buffer
	char client_ip[20];

	// Time 
	time_t t = time(NULL);
	struct tm tm;

	// init winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0 ){
		return 1;
	}

	// Create TCP socket
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET) {
		return 1;
	}

	// IPv4
	server.sin_family = AF_INET;
	// Bind to any interface
	server.sin_addr.s_addr = INADDR_ANY;
	// bind to port 1234
	server.sin_port = htons(1234);

	// Bind socket
	if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		return 1;
	}

	// listen forever
	while (1) {

		// Start listening
		// Queue 3 pending conns
		listen(server_socket, 3);

		//Accept conn
		client_size = sizeof(struct sockaddr_in);
		// same params as bind()
		client_socket = accept(server_socket, (struct sockaddr*)&client, &client_size);
		if (client_socket == INVALID_SOCKET) {
			return 1;
		}

		//Get conn time
		localtime_s(&tm, &t);

		// Get the client IP
		inet_ntop(AF_INET, &(client.sin_addr), client_ip, sizeof(client_ip));
		printf("Client %s connected at %d-%02d-%02d %02d:%02d:%02d\n", client_ip, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

		// do not close session until client exits
		while (1) {
			// clear client_message buffer
			memset(client_message, 0, sizeof(client_message));

			//receive message - 500 bytes
			recv_size = recv(client_socket, client_message, 500, 0);
			if (recv_size == SOCKET_ERROR) {
				printf("Error\n\r");
			}
			else if (recv_size == 0) {
				break;
			}


			//print message
			client_message[recv_size] = '\0';
			

			printf("CLIENT (%s)> %s",client_ip, client_message);

			// Send message
			const char* message = "Message received!\n";
			send(client_socket, message, strlen(message), 0);


			// Clear recv_size and client_message

		}
		// get disconn time 
		localtime_s(&tm, &t);
		printf("Client %s disconnected at %d-%02d-%02d %02d:%02d:%02d\n", client_ip, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	// close after 1 message for now
	closesocket(client_socket);
	closesocket(server_socket);
	WSACleanup();

	return 0;

}	
