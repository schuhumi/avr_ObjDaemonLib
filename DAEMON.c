#include <avr/io.h>
#include <avr/interrupt.h>
#include "DAEMON.h"
#include "math.h"

static type_DAEMON				autoHeartBeat;

void daemonHandler_autoHeartBeat (void)
{
	float loadaverage = DAEMONCONTROLLER_LOADAVG();
	float newheartbeat = (ActiveDaemonController->heartbeat_us)*((ActiveDaemonController->preferedLoadAvg) / loadaverage);
	ActiveDaemonController->heartbeat_us = ((ActiveDaemonController->heartbeat_us)*3+newheartbeat) / 4;
	DAEMONCONTROLLER_TIMERCTRL(ActiveDaemonController);
}

void DAEMONCONTROLLER_TIMERCTRL(type_DAEMONCONTROLLER *controller)
{
	/** prescaler_select equals:  0            1  2  3   4    5   **/
	uint16_t prescaler_list[7] = {1 /*dummy*/, 1, 8, 64, 256, 1024};
	
	uint8_t prescalerFound = False;
	uint8_t prescaler_selected;
	for(prescaler_selected = 1; prescaler_selected <5; prescaler_selected++)
	{
		
		float temp = 1000000;				// 1000000 wegen µSekunden
		temp /= F_CPU;
		temp *= pow(2,TIMER_BITS); // 1<<TIMER_BITS nicht mögliche, falsches Ergebnis
		temp *= prescaler_list[prescaler_selected];
		if(temp > (controller->heartbeat_us))
		{
			prescalerFound = True;
			break;
		}
	}
	
	if(!prescalerFound)
	{
		// ERROR!! Kein Prescaler reicht!	
		// Benutze 1024, langsamer geht es nicht:
		prescaler_selected = 5;
		ICR1 = pow(2,TIMER_BITS)-1;										// Timer erst beim Ende seines Maximalwertes zurücksetzen
	}
	else
	{
		// prescaler_selected beibehalten
		float temp;
		temp = F_CPU;
		temp /= prescaler_list[prescaler_selected];
		temp = 1000000/temp;	// wegen µSekunden
		temp = (controller->heartbeat_us)/temp;
		OCR1A = temp;
	}
	
	/** Timer Mode:
	 * Clear Timer on Compare Match (CTC) Mode
	 * (Datasheet Atmega32 : Page 98) **/
	 
	TCCR1A =	(0<< COM1A0)	\
			|	(0<< COM1A1)	\
			|	(0<< FOC1A)		\
			|	(0<< FOC1B)		\
			|	(0<< WGM10)		\
			|	(0<< WGM11);
	TCCR1B =	(1<< WGM12)		\
			|	(0<< WGM13)		\
			|	(0<< ICNC1)		\
			|	(0<< ICES1)		\
			|	(((prescaler_selected&0b001)?1:0)<< CS10)	\
			|	(((prescaler_selected&0b010)?1:0)<< CS11)	\
			|	(((prescaler_selected&0b100)?1:0)<< CS12);

	TCNT1 = 0;															// Timer auf 0 zurücksetzen

}

void DAEMONCONTROLLER_RUN(type_DAEMONCONTROLLER *controller)
{
	// isr aktivieren
	TIMSK |= (1 << OCIE1A);
	sei();
}

void DAEMONCONTROLLER_AUTOHEARTBEAT_ACTIVATE (void)
{
	autoHeartBeat.handler = daemonHandler_autoHeartBeat;
	autoHeartBeat.interval_us = 100000;
	autoHeartBeat.interval_max_us = 200000;
	
	DAEMON_INIT(ActiveDaemonController, &autoHeartBeat);
	DAEMON_SETSTATUS(ActiveDaemonController, &autoHeartBeat, STATUS_RUNNING);
}

void DAEMONCONTROLLER_AUTOHEARTBEAT_DISABLE (void)
{
	DAEMON_KILL(ActiveDaemonController, &autoHeartBeat);
}

void DAEMONCONTROLLER_PAUSE(type_DAEMONCONTROLLER *controller)
{
	TIMSK &= ~(1 << OCIE1A);											// Interrupt für Timer 1 sperren
}

