// Includes
#include <stdio.h>          // Standard I/O functions for input and output
#include <WinSock2.h>      // Windows Sockets API for network communication
#include <time.h>          // Time functions for getting current time
#include <ws2tcpip.h>      // Additional socket functions and definitions

// Includes for multi-threading
#include <Windows.h>       // Windows API functions
#include <stdlib.h>        // Standard library for memory allocation and conversion functions
#include <string.h>        // String manipulation functions
#include <conio.h>         // Console input/output functions
#include <process.h>       // For _beginthread() function

// Define max threads 
#define MAX_THREADS 32    // Maximum number of threads allowed

HANDLE threads[MAX_THREADS]; // Array to hold thread handles
int thread_count = 0;        // Counter to track the current number of active threads
HANDLE thread_mutex;         // Mutex handle for thread synchronization

// For broadcasting
SOCKET client_sockets[MAX_THREADS]; // Socket Array of MAX_THREADS size
int connected_clients = 0;
HANDLE socket_mutex; // mutex for thread-safe access to client_sockets

// Time global
time_t t;                    // Global variable to hold the current time
struct tm tm;                // Global variable to hold the broken-down time structure

// pragma winsock lib
#pragma comment(lib, "ws2_32.lib") // Link against the Winsock library

// Create a struct to pass our values
struct arg_pass {
	SOCKET client_socket;    // Socket for the connected client
	char client_ip[20];      // Buffer for storing the client's IP address
};

// Broadcast function
void broadcast_message(const char *message, char* client_ip, int system_message) {
	struct sockaddr_in client_addr;
	int addr_size = sizeof(client_addr);
	
	WaitForSingleObject(socket_mutex, INFINITE); // locak mutex to access the socket
	for (int i = 0; i < connected_clients; i++) {
		if (client_sockets[i] != INVALID_SOCKET) {
			if (getpeername(client_sockets[i], (struct sockaddr*)&client_addr,&addr_size)==0) {
				char* current_ip[20];
				inet_ntop(AF_INET, &(client_addr.sin_addr), current_ip, sizeof(current_ip));
				if (strcmp(client_ip,current_ip)!=0 && system_message == 0) {
					send(client_sockets[i], message, strlen(message), 0);
				}
				else if (system_message == 1) {
					send(client_sockets[i], message, strlen(message), 0);
				}
			}
		}
	}

	ReleaseMutex(socket_mutex);
}


