#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "messageProtocol.h"


int port = 0;
char buffer[256] = {0};
int serverSocketHandle = -1;
int connectedSocketHandle[queueLength] = {0};
int connectedUserIndex[queueLength];
int connectionIndex = -1;
struct sockaddr_in socketAddress;
int socketAddressLength = sizeof(socketAddress);
int connectedClients = 0;
char pendingMsg[usersSize][pendingLength][256];
int pendingIndex[usersSize] = {0};
time_t lastKeepAlive[queueLength];


int initServer();
void startListening();
void stopConnection(int index);
int readSocket(int index, char * buffer);
void sendMessage(int index, char * message);
void handleClientCommand(int index, char * buffer);
void handlePendingMsg(int index);
int handleUserLogin(int index, char * temp, char * buffer);
int verifyUserPass(char * temp, char * buffer);
int handleUnicast(int index, char * buffer, char * temp, char * msg);
int handleBroadcast(int index, char * buffer, char * msg);
int checkLastKeepAliveTime(int index);
void * handleConnectRequests();
void * handleClient(void * index);
void handleInput(char * input);


void main()
{
	int result;
	char input[256];
	char inRes[256];

	printf("Messenger Server Started.\n");
	
	while(1)
	{
		printf("Enter listener port:\n");
		result = scanf("%d", &port);
		if(result < 0 || port == 0)
		{
			printf("wrong input.\n");
			return;
		}
		break;
	}
	
	if(initServer() < 0)
	{
		printf("Error stablishing connection.\n");
		return;
	}
	
	printf("Server is running on port %d\n", port);
	
	pthread_t socketListenerThread;
	pthread_create(&socketListenerThread, NULL, handleConnectRequests, index);
	
	fgets(input, sizeof(input), stdin);
	
	while(1)
	{	
		memset(input, 0, sizeof(input));
		fgets(input, sizeof(input), stdin);
		input[strlen(input)-1] = 0;
		
		handleInput(input);
	}
	
	printf("Messenger Server Stoped.\n");
}

int initServer()
{
	int result;
	
	for(int i=0; i<queueLength; i++)
		connectedUserIndex[i] = -1;

	serverSocketHandle = socket(AF_INET, SOCK_STREAM, 0);
	//printf("serverSocketHandle = %d\n", serverSocketHandle);
	
	result = fcntl(serverSocketHandle, F_SETFL, O_NONBLOCK);
	//printf("fcntl result = %d\n", result);
	
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_addr.s_addr = INADDR_ANY;
	socketAddress.sin_port = htons(port);
	
	result = bind(serverSocketHandle, (struct sockaddr *) &socketAddress, socketAddressLength);
	//printf("bind result = %d\n", result);
	if(result < 0)
		return result;
	
	result = listen(serverSocketHandle, queueLength);
	//printf("listen result = %d\n", result);
	if(result < 0)
		return result;
		
	return 0;
}

void startListening()
{
	int result;
	
	result = accept(serverSocketHandle, (struct sockaddr *) &socketAddress, (socklen_t *) &socketAddressLength);
	//printf("accept res = %d\n", result);
	if(result >= 0)
	{	
		connectedClients++;
		
		for(int i=0; i<queueLength; i++)
		{
			if(connectedSocketHandle[i] > 0)
				continue;
			
			connectedSocketHandle[i] = result;
			result = fcntl(connectedSocketHandle[i], F_SETFL, O_NONBLOCK);

			//printf("connectedSocketHandle[%d] = %d\n", i, result);
			
			pthread_t clientHandlerThread;
			int index[1] = {i};
			pthread_create(&clientHandlerThread, NULL, handleClient, index);
			
			break;
		}
		
		//for(int i=0; i<queueLength; i++)
			//printf("connectedSocketHandle[%d] = %d\n", i, connectedSocketHandle[i]);
		//printf("*****\n");
	}
}

void stopConnection(int index)
{
	int result;
	
	result = shutdown(connectedSocketHandle[index], SHUT_RDWR);
	//printf("shutdown connectedSocketHandle[%d] result = %d\n", index, result);
	
	result = close(connectedSocketHandle[index]);
	//printf("close connectedSocketHandle[%d] result = %d\n", index, result);
	
	connectedSocketHandle[index] = 0;
	connectedUserIndex[index] = -1;
	connectedClients--;
}

