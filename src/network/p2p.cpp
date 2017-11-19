/**
* CP2P
*
* Description: 
*/

#include "p2p.h"



CP2P::CP2P(){
    std::cout << "P2P " << "\n " << std::endl;    
 
    controlling = true;
    stun_port = 3478;
    stun_addr = "$(host -4 -t A stun.stunprotocol.org | awk '{ print $4 }')";

    g_networking_init(); 

    gloop = g_main_loop_new(NULL, FALSE);

    
    #ifdef G_OS_WIN32
    io_stdin = g_io_channel_win32_new_fd(_fileno(stdin));
    #else
    io_stdin = g_io_channel_unix_new(fileno(stdin));
    #endif

    
    // Create the nice agent
    agent = nice_agent_new(g_main_loop_get_context (gloop), NICE_COMPATIBILITY_RFC5245);
    if (agent == NULL)
        g_error("Failed to create agent");
    
    // Set the STUN settings and controlling mode
    if (stun_addr) {
         g_object_set(agent, "stun-server", stun_addr, NULL);
         g_object_set(agent, "stun-server-port", stun_port, NULL);
    }
    g_object_set(agent, "controlling-mode", controlling, NULL);

    // Connect to the signals
    //g_signal_connect(agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), NULL);
    //g_signal_connect(agent, "new-selected-pair", G_CALLBACK(cb_new_selected_pair), NULL);
    //g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), NULL);



}


void CP2P::connect(){

	

}


static void cb_candidate_gathering_done(NiceAgent *agent, guint _stream_id, gpointer data)
{

  g_debug("SIGNAL candidate gathering done\n");

  // Candidate gathering is done. Send our local candidates on stdout
  printf("Copy this line to remote client:\n");
  printf("\n  ");
  print_local_data(agent, _stream_id, 1);
  printf("\n");

  // Listen on stdin for the remote candidate list
  printf("Enter remote data (single line, no wrapping):\n");
  g_io_add_watch(io_stdin, G_IO_IN, stdin_remote_info_cb, agent);
  printf("> ");
  fflush (stdout);
}

