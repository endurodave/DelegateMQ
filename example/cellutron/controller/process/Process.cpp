#include "Process.h"
#include "actuators/Actuators.h"
#include <cstdio>

using namespace dmq;

namespace cellutron {

Process::~Process() { Shutdown(); }

void Process::Initialize()
{
    m_thread.CreateThread(std::chrono::seconds(2));
    m_pumpProcess.SetThread(m_thread);
    m_cellProcess.SetThread(m_thread);
    printf("Process: Subsystem initialized.\n");
}

void Process::Shutdown() { m_thread.ExitThread(); }

void Process::Start()
{
    MakeDelegate(this, &Process::InternalStart, m_thread).AsyncInvoke();
}

void Process::Abort()
{
    MakeDelegate(this, &Process::InternalAbort, m_thread).AsyncInvoke();
}

void Process::Fault()
{
    MakeDelegate(this, &Process::InternalFault, m_thread).AsyncInvoke();
}

void Process::InternalStart() { m_cellProcess.StartProcess(); }
void Process::InternalAbort() { m_cellProcess.AbortProcess(); }
void Process::InternalFault() { m_cellProcess.GenerateFault(); }

} // namespace cellutron