int readSocket(int index, char * buffer)
{
	int result;
	
	memset(buffer, 0, sizeof(buffer));
	//printf("going for read connectedSocketHandle[%d].\n", index);
	result = recv(connectedSocketHandle[index], buffer, 256, 0);
	//printf("read res = %d\n", result);
	if(result > 0)
	{
		buffer[result] = 0;
		//printf("read connection %d result = %d message = %s\n", index, result, buffer);
		return result;
	}
	else if(result == 0)
	{
		return 0;
	}
	else if(result < 0)
	{
		//printf("read connection %d result = %d\n", index, result);
		return -1;
	}
}

void sendMessage(int index, char * message)
{
	int result;
	
	result = send(connectedSocketHandle[index], message, strlen(message), 0);
	//printf("send to connectedSocketHandle[%d] result=%d msg=%s\n", index, result, message);

	sleep(delayAfterSend);
}

void handleClientCommand(int index, char * buffer)
{
	char * cmd;
	char temp[256];
	int result;

	sprintf(temp, buffer, strlen(buffer));
	//printf("temp = %s\n", temp);
	cmd = strtok(temp, delimiterStr);
	//printf("cmd = %s\n", cmd);
	
	if((strcmp(cmd, keepAliveCmd) == 0))
	{
		time_t now = time(0);
		lastKeepAlive[index] = now;
		//printf("connectedSocketHandle[%d] is alive at %d\n", index, now);
	}

	else if(strcmp(cmd, loginCmd) == 0)
	{	
		memset(temp, 0, sizeof(temp));
		result = handleUserLogin(index, temp, buffer);
		//printf("handle login res = %d\n", result);
		
		if(result < 0)
		{
			sendMessage(index, nokCmd);
		}
		else
		{
			sendMessage(index, okCmd);
			connectedUserIndex[index] = result;
		}
		printf("%s\n", temp);
	}
	
	else if(strcmp(cmd, whoIsOnlineCmd) == 0)
	{
		memset(temp, 0, sizeof(temp));
		sprintf(temp, "%s", whoIsOnlineCmd);
		
		for(int i=0; i<queueLength; i++)
		{
			if(connectedSocketHandle[i] > 0)
			{
				sprintf(temp, "%s;%s", temp, users[connectedUserIndex[i]]);
			}
		}
		//printf("will send: %s\n", temp);
		printf("List of online users sent to @%s\n", users[connectedUserIndex[index]]);
		sendMessage(index, temp);
	}
	
	else if(strcmp(cmd, unicastCmd) == 0)
	{
		char msg[256];
		
		memset(temp, 0, sizeof(temp));
		result = handleUnicast(index, buffer, temp, msg);
		
		if(result >= 0)
		{
			sprintf(buffer, "%s;%s;%s", unicastCmd, users[connectedUserIndex[index]], msg);
			//printf("will send cmd=%s to connectedSocketHandle[%d]=%d\n", buffer, result, connectedSocketHandle[result]);
			sendMessage(result, buffer);
		}
		else
			;
		printf("%s\n", temp);
	}
	
	else if(strcmp(cmd, broadcastCmd) == 0)
	{
		char msg[256];
		
		memset(temp, 0, sizeof(temp));
		result = handleBroadcast(index, buffer, msg);
	}
	
	else
	{
		printf("Undefined command received.\n");
		return;
	}
}

int handleUserLogin(int index, char * temp, char * buffer)
{
	int result;
	
	if(buffer[strlen(loginCmd)] != delimiterChar)
	{
		sprintf(temp, "Wrong command format detected.");
		return -1;
	}
	
	sprintf(buffer, buffer+strlen(loginCmd)+1, strlen(buffer)-strlen(loginCmd)-1);
	//printf("buffer=%s\n", buffer);
	
	result = verifyUserPass(temp, buffer);
	//printf("verify res = %d\n", result);
	return result;
}

