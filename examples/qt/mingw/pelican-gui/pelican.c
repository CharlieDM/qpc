/*****************************************************************************
* Model: pelican.qm
* File:  ././pelican.c
*
* This file has been generated automatically by QP Modeler (QM).
* DO NOT EDIT THIS FILE MANUALLY.
*
* Please visit www.state-machine.com/qm for more information.
*****************************************************************************/
#include "qp_port.h"
#include "bsp.h"
#include "pelican.h"

Q_DEFINE_THIS_FILE

enum PelicanTimeouts {  /* various timeouts in ticks */
    CARS_GREEN_MIN_TOUT = BSP_TICKS_PER_SEC * 8, /* min green for cars */
    CARS_YELLOW_TOUT = BSP_TICKS_PER_SEC * 3,  /* yellow for cars */
    PEDS_WALK_TOUT   = BSP_TICKS_PER_SEC * 3,  /* walking time for peds */
    PEDS_FLASH_TOUT  = BSP_TICKS_PER_SEC / 5,  /* flashing timeout for peds */
    PEDS_FLASH_NUM   = 5*2,                    /* number of flashes for peds */
    OFF_FLASH_TOUT   = BSP_TICKS_PER_SEC / 2   /* flashing timeout when off */
};

/* Pelican class -----------------------------------------------------------*/
/* @(/1/0) .................................................................*/
/** 
* PEdestrian LIght CONtrolled (PELICAN) crossing
*/
typedef struct PelicanTag {
/* protected: */
    QActive super;

/* private: */
    QTimeEvt timeout;
    uint8_t flashCtr;
} Pelican;

/* protected: */
static QState Pelican_initial(Pelican * const me, QEvt const * const e);
static QState Pelican_operational(Pelican * const me, QEvt const * const e);
static QState Pelican_carsEnabled(Pelican * const me, QEvt const * const e);
static QState Pelican_carsGreen(Pelican * const me, QEvt const * const e);
static QState Pelican_carsGreenNoPed(Pelican * const me, QEvt const * const e);
static QState Pelican_carsGreenInt(Pelican * const me, QEvt const * const e);
static QState Pelican_carsGreenPedWait(Pelican * const me, QEvt const * const e);
static QState Pelican_carsYellow(Pelican * const me, QEvt const * const e);
static QState Pelican_pedsEnabled(Pelican * const me, QEvt const * const e);
static QState Pelican_pedsWalk(Pelican * const me, QEvt const * const e);
static QState Pelican_pedsFlash(Pelican * const me, QEvt const * const e);
static QState Pelican_offline(Pelican * const me, QEvt const * const e);


/* Local objects -----------------------------------------------------------*/
static Pelican l_Pelican; /* the single instance of Pelican active object */

/* Global objects ----------------------------------------------------------*/
QActive * const AO_Pelican = (QActive *)&l_Pelican; /* the opaque pointer */

