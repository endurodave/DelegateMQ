#ifndef ALARM_MGR_H
#define ALARM_MGR_H

#include "DelegateMQ.h"
#include "NetworkMgr.h"
#include "AlarmMsg.h"
#include <string>

// AlarmMgr handles client and server alarms. 
class AlarmMgr
{
public:
    static AlarmMgr& Instance()
    {
        static AlarmMgr instance;
        return instance;
    }

    void SetAlarm(AlarmMsg& msg, AlarmNote& note)
    {
#ifdef SERVER_APP
        // Send alarm to client for handling
        NetworkMgr::Instance().SendAlarmMsg(msg, note);
#else
        // Reinvoke function call on the AlarmMgr thread of control
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &AlarmMgr::SetAlarm, m_thread)(msg, note);

        // Client handles alarm locally
        SetAlarm(msg, note);
#endif
    }

private:
    AlarmMgr() :
        m_thread("AlarmMgr")
    {
        m_thread.CreateThread();

        NetworkMgr::ErrorCb += MakeDelegate(this, &AlarmMgr::ErrorHandler, m_thread);
        NetworkMgr::AlarmMsgCb += MakeDelegate(this, &AlarmMgr::RecvAlarmMsg, m_thread);
    }

    ~AlarmMgr()
    {
        NetworkMgr::ErrorCb -= MakeDelegate(this, &AlarmMgr::ErrorHandler, m_thread);
        NetworkMgr::AlarmMsgCb -= MakeDelegate(this, &AlarmMgr::RecvAlarmMsg, m_thread);

        m_thread.ExitThread();
    }

    void AlarmHandler(AlarmMsg& msg, AlarmNote& note)
    {
        if (msg.source == AlarmMsg::Source::CLIENT)
        {
            std::cout << "Client Alarm: " << note.note << std::endl;
        }
        else if (msg.source == AlarmMsg::Source::SERVER)
        {
            std::cout << "Server Alarm: " << note.note << std::endl;
        }
    }

    void RecvAlarmMsg(AlarmMsg& msg, AlarmNote& note)
    {
        AlarmHandler(msg, note);
    }

    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        if (error != dmq::DelegateError::SUCCESS && id == ids::ALARM_MSG_ID)
            std::cout << "AlarmMgr Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    Thread m_thread;
};

#endif