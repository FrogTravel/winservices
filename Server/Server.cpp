#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include "Server.h"
#include "SVC.h"
using namespace std;

#define BUFSIZE 4096 


// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
LPSTR cmdpath = LPSTR("C:\\Windows\\System32\\cmd.exe");

HANDLE hStdIn = NULL;
int iResult;
HANDLE hChildProcess = NULL;


HANDLE g_hInputFile = NULL;

Server::Server() {
}

void runCommand(HANDLE hChildStdOut, HANDLE hChildStdIn, HANDLE hChildStdErr)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;

	// Set up the start up info struct.
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = hChildStdOut;
	si.hStdInput = hChildStdIn;
	si.hStdError = hChildStdErr;

	if (!CreateProcess(NULL,
		cmdpath,
		NULL,
		NULL,
		TRUE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&si,
		&pi))
		printf("CreateProcess");


	// Set global child process handle to cause threads to exit.
	hChildProcess = pi.hProcess;


	// Close any unnecessary handles.
	if (!CloseHandle(pi.hThread)) printf("CloseHandle");
}

BOOL bRunThread = TRUE;

DWORD WINAPI GetAndSendInputThread(LPVOID lpvThreadParam){
	CHAR read_buff[DEFAULT_BUFLEN];
	DWORD nBytesRead, nBytesWrote;
	HANDLE hPipeWrite = (HANDLE)lpvThreadParam;

	// Get input from our console and send it to child through the pipe.
	while (bRunThread)
	{
		if (!ReadFile(hStdIn, read_buff, 1, &nBytesRead, NULL))
			printf("ReadStdin");

		read_buff[nBytesRead] = '\0'; // Follow input with a NULL.

		if (!WriteFile(hPipeWrite, read_buff, nBytesRead, &nBytesWrote, NULL))
		{
			if (GetLastError() == ERROR_NO_DATA) {
				printf("ERROR NO DATA");
				break; // Pipe was closed (normal exit path).
			}
			else
				printf("WriteFile");
		}
	}

	return 1;
}

int Server::startServer() {
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// Accept a client socket
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// No longer need server socket
	closesocket(ListenSocket);

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			recvbuf[iResult] = 0;

			//runCommand(recvbuf);
			string commandResult = "At least I tried";

			printf("Bytes received: %d\n", iResult);

			printf("COMMAND RESULT: %s\n", commandResult.c_str());

			// Echo the buffer back to the sender
			iSendResult = send(ClientSocket, commandResult.c_str(), strlen(commandResult.c_str()), 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 1;
			}
			printf("Bytes sent: %d\n", iSendResult);
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

}

int main() {
	//_ttmain(0, NULL);//Now only as a service
	//Server server = Server();
	//server.startServer();
	SECURITY_ATTRIBUTES saAttr;

	printf("\n->Start of parent execution.\n");

	// Set the bInheritHandle flag so pipe handles are inherited. 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Starting to create pipes
	HANDLE hOutputReadTmp, hOutputRead, hOutputWrite;
	HANDLE hInputWriteTmp, hInputRead, hInputWrite;
	HANDLE hErrorWrite;

	HANDLE hThread, lThread;
	DWORD ThreadId1, ThreadId2;

	// Set up the security attributes struct.
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.lpSecurityDescriptor = NULL;
	saAttr.bInheritHandle = TRUE;


	// Create the child output pipe.
	if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, &saAttr, 0))
		printf("CreatePipe\n");


	// Create a duplicate of the output write handle for the std error
	// write handle. This is necessary in case the child application
	// closes one of its std output handles.
	if (!DuplicateHandle(GetCurrentProcess(), hOutputWrite,
		GetCurrentProcess(), &hErrorWrite, 0,
		TRUE, DUPLICATE_SAME_ACCESS))
		printf("DuplicateHandle\n");


	// Create the child input pipe.
	if (!CreatePipe(&hInputRead, &hInputWriteTmp, &saAttr, 0))
		printf("CreatePipe\n");


	// Create new output read handle and the input write handles. Set
	// the Properties to FALSE. Otherwise, the child inherits the
	// properties and, as a result, non-closeable handles to the pipes
	// are created.
	if (!DuplicateHandle(GetCurrentProcess(), hOutputReadTmp,
		GetCurrentProcess(),
		&hOutputRead, // Address of new handle.
		0, FALSE, // Make it uninheritable.
		DUPLICATE_SAME_ACCESS))
		printf("DuplicateHandle\n");

	if (!DuplicateHandle(GetCurrentProcess(), hInputWriteTmp,
		GetCurrentProcess(),
		&hInputWrite, // Address of new handle.
		0, FALSE, // Make it uninheritable.
		DUPLICATE_SAME_ACCESS))
		printf("DuplicateHandle\n");


	// Close inheritable copies of the handles you do not want to be
	// inherited.
	if (!CloseHandle(hOutputReadTmp)) printf("CloseHandle");
	if (!CloseHandle(hInputWriteTmp)) printf("CloseHandle");

	if ((hStdIn = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		printf("STANDART_INPUT ERROR\n");

	runCommand(hOutputWrite, hInputRead, hErrorWrite);
	printf("RUN COMMAND FINISHED!\n");

	if (!CloseHandle(hOutputWrite)) {
		printf("CloseHandle hOutputWrite ERROR\n");
	}

	if (!CloseHandle(hInputRead)) {
		printf("CloseHandle hInputRead ERROR\n");
	}

	if (!CloseHandle(hErrorWrite)) {
		printf("CloseHandle hErrorWrite ERROR\n");
	}

	hThread = CreateThread(NULL, 0, GetAndSendInputThread, (LPVOID)hInputWrite, 0, &ThreadId1);
	if (hThread == NULL) printf("CreateThread\n");

	char lpBuffer[DEFAULT_BUFLEN];
	DWORD nBytesWrote, nBytesRead;
	const char* command = "0";
	while (true) {

		if (!WriteFile(hInputWrite, command, strlen(command) * sizeof(char), &nBytesWrote, NULL)) {
			printf("WRITE FILE ERROR %lu\n", GetLastError());
			// Получаем данные тута
			break;
		}
		

		if (!ReadFile(hOutputRead, lpBuffer, sizeof(lpBuffer),
			&nBytesRead, NULL) || !nBytesRead)
		{
			printf("READ FILE ERROR %lu\n", GetLastError());
			break;
		}
		lpBuffer[nBytesRead] = '\0';

		printf(lpBuffer);

	}


	if (!CloseHandle(hStdIn)) printf("CloseHandle");

	// Tell the thread to exit and wait for thread to die.
	bRunThread = FALSE;

	if (WaitForSingleObject(hThread, INFINITE) == WAIT_FAILED)
		printf("WaitForSingleObject");
	//if (WaitForSingleObject(lThread, INFINITE) == WAIT_FAILED)
	//	printf("WaitForSingleObject");

	if (!CloseHandle(hOutputRead)) printf("CloseHandle");
	if (!CloseHandle(hInputWrite)) printf("CloseHandle");
	// Write to the pipe that is the standard input for a child process. 
	// Data is written to the pipe's buffers, so it is not necessary to wait
	// until the child process is running before writing data.

	//WriteToPipe();
//	printf("\n->Contents of %s written to child STDIN pipe.\n", "BLABLABLA");

	// Read from pipe that is the standard output for child process. 

	//printf("\n->Contents of child process STDOUT:\n\n", "BLABLABLA");
	//ReadFromPipe();

	printf("\n->End of parent execution.\n");

	// The remaining open handles are cleaned up when this process terminates. 
	// To avoid resource leaks in a larger application, close handles explicitly. 

	return 0;
}