/* Pelican class definition ------------------------------------------------*/
/* @(/1/2) .................................................................*/
void Pelican_ctor(void) {
    QActive_ctor((QActive *)&l_Pelican, (QStateHandler)&Pelican_initial);
    QTimeEvt_ctor(&l_Pelican.timeout, TIMEOUT_SIG);
}
/* @(/1/0) .................................................................*/
/* @(/1/0/2) ...............................................................*/
/* @(/1/0/2/0) */
static QState Pelican_initial(Pelican * const me, QEvt const * const e) {
    QActive_subscribe((QActive *)me, PEDS_WAITING_SIG);
    QActive_subscribe((QActive *)me, TERMINATE_SIG);

    QS_OBJ_DICTIONARY(&l_Pelican);
    QS_OBJ_DICTIONARY(&l_Pelican.timeout);

    QS_FUN_DICTIONARY(&QHsm_top);
    QS_FUN_DICTIONARY(&Pelican_initial);
    QS_FUN_DICTIONARY(&Pelican_offline);
    QS_FUN_DICTIONARY(&Pelican_operational);
    QS_FUN_DICTIONARY(&Pelican_carsEnabled);
    QS_FUN_DICTIONARY(&Pelican_carsGreen);
    QS_FUN_DICTIONARY(&Pelican_carsGreenNoPed);
    QS_FUN_DICTIONARY(&Pelican_carsGreenPedWait);
    QS_FUN_DICTIONARY(&Pelican_carsGreenInt);
    QS_FUN_DICTIONARY(&Pelican_carsYellow);
    QS_FUN_DICTIONARY(&Pelican_pedsEnabled);
    QS_FUN_DICTIONARY(&Pelican_pedsWalk);
    QS_FUN_DICTIONARY(&Pelican_pedsFlash);

    QS_SIG_DICTIONARY(PEDS_WAITING_SIG, 0);  /* global signals */
    QS_SIG_DICTIONARY(ON_SIG,           0);
    QS_SIG_DICTIONARY(OFF_SIG,          0);

    QS_SIG_DICTIONARY(TIMEOUT_SIG,      &l_Pelican); /* just for Pelican */
    return Q_TRAN(&Pelican_operational);
}
/* @(/1/0/2/1) .............................................................*/
static QState Pelican_operational(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1) */
        case Q_ENTRY_SIG: {
            BSP_signalCars(CARS_RED);
            BSP_signalPeds(PEDS_DONT_WALK);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/0) */
        case Q_INIT_SIG: {
            status = Q_TRAN(&Pelican_carsEnabled);
            break;
        }
        /* @(/1/0/2/1/1) */
        case OFF_SIG: {
            status = Q_TRAN(&Pelican_offline);
            break;
        }
        /* @(/1/0/2/1/2) */
        case TERMINATE_SIG: {
            BSP_terminate(0);
            status = Q_HANDLED();
            break;
        }
        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/3) ...........................................................*/
static QState Pelican_carsEnabled(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/3) */
        case Q_EXIT_SIG: {
            BSP_signalCars(CARS_RED);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/0) */
        case Q_INIT_SIG: {
            status = Q_TRAN(&Pelican_carsGreen);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_operational);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/3/1) .........................................................*/
static QState Pelican_carsGreen(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/3/1) */
        case Q_ENTRY_SIG: {
            BSP_signalCars(CARS_GREEN);
            QTimeEvt_postIn(&me->timeout, (QActive *)me, CARS_GREEN_MIN_TOUT);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/1) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->timeout);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/1/0) */
        case Q_INIT_SIG: {
            status = Q_TRAN(&Pelican_carsGreenNoPed);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_carsEnabled);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/3/1/1) .......................................................*/
static QState Pelican_carsGreenNoPed(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/3/1/1) */
        case Q_ENTRY_SIG: {
            BSP_showState("carsGreenNoPed");
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/1/1/0) */
        case PEDS_WAITING_SIG: {
            status = Q_TRAN(&Pelican_carsGreenPedWait);
            break;
        }
        /* @(/1/0/2/1/3/1/1/1) */
        case TIMEOUT_SIG: {
            status = Q_TRAN(&Pelican_carsGreenInt);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_carsGreen);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/3/1/2) .......................................................*/
static QState Pelican_carsGreenInt(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/3/1/2) */
        case Q_ENTRY_SIG: {
            BSP_showState("carsGreenInt");
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/1/2/0) */
        case PEDS_WAITING_SIG: {
            status = Q_TRAN(&Pelican_carsYellow);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_carsGreen);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/3/1/3) .......................................................*/
static QState Pelican_carsGreenPedWait(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/3/1/3) */
        case Q_ENTRY_SIG: {
            BSP_showState("carsGreenPedWait");
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/1/3/0) */
        case TIMEOUT_SIG: {
            status = Q_TRAN(&Pelican_carsYellow);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_carsGreen);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/3/2) .........................................................*/
