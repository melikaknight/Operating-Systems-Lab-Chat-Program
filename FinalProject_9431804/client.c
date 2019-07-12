#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "messageProtocol.h"


int userIndex = -1;

char serverAddr[16];
int port = 0;
char input[256] = {0};
char input2[256] = {0};
char buffer[256] = {0};
int clientSocketHandle = -1;
int connectedSocketHandle = -1;
struct sockaddr_in socketAddress;
int socketAddressLength = sizeof(socketAddress);
pthread_t listenerThread;
pthread_t keepAliveThread;

int checkCredentials();

void initClient();
int startConnecting();
void closeConnection();
void sendMessage(char * message);
int readSocket(char * buffer);
void * handleIncomingMessages();
void * sendKeepAlive();
void handleInput();
void handleWhoIsOnlineResponse(char * buffer);
void handleUnicastReceived(char * buffer);
void handleBroadcastReceived(char * buffer);

void main()
{
	int result;
	
	printf("Messenger Client Started.\n");

	while(1)
	{
		printf("Enter server address:\n");
		result = scanf("%s", serverAddr);
		if(result < 0)
		{
			printf("wrong input.\n");
			return;
		}
		//printf("serverAddr=%s\n", serverAddr);
		//sprintf(serverAddr, "127.0.0.1");
		
		printf("Enter server port:\n");
		result = scanf("%d", &port);
		if(result < 0 || port == 0)
		{
			printf("wrong input.\n");
			return;
		}
		//printf("port=%d\n", port);
		
		break;
	}	
	
	initClient();
	int cnt = 10;
	while(cnt > 0)
	{
		result = startConnecting();
		if(result < 0)
		{
			cnt--;
			continue;
		}
		else
			break;
	}
	if(cnt == 0)
	{
		printf("Couldnt stablish connection.\n");
		return;
	}
	
	printf("Connected to %s:%d\n", serverAddr, port);
	
	while(1)
	{
		printf("User Name:\n");
		scanf("%s", input);
		
		printf("Password:\n");
		scanf("%s", input2);
		
		result = checkCredentials();
		if(result == 0)
		{
			printf("Logged in.\n");
			break;
		}
		else if(result == -3)
		{
			printf("Connection with server lost.\n");
			return;
		}
		
		printf("Login failed.\n");
	}
	
	pthread_create(&listenerThread, NULL, handleIncomingMessages, NULL);
	pthread_create(&keepAliveThread, NULL, sendKeepAlive, NULL);
	
	fgets(input, sizeof(input), stdin);
	
	while(1)
	{
		memset(input, 0, sizeof(input));
		fgets(input, sizeof(input), stdin);
		input[strlen(input)-1] = 0;
		
		handleInput();
	}
	
	closeConnection();
	
	printf("Messenger Client Stoped.\n");
}

int checkCredentials()
{
	char buffer[256];	
	
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, "%s;%s;%s", loginCmd, input, input2);
	sendMessage(buffer);
	
	int cnt = 100;
	while(cnt > 0)
	{
		if(readSocket(buffer) > 0)
		{
			if(strcmp(buffer, okCmd) == 0)
				return 0;
			
			else if(strcmp(buffer, nokCmd) == 0)
				return -1;
				
			else
				return -2;
		}
		cnt--;
	}
	
	return -3;
}

void initClient()
{
	clientSocketHandle = socket(AF_INET, SOCK_STREAM, 0);
	//printf("clientSocketHandle = %d\n", clientSocketHandle);
	
	fcntl(clientSocketHandle, F_SETFL, O_NONBLOCK);
	
	socketAddress.sin_family = AF_INET;
	inet_pton(AF_INET, serverAddr, &socketAddress.sin_addr);
	socketAddress.sin_port = htons(port);
}

int startConnecting()
{
	int result;
	
	result = connect(clientSocketHandle, (struct sockaddr *) &socketAddress, socketAddressLength);
	//printf("connect result = %d\n", result);
	
	return result;
}

void closeConnection()
{
	int result;
	
	result = shutdown(clientSocketHandle, SHUT_RDWR);
	//printf("shutdown result = %d\n", result);
	
	result = close(clientSocketHandle);
	//printf("close result = %d\n", result);
}

void sendMessage(char * message)
{
	int result;
	
	result = send(clientSocketHandle, message, strlen(message), 0);
	//printf("send result = %d msg=%s\n", result, message);
	
	sleep(delayAfterSend);
}

int readSocket(char * buffer)
{
	int result;
	
	memset(buffer, 0, sizeof(buffer));
	result = recv(clientSocketHandle, buffer, 256, 0);
	if(result > 0)
	{
		buffer[result] = 0;
		//printf("read result = %d message = %s\n", result, buffer);
		return result;
	}
	else if(result == 0)
	{
		return 0;
	}
	else if(result < 0)
	{
		//printf("read result = %d\n", result);
		return -1;
	}
}

