#ifndef DATA_MGR_H
#define DATA_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "DataMsg.h"
#include "NetworkMgr.h"
#include <mutex>

// DataMgr collects data from local and remote data sources. 
class DataMgr
{
public:
    // Register with delegate to receive callbacks when data is received
    static DelegateMQ::MulticastDelegateSafe<void(DataMsg&)> DataMsgCb;

    static DataMgr& Instance()
    {
        static DataMgr instance;
        return instance;
    }

    void SetDataMsg(DataMsg& data) 
    { 
        LocalDataMsgUpdate(data); 
    }

    DataMsg GetDataMsg() 
    { 
        return GetCombindedDataMsg(); 
    }

private:
    DataMgr() : m_thread("DataMgr")
    {
        // Create the receiver thread
        m_thread.CreateThread();

        // Register to receive remote data updates
        NetworkMgr::DataMsgCb += MakeDelegate(this, &DataMgr::RemoteDataMsgUpdate, m_thread);
    }

    ~DataMgr()
    {
        m_thread.ExitThread();
    }

    // Get all remote and local data packages
    DataMsg GetCombindedDataMsg()
    {
        std::lock_guard<std::mutex> lk(m_mutex);

        // Combine local and remote data
        DataMsg totalData;
        totalData.actuators.insert(totalData.actuators.end(), m_localDataMsg.actuators.begin(), m_localDataMsg.actuators.end());
        totalData.sensors.insert(totalData.sensors.end(), m_localDataMsg.sensors.begin(), m_localDataMsg.sensors.end());
        totalData.actuators.insert(totalData.actuators.end(), m_remoteDataMsg.actuators.begin(), m_remoteDataMsg.actuators.end());
        totalData.sensors.insert(totalData.sensors.end(), m_remoteDataMsg.sensors.begin(), m_remoteDataMsg.sensors.end());

        return totalData;
    }

    void LocalDataMsgUpdate(DataMsg& dataMsg)
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_localDataMsg = dataMsg;
        }

        // Nofity all subscribers new data arrived
        DataMsgCb(GetCombindedDataMsg());
    }

    void RemoteDataMsgUpdate(DataMsg& dataMsg)
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_remoteDataMsg = dataMsg;
        }

        // Nofity all subscribers new data arrived
        DataMsgCb(GetCombindedDataMsg());
    }

    Thread m_thread;
    Timer m_timer;
    std::mutex m_mutex;

    DataMsg m_localDataMsg;
    DataMsg m_remoteDataMsg;
};

#endif
