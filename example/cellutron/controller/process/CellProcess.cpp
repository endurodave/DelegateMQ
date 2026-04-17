#include "CellProcess.h"
#include "messages/RunStatusMsg.h"
#include "Constants.h"
#include "actuators/Actuators.h"
#include "sensors/Sensors.h"
#include <iostream>
#include <cstdio>

using namespace dmq;
using namespace cellutron;

CellProcess::CellProcess(Centrifuge& centrifuge) :
    StateMachine(ST_MAX_STATES),
    m_centrifuge(centrifuge)
{
}

CellProcess::~CellProcess()
{
}

void CellProcess::SetThread(dmq::IThread& thread)
{
    StateMachine::SetThread(thread);
    m_centrifugeConn = m_centrifuge.OnTargetReached.Connect(dmq::MakeDelegate(this, &CellProcess::OnCentrifugeTargetReached, thread));
}

void CellProcess::StartProcess()
{
    m_abortPending = false;
    BEGIN_TRANSITION_MAP(CellProcess, StartProcess)
        TRANSITION_MAP_ENTRY(ST_FILL_SOLUTION_A) // ST_IDLE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_SPIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_DRAIN
        TRANSITION_MAP_ENTRY(ST_FILL_SOLUTION_A) // ST_COMPLETE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_FAULT (Locked)
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::GenerateFault()
{
    this->FaultEvent();
}

void CellProcess::FaultEvent()
{
    BEGIN_TRANSITION_MAP(CellProcess, FaultEvent)
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_SPIN
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_DRAIN
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_COMPLETE
        TRANSITION_MAP_ENTRY(ST_FAULT) // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_FAULT
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::AbortProcess()
{
    m_abortPending = true;
    m_centrifuge.StopRamp();

    BEGIN_TRANSITION_MAP(CellProcess, AbortProcess)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_ABORTING)        // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(ST_ABORTING)        // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(ST_ABORTING)        // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(ST_ABORTING)        // ST_SPIN
        TRANSITION_MAP_ENTRY(ST_ABORTING)        // ST_DRAIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_COMPLETE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_FAULT
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::OnCentrifugeTargetReached(uint16_t rpm)
{
    this->CentrifugeReached(rpm);
}

void CellProcess::CentrifugeReached(uint16_t rpm)
{
    if (rpm > 0)
    {
        BEGIN_TRANSITION_MAP(CellProcess, CentrifugeReached, rpm)
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_IDLE
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_A
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_B
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_CELLS
            TRANSITION_MAP_ENTRY(ST_DRAIN)       // ST_SPIN
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_DRAIN
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_COMPLETE
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_ABORTING
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FAULT
        END_TRANSITION_MAP(nullptr)
    }
    else // rpm == 0
    {
        BEGIN_TRANSITION_MAP(CellProcess, CentrifugeReached, rpm)
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_IDLE
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_A
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_B
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_CELLS
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_SPIN
            TRANSITION_MAP_ENTRY(ST_COMPLETE)    // ST_DRAIN
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_COMPLETE
            TRANSITION_MAP_ENTRY(ST_IDLE)        // ST_ABORTING
            TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FAULT
        END_TRANSITION_MAP(nullptr)
    }
}

STATE_DEFINE(CellProcess, Idle, NoEventData)
{
    printf("CellProcess: ST_IDLE\n");
    m_abortPending = false;
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::IDLE });
}

STATE_DEFINE(CellProcess, FillSolutionA, NoEventData)
{
    if (m_abortPending) return;

    printf("CellProcess: ST_FILL_SOLUTION_A\n");
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::PROCESSING });
    
    Actuators::GetInstance().SetValve(1, true);
    Actuators::GetInstance().SetPump(1, 50);
    Sensors::GetInstance().IsAirInLine();
    Actuators::GetInstance().SetPump(1, 0);
    Actuators::GetInstance().SetValve(1, false);

    if (!m_abortPending) InternalEvent(ST_FILL_SOLUTION_B);
}

STATE_DEFINE(CellProcess, FillSolutionB, NoEventData)
{
    if (m_abortPending) return;

    printf("CellProcess: ST_FILL_SOLUTION_B\n");
    Actuators::GetInstance().SetValve(2, true);
    Actuators::GetInstance().SetPump(1, 50);
    Actuators::GetInstance().SetPump(1, 0);
    Actuators::GetInstance().SetValve(2, false);

    if (!m_abortPending) InternalEvent(ST_FILL_CELLS);
}

STATE_DEFINE(CellProcess, FillCells, NoEventData)
{
    if (m_abortPending) return;

    printf("CellProcess: ST_FILL_CELLS\n");
    Actuators::GetInstance().SetValve(3, true);
    Actuators::GetInstance().SetPump(2, 25);
    Actuators::GetInstance().SetPump(2, 0);
    Actuators::GetInstance().SetValve(3, false);

    if (!m_abortPending) InternalEvent(ST_SPIN);
}

STATE_DEFINE(CellProcess, Spin, NoEventData)
{
    if (m_abortPending) return;

    printf("CellProcess: ST_SPIN (Starting Centrifuge Ramp)\n");
    m_centrifuge.StartRamp(MAX_CENTRIFUGE_RPM, std::chrono::milliseconds(5000));
}

STATE_DEFINE(CellProcess, Drain, NoEventData)
{
    if (m_abortPending) return;

    printf("CellProcess: ST_DRAIN (Stopping Centrifuge)\n");
    Actuators::GetInstance().SetValve(10, true);
    Actuators::GetInstance().SetPump(3, 100);
    Sensors::GetInstance().GetPressure();
    Actuators::GetInstance().SetPump(3, 0);
    Actuators::GetInstance().SetValve(10, false);

    m_centrifuge.StopRamp();
}

STATE_DEFINE(CellProcess, Complete, NoEventData)
{
    printf("CellProcess: ST_COMPLETE\n");
    InternalEvent(ST_IDLE);
}

STATE_DEFINE(CellProcess, Aborting, NoEventData)
{
    printf("CellProcess: ST_ABORTING (Decelerating Centrifuge)\n");
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::ABORTING });
    m_centrifuge.StopRamp();
}

STATE_DEFINE(CellProcess, Fault, NoEventData)
{
    printf("CellProcess: ST_FAULT\n");
    m_abortPending = true; // Block any further automatic transitions
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::FAULT });
    
    Actuators::GetInstance().SetPump(1, 0);
    Actuators::GetInstance().SetPump(2, 0);
    Actuators::GetInstance().SetPump(3, 0);
    
    m_centrifuge.StopRamp();
}
