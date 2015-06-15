/***********************************************************
* IOT Poject
* Logic for Light Bulb
* Written from Scratch

Parent Thread --> Runs Bulb Logic
Thread 1 	  --> Runs Push Notification Service to handle Pushes from Parse
***********************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <parse.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define PORT_WEATHER 50008
#define PORT_LOCALINFO 50009
#define PORT_INTENSITY 50013
#define PORT_TIME 50011
#define PORT_SENSE 50014
#define HOST "127.0.0.1"
#define MAXPENDING 5    /* Maximum outstanding connection requests */
#define PARSE_TIME_INTERVAL 5 /* Default time (s) for updating simulated time to Parse Cloud */

//Directives indicating increments to bulb intensity for clear, cloudy, and rainy weather
#define ADD_CLEAR 1
#define ADD_CLOUDS 25
#define ADD_RAIN 10

static numClimates = 3;
char *climateList[] = {"Clouds", "Clear", "Rain"};
char *healthList[] = {"GOOD", "POOR", "DAMAGED"};

// This is lock needed to ensure proper synchronization while updating some data of lightBulb struct
pthread_mutex_t lock;
char dataUUID[] = "bb946c14-758a-4448-8b78-69b04ba1bb8b";
struct lightBulb bulb;

// Struct holding intensity / health values for light bulb
struct lightBulb {
	int intensity; // Intensity takes values 0 - 99
	int health; // Takes 0, 1, 2 for GOOD, POOR, DAMAGED. Could have been done better using enums!
};

// Struct holding hours and minutes for simulating time
struct timeEmulate {
	int hour;
	int min;
};

void logThis()
{
	/* NOTE: Add code here for logger module
	 * We will be calling this logThis function whenever a log needs to be printed to Logs/logs.txt
	 * Not mandatory though
	 */
}

/* This is a callback function (function pointer) that registers itself with the Parse app to receive push notifications 
*  from the cloud. Whenever an explicit change is made on the cloud by an end user from the web site, we will over ride any
*  existing changes and update the bulb accordingly.
*/
void healthCallback(ParseClient client, int error, const char *buffer) 
{	
	if (error == 0 && buffer != NULL) {
		printf("lightbulb::healthCallback()::Received Push Data: '%s'\n", buffer);
	}

	char *health = malloc(10);
	char c;
	char *ptr = buffer;
	int healthVal;
	int i = 0;
	for (c = *(ptr + 19); c != '\"'; c = *(ptr + 19 + i) )
	{
	health[i] = c;
	i++;
	}
	health[i] = '\0';

	if (strcmp(health, healthList[0]) == 0)
	healthVal = 0;
	else if (strcmp(health, healthList[1]) == 0)
	healthVal = 1;
	else if (strcmp(health, healthList[2]) == 0)
	{
	healthVal = 2;

	pthread_mutex_lock(&lock);
	bulb.intensity = 0;
	pthread_mutex_unlock(&lock);
	}

	pthread_mutex_lock(&lock);
	bulb.health = healthVal;
	pthread_mutex_unlock(&lock);

	free(health);
}

//callback function to retrieve object identifier
void getObjectCallback(ParseClient client, int error, const char *buffer) 
{
	printf("lightbulb::getObjectCallback()::Got http response: '%s'\n", buffer);
}

// This function pushes data onto the Parse Application in cloud. It is invoked by updateIntensity().
void *threadPushNotifications()
{
	pthread_detach(pthread_self());
	//ParseClient client = parseInitialize("kayWALfBm6h1SQdANXoZtTZqA0N9sZsB7cwUUVod", "7Nw0R9LTDXR7lRhmPsArePQMralFW8Yt7DL2zWTS");
	ParseClient client = parseInitialize("TSexah1prtbhpF6w4dellQ2XYWyk2bqcljOiElrN", "xLnvnzcTMO1w9MwuFNBTO6hLjOtnKmZn4iz4SBnu");
	char *installationId = parseGetInstallationId(client);

	/* We need to set the InstallationId forcefully. Setting installationId to dataUUID based on null string is incorrect
	logic as there is a possibility that the installationId was previously set to some junk value.
	Typically this will break the push notification subscription */
	parseSetInstallationId(client, dataUUID);
	printf("lightbulb::threadPushNotifications():New Installation ID set to : %s\n", installationId);
	printf("lightbulb::threadPushNotifications():Installation ID is : %s\n", installationId);
	parseSetPushCallback(client, healthCallback);
	parseStartPushService(client);
	parseRunPushLoop(client);
}

