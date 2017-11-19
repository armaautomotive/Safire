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

//define CHUNK_SIZE 1024
static const gchar *candidate_type_name[] = {"host", "srflx", "prflx", "relay"};
static const gchar *state_name[] = {"disconnected", "gathering", "connecting", "connected", "ready", "failed"};

static int print_local_data(NiceAgent *agent, guint stream_id, guint component_id);
static int parse_remote_data(NiceAgent *agent, guint stream_id, guint component_id, char *line);
static void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data);
static void cb_new_selected_pair(NiceAgent *agent, guint stream_id, guint component_id, gchar *lfoundation, gchar *rfoundation, gpointer data);
static void cb_component_state_changed(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data);
static void cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer data);
static gboolean stdin_remote_info_cb (GIOChannel *source, GIOCondition cond, gpointer data);
static gboolean stdin_send_data_cb (GIOChannel *source, GIOCondition cond, gpointer data);

static GMainLoop *gloop;
static GIOChannel* io_stdin;

//NiceAgent *agent;

class CP2P
{
private:
    NiceAgent *agent;

    //static GMainLoop *gloop;
    //static GIOChannel* io_stdin;
    static guint stream_id;

    gchar *stun_addr = NULL;
    guint stun_port = 0; 
    gboolean controlling;


public:
    CP2P();

    //! Destructor.
    ~CP2P()
    {
    }

    void connect();    
    //void cb_candidate_gathering_done(NiceAgent *agent, guint stream_id, gpointer data);
};

#endif // P2P_H