static QState Pelican_carsYellow(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/3/2) */
        case Q_ENTRY_SIG: {
            BSP_showState("carsYellow");
            BSP_signalCars(CARS_YELLOW);
            QTimeEvt_postIn(&me->timeout, (QActive *)me, CARS_YELLOW_TOUT);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/2) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->timeout);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/3/2/0) */
        case TIMEOUT_SIG: {
            status = Q_TRAN(&Pelican_pedsEnabled);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_carsEnabled);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/4) ...........................................................*/
static QState Pelican_pedsEnabled(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/4) */
        case Q_EXIT_SIG: {
            BSP_signalPeds(PEDS_DONT_WALK);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/4/0) */
        case Q_INIT_SIG: {
            status = Q_TRAN(&Pelican_pedsWalk);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_operational);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/4/1) .........................................................*/
static QState Pelican_pedsWalk(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/4/1) */
        case Q_ENTRY_SIG: {
            BSP_showState("pedsWalk");
            BSP_signalPeds(PEDS_WALK);
            QTimeEvt_postIn(&me->timeout, (QActive *)me, PEDS_WALK_TOUT);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/4/1) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->timeout);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/4/1/0) */
        case TIMEOUT_SIG: {
            status = Q_TRAN(&Pelican_pedsFlash);
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_pedsEnabled);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/1/4/2) .........................................................*/
static QState Pelican_pedsFlash(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/1/4/2) */
        case Q_ENTRY_SIG: {
            BSP_showState("pedsFlash");
            QTimeEvt_postEvery(&me->timeout, (QActive *)me, PEDS_FLASH_TOUT);
            me->flashCtr = PEDS_FLASH_NUM*2 + 1;
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/4/2) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->timeout);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/1/4/2/0) */
        case TIMEOUT_SIG: {
            /* @(/1/0/2/1/4/2/0/0) */
            if (me->flashCtr != 0) {
                --me->flashCtr;
                /* @(/1/0/2/1/4/2/0/0/0) */
                if ((me->flashCtr & 1) == 0) {
                    BSP_signalPeds(PEDS_DONT_WALK);
                    status = Q_HANDLED();
                }
                /* @(/1/0/2/1/4/2/0/0/1) */
                else {
                    BSP_signalPeds(PEDS_BLANK);
                    status = Q_HANDLED();
                }
            }
            /* @(/1/0/2/1/4/2/0/1) */
            else {
                status = Q_TRAN(&Pelican_carsEnabled);
            }
            break;
        }
        default: {
            status = Q_SUPER(&Pelican_pedsEnabled);
            break;
        }
    }
    return status;
}
/* @(/1/0/2/2) .............................................................*/
static QState Pelican_offline(Pelican * const me, QEvt const * const e) {
    QState status;
    switch (e->sig) {
        /* @(/1/0/2/2) */
        case Q_ENTRY_SIG: {
            BSP_showState("offline");
            QTimeEvt_postEvery(&me->timeout, (QActive *)me, OFF_FLASH_TOUT);
            me->flashCtr = 0;
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/2) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->timeout);
            status = Q_HANDLED();
            break;
        }
        /* @(/1/0/2/2/0) */
        case TIMEOUT_SIG: {
            me->flashCtr ^= 1;
            /* @(/1/0/2/2/0/0) */
            if ((me->flashCtr & 1) == 0) {
                BSP_signalCars(CARS_RED);
                BSP_signalPeds(PEDS_DONT_WALK);
                status = Q_HANDLED();
            }
            /* @(/1/0/2/2/0/1) */
            else {
                BSP_signalCars(CARS_BLANK);
                BSP_signalPeds(PEDS_BLANK);
                status = Q_HANDLED();
            }
            break;
        }
        /* @(/1/0/2/2/1) */
        case ON_SIG: {
            status = Q_TRAN(&Pelican_operational);
            break;
        }
        /* @(/1/0/2/2/2) */
        case TERMINATE_SIG: {
            QF_stop();
            status = Q_HANDLED();
            break;
        }
        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status;
}