// This function pushes changes to the cloud for two specific column names, and pushes new information every time without updating previous records
void updateOnParse(const char *intensityColumn, int intensityValue, const char *healthColumn, int healthValue)
{	
	
	ParseClient client = parseInitialize("TSexah1prtbhpF6w4dellQ2XYWyk2bqcljOiElrN", "xLnvnzcTMO1w9MwuFNBTO6hLjOtnKmZn4iz4SBnu");
	char strIntensityValue[4];
	char strHealthValue[4];
	sprintf(strIntensityValue, "%d", intensityValue);
	sprintf(strHealthValue, "%d", healthValue);
	//Properly format and concatenate data to push to Parse
	char dataIntensity[100] = "{ \"";
	char dataHealth[100] = "{ \"";
	strcat(dataIntensity, intensityColumn);
	strcat(dataIntensity, "\": ");
	strcat(dataIntensity, strIntensityValue);
	strcat(dataIntensity, " }");
	strcat(dataHealth, healthColumn);
	strcat(dataHealth, "\": ");
	strcat(dataHealth, strHealthValue);
	strcat(dataHealth, " }");
	parseSendRequest(client, "POST", "/1/classes/Bulb", dataIntensity, NULL);
	//get new object ID
	parseSendRequest(client, "GET", "/1/classes/Bulb", dataIntensity, NULL);
	//printf("Parse object ID for Bulb entry is: %s\n", client.objectID);
	parseSendRequest(client, "POST", "/1/classes/Bulb", NULL, getObjectCallback);
	//parseSendRequest(client, "PUT", "/1/classes/Bulb/Xb0o8Y5dMa", data, NULL);
	parseSendRequest(client, "POST", "/1/classes/Bulb", dataHealth, NULL);
	printf("lightbulb::updateOnParse(): Pushed data  %s, %s\n", dataIntensity, dataHealth);
}

// Function returns current intensity of light bulb
int getIntensity(struct lightBulb *bulb)
{
	return bulb->intensity;
}

// Function updates bulb intensity. Varies based on climate also
/* NOTE: This intensity logic update requires some modifications
 * for corner cases
 */
void updateIntensity(int newIntensity, char *climate)
{
	//DAVE: perhaps add information to update based on time of day?? Or maybe this is done in updateBasedOnTime()
	if(strcmp(bulb.health,"DAMAGED") && (climate != NULL)){
		if(!(strcmp(*climate,"Clear"))){
			if(!(strcmp(bulb.health,"GOOD"))){
				newIntensity += ADD_CLEAR;
			}else if(!(strcmp(bulb.health,"POOR"))){
				newIntensity -= ADD_CLEAR;
			}

		}else if(!(strcmp(*climate,"Clouds"))){
			if(!(strcmp(bulb.health,"GOOD"))){
				newIntensity += ADD_CLOUDS;
			}else if(!(strcmp(bulb.health,"POOR"))){
				newIntensity -= ADD_CLOUDS;
			}

		}else if(!(strcmp(*climate,"Rain"))){
			if(!(strcmp(bulb.health,"GOOD"))){
				newIntensity += ADD_RAIN;
			}else if(!(strcmp(bulb.health,"POOR"))){
				newIntensity -= ADD_RAIN;
			}
		}else{
			printf("ERROR: INVALID WEATHER STATUS\n");
			newIntensity = 0;
		}
	}else{
		newIntensity = 0;
		printf("ERROR: INVALID WEATHER STATUS OR BULB DAMAGED\n");
	}

	bulb.intensity = newIntensity;
	//ADD threadPushNotifications() via updateOnParse()
	updateOnParse("Intensity", newIntensity, "Health", bulb.health);
}

// Function to retrieve light bulb health status
int getHealth(struct lightBulb *bulb)
{
	return bulb->health;
}

// Function to update simulated time. We set 1 second in real time as 5 minutes in simulation time
void *updateBulbTime(void *time)
{
    struct timeEmulate *timer = (struct timeEmulate *)time;
    int ticker = 0;
    while (1)
    {
        char strTime[10];
        if (timer->min == 55)
            timer->hour = (timer->hour + 1)%24;
        timer->min = (timer->min + 5)%60;
        printf("lightbulb::updateBulbTime():: Time: %d:%d\n", timer->hour, timer->min);
        sprintf(strTime, "%d:%d", timer->hour, timer->min);
        clientSendSocket(PORT_TIME, strTime);
        
        // Push the time to Parse cloud at periodic intervals (5 seconds default)
        if (++ticker > PARSE_TIME_INTERVAL)
        {
		updateOnParse("Hour", timer->hour, "Minute", timer->min);
            //updateOnParse("Hour", timer->hour);
            //updateOnParse("Minute", timer->min);
            ticker = 0;
            
        }
        sleep(1);
    }
}

// Function to extract hour from weather.txt
int extractHour(FILE *weatherFile)
{
	char buf[255], *pch;
	fgets(buf, 255, weatherFile);
	pch = strtok(buf, ":");
	return atoi(pch);
}

// Function to extract climate from weather.txt
char *extractClimate(FILE *weatherFile)
{
	char *buf = malloc(sizeof(char)*255);
	char *test = malloc(sizeof(char)*255);
	fgets(buf, 255, weatherFile);
	buf[strlen(buf) - 1] = '\0';
	return buf;

}