void * handleIncomingMessages()
{
	char * cmd;
	char temp[256];
	char buffer[256];
	int result = 0;
	
	while(1)
	{
		result = readSocket(buffer);
		
		if(result < 0)
			continue;

		sprintf(temp, buffer, strlen(buffer));
		cmd = strtok(temp, delimiterStr);
		//printf("cmd = %s\n", cmd);
		
		if(strcmp(cmd, whoIsOnlineCmd) == 0)
		{
			handleWhoIsOnlineResponse(buffer);
		}
		
		else if(strcmp(cmd, unicastCmd) == 0)
		{
			handleUnicastReceived(buffer);
		}
	
		else if(strcmp(cmd, broadcastCmd) == 0)
		{
			handleBroadcastReceived(buffer);
		}
		
		else
		{
			printf("Undefined command received.\n");
		}
	}
}

void * sendKeepAlive()
{
	while(1)
	{
		sendMessage(keepAliveCmd);
		sleep(keepAliveDelay);
	}
}

void handleInput()
{
	char * cmd;
	char * destUser;
	char buffer[256];
	char temp[256];
	char toSend[256];
	
	sprintf(temp, input, strlen(input));
	//printf("temp=%s\n", temp);
	cmd = strtok(temp, spaceStr);
	//printf("cmd=%s\n", cmd);
	
	if(strcmp(cmd, whoIsOnlineStr) == 0)
	{
		sendMessage(whoIsOnlineCmd);
	}
	
	else if(strcmp(cmd, unicastStr) == 0)
	{
		sprintf(input, input+strlen(unicastStr)+1, strlen(input)-strlen(unicastStr)-1);
		
		destUser = strtok(NULL, spaceStr);
		if(destUser == NULL)
		{
			printf("Wrong input.\n");
			return;
		}
		
		sprintf(buffer, input+strlen(destUser)+1, strlen(input)-strlen(destUser)-1);
		//printf("msg=%s\n", buffer);
		
		sprintf(toSend, "%s;%s;%s", unicastCmd, destUser, buffer);
		sendMessage(toSend);
	}
	
	else if(strcmp(cmd, broadcastStr) == 0)
	{
		sprintf(input, input+strlen(broadcastStr)+1, strlen(input)-strlen(broadcastStr)-1);
		//printf("msg=%s\n", input);
		
		sprintf(toSend, "%s;%s", broadcastCmd, input);
		sendMessage(toSend);
	}
	
	else
	{
		printf("Unknown command: \"%s\".\n", input);
	}
}

void handleWhoIsOnlineResponse(char * buffer)
{
	char * token;
	
	if(buffer[strlen(whoIsOnlineCmd)] != delimiterChar)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	sprintf(buffer, buffer+strlen(whoIsOnlineCmd)+1, strlen(buffer)-strlen(whoIsOnlineCmd)-1);
	//printf("buffer=%s\n", buffer);
	
	printf("Online Users:\n");
	token = strtok(buffer, delimiterStr);
	if(token == NULL)
	{
		printf("[none].\n");
		return;
	}
	
	while(token != NULL)
	{
		printf("@%s ", token);
		token = strtok(NULL, delimiterStr);
	}
	
	printf("\n[End of list]\n");
}

void handleUnicastReceived(char * buffer)
{
	char * srcUser;
	char * msg;
	
	if(buffer[strlen(unicastCmd)] != delimiterChar)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	sprintf(buffer, buffer+strlen(unicastCmd)+1, strlen(buffer)-strlen(unicastCmd)-1);
	//printf("buffer=%s\n", buffer);
	
	srcUser = strtok(buffer, delimiterStr);
	if(srcUser == NULL)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	msg = strtok(NULL, delimiterStr);
	if(msg == NULL)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	printf("Unicast received from @%s: %s\n", srcUser, msg);
}

void handleBroadcastReceived(char * buffer)
{
	char * srcUser;
	char * msg;
	
	if(buffer[strlen(broadcastCmd)] != delimiterChar)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	sprintf(buffer, buffer+strlen(broadcastCmd)+1, strlen(buffer)-strlen(broadcastCmd)-1);
	//printf("buffer=%s\n", buffer);
	
	srcUser = strtok(buffer, delimiterStr);
	if(srcUser == NULL)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	msg = strtok(NULL, delimiterStr);
	if(msg == NULL)
	{
		printf("Wrong command format detected.\n");
		return;
	}
	
	printf("Broadcast received from @%s: %s\n", srcUser, msg);
}




