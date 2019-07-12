#include <string.h>

#define queueLength 5
#define usersSize 5
#define pendingLength 10
#define keepAliveDelay 5
#define delayAfterSend 1.5

char * keepAliveCmd = "keepAlive";
char * loginCmd = "login";
char * whoIsOnlineCmd = "whoIsOnline";
char * unicastCmd = "unicast";
char * broadcastCmd = "broadcast";
char * okCmd = "ok";
char * nokCmd = "nok";

char delimiterChar = ';';
char * delimiterStr = ";";
char spaceChar = ' ';
char * spaceStr = " ";
char endChar = '#';
char * endStr = "#";

char * addUserCmd = "/adduser";
char * whoIsOnlineStr = "/whoisonline";
char * unicastStr = "/unicast";
char * broadcastStr = "/broadcast";

char * users[usersSize] = {"ali", "hasan", "omid"};
char * passes[usersSize] = {"ali", "hasan", "omid"};

int usersCnt = 3;
