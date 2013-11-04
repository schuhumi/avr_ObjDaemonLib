#ifndef _DAEMON_h_
#define _DAEMON_h_



typedef struct {
	void		(*handler) (void);										// Unterprogramm, in dem der Quellcode für den Daemon steht
	float	interval_us;												// Gewünschte Wiederholungszeit
	float	interval_max_us;											// Nach spätestens dieser Zeit MUSS der Daemon wieder laufen
	uint8_t	useIdle;												// Wenn keine anderen Daemons Schlange stehen diesen ausführen 
	uint8_t	nice;													// Priorität des Daemons
	uint8_t	status;													// Daemon läuft/ist pausiert etc. (Siehe "#define STATUS_xxxx x")
	float __timeSinceLastRun;											// Wie lange es her ist, dass der Daemon das letzte mal gelaufen ist
} type_DAEMON;

#define	maxDaemons			10											// Anzahl der Daemons, die der Controller verwalten kann
																		// (Kann nicht zur Laufzeit bestimmt werden, das dies die Größe
																		//  des Arrays für die Pointer auf die Daemons beeinflusst)


typedef struct {
	uint16_t	heartbeat_us;											// Zeitabstand, in denen der Controller einen Daemon aufruft
	uint8_t	auto_heartbeat;											// Automatische Anpassung des Hearbeat Ja/Nein
	uint8_t	heartbeat_min_us;										// Minimaler Zeitabstand, in dem ein Daemon gestartet wird
																		// (Sonst kommt irgendwas das eigentliche Programm zu kurz)
	float 		preferedLoadAvg;										// Wie groß die ideale Auslastung des Daemoncontrollers ist (für Heartbeat Anpassung)		
	type_DAEMON *DaemonsList[maxDaemons];
} type_DAEMONCONTROLLER;

void DAEMONCONTROLLER_INIT(type_DAEMONCONTROLLER *controller, uint8_t isFirstController, uint8_t startNow);
void DAEMONCONTROLLER_TIMERCTRL(type_DAEMONCONTROLLER *controller);
void DAEMONCONTROLLER_PAUSE(type_DAEMONCONTROLLER *controller);
void DAEMONCONTROLLER_RUN(type_DAEMONCONTROLLER *controller);
void DAEMONCONTROLLER_CHANGETO(type_DAEMONCONTROLLER *controller);
float DAEMONCONTROLLER_LOADAVG(void);
void DAEMONCONTROLLER_AUTOHEARTBEAT_ACTIVATE (void);
void DAEMONCONTROLLER_AUTOHEARTBEAT_DISABLE (void);

uint8_t DAEMON_INIT(type_DAEMONCONTROLLER *controller, type_DAEMON *daemon);
void DAEMON_KILL(type_DAEMONCONTROLLER *controller, type_DAEMON *daemon);
void DAEMON_SETSTATUS(type_DAEMONCONTROLLER *controller,type_DAEMON *daemon, uint8_t status);



type_DAEMONCONTROLLER *ActiveDaemonController;

#define	TIMER_BITS			16											// Timerauflösung

#define LOADAVG_SAMPLES		10
uint8_t load_ringbuffer[LOADAVG_SAMPLES];
uint8_t load_ringbufferPos;


#define STATUS_RUNNING		0
#define STATUS_WAITING		1
#define STATUS_PAUSED		2
#define STATUS_KILLED		3

#define True	1
#define False	0

#define	Success	0
#define Error	1

#endif
