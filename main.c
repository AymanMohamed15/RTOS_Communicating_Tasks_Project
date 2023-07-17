/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "diag/trace.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "Semphr.h"

#define CCM_RAM __attribute__((section(".ccmram")))

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

// Task, queue and semaphore handles
TaskHandle_t xSender1TaskHandle;
TaskHandle_t xSender2TaskHandle;
TaskHandle_t xSender3TaskHandle;
SemaphoreHandle_t xSenderSemaphore[3];

TaskHandle_t xReceiverTaskHandle;
SemaphoreHandle_t xReceiverSemaphore;

QueueHandle_t Message_queue;

/*Global_variables*/
int Sender_Transmit_Counter[3]={0};
int Sender_Blocked_Counter[3]={0};
int Received_Counter=0;
int Upper_Boundaries[]={150,200,250,300,350,400};
int Lower_Boundaries[]={50,80,110,140,170,200};
int Iteration_Counter=0;
int Total_sending1_time=0, Total_sending2_time=0,Total_sending3_time=0 ;

// Timer handle and initial period
TimerHandle_t xSender1Timer;
TickType_t xSender1TimerPeriod;
TimerHandle_t xSender2Timer;
TickType_t xSender2TimerPeriod;
TimerHandle_t xSender3Timer;
TickType_t xSender3TimerPeriod;
TimerHandle_t xReceiverTimer;
TickType_t xReceiverTimerPeriod = pdMS_TO_TICKS(100);
BaseType_t xSender1TimerStarted = 0
		, xSender2TimerStarted = 0
		, xSender3TimerStarted = 0
		, xReceiverTimerStarted = 0;


/*Random number generator*/
int GetRandomUsingUniformDist(void)
{
	int Range = Upper_Boundaries [Iteration_Counter] - Lower_Boundaries [Iteration_Counter] +1 ;
	int Random_value= (rand() %  Range)+ Lower_Boundaries [Iteration_Counter] ;
	return(Random_value);
}

void Reset(void)
{
	/* static variable so it checks whether it is first time called from main function or not*/
	static int first_call=0;
	/*If it is not its first call*/
	if(first_call!=0)
	{
		/*Variables to store the total number of sent and blocked messages*/
		int Sum_Transmit=0,Sum_Blocked=0;
		/*Print the number of iteration*/
		printf("******************************************************************************************\n");
		printf("Iteration number : %d\n" , Iteration_Counter+1);
		/*Get the sum of all transmitted and blocked messages*/
		for(int i=0 ;i<3;i++)
		{
			Sum_Transmit+=Sender_Transmit_Counter[i];
			Sum_Blocked+=Sender_Blocked_Counter[i];
		}
		/*Print the number of total successfully sent messages, blocked messages and statistics for each task */
		printf("Total number of successfully sent messages :%d\n",Sum_Transmit);
		printf("Total number of blocked messages :%d\n\n",Sum_Blocked);
		printf("Number of successfully sent messages for the higher priority sender task :%d\n",Sender_Transmit_Counter[0]);
		printf("Number of blocked messages for the higher priority sender task :%d\n",Sender_Blocked_Counter[0]);
		printf("Average sending time for the higher priority sender task: %d\n\n",Total_sending1_time/(Sender_Transmit_Counter[0]+Sender_Blocked_Counter[0]));
		printf("Number of successfully sent messages for the lower sender priority sender task 1 :%d\n",Sender_Transmit_Counter[1]);
		printf("Number of blocked messages for the lower priority sender task 1 :%d\n",Sender_Blocked_Counter[1]);
		printf("Average sending time for the lower priority sender task1: %d\n\n",Total_sending2_time/(Sender_Transmit_Counter[1]+Sender_Blocked_Counter[1]));
		printf("Number of successfully sent messages for the lower priority sender task 2 :%d\n",Sender_Transmit_Counter[2]);
		printf("Number of blocked messages for the lower priority sender task 2 :%d\n",Sender_Blocked_Counter[2]);
		printf("Average sending time for the lower priority sender task2: %d\n\n",Total_sending3_time/(Sender_Transmit_Counter[2]+Sender_Blocked_Counter[2]));
		printf("******************************************************************************************\n");
		/*Increment the iteration counter*/
		Iteration_Counter++;
	}
	else/*if its first call*/
	{
		/*increment so the a first call is detected*/
		first_call++;
	}
	/*Reset successfully sent and blocked messages for all sender tasks*/
	for(int i=0;i<3;i++)
	{
		Sender_Transmit_Counter[i]=0;
		Sender_Blocked_Counter[i]=0;
	}
	/*Reset the total sendeing time for senders task that used to calculate the average sending time*/
	Total_sending1_time=0;
	Total_sending2_time=0;
	Total_sending3_time=0;
	Received_Counter=0;
	/*Clear the message queue*/
	xQueueReset(Message_queue);
	/*Check if the number of iterations is exceeded*/
	if(Iteration_Counter == 6)
	{
		printf(" Game Over \n ");
		/*Delete all timers*/
		xTimerDelete(xSender1Timer , NULL);
		xTimerDelete(xSender2Timer , NULL);
		xTimerDelete(xSender3Timer , NULL);
		xTimerDelete(xReceiverTimer , NULL);
		/*exit the program*/
		exit (0);
		vTaskEndScheduler();
	}

}



