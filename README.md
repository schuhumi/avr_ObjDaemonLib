avr_ObjDaemonLib
================

This is a more object-orientated background-process-library for AVR Microcontrollers written in C.

This library consits of:

    DAEMON.h
    DAEMON.c

and offers:

    This is a library, that can manage background-processes. This means, it can run functions periodically (the user can adjust the time for each function).
    capabilities:
        - set prefered time period
        - set maximum time period
        - set priorities (nice-value like on Linux)
        - use idle-time (run if no other daemons wait for execution)

    functions:
        DAEMONCONTROLLER_INIT: Init a function, that is periodically called by an interrupt an then checks which functions of the user should be ran
        DAEMONCONTROLLER_PAUSE: No functions will be called until the controller is unpaused
        DAEMONCONTROLLER_RUN: unpause the controller
        DAEMONCONTROLLER_CHANGETO: Change to another daemoncontroller on the fly (which has different settings and other functions registered)
        DAEMONCONTROLLER_LOADAVG: Gives you the load-average of the daemon. A load of ~0.7 is prefered
        DAEMONCONTROLLER_AUTOHEARTBEAT_ACTIVATE: The daemoncontroller automatically adjusts the interrupt-time, so that he reaches a preferd loadaverage
        DAEMONCONTROLLER_AUTOHEARTBEAT_DISABLE: Keep the interrupt-time that is present at this time until manually changed or autoheartbeat is being activated
        
        DAEMON_INIT: Register a function (=daemon) at a daemoncontroller
        DAEMON_KILL: Remove a function (=daemon) from a daemoncontroller


There are examples in main.c, which show the basics.