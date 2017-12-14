/**
* CP2P
*
* Description:
*  CP2P implements Peer to peer networking functions using the libnice library.  
*  - http://173.255.218.54/getnode.php?r=abc
*/

#include "p2p.h"
#include "wallet.h"
#include <sstream>

std::string CP2P::myPeerAddress;
bool CP2P::running;
bool CP2P::connected;
std::string CP2P::remotePeerAddress;

CP2P::CP2P(){
    //std::cout << "P2P " << "\n " << std::endl;    
    running = true; 
    connected = false;
    controlling = true;
    stun_port = 3478;
    stun_addr = "$(host -4 -t A stun.stunprotocol.org | awk '{ print $4 }')";
}


static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


/**
* getNetworkPeer
*
* Description: requests a new peer node connection from a: the current network or b: a connection server.  
*/
std::string CP2P::getNewNetworkPeer(std::string myPeerAddress){
    std::string readBuffer;
    CURLcode res;    
    CURL * curl;
    curl_global_init(CURL_GLOBAL_ALL); //pretty obvious
    curl = curl_easy_init();
    if(curl) {

        std::string publicKey;
        std::string privateKey;
        CWallet wallet;
        wallet.read(privateKey, publicKey);

        std::string url_string = "http://173.255.218.54/getnode.php";
	std::string post_data = "connection_string=";
        post_data.append(myPeerAddress);
        post_data.append("&public_key=");
        post_data.append(publicKey);

        curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        //std::cout << readBuffer << std::endl;

        // TODO: parse results and write to local peer node file.
	// [{"public_key":"03695EB28563D74CCAA17070660767F045DD089C459D0A79865BC2376506B2FD21","connection_string":"2ANg JQyxT3POXQeLTNfsv0y4dA 1,2013266431,2600:3c03::f03c:91ff:fedf:82c9,36826,host 2,2013266431,96.126.105.53,35238,host"}]

        std::size_t open_paren = readBuffer.find("{");       
        std::size_t close_paren = readBuffer.find("}", open_paren + 1); 	
        std::string section = readBuffer.substr( open_paren + 1, close_paren - open_paren -1 );
        //std::cout << ":" << section << std::endl;
        std::size_t con_start = section.find("connection_string\":\"");
        std::size_t con_end = section.rfind("\"");
        std::string con_string = section.substr(con_start + 20, con_end - con_start - 20);
        //std::cout << "___" << con_string << "___" << std::endl; 

        return con_string;
    }
    return "";
}


/**
* p2pNetworkThread
*
* Description: Implements P2P networking with other nodes in the network.
*/
void CP2P::p2pNetworkThread(int argc, char* argv[]){

    while(running){

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

        const gchar *local_ufrag = "h34S";
        const gchar *local_passwd = "Vme8u6iKNXvNsfkExsaHYd"; 
        gboolean cred_set = nice_agent_set_local_credentials ( agent, stream_id, local_ufrag, local_passwd ); // Hard code ICE  user and password.

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

        g_main_loop_unref(gloop);
        g_object_unref(agent);
        g_io_channel_unref (io_stdin);
        
        if(running){
            usleep(1000000 * 10); // 1 second
            
        }
    }    
    std::cout << "P2P Network Shutdown." << std::endl;
}

void CP2P::exit(){
    running = false;
    g_main_loop_quit (gloop);
    //std::cout << " P2P exit " << "\n " << std::endl; 
}

void CP2P::connect(){

}

void CP2P::sendData(std::string data){
   if(connected){
       gchar *line = (gchar *)data.c_str(); 
       nice_agent_send(agent, stream_id, 1, strlen(line), line); 
   } 
}

static void cb_candidate_gathering_done(NiceAgent *agent, guint _stream_id, gpointer data){
  //std::cout << " cb_candidate_gathering_done " << "\n " << std::endl;
  //g_debug("SIGNAL candidate gathering done\n");

  // Candidate gathering is done. Send our local candidates on stdout
  //printf("Copy this line to remote client:\n");
  //printf("\n  ");
  //print_local_data(agent, _stream_id, 1);
  //printf("\n");

  std::string local_addr = get_local_data (agent, _stream_id, 1);
  //std::cout << " local_addr _" << local_addr << "_ " << std::endl;

  CP2PHandle h = create_cp2p(); 
  setPeerAddress_cp2p(h, local_addr); 

  //CP2P::connected = true;

  // Listen on stdin for the remote candidate list
  //printf("Enter remote data (single line, no wrapping):\n");
  //g_io_add_watch(io_stdin, G_IO_IN, stdin_remote_info_cb, agent);
  //printf("> ");
  //fflush (stdout);
 
  // set remote connection here? 
  std::cout << " CP2P::myPeerAddress _" << CP2P::myPeerAddress << "_ " << std::endl;   
  std::string remotePeerAddress = getNewNetworkPeer_cp2p(h, CP2P::myPeerAddress); // TODO: Load from peers db stored locally
  std::cout << " remote peer " << remotePeerAddress << std::endl;
 
  gchar *line = (gchar *)remotePeerAddress.c_str();
  int rval = parse_remote_data(agent, stream_id, 1, line);
  if (rval == EXIT_SUCCESS) {
    std::cout << " remote parse success " << std::endl;
  } else {
    std::cout << " remote parse failure " << std::endl; 
  }

 
  // send test
  std::cout << " sending data " << std::endl; 
  gchar *data_line = "test data xxx 123 data data xxx 123";
  nice_agent_send(agent, stream_id, 1, strlen(data_line), data_line);


  free_cp2p(h);
}

