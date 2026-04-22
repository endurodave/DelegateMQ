#include "CellProcess.h"
#include "PumpProcess.h"
#include "messages/RunStatusMsg.h"
#include "Constants.h"
#include "actuators/Actuators.h"
#include "sensors/Sensors.h"
#include <iostream>
#include <cstdio>

using namespace dmq;

namespace cellutron {
namespace process {

CellProcess::CellProcess(actuators::Centrifuge& centrifuge, PumpProcess& pumpProcess) :
    StateMachine(ST_MAX_STATES),
    m_centrifuge(centrifuge),
    m_pumpProcess(pumpProcess),
    m_newChange(true)
{
}

CellProcess::~CellProcess()
{
}

void CellProcess::SetThread(dmq::IThread& thread)
{
    StateMachine::SetThread(thread);
    m_centrifugeConn = m_centrifuge.OnTargetReached.Connect(dmq::MakeDelegate(this, &CellProcess::OnCentrifugeTargetReached, thread));
    m_valveConn = actuators::Actuators::GetInstance().OnValveChanged.Connect(dmq::MakeDelegate(this, &CellProcess::OnValveChanged, thread));
    m_pumpConn = actuators::Actuators::GetInstance().OnPumpChanged.Connect(dmq::MakeDelegate(this, &CellProcess::OnPumpChanged, thread));
    m_timerConn = m_timer.OnExpired.Connect(dmq::MakeDelegate(this, &CellProcess::OnTimerExpired, thread));
    m_pumpCompleteConn = m_pumpProcess.OnComplete.Connect(dmq::MakeDelegate(this, &CellProcess::OnPumpComplete, thread));
    (void)this->OnTransition.Connect(dmq::MakeDelegate([this](uint8_t, uint8_t) { m_newChange = true; }));
}

void CellProcess::StartProcess()
{
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, StartProcess)
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
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, FaultEvent)
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
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, AbortProcess)
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

void CellProcess::OnValveChanged(int id, bool open)
{
    this->ValveChanged(id, open);
}

void CellProcess::OnPumpChanged(int id, int speed)
{
    this->PumpChanged(id, speed);
}

void CellProcess::OnTimerExpired()
{
    this->TimerExpired();
}

void CellProcess::OnPumpComplete()
{
    this->PumpComplete();
}

void CellProcess::PumpComplete()
{
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, PumpComplete)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_FILL_SOLUTION_B) // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(ST_FILL_CELLS)      // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(ST_SPIN)            // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_SPIN
        TRANSITION_MAP_ENTRY(ST_COMPLETE)        // ST_DRAIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_COMPLETE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)      // ST_FAULT
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::ValveChanged(int id, bool open)
{
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, ValveChanged, id, open)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_IDLE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_SPIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_DRAIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_COMPLETE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FAULT
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::PumpChanged(int id, int speed)
{
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, PumpChanged, id, speed)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_IDLE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_SPIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_DRAIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_COMPLETE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FAULT
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::TimerExpired()
{
    BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, TimerExpired)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_IDLE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_A
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_SOLUTION_B
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FILL_CELLS
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_SPIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_DRAIN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_COMPLETE
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_ABORTING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)  // ST_FAULT
    END_TRANSITION_MAP(nullptr)
}

void CellProcess::CentrifugeReached(uint16_t rpm)
{
    if (rpm > 0)
    {
        BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, CentrifugeReached, rpm)
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
        BEGIN_TRANSITION_MAP(cellutron::process::CellProcess, CentrifugeReached, rpm)
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

STATE_DEFINE(cellutron::process::CellProcess, Idle, NoEventData)
{
    printf("CellProcess: ST_IDLE\n");
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::IDLE });
}

STATE_DEFINE(cellutron::process::CellProcess, FillSolutionA, NoEventData)
{
    printf("CellProcess: ST_FILL_SOLUTION_A\n");
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::PROCESSING });
    // Use Pump ID 1, Valve 1, Forward
    m_pumpProcess.Start(std::make_shared<PumpData>(1, 50, std::chrono::seconds(2), false));
}

STATE_DEFINE(cellutron::process::CellProcess, FillSolutionB, NoEventData)
{
    printf("CellProcess: ST_FILL_SOLUTION_B\n");
    // Use Pump ID 1, Valve 2, Forward
    m_pumpProcess.Start(std::make_shared<PumpData>(2, 50, std::chrono::seconds(2), false));
}

STATE_DEFINE(cellutron::process::CellProcess, FillCells, NoEventData)
{
    printf("CellProcess: ST_FILL_CELLS\n");
    // Use Pump ID 1, Valve 3, Forward
    m_pumpProcess.Start(std::make_shared<PumpData>(3, 25, std::chrono::seconds(2), false));
}

STATE_DEFINE(cellutron::process::CellProcess, Spin, NoEventData)
{
    printf("CellProcess: ST_SPIN (Starting Centrifuge Ramp)\n");
    m_centrifuge.StartRamp(std::make_shared<actuators::Centrifuge::RampData>(MAX_CENTRIFUGE_RPM, std::chrono::milliseconds(5000)));
}

STATE_DEFINE(cellutron::process::CellProcess, Drain, NoEventData)
{
    printf("CellProcess: ST_DRAIN\n");
    // Use Pump ID 1, Valve 4, Reverse
    m_pumpProcess.Start(std::make_shared<PumpData>(4, 100, std::chrono::seconds(3), true));
}

STATE_DEFINE(cellutron::process::CellProcess, Complete, NoEventData)
{
    printf("CellProcess: ST_COMPLETE\n");
    InternalEvent(ST_IDLE);
}

STATE_DEFINE(cellutron::process::CellProcess, Aborting, NoEventData)
{
    printf("CellProcess: ST_ABORTING (Decelerating Centrifuge)\n");
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::ABORTING });
    m_centrifuge.StopRamp(std::make_shared<actuators::Centrifuge::RampData>(0, std::chrono::milliseconds(2000)));
}

STATE_DEFINE(cellutron::process::CellProcess, Fault, NoEventData)
{
    printf("CellProcess: ST_FAULT\n");
    DataBus::Publish<RunStatusMsg>("cell/status/run", { RunStatus::FAULT });
    
    // Stop single pump
    actuators::Actuators::GetInstance().SetPump(1, 0);
    
    m_centrifuge.StopRamp(std::make_shared<actuators::Centrifuge::RampData>(0, std::chrono::milliseconds(2000)));
}

} // namespace process
} // namespace cellutron