/*Timer callback functions*/
void vSender1TimerCallback(TimerHandle_t xTimer)
{
	// Release the semaphore to unblock the Sender_task
	xSemaphoreGive(xSenderSemaphore[0]);
}

void vSender2TimerCallback(TimerHandle_t xTimer)
{
	// Release the semaphore to unblock the Sender_task
	xSemaphoreGive(xSenderSemaphore[1]);
}

void vSender3TimerCallback(TimerHandle_t xTimer)
{
	// Release the semaphore to unblock the Sender_task
	xSemaphoreGive(xSenderSemaphore[2]);
}

void vReceiverTimerCallback(TimerHandle_t xTimer)
{
	// Release the semaphore to unblock the Sender_task
	xSemaphoreGive(xReceiverSemaphore);
	if(Received_Counter>=1000)
	{
		Reset();
	}
	else
	{
		/*Do nothing*/
	}
}


/*Tasks*/

/*Higher priority Sender task function*/
void vSender1Task(void* pvParameters)
{
	while(1)
	{

		// Wait for the semaphore to be released by the timer callback
		xSemaphoreTake(xSenderSemaphore[0], portMAX_DELAY);

		// Check if the queue is full
		if (uxQueueSpacesAvailable(Message_queue) > 0)
		{
			// Generate the string with current system tick
			char Sender_string[20];
			sprintf(Sender_string, "Time is %lu", xTaskGetTickCount());

			// Send the string to the queue
			if (xQueueSendToBack(Message_queue, Sender_string, 0) == pdPASS)
			{
				// Increment transmit counter
				Sender_Transmit_Counter[0]++;
			}
		}
		else
		{
			// Increment blocked counter
			Sender_Blocked_Counter[0]++;
		}
		/*Get a random value in the range of the upper and lower bound of the current iteration*/
		int SenderRandomValue=GetRandomUsingUniformDist();
		/*add the generated random value to the total sending time of the sender task*/
		Total_sending1_time+=SenderRandomValue;
		/*Convert the random value to ticks*/
		xSender1TimerPeriod=pdMS_TO_TICKS(SenderRandomValue);
		/*Change the period of the sender's timer to the converted random value*/
		xTimerChangePeriod(xSender1Timer, xSender1TimerPeriod, 0);

	}
}

/*Lower priority Sender task 1 function*/
void vSender2Task(void* pvParameters)
{
	while(1)
	{
		// Wait for the semaphore to be released by the timer callback
		xSemaphoreTake(xSenderSemaphore[1], portMAX_DELAY);

		// Check if the queue is full
		if (uxQueueSpacesAvailable(Message_queue) > 0)
		{
			// Generate the string with current system tick
			char Sender_string[20];
			sprintf(Sender_string, "Time is %lu", xTaskGetTickCount());

			// Send the string to the queue
			if (xQueueSendToBack(Message_queue, Sender_string, 0) == pdPASS)
			{
				// Increment transmit counter
				Sender_Transmit_Counter[1]++;
			}
		}
		else
		{
			// Increment blocked counter
			Sender_Blocked_Counter[1]++;
		}
		/*Get a random value in the range of the upper and lower bound of the current iteration*/
		int SenderRandomValue=GetRandomUsingUniformDist();
		/*add the generated random value to the total sending time of the sender task*/
		Total_sending2_time+=SenderRandomValue;
		/*Convert the random value to ticks*/
		xSender2TimerPeriod=pdMS_TO_TICKS(SenderRandomValue);
		/*Change the period of the sender's timer to the converted random value*/
		xTimerChangePeriod(xSender2Timer, xSender2TimerPeriod, 0);

	}
}

/*Lower priority Sender task 2 function*/
void vSender3Task(void* pvParameters)
{
	while(1)
	{
		// Wait for the semaphore to be released by the timer callback
		xSemaphoreTake(xSenderSemaphore[2], portMAX_DELAY);

		// Check if the queue is full
		if (uxQueueSpacesAvailable(Message_queue) > 0)
		{
			// Generate the string with current system tick
			char Sender_string[20];
			sprintf(Sender_string, "Time is %lu", xTaskGetTickCount());

			// Send the string to the queue
			if (xQueueSendToBack(Message_queue, Sender_string, 0) == pdPASS)
			{
				// Increment transmit counter
				Sender_Transmit_Counter[2]++;
			}
		}
		else
		{
			// Increment blocked counter
			Sender_Blocked_Counter[2]++;
		}
		/*Get a random value in the range of the upper and lower bound of the current iteration*/
		int SenderRandomValue=GetRandomUsingUniformDist();
		/*add the generated random value to the total sending time of the sender task*/
		Total_sending3_time+=SenderRandomValue;
		/*Convert the random value to ticks*/
		xSender3TimerPeriod=pdMS_TO_TICKS(SenderRandomValue);
		/*Change the period of the sender's timer to the converted random value*/
		xTimerChangePeriod(xSender3Timer, xSender3TimerPeriod, 0);
	}
}

