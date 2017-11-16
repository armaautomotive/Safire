/**
* CP2P
*
* Description: P2P interface. Uses libnice to connect through a stun server.
*/
#ifndef P2P_H
#define P2P_H


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <agent.h>

#include <gio/gnetworking.h>


#include <iostream>
#include <string>
//#include <stdexcept>
//#include <vector>

//#include <stdlib.h>
//#include <stdio.h>
//#include <limits.h>
//#include <string.h>
//#include <assert.h>

#define CHUNK_SIZE 1024
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting", "connected", "ready", "failed"};


class CP2P
{
private:
    NiceAgent *agent;

    static GMainLoop *gloop;
    static GIOChannel* io_stdin;
    static guint stream_id;

    gchar *stun_addr = NULL;
    guint stun_port = 0; 
    gboolean controlling ;


public:
    CP2P();

    //! Destructor.
    ~CP2P()
    {
    }

    
    //bool fileExists(std::string fileName);
    //bool write(std::string privateKey, std::string publicKey);
    //bool read(std::string & privateKey, std::string & publicKey);
};

#endif // P2P_H
