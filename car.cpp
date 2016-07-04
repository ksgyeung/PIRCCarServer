#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <wiringPi.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "command.h"



#define PIN_STEP_MOTOR_A0		8
#define PIN_STEP_MOTOR_A1		9
#define PIN_STEP_MOTOR_B0		7
#define PIN_STEP_MOTOR_B1		0

#define PIN_FORWARD_MOTOR		8
#define PIN_BACKWARD_MOTOR		9

#define DELAY_INTERVAL			3

#define MOTOR_DIRECTION_NONE		0
#define MOTOR_DIRECTION_FORWARD		1
#define MOTOR_DIRECTION_BACKWARD	-1

const int PIN_STEP_MOTOR[] =
{
	PIN_STEP_MOTOR_A0,
	PIN_STEP_MOTOR_A1,
	PIN_STEP_MOTOR_B0,
	PIN_STEP_MOTOR_B1
};

const bool STEPPER_MOTOR_PHASE_VALUES[4][4] = 
{
	{1,1,0,0},
	{0,1,1,0},
	{0,0,1,1},
	{1,0,0,1}
};

const int REVOLUTION = 4;
const double GEAR_REDUCTION = 64;
const double CYCLES = REVOLUTION * GEAR_REDUCTION;
const double ROUND_STEP = CYCLES * GEAR_REDUCTION;
const double CYCLE_ANGLE = 360.0/GEAR_REDUCTION;
const double STEP_ANGLE = CYCLE_ANGLE/ GEAR_REDUCTION;
const int ANGLE_STEP = (int)ceil(1.0 / STEP_ANGLE);


void handleMessage(int& socket,Message& message);
void handleForwardMessage();
void handleBackwardMessage();
void handleStopMessage();
void handleRotateMessage(int& socket);
inline void setPhaseValues(const bool* phaseValues);
inline void reverseBytes(const void* le,void* be,int length);
inline void setStatus();

long g_currentAngle = 0;
int g_currentPhase = 0;

int g_motorDirection = MOTOR_DIRECTION_NONE;

int main()
{
	setvbuf (stdout, NULL, _IONBF, 0);
	
	printf("Car compiled on %s %s\n",__DATE__,__TIME__);
	printf("G++ %s\n\n",__VERSION__);
	
	printf("Configurations\n");
	printf("Revolution: \t\t%d\n",REVOLUTION);
	printf("Gear Reduction: \t%g\n",GEAR_REDUCTION);
	printf("Cycles: \t\t%g\n",CYCLES);
	printf("Round Step: \t\t%g\n",ROUND_STEP);
	printf("Cycle Angle: \t\t%g\n",CYCLE_ANGLE);
	printf("Step Angle is: \t\t%g\n",STEP_ANGLE);
	printf("Angle Step is: \t\t~%d\n\n",ANGLE_STEP);
	
	//setup gpio
	
	wiringPiSetup();
	pinMode(PIN_STEP_MOTOR_A0, OUTPUT);
	pinMode(PIN_STEP_MOTOR_A1, OUTPUT);
	pinMode(PIN_STEP_MOTOR_B0, OUTPUT);
	pinMode(PIN_STEP_MOTOR_B1, OUTPUT);
	pinMode(PIN_FORWARD_MOTOR, OUTPUT);
	pinMode(PIN_BACKWARD_MOTOR, OUTPUT);
	
	int serverSocket;
	sockaddr_in local;
	serverSocket = socket(PF_INET,SOCK_STREAM,0);
	bzero(&local,sizeof(sockaddr_in));
	local.sin_family = AF_INET;
	local.sin_port = htons(10000);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(serverSocket,(sockaddr*)&local,sizeof(sockaddr));
	listen(serverSocket,5);
	
	int sock;
	socklen_t slen = sizeof(sockaddr_in);
	int len;
	sockaddr_in remote;
	Message msg;
	
	printf("Waitting connection......\n");
	
	sock = accept(serverSocket,(sockaddr*)&remote,&slen);
	close(serverSocket);
	
	printf("%s:%d Connected\n",inet_ntoa(remote.sin_addr),ntohs(remote.sin_port));
	while(true)
	{
		len = recv(sock, &msg, sizeof(Message),0);
		//printf("len: %d\n",len);
		if(len <= 0)
			break;
		handleMessage(sock,msg);
	}
	
	close(sock);
	
	//stop while disconnect
	handleStopMessage();
	
	printf("\n\n");
	
	return 0;
}

