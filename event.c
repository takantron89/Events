#include "c_event.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint eventListLength = 1;
sys_event* events;

#define EVENT_CLEAN_UP_RATE 30

//Debug thing
uint errorCount = 0;

bool runEventLoop = true;

sys_event* createNewEvent(event_func fn, uint ac, ...) {
	VA_SETUP(ac);
	//TODO FIX THIS!!! this is not good////////////////////
	sys_event tmp = {fn , ac, vl, false, _E_UNRUN};
	sys_event* out = &tmp; 
	VA_CLEANUP;
	return out;
}

void _pushBackEvents(event_func ev, int argCnt, va_list vl) {
	va_start(vl, argCnt);
	sys_event* newEvent = createNewEvent(ev, argCnt, vl);

	realloc(events, sizeof(events) + sizeof(sys_event));	
	eventListLength++;

	//eventListLength is the number of events in the array and by nature not zero indexed
	memcpy(&events[eventListLength - 1], newEvent, sizeof(sys_event));

	
	VA_CLEANUP;
}

void* _callEvent(sys_event ev) {
	return (ev.fn)(ev.argCnt, ev.vlArgs);
}

event_func errorSimple(int argCnt, va_list vl) {
	argCnt = 1; // only one arg, the output string
	va_start(vl, argCnt);
	const char* out = va_arg(vl, const char*);
	VA_CLEANUP;
	THROW_EVENT_ERROR((void*)out);
}

sys_event _atEvents(uint pos) {
	if ((pos<=eventListLength-1)&(pos>=0)) {
		return events[pos];
	}

	else {

		char errOut[256];
		sprintf(errOut, "Out of bounds call at:%d\0", pos);
		
		sys_event out = {errorSimple, 1, errOut};
		return out;
	}
}

void _clearEvent(uint pos) {

}

void _CleanUpEvents() {
	uint nulls = 0;

	while (_atEvents(nulls).isCompleted == false)	{
		nulls++;
	}

	eventListLength = eventListLength - nulls;
	sys_event* buff = malloc(sizeof(sys_event)*(eventListLength));
	memcpy(buff, &events[nulls], sizeof(sys_event)*(eventListLength));
	//TODO finish this thing
}

#ifdef __GNUC__

//we are on some form of gcc so we can just go ahead with POSIX threads

#include <pthread.h>
#define THREAD_T pthread_t

#define THREAD_FN void*
#define THREAD_ARG_NAME arguments
#define THREAD_ARG void* THREAD_ARG_NAME

#define DEFAULT_THREAD_RETURN_VAL NULL

#define CREATE_THREAD(T, F, A) pthread_create(T, NULL, F, A)
#define JOIN_THREAD(T) pthread_join(T, NULL)

#else

//Welp we are probably on windows then so we have to deal with their library

#include <Windows.h>
#define THREAD_T HANDLE

#define THREAD_FN DWORD WINAPI
#define THREAD_ARG_NAME lpParam
#define THREAD_ARG LPVOID THREAD_ARG_NAME

#define DEFAULT_THREAD_RETURN_VAL 0

#define CREATE_THREAD(T, F, A) (T = CreateThread(NULL, 0, F, (LPVOID)A, 0, (LPDWORD)A)) /*The A arg will always be an in so we can just pass the i arg in the for loop*/
#define JOIN_THREAD(T) WaitForSingleObject(T, INFINITE)

#endif 

//MULTITHREADING STUFF

THREAD_T* threads;

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

	threads = malloc(sizeof(THREAD_T));

	while (runEventLoop) {
		for (uint i = 1; i <= eventListLength; i++) {
			CREATE_THREAD(threads[i], runEvent, i);
		}
	
		for (uint i = 1; i <= eventListLength; i++) {
			JOIN_THREAD(threads[i]);
		}

		//This funtion is used to remove all null events and just reduce the size of the array
		_CleanUpEvents();
	}

	
}