/***************************************
 ** ECSE 427 - Programming Assignment 1
 ** Amjad Al-Rikabi 
 ** ID: 260143211
 **************************************/

#include "mythreads.h"
#define EMPTY -1

/*********GLOBAL VARIABLES***********/
static List * runQueue;
static ControlBlock TCB_Table[THREAD_MAX];
static Semaphore Semaphore_Table[SEMAPHORE_MAX];
static int quantum;
static int numOfThreads  = 0;
static int curRunThread;
int context_turn_counter;
static int locked_semaphores;

/********HELPER FUNCTION DEFINITIONS*******/
static void startTimer(int);
static void incrementTime(int);

//This function initializes all the global data structures
int mythread_init()
{
    int i;
    locked_semaphores = 0;
    curRunThread = 0;
   
	runQueue = list_create(NULL);
    //Check that we succesfully generated a list.
    if (runQueue == NULL) 
    {
        return -1;
    }

    //Initialize all the TCB_Table entries to EMPTY
    for(i = 0 ; i <THREAD_MAX; i++) 
    {
        TCB_Table[i].state = EMPTY;
    }

	//Initialize all the Semaphore_Table Entries
    for(i = 0; i<SEMAPHORE_MAX; i++) 
    {
		//Initialize semaphore entry in the table
		Semaphore_Table[locked_semaphores].value = 0;
		Semaphore_Table[locked_semaphores].initial = 0;
		Semaphore_Table[locked_semaphores].thread_queue = list_create(NULL);
		//Check that we succesfully generated a list.
		if (Semaphore_Table[locked_semaphores].thread_queue == NULL) return -1;
	}

    return 0;
}

//This function creates a new thread
int mythread_create(char *threadname, void (*threadfunc)(), int stacksize)
{
    //Check that we have not exceeded maximum number of allowed threads
    if (numOfThreads == THREAD_MAX)
    {
        printf("\nERROR: Reached maximum number of threads!");
        return -1;
    }

    //Insert thread to the TCB control table & Check
    if (getcontext(&TCB_Table[numOfThreads].context) != 0)
    {
        printf("\nERROR: Cannot obtain Context successully!");
        return -1;
    }    

    /*
    uc_stack stack;
    stack.ss_sp = TCB_Table[numOfThreads].stack;
    stack.ss_size = stacksize;
    stack.ss_flags = 0; */

    //Populate the Table entry
    strcpy(TCB_Table[numOfThreads].thread_name, threadname); 
    TCB_Table[numOfThreads].thread_id = numOfThreads;
    TCB_Table[numOfThreads].state = RUNNABLE;
    TCB_Table[numOfThreads].context.uc_link = &uctx_main; //Assuming this is run from the context of the main() function
    TCB_Table[numOfThreads].context.uc_stack.ss_sp = malloc(stacksize);

    //Make sure that the stack is allocated successfully
    if (TCB_Table[numOfThreads].context.uc_stack.ss_sp == 0) 
    {
       printf("ERROR: Cannot allocate stack succesfully!");
       return -1;
    }

    TCB_Table[numOfThreads].context.uc_stack.ss_size = stacksize;
    TCB_Table[numOfThreads].context.uc_stack.ss_flags = SS_DISABLE;
    sigemptyset(&TCB_Table[numOfThreads].context.uc_sigmask);
    TCB_Table[numOfThreads].run_time = 0;

    //Create the context
    makecontext(&TCB_Table[numOfThreads].context, threadfunc, 0);   
    //Add thread to runQueue
    runQueue = list_append_int(runQueue, numOfThreads);
 	//Check that we succesfully generated a list.
    if (runQueue == NULL) return -1;
    //Print ThreadName
    printf("\tCreated new thread:\t %s\n", threadname);

    return numOfThreads++;
}

//This function will change the state of the running thread to EXIT but will still leave the TCB in the table
void mythread_exit()
{
    TCB_Table[curRunThread].state = EXIT;
    incrementTime(curRunThread);
}

//This functions runs threads
void runthreads()
{
    //Block Signals
    sigset_t sig;
    sigemptyset(&sig);
    sigaddset(&sig, SIGALRM);
    sigprocmask(SIG_BLOCK, &sig, NULL);

    //Check that we have a thread to run
    if (numOfThreads == 0) 
    {
        printf("There are no Threads to be run!");
        return;
    }

	//Set quantum timer
    struct itimerval tval;
    tval.it_interval.tv_sec = 0;
    tval.it_interval.tv_usec = quantum;
    tval.it_value.tv_sec = 0;
    tval.it_value.tv_usec = quantum;
    setitimer(ITIMER_REAL, &tval, 0);
	
	//Designate evict_thread as signal handler
    sigset(SIGALRM, &evict_thread);
  
    // Unblock signal
    sigprocmask(SIG_UNBLOCK, &sig, NULL);

    while(!list_empty(runQueue)) {
		//Dequeue first runnable and update state & start timer
        curRunThread = list_shift_int(runQueue);
        TCB_Table[curRunThread].state = RUNNING;
        startTimer(curRunThread);
		//Swap context from main() to thread
		swapcontext(&uctx_main, &TCB_Table[curRunThread].context);
    }

    //Make sure we update the status of the last running thread:
    TCB_Table[curRunThread].state = EXIT;
    sigprocmask(SIG_BLOCK, &sig, NULL);

}

