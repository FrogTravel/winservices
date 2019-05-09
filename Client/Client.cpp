#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

WSADATA wsaData;
SOCKET ConnectSocket = INVALID_SOCKET;
struct addrinfo *result = NULL,
	*ptr = NULL,
	hints;
const char *sendbuf = "dir";
char recvbuf[DEFAULT_BUFLEN+1];
int iResult;
int recvbuflen = DEFAULT_BUFLEN;


int start_client(int argc, char **argv) {
	

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	return 0;
}

DWORD WINAPI sendData(LPVOID lpvThreadParam) {

	while (true) {
		std::string line;
		std::cin >> line;
		const char* command = line.c_str();
		// Send an initial buffer
		iResult = send(ConnectSocket, command, (int)strlen(command), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 1;
		}
	}
}


DWORD WINAPI receiveData(LPVOID lpvThreadParam) {
	// Receive until the peer closes the connection
	while (true) {
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			recvbuf[iResult] = 0;
			printf("%s", recvbuf);

			//std::string line;
			//getline(std::cin, line);


			//const char* command = line.c_str();

			//sendData(command);
			iResult = 0;
		}
		else if (iResult == 0)
			printf("iResult == 0\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());


	}
}

int closeSocket() {
	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}

}

int closeClient() {
	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();
	return 0;
}

int __cdecl main(int argc, char **argv)
{
	start_client(argc, argv);
	HANDLE sThread, rThread;

	sThread = CreateThread(NULL, 0, sendData, NULL, 0, NULL);
	rThread	= CreateThread(NULL, 0, receiveData, NULL, 0, NULL);

	WaitForSingleObject(sThread, INFINITE);
	WaitForSingleObject(rThread, INFINITE);
	return 0;
}