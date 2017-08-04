/**
 *  \file test_InyControl.c
 */

/* -------------------------- Development history -------------------------- */
/*
 *  2017.06.21  LeFr  v1.0.00  Initial version
 *  2017.07.07  Daba  v1.1.00  No fixture
 */

/* -------------------------------- Authors -------------------------------- */
/*
 *  LeFr  Leandro Francucci  francuccilea@gmail.com
 *  DaBa  Dario Baliña  	 dariosb@gmail.com
 */

/* --------------------------------- Notes --------------------------------- */
/*     Test de estructura SM inyeccion de motor segun [AUTO-ER-0001]
 *
 *	[REQ0004] El software debe variar el ciclo de trabajo de la señal PWM 
 *            de 1 KHz de frecuencia, que enviará hacia el inyector en función
 *            de los valores leídos de: el sensor de revoluciones por minuto,
 *            el sensor de posición de mariposa de admisión, 
 *            y el sensor de temperatura de motor. 
 *  [REQ0005] El software debe fijar un ciclo de trabajo de 50% en el PWM 
 *            que envía hacia el inyector, durante 2 segundos luego de la
 *            señal de arranque del motor. 
 *  [REQ0006] El software debe fijar un ciclo de trabajo mínimo de entre 
 *            el 20% y el 30% en el PWM que envía hacia el inyector cuando 
 *            el motor se encuentra “regulando”, para asegurar que el motor
 *            genere 2000 RPM +/- 20 RPM.
 *  [REQ0007] El software debe fijar un ciclo de trabajo entre 30% y 80% en
 *            el PWM que envía hacia el inyector en función proporcionalmente
 *            lineal en que la mariposa de admisión se encuentre abierta.
 *            (Nota: El pedal de aceleración del conductor se conecta 
 *            mecánicamente con la mariposa de admisión. Cuando el pedal
 *            se encuentra sin presionar, la mariposa queda un 30% abierta.
 *            Cuando se presiona a fondo el pedal, la mariposa se abre un 80%.
 *            El ciclo de trabajo del PWM del inyector debe ser linealmente
 *            proporcional con el grado de apertura de dicha mariposa).
 *  [REQ0008] Si el software detecta que la temperatura del motor es menor
 *            a 70C, incrementará en 10% el ciclo de trabajo en el PWM que
 *            ha determinado aplicar al inyector.
 */

/* ----------------------------- Include files ----------------------------- */
#include "unity.h"
#include "unitrazer.h"
#include "common.h"
#include "InyControl.h"
#include "InyControlEvt.h"
#include "Mock_InyControlAct.h"
#include "Mock_rkhassert.h"
//#include "Mock_rkhtrc_out.h"

#include "rkhfwk_dynevt.h"
#include "rkhsm.h"
#include "rkhtrc.h"
#include "rkhtrc_filter.h"
#include "rkhtrc_record.h"
#include "rkhfwk_bittbl.h"
#include "rkhport.h"
#include "rkhtrc_stream.h"

#include "unitrzlib.h"
#include "tzlink.h"
#include "tzparse.h"

/* ----------------------------- Local macros ------------------------------ */
/* ------------------------------- Constants ------------------------------- */
/* ---------------------------- Local data types --------------------------- */
/* ---------------------------- Global variables --------------------------- */
/* ---------------------------- Local variables ---------------------------- */
static RKH_ST_T *state, *expectNextState;

static RKH_SMA_T *sma;
static InyControl *ic;
static RKH_SM_T *sm;

/* ----------------------- Local function prototypes ----------------------- */
/* ---------------------------- Local functions ---------------------------- */
static void
setProfile(RKH_ST_T *currState, RKH_ST_T *nextState)
{
    setState((RKH_SMA_T *)inyControl, currState);
    expectNextState = nextState;
}

/* ---------------------------- Global functions --------------------------- */
void
setUp(void)
{
	int argc=0;
	char *argv[]={"programName", NULL};

	sma = inyControl;
	ic = (InyControl *)inyControl;
	sm = (RKH_SM_T *)inyControl;

	tzlink_open(argc, argv);
    sm_init(); 
    Mock_InyControlAct_Init();
    Mock_rkhassert_Init();
//    Mock_rkhtrc_out_Init();

    RKH_TR_FWK_AO(inyControl);
    RKH_TR_FWK_STATE(inyControl, &off);
    RKH_TR_FWK_STATE(inyControl, &starting);
    RKH_TR_FWK_STATE(inyControl, &normal);
    RKH_TR_FWK_SIG(evStart);
    RKH_TR_FWK_SIG(evStartTimeout);
    RKH_TR_FWK_SIG(evTick);

	RKH_FILTER_OFF_ALL_SIGNALS();
    RKH_FILTER_OFF_GROUP_ALL_EVENTS(RKH_TG_SM);
    RKH_FILTER_OFF_SMA(inyControl);
    RKH_FILTER_OFF_EVENT(RKH_TE_FWK_ASSERT);

	sm_trn_ignore();
	sm_tsState_ignore();
	sm_ntrnact_ignore();
	sm_nenex_ignore();
	sm_evtProc_ignore();
}

