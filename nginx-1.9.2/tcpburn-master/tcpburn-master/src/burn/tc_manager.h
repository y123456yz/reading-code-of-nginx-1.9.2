#ifndef  TC_MANAGER_INCLUDED
#define  TC_MANAGER_INCLUDED

#include <xcopy.h>
#include <burn.h>

int  burn_init(tc_event_loop_t *event_loop);
void burn_over(const int sig);
void tc_release_resources();

#endif   /* ----- #ifndef TC_MANAGER_INCLUDED ----- */