int verifyUserPass(char * temp, char * buffer)
{
	int userIndex = -1;
	char * token;
	char user[50];
	char pass[50];
	
	token = strtok(buffer, delimiterStr);
	if(token == NULL)
	{
		sprintf(temp, "Wrong command format detected.");
		return -1;
	}
	sprintf(user, token, strlen(token));
	
	token = strtok(NULL, delimiterStr);
	if(token == NULL)
	{
		sprintf(temp, "Wrong command format detected.");
		return -1;
	}
	sprintf(pass, token, strlen(token));
	
	//printf("user=%s pass=%s\n", user, pass);

	for(int i=0; i<usersCnt; i++)
	{
		if(strcmp(users[i], user) == 0)
		{
			userIndex = i;

			for(int j=0; j<queueLength; j++)
			{
				if(connectedUserIndex[j] == userIndex)
				{
					sprintf(temp, "User @%s login failed: Already logged in from another host.", user);
					return -3;
				}
				//printf("connected user index[%d] = %d\n", j, connectedUserIndex[j]);
			}

			if(strcmp(passes[userIndex], pass) == 0)
			{
				sprintf(temp, "User @%s login Successfull.", user);
				//printf("userIndex = %d\n", userIndex);
				return userIndex;
			}
			else
			{
				sprintf(temp, "User @%s login failed: Wrong password.", user);
				return -4;
			}
		}
	}
	
	sprintf(temp, "User @%s login failed: username undefined.", user);
	return -2;
}

int handleUnicast(int index, char * buffer, char * temp, char * msg)
{
	int destUserIndex = -1;
	char * token;
	char destUser[50];
	
	sprintf(buffer, buffer+strlen(unicastCmd)+1, strlen(buffer)-strlen(unicastCmd)-1);
	//printf("buffer=%s\n", buffer);	
	
	token = strtok(buffer, delimiterStr);
	if(token == NULL)
	{
		sprintf(temp, "Wrong command format detected.");
		return -1;
	}
	sprintf(destUser, token, strlen(token));
	//printf("destUser=%s\n", destUser);
	
	token = strtok(NULL, delimiterStr);
	if(token == NULL)
	{
		sprintf(temp, "Wrong command format detected.");
		return -1;
	}
	sprintf(msg, token, strlen(token));
	//printf("msg=%s\n", msg);
	
	for(int i=0; i<usersCnt; i++)
	{
		if(strcmp(destUser, users[i]) == 0)
		{
			destUserIndex = i;
			
			for(int j=0; j<queueLength; j++)
			{
				if(destUserIndex == connectedUserIndex[j])
				{
					sprintf(temp, "Unicast to @%s successful.", destUser);
					return j;
				}
			}
			
			if(pendingIndex[destUserIndex] > pendingLength)
			{
				sprintf(temp, "Unicast to @%s failed: user is offline and its inbox is full.", destUser);
				return -4;
			}
			
			sprintf(pendingMsg[destUserIndex][pendingIndex[destUserIndex]++], "%s;%s;%s", unicastCmd, users[connectedUserIndex[index]], msg);
			sprintf(temp, "Unicast to @%s failed: user is offline.", destUser);
			return -3;
		}
	}
	
	sprintf(temp, "Unicast to @%s failed: username undefined.", destUser);
	return -2;
}

int handleBroadcast(int index, char * buffer, char * msg)
{
	int destUserIndex = -1;
	char destUser[50];
	int flag = 0;

	sprintf(buffer, buffer+strlen(broadcastCmd)+1, strlen(buffer)-strlen(broadcastCmd)-1);
	//printf("buffer=%s\n", buffer);
	
	msg = strtok(buffer, delimiterStr);
	if(msg == NULL)
	{
		printf("Wrong command format detected.\n");
		return -1;
	}
	
	for(int i=0; i<usersCnt; i++)
	{
		flag = 0;
		destUserIndex = i;
		memset(destUser, 0, sizeof(destUser));
		sprintf(destUser, users[i], strlen(users[i]));
		
		if(strcmp(destUser, users[connectedUserIndex[index]]) == 0)
			continue;
		
		for(int j=0; j<queueLength; j++)
		{
			if(destUserIndex == connectedUserIndex[j])
			{
				char toSend[256];
				
				sprintf(toSend, "%s;%s;%s", broadcastCmd, users[connectedUserIndex[index]], msg);
				//printf("will send cmd=%s to connectedSocketHandle[%d]=%d\n", toSend, j, connectedSocketHandle[j]);
				sendMessage(j, toSend);
			
				printf("Broadcast to @%s successful.\n", destUser);
				
				flag = 1;
				break;
			}
		}
		
		if(flag == 1)
			continue;
		
		if(pendingIndex[destUserIndex] > pendingLength)
		{
			printf("Broadcast to @%s failed: user is offline and its pending queue is full.\n", destUser);
			continue;
		}
		
		sprintf(pendingMsg[destUserIndex][pendingIndex[destUserIndex]++], "%s;%s;%s", broadcastCmd, users[connectedUserIndex[index]], msg);
		printf("Broadcast to @%s failed: user is offline.\n", destUser);
		
		continue;
	}
	
	return 0;
}