void
tearDown(void)
{
    sm_verify();
    Mock_InyControlAct_Verify();
    Mock_rkhassert_Verify();

    sm_cleanup();
    Mock_InyControlAct_Destroy();
    Mock_rkhassert_Destroy();
}

void
test_DefaultStateAfterInit(void)
{
	sm_init_expect(RKH_STATE_CAST(&off));
    sm_enstate_expect(RKH_STATE_CAST(&off));
    InyControlAct_init_Expect(ic);

    rkh_sm_init(sm);

    TEST_ASSERT_EQUAL(UT_PROC_SUCCESS, unitrazer_getLastOut()->status);
}

void
test_AnUnhandledEventDoesNotChangeState(void)
{
	sm_dch_expect(evStartTimeout, RKH_STATE_CAST(&off));
	sm_evtNotFound_expect(evStartTimeout);

    rkh_sm_dispatch(sm, &e_StartTimeout);

    TEST_ASSERT_EQUAL(UT_PROC_SUCCESS, unitrazer_getLastOut()->status);
}

void
test_StateTransitionTableForOff(void)
{
    setProfile(RKH_STATE_CAST(&off), RKH_STATE_CAST(&starting));
	sm_dch_expect(evStart, RKH_STATE_CAST(&off));
	sm_exstate_expect(RKH_STATE_CAST(&off));
	sm_enstate_expect(RKH_STATE_CAST(&starting));
	sm_state_expect(RKH_STATE_CAST(&starting));
    InyControlAct_starting_Expect(ic);

    rkh_sm_dispatch(sm, &e_Start);

    TEST_ASSERT_EQUAL(UT_PROC_SUCCESS, unitrazer_getLastOut()->status);
}

void
test_StateTransitionTableForStarting(void)
{
    setProfile(RKH_STATE_CAST(&starting), RKH_STATE_CAST(&idleSpeed));
	sm_dch_expect(evStartTimeout, RKH_STATE_CAST(&starting));
	sm_exstate_expect(RKH_STATE_CAST(&starting));
	sm_enstate_expect(RKH_STATE_CAST(&idleSpeed));
	sm_state_expect(RKH_STATE_CAST(&idleSpeed));

    rkh_sm_dispatch(sm, &e_StartTimeout);

    TEST_ASSERT_EQUAL(UT_PROC_SUCCESS, unitrazer_getLastOut()->status);
}

#if 0
void
test_StateTransitionTableForIdleSpeed(void)
{
    RKH_SMA_T *sma = inyControl;
    InyControl *const ic = (InyControl *const)inyControl;
    RKH_SM_T *sm = (RKH_SM_T *)inyControl;

    setProfile(RKH_STATE_CAST(&idleSpeed), RKH_STATE_CAST(&idleSpeed));

    InyControlAct_isRelThrottle_ExpectAndReturn(ic, RKH_TRUE);
    InyControlAct_onIdleSpeed_Expect(ic);

    rkh_sm_dispatch(sm, &e_Tick);
    TEST_ASSERT_EQUAL(expectNextState, getState(sma));

    setProfile(RKH_STATE_CAST(&idleSpeed), RKH_STATE_CAST(&normal));
    InyControlAct_isRelThrottle_ExpectAndReturn(ic, RKH_FALSE);

    rkh_sm_dispatch(sm, &e_Tick);
    TEST_ASSERT_EQUAL(expectNextState, getState(sma));
}

void
test_StateTransitionTableForNormal(void)
{
    RKH_SMA_T *sma = inyControl;
    InyControl *const ic = (InyControl *const)inyControl;
    RKH_SM_T *sm = (RKH_SM_T *)inyControl;

    setProfile(RKH_STATE_CAST(&normal), RKH_STATE_CAST(&normal));

    InyControlAct_isPressThrottle_ExpectAndReturn(ic, RKH_TRUE);
    InyControlAct_onNormal_Expect(ic);

    rkh_sm_dispatch(sm, &e_Tick);
    TEST_ASSERT_EQUAL(expectNextState, getState(sma));

    setProfile(RKH_STATE_CAST(&normal), RKH_STATE_CAST(&idleSpeed));
    InyControlAct_isPressThrottle_ExpectAndReturn(ic, RKH_FALSE);

    rkh_sm_dispatch(sm, &e_Tick);
    TEST_ASSERT_EQUAL(expectNextState, getState(sma));
}

void
test_StateTransitionTableForNormal_UnhandledEvents(void)
{
    RKH_SMA_T *sma = inyControl;
    InyControl *const ic = (InyControl *const)inyControl;
    RKH_SM_T *sm = (RKH_SM_T *)inyControl;

    setProfile(RKH_STATE_CAST(&normal), RKH_STATE_CAST(&normal));

    rkh_sm_dispatch(sm, &e_Start);
    TEST_ASSERT_EQUAL(expectNextState, getState(sma));

    setProfile(RKH_STATE_CAST(&normal), RKH_STATE_CAST(&normal));

    rkh_sm_dispatch(sm, &e_StartTimeout);
    TEST_ASSERT_EQUAL(expectNextState, getState(sma));
}
#endif
/* ------------------------------ File footer ------------------------------ */
