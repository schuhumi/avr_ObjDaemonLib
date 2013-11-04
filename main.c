#include <avr/io.h>
#include "DAEMON.h"

/* This is a small example for the use of daemons:
 * 
 * Image you have a dangerous machine, that has a OK-LED and a ERROR-LED as well as a speaker for alarming.
 * Now you need to check if the system is ok every 5ms, and if not, raise the alarm:
 * 
 * (BTW: I wouldn't recommed this buggy code for a dangerous machine yet ;-) )
 */
 
void daemonHandler_LED_OK (void);
void daemonHandler_CHECKSYSTEM (void);
void daemonHandler_LED_ERROR (void);
void daemonHandler_ALARMSPEAKER (void);

type_DAEMONCONTROLLER 	DaemonCtrl_Normal;
type_DAEMONCONTROLLER	DaemonCtrl_Error;

type_DAEMON 				Daemon_LED_OK;
type_DAEMON				Daemon_CHECKSYSTEM;
type_DAEMON 				Daemon_LED_ERROR;
type_DAEMON 				Daemon_ALARMSPEAKER;

int main (void)
{	
	/// Configure Devices //////////////////////////////////////
	// Daemon-controller Normal configuration
		DaemonCtrl_Normal.heartbeat_us = 200;			// Daemoncontroller runs every 200 microseconds
		DaemonCtrl_Normal.preferedLoadAvg = 0.7;		// At 70% of all runs of the Daemoncontroller, he has to manage something 
														//   (is prepared for more load, but not to much resources wasted)
		DaemonCtrl_Normal.auto_heartbeat = True;		// Daemoncontroller adjusts heartbeat itself, so that he reaches a loagaverage of 0.7
		
	// Daemon-controller Error configuration
		DaemonCtrl_Error.heartbeat_us = 200;
		DaemonCtrl_Error.preferedLoadAvg = 0.7;
		DaemonCtrl_Error.auto_heartbeat = True;

	// LED OK daemon configuration
		Daemon_LED_OK.handler = daemonHandler_LED_OK;
		Daemon_LED_OK.interval_us =     500000;		// routine "daemonHandler_LED_OK" is called every ca. 500ms (nice relaxed blinking)
		Daemon_LED_OK.interval_max_us = 800000;		// after 800ms it must be run, no matter how bussy the system is
		
	// CHECKSYSTEM daemon configuration
		Daemon_CHECKSYSTEM.handler = daemonHandler_CHECKSYSTEM;
		Daemon_CHECKSYSTEM.interval_us =     5000;	// Check system every 5ms
		Daemon_CHECKSYSTEM.interval_max_us = 6000;	// When the system is not checked for >6ms, it could get risky
		
	// LED ERROR daemon configuration
		Daemon_LED_ERROR.handler = daemonHandler_LED_ERROR;
		Daemon_LED_ERROR.interval_us =     200000;	// Faster, alarming blinking	
		Daemon_LED_ERROR.interval_max_us = 300000;	
		
	// ALARMSPEAKER daemon configuration
		Daemon_ALARMSPEAKER.handler = daemonHandler_ALARMSPEAKER;
		Daemon_ALARMSPEAKER.interval_us =     500;	// Invert speaker-signal every 0.5ms, so one period is 1ms -> 1khz sound
		Daemon_ALARMSPEAKER.interval_max_us = 550;	
		
	/// END Configure Devices //////////////////////////////////
	
	/// INIT Devices ///////////////////////////////////////////
	// Init daemon controllers
		//                    &Controller       , is first Daeoncontroller, autostart
		DAEMONCONTROLLER_INIT(&DaemonCtrl_Normal, True                    , True     );		// Start with normal operation
		DAEMONCONTROLLER_INIT(&DaemonCtrl_Error , False                   , False    );
	
	// Init daemons for normal operation (Normal-Daemoncontroller)
		//               &Controller       , &Daemon
		DAEMON_INIT(     &DaemonCtrl_Normal, &Daemon_LED_OK);
		DAEMON_SETSTATUS(&DaemonCtrl_Normal, &Daemon_LED_OK, STATUS_RUNNING);
		
		DAEMON_INIT(     &DaemonCtrl_Normal, &Daemon_CHECKSYSTEM);
		DAEMON_SETSTATUS(&DaemonCtrl_Normal, &Daemon_CHECKSYSTEM, STATUS_RUNNING);
		
	// Init daemons for critical situation (Error-Daemoncontroller)
		DAEMON_INIT(     &DaemonCtrl_Error , &Daemon_LED_ERROR);
		DAEMON_SETSTATUS(&DaemonCtrl_Error , &Daemon_LED_ERROR, STATUS_RUNNING);	// Won't start now, because DaemonCtrl_Error isn't started yet
		
		DAEMON_INIT(     &DaemonCtrl_Error , &Daemon_ALARMSPEAKER);
		DAEMON_SETSTATUS(&DaemonCtrl_Error , &Daemon_ALARMSPEAKER, STATUS_RUNNING); // Won't start now, because DaemonCtrl_Error isn't started yet
	
	/// END INIT Devices ///////////////////////////////////////

	while(1)
	{
		// code here will be run normally (only slower, because µC has to handle daemons at the same time)
		// -> Here you can maintain user input etc. without bothering about the every-5ms-systemcheck
	}

	return 0;
}

void daemonHandler_LED_OK (void)
{
	// toggle green LED OK here
}

void daemonHandler_CHECKSYSTEM (void)
{
	if(1) //if(system is ok)
	{
		return;
	}
	else
	{
		// Change active Daemoncontroller on the fly:
		DAEMONCONTROLLER_CHANGETO(&DaemonCtrl_Error);
		// This does the same as:
		// DAEMONCONTROLLER_PAUSE(&DaemonCtrl_Normal);
		// DAEMONCONTROLLER_RUN(&DaemonCtrl_Error);
	}
}

void daemonHandler_LED_ERROR (void)
{
	// toggle red LED ERROR here
}

void daemonHandler_ALARMSPEAKER (void)
{
	// toggle pin with speaker on it
}