void handlePendingMsg(int index)
{
	int userIndex = connectedUserIndex[index];
	//printf("pendingMsg[%d][%d]=%s\n", userIndex, 0, pendingMsg[userIndex][0]);
	
	sendMessage(index, pendingMsg[userIndex][0]);
	printf("Pending message sent to @%s\n", users[userIndex]);

	for(int i=0; i<pendingIndex[userIndex]-1; i++)
	{
		memset(pendingMsg[userIndex][i], 0, sizeof(pendingMsg[userIndex][i]));
		sprintf(pendingMsg[userIndex][i], pendingMsg[userIndex][i+1], sizeof(pendingMsg[userIndex][i+1]));
	}
	
	/*for(int i=0; i<pendingLength; i++)
		printf("pendingMsg[1][%d]=%s\n", i, pendingMsg[1][i]);*/
	
	memset(pendingMsg[userIndex][pendingIndex[userIndex]-1], 0, sizeof(pendingMsg[userIndex][pendingIndex[userIndex]-1]));
	pendingIndex[userIndex]--;
}

int checkLastKeepAliveTime(int index)
{
	time_t now = time(0);
	
	if(lastKeepAlive[index] == 0)
	{
		//printf("connectedSocketHandle[%d] has not received first keep alive.\n", index);
		lastKeepAlive[index] = now + 2*keepAliveDelay;
		return -1;
	}
	
	if(now - lastKeepAlive[index] > 2*keepAliveDelay)
	{
		if(connectedUserIndex[index] < 0)
			printf("Time for login has expired for client on socket index %d.\n", index);
		else
			printf("User @%s has gone offline.\n", users[connectedUserIndex[index]]);
			
		lastKeepAlive[index] = 0;
		stopConnection(index);
		
		return -2;
	}
	
	return 0;
}

void * handleConnectRequests()
{
	printf("Thread for handling connection requests has started.\n");
	
	while(1)
	{
		if(connectedClients < queueLength)
			startListening();
	}
	
	printf("Thread for handling connection requests has stopped.\n");
}

void * handleClient(void * index)
{
	int i = * ((int *) index);
	char buffer[256];
	int result = 0;
	
	printf("Thread started for handling client on socket index %d.\n", i);
	
	while(1)
	{
		memset(buffer, 0, sizeof(buffer));
		
		if(readSocket(i, buffer) > 0)
		{
			handleClientCommand(i, buffer);
		}
	
		if(connectedUserIndex[i] >= 0 && pendingIndex[connectedUserIndex[i]] > 0 && connectedUserIndex[i] >= 0)
		{
			handlePendingMsg(i);
		}
	
		result = checkLastKeepAliveTime(i);
		if(result == -2)
		{
			sleep(1);
			break;
		}
	}
	
	printf("Thread stopped for handling client on socket index %d.\n", i);
}

void handleInput(char * input)
{
	char temp[256];
	char * cmd;
	char * newUser;
	char * newPass;
	
	sprintf(temp, input, strlen(input));

	cmd = strtok(temp, spaceStr);
	
	if(cmd == NULL)
	{
		printf("Unknown command: \"%s\".\n", input);
		return;
	}
	
	if(strcmp(cmd, addUserCmd) == 0)
	{
		newUser = strtok(NULL, spaceStr);
		
		if(newUser == NULL)
		{
			printf("Unknown command: \"%s\".\n", input);
			return;
		}
		
		newPass = strtok(NULL, spaceStr);
		
		if(newPass == NULL)
		{
			printf("Unknown command: \"%s\".\n", input);
			return;
		}
		
		if(usersCnt >= usersSize)
		{
			printf("Cannot add new user: capacity is full.\n");
			return;
		}
		
		for(int i=0; i<usersCnt; i++)
		{
			if(strcmp(newUser, users[i]) == 0)
			{
				printf("Cannot add new user: username @%s is taken.\n", newUser);
				return;
			}
		}
		
		users[usersCnt] = malloc(sizeof(newUser));
		passes[usersCnt] = malloc(sizeof(newPass));
		
		sprintf(users[usersCnt], newUser, strlen(newUser));
		sprintf(passes[usersCnt], newPass, strlen(newPass));
		
		usersCnt++;
		
		printf("New user added successfully. user:\"%s\" pass:\"%s\".\n", newUser, newPass);
	}
	
	else
		printf("Unknown command: \"%s\".\n", input);
}





