#ifndef EVENTS


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

//Basic event statuses
#define _E_OK 0
#define _E_FAILURE 1
#define _E_UNRUN 0xfffffffu

//various sys_event helper functions
#define VA_SETUP(arg_cnt) va_list vl;\
	va_start(vl, arg_cnt);

#define VA_CLEANUP va_end(vl)

//event error things
#define THROW_EVENT_ERROR(E) return E; //Make sure string is null terminated



typedef void* (*event_func)(int argCnt, va_list vl);
typedef unsigned int uint;

typedef struct sys_event { // fucking c is weird with this
	event_func fn;
	uint argCnt;
	va_list vlArgs;
	//State stuff
	bool isCompleted;
	uint endCode;
} sys_event;

sys_event* createNewEvent(event_func fn, uint ac, ...);

void _pushBackEvents(event_func ev, int argCnt, va_list vl); // adds a new event to the bottom of the list
void _clearEvent(uint pos); // sets the event at the position to NULL
sys_event _atEvents(uint pos); // access events at position (with saftey precautions)
void _CleanUpEvents(); // removed all null events
void runEventHandler(); // the funtion for the event handler thread

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