//This is the thread switcher
void evict_thread()
{
    int oldCurRunThread = curRunThread;
    
	//if there is nothing in the queue to run
    if (list_empty(runQueue) && TCB_Table[curRunThread].state == EXIT) 
    {
        incrementTime(curRunThread);
        setcontext(&uctx_main);
    }

	//If the runQueue is not empty
    if (!list_empty(runQueue)) 
    {
		//Update curRunThread index from the runQueue
        curRunThread = list_shift_int(runQueue);
		//Update timers for both oldRunThread
        incrementTime(oldCurRunThread);
        //Queue in runQueue if OldRunThread is not complete
        if ( (TCB_Table[oldCurRunThread].state==RUNNING) || (TCB_Table[oldCurRunThread].state==RUNNABLE) ) 
            runQueue = list_append_int(runQueue, oldCurRunThread);
		
		context_turn_counter++;
		//Start timer for currentRunThread and switch context to it
		startTimer(curRunThread);
        swapcontext(&TCB_Table[oldCurRunThread].context, &TCB_Table[curRunThread].context);
        
    } 
 	//runQueue list is empty
	else
	{
		//Restore context to main()
		swapcontext(&TCB_Table[curRunThread].context, &uctx_main);
    }
}

//Quantum value setter
void set_quantum_size(int quantem)
{
    quantum = quantem;
}

//Prints out a table of Current Threads in the table
void mythread_state()
{
	char * str_tmp; int i;

	printf("\nStatus of Threads:");
	printf("\n===================================================="); 
    printf("\nTHREADNAME\tTHREAD_ID\tSTATE\tRUN TIME(ns)\n");
	printf("===================================================="); 
    
    for(i = 0; i < THREAD_MAX; i++) 
	{    
        switch(TCB_Table[i].state) {
			case -1:
                str_tmp = "EMPTY";
                break;
            case 0:
                str_tmp = "RUNNING";
                break;
            case 1:
                str_tmp = "RUNNABLE";
                break;
			case 2:
                str_tmp = "BLOCKED";
                break;
            case 3:
                str_tmp = "EXIT";
                break;
            default:
                str_tmp = "NOT RECOGNIZED!";
                break;
        }
        printf("\n%d\t\t%s\t%s\t%f", i, TCB_Table[i].thread_name, str_tmp, TCB_Table[i].run_time); 
    }
	printf("\n====================================================\n");
}

//Create and initialize a semaphore
int create_semaphore(int value)
{
    if (locked_semaphores == SEMAPHORE_MAX || locked_semaphores < 0) 
    {
		printf("ERROR: Used wrong number of semaphores!");
        return -1;
    }
    
	//Initialize semaphore entry in the table
    Semaphore_Table[locked_semaphores].value = value;
    Semaphore_Table[locked_semaphores].initial = value;
    Semaphore_Table[locked_semaphores].thread_queue = list_create(NULL);

    return locked_semaphores++;
}

void semaphore_wait(int Semaphore)
{
	//Disable signal
    sigset_t sig;
    sigemptyset(&sig);
    sigaddset(&sig, SIGALRM);
    sigprocmask(SIG_BLOCK, &sig, NULL);

    int old_context_turn_counter = context_turn_counter;

	//Decrement Semaphonre value in the table 
    Semaphore_Table[Semaphore].value = Semaphore_Table[Semaphore].value -1;

	//Block Thread if semaphore value < 0 and store in queue
    if(Semaphore_Table[Semaphore].value<0) 
	{
        TCB_Table[curRunThread].state = BLOCKED;
        Semaphore_Table[Semaphore].thread_queue = list_append_int(Semaphore_Table[Semaphore].thread_queue, curRunThread);
    }

	//Enable Signalling
    sigprocmask(SIG_UNBLOCK,&sig,NULL);
    while(old_context_turn_counter == context_turn_counter); //spinlock
}

void semaphore_signal(int Semaphore)
{
	int next;
    
    //Increment semaphore value
    Semaphore_Table[Semaphore].value = Semaphore_Table[Semaphore].value + 1;
    
    if (!(Semaphore_Table[Semaphore].value > 0))
	{        
        next = list_shift_int(Semaphore_Table[Semaphore].thread_queue);
        TCB_Table[next].state = RUNNABLE;
        runQueue = list_append_int(runQueue, next);
    } 

    evict_thread();
}

//Remove a semaphore from the system
void destroy_semaphore(int Semaphore)
{
    if (!list_empty(Semaphore_Table[Semaphore].thread_queue)) 
	{
       printf("ERROR: Threads are still waiting on this semaphore!");
       exit(1);
    }

    if (Semaphore_Table[Semaphore].initial != Semaphore_Table[Semaphore].value) 
	{
        printf("WARNING: Initial value is not equal to the current value for the semaphore!\n");
	}

	//re-initialize semaphore entry in the table
    Semaphore_Table[Semaphore].value = 0;
    Semaphore_Table[Semaphore].initial = 0;
    Semaphore_Table[Semaphore].thread_queue = list_create(NULL);
}

//Increment Timer of thread in right place in the table
static void incrementTime(int thread_index)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    TCB_Table[thread_index].run_time += now.tv_nsec - TCB_Table[thread_index].start.tv_nsec;
}

//Record Starting time of thread in right place in the table
static void startTimer(int thread_index)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);  
    TCB_Table[thread_index].start.tv_nsec = now.tv_nsec;
}