// Function that returns a socket descriptor
int createServerSocket(int port)
{
	int servSock;
    struct sockaddr_in servAddr;
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
    	printf("lightbulb::createServerSocket(): ERROR: Cannot open Server Socket\n");
    }

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));       /* Zero out structure */
    servAddr.sin_family = AF_INET;                /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    servAddr.sin_port = htons(port);              /* Local port */
    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
    	printf("lightbulb::createServerSocket(): ERROR: Bind on port %d failed!\n", port);
    }
    if (listen(servSock, MAXPENDING) < 0)
    {
    	printf("lightbulb::createServerSocket(): ERROR: Listen on port %d failed!\n", port);
    }
    return servSock;

}

// Here we establish socket connectivity with another endpoint/program on 'port' argument
void clientSendSocket(int port, char *buffer)
{
	int sockfd = 0;
	struct sockaddr_in servAddr;
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("lightbulb::clientSocket():ERROR : Could not create socket \n");
    }
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = inet_addr(HOST);
    if (connect(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
       printf("lightbulb::clientSocket():ERROR : Connect Failed on Port: %d\n", port);
    }
    else
    {
    	send(sockfd, buffer, strlen(buffer), 0);
    	printf("lightbulb::clientSendSocket(): Data sent Successfully on Port: %d\n", port);
    }
    close(sockfd);
}

// Function to update the bulb Intensity based on time. Here we also act as server.
// NOTE: select() method might not be required as we use only one server socket here. Can be made much neater
void updateBasedOnTime(struct timeEmulate *bulbTime, int level[10], int servSock)
{
/*
 * Initialize active sockets - code modified from 
 * www.gnu.org/software/libc/manual/html_node/Server-Example.html 
 * because no other reference was provided...
 *
 */
	fd_set active_fd_set, read_fd_set;
	FD_ZERO(&active_fd_set);
	FD_SET(servSock, &active_fd_set);
	struct sockaddr_in clientAddr;
	unsigned int clientLength = sizeof(clientAddr);

	//Wait for an active connection to arrive
	read_fd_set = active_fd_set;
	if(select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0){
		printf("select\n");
		//exit(EXIT_FAILURE);
	}else if(FD_ISSET(servSock, &read_fd_set) && (strcmp(bulb.health,"DAMAGED"))){
		int clientSock = accept(servSock, (struct sockaddr_in *)&clientAddr, clientLength);
		FILE *weatherFile = fdopen(clientSock, "r");
		char *climate = extractClimate(weatherFile);
		int sunriseHour = extractHour(weatherFile);
		int sunsetHour = extractHour(weatherFile);
		close(weatherFile);

		//if it's before sunrise
		if(((*bulbTime).hour >+ 0) && ((*bulbTime).hour < sunriseHour)){ 
			updateIntensity(level[8], climate);
		//after sunrise but before midday(ish)
		}else if(((*bulbTime).hour < sunriseHour+4) && ((*bulbTime).hour >= sunriseHour)){
			updateIntensity(level[2], climate);
		//before sunset and after midday(ish)
		}else if(((*bulbTime).hour >= sunriseHour+4) && ((*bulbTime).hour < sunsetHour)){
			updateIntensity(0, climate);
		//after sunset and before late night
		}else if(((*bulbTime).hour >= sunsetHour) && ((*bulbTime).hour < sunsetHour+4)){
			updateIntensity(level[5], climate);
		}else if(((*bulbTime).hour >= sunsetHour+4)){
			updateIntensity(level[8], climate);
		}else{
			printf("ERROR: NO UPDATE TO INTENSITY, BAD TIMING\n");
		}
		free(climate);
		close(weatherFile);
		printf("SUCCESS: UPDATED BASED ON TIME\n");
	}
}

// main()
int main() {
	
	//struct lightBulb bulb;
	struct timeEmulate bulbTime;
	int i, level[10];
	bool isHealth = true;
	pthread_t threadPush, threadTime;
	pthread_mutex_init(&lock, NULL);
	updateIntensity(0, NULL);

	// Obtain socket FD using PORT_WEATHER to communicate with weather.py
	int servSockWeather = createServerSocket(PORT_WEATHER);
	pthread_mutex_lock(&lock);
	bulb.health = 0;
	pthread_mutex_unlock(&lock);
	bulbTime.hour = 0;
	bulbTime.min = 0;
	for (i = 0; i < 10; i++){
		level[i] = i+1;
	}

	/* Two threads that run in background to update the time of the bulb and also push/pull notifications periodically to 
	* Parse Cloud
	*/
	pthread_create(&threadPush, NULL, threadPushNotifications, NULL);
	pthread_create(&threadTime, NULL, updateBulbTime, (void *)(&bulbTime));

	while(1)
	{	
		char str[4];
		updateBasedOnTime(&bulbTime, level, servSockWeather);
		sprintf(str, "%d", getIntensity(&bulb));
		clientSendSocket(PORT_INTENSITY, str);
		while (bulb.health == 2)
		{
			if (isHealth)
			{
				//updateOnParse("Intensity", 0);
				//updateOnParse("Health", 2);
				updateOnParse("Intensity", 0, "Health", 2);
				isHealth = false;
			}
		} // Avoid crazy looping during bulb DAMAGED condition. Optimization to avoid excess CPU cycle usage.
		isHealth = true;
	}
	
	return 0;
}
