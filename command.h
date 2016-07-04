#pragma once

#define COMMAND_NOOP		'N'
#define COMMAND_FORWARD		'F'
#define COMMAND_BACKWARD	'B'
#define COMMAND_STOP		'T'
#define COMMAND_ROTATE		'A'

struct Message
{
	char command;
};

struct RotateCommand
{
	long angle;
};


