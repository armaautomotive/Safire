

#include "simple-example-2.h"



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
    if (agent == NULL){
        std::cout << " error " << "\n " << std::endl;
        g_error("Failed to create agent");
    }

    // Set the STUN settings and controlling mode
    if (stun_addr) {
         g_object_set(agent, "stun-server", stun_addr, NULL);
         g_object_set(agent, "stun-server-port", stun_port, NULL);
    }
    g_object_set(agent, "controlling-mode", controlling, NULL);

    // Connect to the signals
    g_signal_connect(agent, "candidate-gathering-done", G_CALLBACK(cb_candidate_gathering_done), NULL);
    //g_signal_connect(agent, "new-selected-pair", G_CALLBACK(cb_new_selected_pair), NULL);
    //g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), NULL);

    std::cout << " done " << "\n " << std::endl;
    g_debug("g_debug done\n");

}


static void cb_candidate_gathering_done(NiceAgent *agent, guint _stream_id, gpointer data)
{
  std::cout << " AHHH " << "\n " << std::endl;
  g_debug("SIGNAL candidate gathering done\n");

  // Candidate gathering is done. Send our local candidates on stdout
  printf("Copy this line to remote client:\n");
  printf("\n  ");
  print_local_data(agent, _stream_id, 1);
  printf("\n");

  // Listen on stdin for the remote candidate list
  //printf("Enter remote data (single line, no wrapping):\n");
  //g_io_add_watch(io_stdin, G_IO_IN, stdin_remote_info_cb, agent);
  //printf("> ");
  //fflush (stdout);
}


static int
print_local_data (NiceAgent *agent, guint _stream_id, guint component_id)
{
  int result = EXIT_FAILURE;
  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];
  GSList *cands = NULL, *item;

  if (!nice_agent_get_local_credentials(agent, _stream_id,
      &local_ufrag, &local_password))
    goto end;

  cands = nice_agent_get_local_candidates(agent, _stream_id, component_id);
  if (cands == NULL)
    goto end;

  printf("%s %s", local_ufrag, local_password);

  for (item = cands; item; item = item->next) {
    NiceCandidate *c = (NiceCandidate *)item->data;

    nice_address_to_string(&c->addr, ipaddr);

    // (foundation),(prio),(addr),(port),(type)
    printf(" %s,%u,%s,%u,%s",
        c->foundation,
        c->priority,
        ipaddr,
        nice_address_get_port(&c->addr),
        candidate_type_name[c->type]);
  }
  printf("\n");
  result = EXIT_SUCCESS;

 end:
  if (local_ufrag)
    g_free(local_ufrag);
  if (local_password)
    g_free(local_password);
  if (cands)
    g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);

  return result;
}


int main(int argc, char* argv[])
{
    std::cout << "Test " << std::endl;
    CP2P p2p;


}
