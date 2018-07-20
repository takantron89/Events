#include "event.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

uint eventListLength = 0;
sys_event* events;

//Debug thing
uint errorCount = 0;

bool runEventLoop = true;

sys_event* createNewEvent(event_func fn, uint ac, ...) {
	VA_SETUP(ac);
	sys_event* out = malloc(sizeof(sys_event));
    out->fn = fn;
    out->argCnt = ac,
    memcpy(out->vlArgs, vl, sizeof(vl));
    out->isCompleted = false;
    out->endCode = _E_UNRUN;
	VA_CLEANUP;
	return out;
}

void _pushBackEvents(event_func ev, int argCnt, va_list vl) {
	sys_event* newEvent = createNewEvent(ev, argCnt, vl);

	void* res = realloc(events, sizeof(events) + sizeof(sys_event));	
	assert(res != NULL);
    eventListLength++;

	//eventListLength is the number of events in the array and by nature not zero indexed
	memcpy(&events[eventListLength - 1], newEvent, sizeof(sys_event));

	
	VA_CLEANUP;
}

void* _callEvent(sys_event ev) {
	return (ev.fn)(ev.argCnt, ev.vlArgs);
}

void* errorSimple(int argCnt, va_list vl) {
	argCnt = 1; // only one arg, the output string
	const char* out = va_arg(vl, const char*);
	VA_CLEANUP;
	THROW_EVENT_ERROR((void*)out);
}

sys_event _atEvents(uint pos) {
	if (pos<=eventListLength-1) {
		return events[pos];
	}

	else {

		char errOut[256];
		sprintf(errOut, "Out of bounds call at:%d", pos);
		
		sys_event out;
        out.fn = errorSimple;
        out.argCnt = 1;
        memcpy(&out.vlArgs[0], errOut, sizeof(errOut));
		return out;
	}
}

void _CleanUpEvents() {
    //This function is designed to truncate the array down to however many events have not been called

	uint nulls = 0;

	while (_atEvents(nulls).isCompleted == true)	{ 
		nulls++;
	}

	eventListLength = eventListLength - nulls;//the new event size
	sys_event* buff = malloc(sizeof(sys_event)*(eventListLength));
	memcpy(buff, &events[nulls], sizeof(sys_event)*(eventListLength));//copy all uncalled events into a buffer
    void* res = realloc(events, sizeof(sys_event)*eventListLength); //truncate array
    assert(res != NULL && "realloc failure");
    memcpy(events, buff, sizeof(sys_event)*eventListLength); //copy back all uncalled arrays into main event list
    free(buff); // cleanup
}

#ifdef __GNUC__

//we are on some form of gcc so we can just go ahead with POSIX threads

#include <pthread.h>
#define THREAD_T pthread_t

#define THREAD_FN void*
#define THREAD_ARG_NAME arguments
#define THREAD_ARG_T void*

#define DEFAULT_THREAD_RETURN_VAL NULL

#define CREATE_THREAD(T, F, A) pthread_create(T, NULL, F, A)
#define JOIN_THREAD(T) pthread_join(T, NULL)

#else

//Welp we are probably on windows then so we have to deal with their library

#include <Windows.h>
#define THREAD_T HANDLE

#define THREAD_FN DWORD WINAPI
#define THREAD_ARG_NAME lpParam
#define THREAD_ARG_T LPVOID


#define DEFAULT_THREAD_RETURN_VAL 0

#define CREATE_THREAD(T, F, A) (T = CreateThread(NULL, 0, F, (LPVOID)A, 0, (LPDWORD)A)) /*The A arg will always be an int so we can just pass the i arg in the for loop*/
#define JOIN_THREAD(T) WaitForSingleObject(T, INFINITE)

#endif 

//MULTITHREADING STUFF

#define THREAD_ARG THREAD_ARG_T THREAD_ARG_NAME

THREAD_T** threads;

THREAD_FN runEvent(THREAD_ARG) {

	uint pos = (uint)THREAD_ARG_NAME;

	uint* res = (events[pos].fn)(events[pos].argCnt, events[pos].vlArgs); // Func call

	if (res == _E_OK) {
		//The event call was successful and has no errors
		//so we dont need the event anymore and thus we can mark it for clean up
		events[pos].isCompleted = true;
		events[pos].endCode = _E_OK;
	}

	else {
		const char* err = (const char*)res;
		events[pos].endCode = _E_FAILURE;
		fprintf(stderr, " %s", err);
		errorCount++;
	}


	return DEFAULT_THREAD_RETURN_VAL;
}

//Event handler thread

void runEventHandler() {

	while (runEventLoop) {
        //Every time all the previouse threads were called and joined the thread array is resized to the proper length
        void* res = realloc(threads, sizeof(THREAD_T)*eventListLength);
        
        assert(res != NULL && "realloc failure");

		for (uint i = 0; i <= eventListLength; i++) {
			CREATE_THREAD(threads[i], runEvent, (THREAD_ARG_T)i);
		}
	
		for (uint i = 0; i <= eventListLength; i++) {
			JOIN_THREAD(threads[i]);
		}

		//This funtion is used to remove all null events and just reduce the size of the array
		_CleanUpEvents();
	}

	
}