void DAEMONCONTROLLER_CHANGETO(type_DAEMONCONTROLLER *controller)
{
	if(ActiveDaemonController->auto_heartbeat)
			DAEMONCONTROLLER_AUTOHEARTBEAT_DISABLE();
	DAEMONCONTROLLER_PAUSE(ActiveDaemonController);
	ActiveDaemonController = controller;
	DAEMONCONTROLLER_TIMERCTRL(ActiveDaemonController);
	if(ActiveDaemonController->auto_heartbeat)
			DAEMONCONTROLLER_AUTOHEARTBEAT_ACTIVATE();
	DAEMONCONTROLLER_RUN(ActiveDaemonController);
}

void DAEMONCONTROLLER_INIT(type_DAEMONCONTROLLER *controller, uint8_t isFirstController, uint8_t startNow)
{
	for(uint8_t foo=0; foo<maxDaemons; foo++)
		controller->DaemonsList[foo] = 0;


	if(isFirstController)
	{
		/// Initialize ringbuffer for loadaverage
		for (uint8_t foo=0; foo<LOADAVG_SAMPLES-1; foo++)
			load_ringbuffer[foo] = 1;
		load_ringbufferPos = 0;
		
		ActiveDaemonController = controller;
	}
	if(startNow && isFirstController)
	{
		DAEMONCONTROLLER_TIMERCTRL(controller);
		if(ActiveDaemonController->auto_heartbeat)
			DAEMONCONTROLLER_AUTOHEARTBEAT_ACTIVATE();
		DAEMONCONTROLLER_RUN(controller);
	}
	else if(startNow && !isFirstController)
		DAEMONCONTROLLER_CHANGETO(controller);
}


/// Calculate loadaverage of the daemoncontroller ///
float DAEMONCONTROLLER_LOADAVG(void)
{
	float average = 0;
	for (uint8_t foo=0; foo<LOADAVG_SAMPLES; foo++)
			average += load_ringbuffer[foo];
	average /= LOADAVG_SAMPLES;
	return average;
}