void handleMessage(int& sock,Message& msg)
{
	//printf("Message received: %d\n",msg.command);
	switch(msg.command)
	{
		default:
		case COMMAND_NOOP:
			//printf("zzzz...\n");
			break;
		
		case COMMAND_FORWARD:
			handleForwardMessage();
			break;
			
		case COMMAND_BACKWARD:
			handleBackwardMessage();
			break;
			
		case COMMAND_STOP:
			handleStopMessage();
			break;
		
		case COMMAND_ROTATE:
			handleRotateMessage(sock);
			break;
	}
}

void handleForwardMessage()
{
	digitalWrite(PIN_BACKWARD_MOTOR,LOW);
	digitalWrite(PIN_FORWARD_MOTOR,HIGH);
	g_motorDirection = MOTOR_DIRECTION_FORWARD;
	//printf("Spin up forward ( ´ ▽ ` )ﾉ\n");
	setStatus();
}

void handleBackwardMessage()
{
	digitalWrite(PIN_FORWARD_MOTOR,LOW);
	digitalWrite(PIN_BACKWARD_MOTOR,HIGH);
	g_motorDirection = MOTOR_DIRECTION_BACKWARD;
	//printf("Spin up backward ( ´ ▽ ` )ﾉ\n");
	setStatus();
}

void handleStopMessage()
{
	digitalWrite(PIN_FORWARD_MOTOR,LOW);
	digitalWrite(PIN_BACKWARD_MOTOR,LOW);
	g_motorDirection = MOTOR_DIRECTION_NONE;
	//printf("Spin down (｡•́︿•̀｡)\n");
	setStatus();
}

void handleRotateMessage(int& sock)
{
	static RotateCommand cmd;
	recv(sock,&cmd,sizeof(RotateCommand),0);
	
	long diff = cmd.angle - g_currentAngle;
	long udiff = abs(diff);
	if(diff == 0)
		return;
	
	if(abs(cmd.angle) > 45)
		return;
	
	//printf("Turning to %d\n",cmd.angle);
	
	if(diff < 0)
	{
		//left
		for(long i=0;i<ANGLE_STEP;i++)
		{
			--g_currentPhase;
			if(g_currentPhase < 0)
				g_currentPhase = REVOLUTION-1;
			
			const bool* phase = STEPPER_MOTOR_PHASE_VALUES[g_currentPhase];
			setPhaseValues(phase);
			delay(DELAY_INTERVAL);
		}
		g_currentAngle--;
		setStatus();
	}
	else
	{
		//right
		for(long i=0;i<ANGLE_STEP;i++)
		{
			++g_currentPhase;
			if(g_currentPhase >= REVOLUTION)
				g_currentPhase = 0;
			
			const bool* phase = STEPPER_MOTOR_PHASE_VALUES[g_currentPhase];
			setPhaseValues(phase);
			delay(DELAY_INTERVAL);
		}
		g_currentAngle++;
		setStatus();
	}
}

inline void setPhaseValues(const bool* phaseValues)
{
	for(int j=0;j<4;j++)
	{
		digitalWrite(PIN_STEP_MOTOR[j],phaseValues[j]);
	}
}

inline void reverseBytes(const char* in,char* out,int length)
{
	int j = 0;
	for(int i=length-1;i>=0;i--)
	{
		out[j++] = in[i];
	}
}

inline void setStatus()
{
	static const char MOTOR_DIRECTION_FORWARD_EXPLAIN[] = "Forward";
	static const char MOTOR_DIRECTION_BACKWARD_EXPLAIN[] = "Backward";
	static const char MOTOR_DIRECTION_NONE_EXPLAIN[] = "Stopped";
	
	const char* explain;
	if(g_motorDirection == MOTOR_DIRECTION_NONE)
	{
		explain = MOTOR_DIRECTION_NONE_EXPLAIN;
	}
	else if(g_motorDirection == MOTOR_DIRECTION_FORWARD)
	{
		explain = MOTOR_DIRECTION_FORWARD_EXPLAIN;
	}
	else if(g_motorDirection == MOTOR_DIRECTION_BACKWARD)
	{
		explain = MOTOR_DIRECTION_BACKWARD_EXPLAIN;
	}
	
	printf("Status: %-08s, Angle: %-04d\033[100D",explain,g_currentAngle);

	//printf("\033[100D");
	//\033[1A
}


