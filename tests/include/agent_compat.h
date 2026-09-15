#pragma once
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#define OK 0
#define ERROR -1
int agent_task_create(void *(*fn)(void *), const char*, int, void*, int);