///***********************************************************************///
/// In jedem Durchgang werden alle Daemons, deren interval_max_us			///
/// in der nächsten Runde überrschritten würde, abgearbeitet.				///
/// Zusätzlich wird in jeder Runde ein Daemon abgearbeitet, dessen			///
/// interval_us in der nächsten Runde ablaufen würde. Oder, falls 			///
/// kein Daemon wartet, einer, der übliche Rechenzeit nutzen möchte 		///
/// (useIdle). Es wird jeweils der Daemon mit der höchsten Priorität 		///
/// gewählt (kleinster nice-Wert von +20 bis -20 (siehe Linux))				///
///***********************************************************************///
ISR(TIMER1_COMPA_vect)
{
	
	int8_t daemonNr;
	int8_t daemonNrWaiting = -1;
	int8_t daemonNrWaitingNice = 20;		/// 20 ist der größtmögliche Nice-Wert, es wird nach dem niedrigsten gesucht
	int8_t daemonNrUsingIdle = -1;
	int8_t daemonNrUsingIdleNice = 20;
	int8_t NrOfDaemonsProcessed = 0;
	for(daemonNr=0; daemonNr<maxDaemons; daemonNr++)
	{
		if(ActiveDaemonController->DaemonsList[daemonNr] != 0)
		{
			if( (ActiveDaemonController->DaemonsList[daemonNr]->status) != STATUS_RUNNING)
				continue; 	/// Daemon ist nicht auf "RUNNING" gestellt -> überspringen
			if( ((ActiveDaemonController->DaemonsList[daemonNr]->__timeSinceLastRun) \
					+ ActiveDaemonController->heartbeat_us) > (ActiveDaemonController->DaemonsList[daemonNr]->interval_max_us) )
			{		/// Muss jetzt abgearbeitet werden, da sonst interval_max_us abläuft
				ActiveDaemonController->DaemonsList[daemonNr]->handler();
				ActiveDaemonController->DaemonsList[daemonNr]->__timeSinceLastRun = 0;
				NrOfDaemonsProcessed++;
			}
			else if( ((ActiveDaemonController->DaemonsList[daemonNr]->__timeSinceLastRun) \
					+ ActiveDaemonController->heartbeat_us) > (ActiveDaemonController->DaemonsList[daemonNr]->interval_us) )
			{		/// interval_us läuft in der nächsten Runde ab, es wird nach Daemon mit der größten Priorität gesucht
				if( (ActiveDaemonController->DaemonsList[daemonNr]->nice) < daemonNrWaitingNice)
				{
					daemonNrWaiting = daemonNr;
					daemonNrWaitingNice = (ActiveDaemonController->DaemonsList[daemonNr]->nice);
				}
			}
			else if(ActiveDaemonController->DaemonsList[daemonNr]->useIdle)
			{		/// Daemon suchen, der ausgeführt wird, falls nichts zu tun ist
				if((ActiveDaemonController->DaemonsList[daemonNr]->nice) < daemonNrUsingIdleNice)
				{
					daemonNrUsingIdle = daemonNr;
					daemonNrUsingIdleNice = (ActiveDaemonController->DaemonsList[daemonNr]->nice);
				}
			}
			else
			{		/// Noch kein Bedarf um ausgeführt zu werden
				ActiveDaemonController->DaemonsList[daemonNr]->__timeSinceLastRun += ActiveDaemonController->heartbeat_us;
			}
			
		}
	}
	
	if(daemonNrWaiting != -1) /// Es wurde ein Daemon gefunden, der wieder mal ausgeführt werden möchte
	{
		ActiveDaemonController->DaemonsList[daemonNrWaiting]->handler();
		ActiveDaemonController->DaemonsList[daemonNrWaiting]->__timeSinceLastRun = 0;
		NrOfDaemonsProcessed++;
	}
	else if (daemonNrUsingIdle != -1) /// Es wurde ein Daemon gefunden, der ausgeführt wird, falls nichts zu tun ist
	{
		ActiveDaemonController->DaemonsList[daemonNrUsingIdle]->handler();
		ActiveDaemonController->DaemonsList[daemonNrUsingIdle]->__timeSinceLastRun = 0;
		NrOfDaemonsProcessed++;
	}
	
	load_ringbuffer[load_ringbufferPos] = NrOfDaemonsProcessed;
	load_ringbufferPos++;
	if(load_ringbufferPos >= LOADAVG_SAMPLES)
		load_ringbufferPos = 0;
	
}


/// DAEMONS ///////////////////////////////////////////////////////////////////////////////////////////////

void DAEMON_SETSTATUS(type_DAEMONCONTROLLER *controller,type_DAEMON *daemon, uint8_t status)
{
	int8_t daemonNr;
	for(daemonNr=0; daemonNr<maxDaemons; daemonNr++)
		if(daemon == (controller->DaemonsList[daemonNr]))
			break;														// Daemon found
	if(daemonNr == maxDaemons)
	{
		// Daemon not found! Quit
		return;
	}
	controller->DaemonsList[daemonNr]->status = status;	
}

uint8_t DAEMON_INIT(type_DAEMONCONTROLLER *controller, type_DAEMON *daemon)
{
	int8_t daemonNr;
	for(daemonNr=0; daemonNr<maxDaemons; daemonNr++)
		if((controller->DaemonsList[daemonNr]) == 0)
			break;														// Free space for Daemon found
	if(daemonNr == maxDaemons)
	{
		// No space for more Daemons left! Quit
		return Error;
	}
	daemon->status = STATUS_PAUSED;
	controller->DaemonsList[daemonNr] = daemon;							// Save pointer to daemon in free space
	return Success;
}

void DAEMON_KILL(type_DAEMONCONTROLLER *controller, type_DAEMON *daemon)
{
	int8_t daemonNr;
	for(daemonNr=0; daemonNr<maxDaemons; daemonNr++)
		if(daemon == (controller->DaemonsList[daemonNr]))
			break;														// Daemon found
	if(daemonNr == maxDaemons)
	{
		// Daemon not found! Quit
		return;
	}
	controller->DaemonsList[daemonNr] = 0;								// Set pointer to 0 -> Daemoncontroller ignores it
	daemon->status = STATUS_KILLED;
}
