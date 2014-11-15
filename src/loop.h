#ifndef COAL_LOOP_H
#define COAL_LOOP_H

void loop_mark_start( void );
void loop_mark_done( void );

void loop_end( char *reason );
void loop_kill( int sig );
void loop_run( void );


#endif
