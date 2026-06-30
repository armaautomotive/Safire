#ifndef SAFIRE_NAT_MAPPER_H
#define SAFIRE_NAT_MAPPER_H

#include <string>
#include <thread>

class CNatMapper
{
public:
    struct status {
        bool enabled;
        bool mapped;
        int internalPort;
        int externalPort;
        std::string method;
        std::string externalAddress;
        std::string message;
    };

    CNatMapper();
    ~CNatMapper();

    bool start(int port, int leaseSeconds = 3600);
    void stop();

    static status currentStatus();

private:
    void run();
    bool mapOnce();
    void clearMapping();

    int m_port;
    int m_leaseSeconds;
    bool m_running;
    std::thread m_thread;
};

#endif