// Thread function to handle client connections
void client_conn(void* args) {
	// Cast the argument back to the correct type
	struct arg_pass* client_args = (struct arg_pass*)args;
	SOCKET client_socket = client_args->client_socket; // Retrieve client socket from struct
	char* client_ip = client_args->client_ip;         // Retrieve client IP from struct
	char client_message[500];                          // Buffer for incoming messages from client
	int recv_size;                                     // Variable to store the size of received messages

	// Loop to handle client communication until they disconnect
	while (1) {
		// Clear client_message buffer to avoid old data
		memset(client_message, 0, sizeof(client_message));

		// Receive message - up to 500 bytes
		recv_size = recv(client_socket, client_message, 500, 0);
		if (recv_size == SOCKET_ERROR) { // Check for errors during receiving
			printf("Error\n\r");
		}
		else if (recv_size == 0) { // Check if the client has disconnected
			break; // Exit the loop if no data received
		}

		// Print message to console
		client_message[recv_size] = '\0'; // Null-terminate the received message
		printf("CLIENT (%s)> %s", client_ip, client_message); // Print client message with IP
		char* return_message[500];
		sprintf_s(return_message, sizeof(return_message), "CLIENT (%s)> %s", client_ip, client_message);
		broadcast_message(return_message, client_ip, FALSE);
		//char* term[30];
		//sprintf_s(term, sizeof(term), "CLIENT (%s)>", client_ip);
		//send(client_socket,term,strlen(term),0);

	}

	// Get disconnect time
	time(&t); // Get the current time
	localtime_s(&tm, &t); // Convert time to local time structure
	// Send disconnection message with the time
	char* return_message[500];
	sprintf_s(return_message, sizeof(return_message), "Client %s disconnected at %d-%02d-%02d %02d:%02d:%02d\n",
		client_ip, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	printf("%s", return_message);
	broadcast_message(return_message,client_ip, TRUE);

	closesocket(client_socket); // Close the client socket

	WaitForSingleObject(thread_mutex, INFINITE); // Wait to acquire the mutex for thread safety
	thread_count--; // Decrement the thread count
	ReleaseMutex(thread_mutex); // Release the mutex

	// Remove the socket from the client_sockets array
	WaitForSingleObject(socket_mutex, INFINITE);  // Lock the socket mutex for thread-safe access
	for (int i = 0; i < connected_clients; i++) {
		if (client_sockets[i] == client_socket) {
			client_sockets[i] = INVALID_SOCKET;  // Mark the socket as invalid
			break;
		}
	}

	// Free client_args memory now that it's no longer needed
	free(client_args);

	return; // End the thread function
}

int main() {
	// Struct to hold Windows Socket initialization data
	WSADATA wsa;
	SOCKET server_socket, client_socket; // Socket for server and client connections

	// Struct to hold server and client address information
	struct sockaddr_in server, client;

	// Store client and received data size
	int client_size, recv_size;
	// Buffer for client messages
	char client_message[500];

	// Buffer for storing client IP addresses
	char client_ip[20];

	// Initialize Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { // Start Winsock with version 2.2
		return 1; // Exit if initialization failed
	}

	// Create TCP socket
	server_socket = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
	if (server_socket == INVALID_SOCKET) { // Check if socket creation failed
		return 1; // Exit on failure
	}

	// Set up the server address structure
	server.sin_family = AF_INET; // Use IPv4
	server.sin_addr.s_addr = INADDR_ANY; // Bind to any interface
	server.sin_port = htons(1234); // Bind to port 1234

	// Bind socket to the specified address and port
	if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		return 1; // Exit if binding failed
	}

	// Initialize mutex for thread synchronization
	thread_mutex = CreateMutex(NULL, FALSE, NULL); // Create a mutex for synchronizing access to shared resources
	socket_mutex = CreateMutex(NULL, FALSE, NULL);  // Mutex for client socket array

	// Listen for incoming connections in an infinite loop
	while (1) {
		// Start listening for incoming connections
		// Allow a queue of 3 pending connections
		listen(server_socket, 3);

		// Accept a new client connection
		client_size = sizeof(struct sockaddr_in); // Size of the client address structure
		client_socket = accept(server_socket, (struct sockaddr*)&client, &client_size); // Accept connection
		if (client_socket == INVALID_SOCKET) { // Check if accept failed
			return 1; // Exit on failure
		}

		// Get the current connection time
		time(&t); // Get the current time
		localtime_s(&tm, &t); // Convert to local time structure

		// Get the client IP address
		inet_ntop(AF_INET, &(client.sin_addr), client_ip, sizeof(client_ip)); // Convert binary IP to string

		// Allocate memory for the struct to pass client arguments
		struct arg_pass* client_args = (struct arg_pass*)malloc(sizeof(struct arg_pass));
		client_args->client_socket = client_socket; // Assign the client socket to the struct
		strcpy_s(client_args->client_ip, sizeof(client_args->client_ip), client_ip); // Copy client IP to struct

		// Add client socket to the global client_sockets array
		WaitForSingleObject(socket_mutex, INFINITE);  // Lock the socket mutex for thread-safe access
		if (connected_clients < MAX_THREADS) {
			client_sockets[connected_clients] = client_socket;
			connected_clients++;
		}
		ReleaseMutex(socket_mutex);  // Release the socket mutex

		// Print connection message with client IP and connection time
		char* return_message[500];
		sprintf_s(return_message, sizeof(return_message), "Client %s connected at %d-%02d-%02d %02d:%02d:%02d\n",
			client_ip, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		printf("%s", return_message);
		broadcast_message(return_message, client_ip, TRUE);

		// Create a new thread to handle the client connection
		WaitForSingleObject(thread_mutex, INFINITE); // Wait to acquire the mutex
		if (thread_count < MAX_THREADS) { // Check if we can create more threads
			// Start a new thread with the client_conn function and pass the struct
			threads[thread_count] = (HANDLE)_beginthreadex(NULL, 0, client_conn, (void*)client_args, 0, NULL);
			thread_count++; // Increment the active thread count
		}
		else {
			closesocket(client_socket); // Close the client socket if max threads reached
		}

		ReleaseMutex(thread_mutex); // Release the mutex after creating the thread
	}

	// Clean up and close server socket after exiting the loop
	closesocket(server_socket); // Close the server socket
	WSACleanup(); // Clean up Winsock resources
	CloseHandle(thread_mutex); // Close the mutex handle
	CloseHandle(socket_mutex);  // Close the socket mutex handle

	return 0; // Exit the program
}