/*Receiver task*/
void vReceiverTask(void* pvParameters)
{
	while(1)
	{
		// Wait for the semaphore to be released by the timer callback
		xSemaphoreTake(xReceiverSemaphore, portMAX_DELAY);

		// Check if the queue is full
		if (uxQueueMessagesWaiting(Message_queue) > 0)
		{
			// Generate the string with current system tick
			char Receiver_string[20];
			// Send the string to the queue
			if (xQueueReceive(Message_queue, Receiver_string, 0)==pdPASS)
			{
				// Increment transmit counter
				Received_Counter++;
				//printf("Receiver counter %d\n",Received_Counter);
			}
		}
		else
		{
			/*Do nothing*/
		}

	}
}

/*Initialization function that creates the required queue, tasks, semaphores & timers*/
void System_Initialization (void)
{
	/*Create the message queue*/
	Message_queue = xQueueCreate(10 , sizeof(char[20]));
	/*Check if the message queue created correctly*/
	if(Message_queue!= NULL)
	{
		/*Create the senders and receiver tasks and assign the suitable priority for each of them*/
		xSender1TaskHandle=xTaskCreate(vSender1Task , "sender1" ,2048, NULL , 2 , NULL);
		xSender2TaskHandle=xTaskCreate(vSender2Task , "sender2" ,2048, NULL , 1 , NULL);
		xSender3TaskHandle=xTaskCreate(vSender3Task , "sender3" ,2048, NULL , 1 ,NULL);
		xReceiverTaskHandle=xTaskCreate(vReceiverTask , "receiver" ,2048, NULL , 3 , NULL);
	}
	/*Create array that will hold the initial values of the senders' timer period*/
	int Sender_timer_initial_value[3];
	for(int i=0;i<3;i++)
	{
		/*Get the initial value from the random number generator*/
		Sender_timer_initial_value[i]=GetRandomUsingUniformDist();
		/*Create the senders' semaphore*/
		xSenderSemaphore[i] = xSemaphoreCreateBinary();
	}
	/*Create the receiver's semaphore*/
	xReceiverSemaphore=xSemaphoreCreateBinary();

	/*Convert the random values to ticks*/
	xSender1TimerPeriod=pdMS_TO_TICKS(Sender_timer_initial_value[0]);
	xSender2TimerPeriod=pdMS_TO_TICKS(Sender_timer_initial_value[1]);
	xSender3TimerPeriod=pdMS_TO_TICKS(Sender_timer_initial_value[2]);

	/*Create the timersand assign the initial periods for the senders' timers,
	 * the receiver's timer period has been initialized at global variables section above*/
	xSender1Timer=xTimerCreate( "SenderTimer1", xSender1TimerPeriod, pdTRUE, ( void * ) 0, vSender1TimerCallback);
	xSender2Timer=xTimerCreate( "SenderTimer2", xSender2TimerPeriod, pdTRUE, ( void * ) 0, vSender2TimerCallback);
	xSender3Timer=xTimerCreate( "SenderTimer3", xSender3TimerPeriod, pdTRUE, ( void * ) 0, vSender3TimerCallback);
	xReceiverTimer=xTimerCreate( "ReceiverTimer", xReceiverTimerPeriod , pdTRUE, ( void * ) 0, vReceiverTimerCallback);
}

int main(void)
{
	/*Initialization function that creates the required queue, tasks, semaphores & timers*/
	System_Initialization();
	Reset();
	/*Check if the timers created correctly*/
	if( ( xSender1Timer!= NULL ) && ( xSender2Timer != NULL ) && ( xSender3Timer != NULL )&& ( xReceiverTimer != NULL ) && Message_queue != NULL )
	{
		xSender1TimerStarted=xTimerStart( xSender1Timer, 0 );
		xSender2TimerStarted=xTimerStart( xSender2Timer, 0 );
		xSender3TimerStarted=xTimerStart( xSender3Timer, 0 );
		xReceiverTimerStarted=xTimerStart(xReceiverTimer, 0 );

	}
	/*Check if the timers started correctly*/
	if( xSender1TimerStarted== pdPASS && xSender2TimerStarted == pdPASS && xSender3TimerStarted == pdPASS &&xReceiverTimerStarted==pdPASS)
	{
		/*start scheduler*/
		vTaskStartScheduler();
	}
	else
	{
		printf("error to Start Task Scheduler ... \n");
		exit(0);
	}
	return 0;

}



void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}






