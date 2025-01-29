#ifndef DATA_MGR_H
#define DATA_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "DataPackage.h"
#include "NetworkMgr.h"
#include <mutex>

using namespace DelegateMQ;
using namespace std;

// DataMgr collects data from local and remote data sources. Locally SetDataPackage()
// is called to update data. Remotely data arrives via NetworkMgr::DataPackageRecv 
// delegate callback.
class DataMgr
{
public:
    static MulticastDelegateSafe<void(DataPackage&)> DataPackageRecv;

    static DataMgr& Instance()
    {
        static DataMgr instance;
        return instance;
    }

    void SetDataPackage(DataPackage& dataPackage) { LocalDataPackageUpdate(dataPackage); }

    DataPackage GetDataPackage() { return GetCombindedDataPackage(); }

private:
    DataMgr() : m_thread("DataMgr")
    {
        // Create the receiver thread
        m_thread.CreateThread();

        // Register to receive remote data updates
        NetworkMgr::DataPackageRecv += MakeDelegate(this, &DataMgr::RemoteDataPackageUpdate, m_thread);
    }

    ~DataMgr()
    {
        m_thread.ExitThread();
    }

    DataPackage GetCombindedDataPackage()
    {
        std::lock_guard<std::mutex> lk(m_mutex);

        // Combine local and remote data
        DataPackage totalData;
        totalData.actuators.insert(totalData.actuators.end(), m_localDataPackage.actuators.begin(), m_localDataPackage.actuators.end());
        totalData.sensors.insert(totalData.sensors.end(), m_localDataPackage.sensors.begin(), m_localDataPackage.sensors.end());
        totalData.actuators.insert(totalData.actuators.end(), m_remoteDataPackage.actuators.begin(), m_remoteDataPackage.actuators.end());
        totalData.sensors.insert(totalData.sensors.end(), m_remoteDataPackage.sensors.begin(), m_remoteDataPackage.sensors.end());

        return totalData;
    }

    void LocalDataPackageUpdate(DataPackage& dataPackage)
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_localDataPackage = dataPackage;
        }

        // Nofity all subscribers new data arrived
        DataPackageRecv(GetCombindedDataPackage());
    }

    void RemoteDataPackageUpdate(DataPackage& dataPackage)
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_remoteDataPackage = dataPackage;
        }

        // Nofity all subscribers new data arrived
        DataPackageRecv(GetCombindedDataPackage());
    }

    Thread m_thread;
    Timer m_timer;
    std::mutex m_mutex;

    DataPackage m_localDataPackage;
    DataPackage m_remoteDataPackage;
};

#endif
