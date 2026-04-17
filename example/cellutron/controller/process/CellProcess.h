#ifndef _CELL_PROCESS_H
#define _CELL_PROCESS_H

#include "DelegateMQ.h"
#include "state-machine/StateMachine.h"
#include "Centrifuge.h"

/// @brief CellProcess state machine controls the high-level cell processing steps.
class CellProcess : public StateMachine
{
public:
    CellProcess(Centrifuge& centrifuge);
    ~CellProcess();

    virtual void SetThread(dmq::IThread& thread) override;

    /// Start the cell process.
    void StartProcess();

    /// Stop/Abort the cell process.
    void AbortProcess();

    /// Fault event - transitions to ST_FAULT.
    void GenerateFault();

protected:
    enum States
    {
        ST_IDLE,
        ST_FILL_SOLUTION_A,
        ST_FILL_SOLUTION_B,
        ST_FILL_CELLS,
        ST_SPIN,
        ST_DRAIN,
        ST_COMPLETE,
        ST_ABORTING,
        ST_FAULT,
        ST_MAX_STATES
    };

    // States
    STATE_DECLARE(CellProcess, Idle, NoEventData)
    STATE_DECLARE(CellProcess, FillSolutionA, NoEventData)
    STATE_DECLARE(CellProcess, FillSolutionB, NoEventData)
    STATE_DECLARE(CellProcess, FillCells, NoEventData)
    STATE_DECLARE(CellProcess, Spin, NoEventData)
    STATE_DECLARE(CellProcess, Drain, NoEventData)
    STATE_DECLARE(CellProcess, Complete, NoEventData)
    STATE_DECLARE(CellProcess, Aborting, NoEventData)
    STATE_DECLARE(CellProcess, Fault, NoEventData)

    // State map
    BEGIN_STATE_MAP
        STATE_MAP_ENTRY(&Idle)
        STATE_MAP_ENTRY(&FillSolutionA)
        STATE_MAP_ENTRY(&FillSolutionB)
        STATE_MAP_ENTRY(&FillCells)
        STATE_MAP_ENTRY(&Spin)
        STATE_MAP_ENTRY(&Drain)
        STATE_MAP_ENTRY(&Complete)
        STATE_MAP_ENTRY(&Aborting)
        STATE_MAP_ENTRY(&Fault)
    END_STATE_MAP

private:
    // External events
    void CentrifugeReached(uint16_t rpm);
    void FaultEvent();

    void OnCentrifugeTargetReached(uint16_t rpm);

    Centrifuge& m_centrifuge;
    dmq::ScopedConnection m_centrifugeConn;
    bool m_abortPending = false;
};

#endif