extern "C"
{
    CP2PHandle create_cp2p( ) { return new CP2P( ); }   
    void free_cp2p(CP2PHandle p) { delete p; }
    void setPeerAddress_cp2p(CP2PHandle p, std::string address){
           p->setMyPeerAddress( address );
    }
    std::string getNewNetworkPeer_cp2p(CP2PHandle p, std::string myPeerAddress){
           return p->getNewNetworkPeer(myPeerAddress);
    }
}

void CP2P::setMyPeerAddress(std::string address){
  //std::cout << "  CP2P::setMyPeerAddress('" << address << "'); " << std::endl;
  myPeerAddress = address; 

  //getNewNetworkPeer(address); 
}

// depricate
static int print_local_data (NiceAgent *agent, guint _stream_id, guint component_id){
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

  //printf("%s %s", local_ufrag, local_password);

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


/**
* get_local_data 
*
* Description: returns string with network address for conneting to this node.
*/
static std::string get_local_data (NiceAgent *agent, guint _stream_id, guint component_id){
  std::string result = ""; 
  gchar *local_ufrag = NULL;
  gchar *local_password = NULL;
  gchar ipaddr[INET6_ADDRSTRLEN];
  GSList *cands = NULL, *item;
  bool first = true;

  if (!nice_agent_get_local_credentials(agent, _stream_id, &local_ufrag, &local_password))
    goto end;

  cands = nice_agent_get_local_candidates(agent, _stream_id, component_id);
  if (cands == NULL)
    goto end;

  //printf("%s %s", local_ufrag, local_password);
  //result.append(local_ufrag);
  //result.append(" ");
  //result.append(local_password);

  for (item = cands; item; item = item->next) {
    NiceCandidate *c = (NiceCandidate *)item->data;

    nice_address_to_string(&c->addr, ipaddr);

    // (foundation),(prio),(addr),(port),(type)
    //printf(" %s,%u,%s,%u,%s",
    //    c->foundation,
    //    c->priority,
    //    ipaddr,
    //    nice_address_get_port(&c->addr),
    //    candidate_type_name[c->type]);
    if(first == false){
        result.append(" ");
    }
    first = false;

    result.append(c->foundation);
    result.append(","); 
    
    std::ostringstream priority_stream;
    priority_stream << c->priority;   
    result.append(priority_stream.str());  
    result.append(",");
    result.append(ipaddr);
    result.append(",");
    std::ostringstream address_stream;
    address_stream << nice_address_get_port(&c->addr); 
    result.append(address_stream.str());
    result.append(",");
    result.append(candidate_type_name[c->type]);   
  }

 end:
  if (local_ufrag)
    g_free(local_ufrag);
  if (local_password)
    g_free(local_password);
  if (cands)
    g_slist_free_full(cands, (GDestroyNotify)&nice_candidate_free);

  return result;
}





static gboolean stdin_remote_info_cb (GIOChannel *source, GIOCondition cond, gpointer data)
{
  NiceAgent *agent = (NiceAgent *)data;
  gchar *line = NULL;
  int rval;
  gboolean ret = TRUE;

  if (g_io_channel_read_line (source, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {

    // Parse remote candidate list and set it on the agent
    rval = parse_remote_data(agent, stream_id, 1, line);
    if (rval == EXIT_SUCCESS) {
      // Return FALSE so we stop listening to stdin since we parsed the
      // candidates correctly
      ret = FALSE;
      g_debug("waiting for state READY or FAILED signal...");
    } else {
      fprintf(stderr, "ERROR: failed to parse remote data\n");
      printf("Enter remote data (single line, no wrapping):\n");
      printf("> ");
      fflush (stdout);
    }
    g_free (line);
  }
  return ret;
}

// implementation given string not stream for source. 



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
    //printf("\nSend lines to remote (Ctrl-D to quit):\n"); 
    //g_io_add_watch(io_stdin, G_IO_IN, stdin_send_data_cb, agent);
    //printf("> ");
    //fflush (stdout);
     
    // send data
    gchar *line = "123445 \0";
    nice_agent_send(agent, stream_id, 1, strlen(line), line);
    g_free (line);
 

  } else if (state == NICE_COMPONENT_STATE_FAILED) {
    //g_main_loop_quit (gloop);
    CP2P::connected = false;
  }
}



static gboolean stdin_send_data_cb (GIOChannel *source, GIOCondition cond, gpointer data){ 
  NiceAgent *agent = (NiceAgent *)data;
  gchar *line = NULL;
  
  if (g_io_channel_read_line (source, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
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



static void cb_new_selected_pair(NiceAgent *agent, guint _stream_id,
    guint component_id, gchar *lfoundation,
    gchar *rfoundation, gpointer data)
{ 
  g_debug("SIGNAL: selected pair %s %s", lfoundation, rfoundation);
}

static void cb_nice_recv(NiceAgent *agent, guint _stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{ 
  if (len == 1 && buf[0] == '\0')
    g_main_loop_quit (gloop);
  printf("%.*s", len, buf);
  fflush(stdout);
}



static NiceCandidate * parse_candidate(char *scand, guint _stream_id)
{ 
  NiceCandidate *cand = NULL;
  NiceCandidateType ntype;
  gchar **tokens = NULL;
  guint i;
  
  tokens = g_strsplit (scand, ",", 5);
  for (i = 0; tokens[i]; i++);
  if (i != 5)
    goto end;
  
  for (i = 0; i < G_N_ELEMENTS (candidate_type_name); i++) {
    if (strcmp(tokens[4], candidate_type_name[i]) == 0) {
      ntype = (NiceCandidateType)i;
      break;
    }
  }
  if (i == G_N_ELEMENTS (candidate_type_name))
    goto end;
  
  cand = nice_candidate_new(ntype);
  cand->component_id = 1;
  cand->stream_id = _stream_id;
  cand->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  strncpy(cand->foundation, tokens[0], NICE_CANDIDATE_MAX_FOUNDATION);
  cand->foundation[NICE_CANDIDATE_MAX_FOUNDATION - 1] = 0;
  cand->priority = atoi (tokens[1]);
  
  if (!nice_address_set_from_string(&cand->addr, tokens[2])) {
    g_message("failed to parse addr: %s", tokens[2]);
    nice_candidate_free(cand);
    cand = NULL;
    goto end;
  }
  
  nice_address_set_port(&cand->addr, atoi (tokens[3]));
 
 end:
  g_strfreev(tokens);
  
  return cand;
}



static int parse_remote_data(NiceAgent *agent, guint _stream_id, guint component_id, char *line) {
  GSList *remote_candidates = NULL;
  gchar **line_argv = NULL;
  const gchar *ufrag = NULL;
  const gchar *passwd = NULL;
  int result = EXIT_FAILURE;
  int i;

  ufrag = "h34S";
  passwd = "Vme8u6iKNXvNsfkExsaHYd";

  line_argv = g_strsplit_set (line, " \t\n", 0);
  for (i = 0; line_argv && line_argv[i]; i++) {
    if (strlen (line_argv[i]) == 0)
      continue;

    // first two args are remote ufrag and password
    //if (!ufrag) {
    //  ufrag = line_argv[i];
    //} else if (!passwd) {
    //  passwd = line_argv[i];
    //} else {
      
      // Remaining args are serialized canidates (at least one is required)
      NiceCandidate *c = parse_candidate(line_argv[i], _stream_id);

      if (c == NULL) {
        g_message("failed to parse candidate: %s", line_argv[i]);
        goto end;
      }
      remote_candidates = g_slist_prepend(remote_candidates, c);
    //}
  }
  if (ufrag == NULL || passwd == NULL || remote_candidates == NULL) {
    g_message("line must have at least ufrag, password, and one candidate");
    goto end;
  }

  if (!nice_agent_set_remote_credentials(agent, _stream_id, ufrag, passwd)) {
    g_message("failed to set remote credentials");
    goto end;
  }

  // Note: this will trigger the start of negotiation.
  if (nice_agent_set_remote_candidates(agent, _stream_id, component_id,
      remote_candidates) < 1) {
    g_message("failed to set remote candidates");
    goto end;
  }

  result = EXIT_SUCCESS;
  end:
  if (line_argv != NULL)
    g_strfreev(line_argv);
  if (remote_candidates != NULL)
    g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);

  return result;
}
