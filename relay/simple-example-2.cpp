

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
    g_signal_connect(agent, "new-selected-pair", G_CALLBACK(cb_new_selected_pair), NULL);
    g_signal_connect(agent, "component-state-changed", G_CALLBACK(cb_component_state_changed), NULL);

    // Create a new stream with one component
    stream_id = nice_agent_add_stream(agent, 1);
    if (stream_id == 0)
      g_error("Failed to add stream");

    
    // Attach to the component to receive the data
    // Without this call, candidates cannot be gathered
    nice_agent_attach_recv(agent, stream_id, 1, g_main_loop_get_context (gloop), cb_nice_recv, NULL);

    // Start gathering local candidates
    if (!nice_agent_gather_candidates(agent, stream_id))
      g_error("Failed to start candidate gathering");

    g_debug("waiting for candidate-gathering-done signal...");

    // Run the mainloop. Everything else will happen asynchronously
    // when the candidates are done gathering.
    g_main_loop_run (gloop);

    //g_main_loop_unref(gloop);
    //g_object_unref(agent);
    //g_io_channel_unref (io_stdin);




    std::cout << " done " << "\n " << std::endl;
    g_debug("g_debug done\n");

}

//#ifdef __cplusplus
//extern "C"{
//#endif

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

static void cb_new_selected_pair(NiceAgent *agent, guint _stream_id,
    guint component_id, gchar *lfoundation,
    gchar *rfoundation, gpointer data)
{
  g_debug("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}

static void cb_component_state_changed(NiceAgent *agent, guint _stream_id,
    guint component_id, guint state,
    gpointer data)
{

  g_debug("SIGNAL: state changed %d %d %s[%d]\n",
      _stream_id, component_id, state_name[state], state);

  if (state == NICE_COMPONENT_STATE_CONNECTED) {
    NiceCandidate *local, *remote;

    // Get current selected candidate pair and print IP address used
    if (nice_agent_get_selected_pair (agent, _stream_id, component_id,
                &local, &remote)) {
      gchar ipaddr[INET6_ADDRSTRLEN];

      nice_address_to_string(&local->addr, ipaddr);
      printf("\nNegotiation complete: ([%s]:%d,",
          ipaddr, nice_address_get_port(&local->addr));
      nice_address_to_string(&remote->addr, ipaddr);
      printf(" [%s]:%d)\n", ipaddr, nice_address_get_port(&remote->addr));
    }

    // Listen to stdin and send data written to it
    printf("\nSend lines to remote (Ctrl-D to quit):\n");
    g_io_add_watch(io_stdin, G_IO_IN, stdin_send_data_cb, agent);
    printf("> ");
    fflush (stdout);
  } else if (state == NICE_COMPONENT_STATE_FAILED) {
    g_main_loop_quit (gloop);
  }
}


static gboolean stdin_send_data_cb (GIOChannel *source, GIOCondition cond, gpointer data)
{
  NiceAgent *agent = (NiceAgent *)data;
  gchar *line = NULL;

  if (g_io_channel_read_line (source, &line, NULL, NULL, NULL) ==
      G_IO_STATUS_NORMAL) {
    nice_agent_send(agent, stream_id, 1, strlen(line), line);
    g_free (line);
    printf("> ");
    fflush (stdout);
  } else {
    nice_agent_send(agent, stream_id, 1, 1, "\0");
    // Ctrl-D was pressed.
    g_main_loop_quit (gloop);
  }

  return TRUE;
}


static void
cb_nice_recv(NiceAgent *agent, guint _stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);
  printf("%.*s", len, buf);
  fflush(stdout);
}



//#ifdef __cplusplus
//}
//#endif


int main(int argc, char* argv[])
{
    std::cout << "Test " << std::endl;
    CP2P p2p;


}
