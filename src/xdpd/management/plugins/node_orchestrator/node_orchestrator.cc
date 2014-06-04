#include "node_orchestrator.h"
#include <rofl/common/utils/c_logger.h>

using namespace xdpd;

//FIXME: protect this variable with a mutex?
uint64_t NodeOrchestrator::nextDpid = 0x1;
map<uint64_t, unsigned int> NodeOrchestrator::last_ports_id;

map<uint64_t, pthread_t> NodeOrchestrator::threads;

void NodeOrchestrator::sigurs_handler(int signum)
{
	ciosrv::stop();
}

void NodeOrchestrator::init()
{
	//DO something
	ROFL_INFO("\n\n[xdpd]["PLUGIN_NAME"] **************************\n");	
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Plugin receiving commands from the node orchestrator.\n");
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] **************************\n\n");	
	
	signal(SIGUSR1,NodeOrchestrator::sigurs_handler);
	
	pthread_t thread;
	pthread_create(&thread,NULL,Server::listen,NULL);
	pthread_detach(thread);
};

LSI NodeOrchestrator::createLSI(list<string> phyPorts, string controllerAddress, string controllerPort)
{
	map<string,unsigned int> ports;

	uint64_t dpid = nextDpid;	
	nextDpid++;

	lsi_parameters_t lsi_parameters;
	lsi_parameters.controllerAddress = controllerAddress;
	lsi_parameters.controllerPort = controllerPort;
	lsi_parameters.dpid = dpid;
	sem_init(&lsi_parameters.lsi_created, 0, 0);
	sem_init(&lsi_parameters.ports_attached, 0, 0);

	pthread_t thread;
	pthread_create(&thread,NULL,NodeOrchestrator::run,&lsi_parameters);
	pthread_detach(thread);
	
	threads[dpid] = thread;
	assert(thread == threads[dpid]);

	sem_wait(&lsi_parameters.lsi_created);

	//Attach ports
	list<string>::iterator port_it;
	unsigned int i;
	for(port_it = phyPorts.begin(), i=1; port_it != phyPorts.end(); ++port_it, ++i)
	{		
		//Ignore empty ports	
		if(*port_it == "")
			continue;
	
		try{
			//Attach
			port_manager::attach_port_to_switch(dpid, *port_it, &i);
			
			//FIXME: tmp code
			last_ports_id[dpid] = i;
			
			//Bring up
			port_manager::bring_up(*port_it);
		}catch(...){	
			ROFL_ERR("[xdpd]["PLUGIN_NAME"] Unable to attach port '%s'",(*port_it).c_str());
			throw;
		}
		ports[*port_it] = i;
	}
	
	sem_post(&lsi_parameters.ports_attached);

	sem_destroy(&lsi_parameters.lsi_created);

	return LSI(dpid,ports);
}

void *NodeOrchestrator::run(void *parameters)
{
	lsi_parameters_t *lsi_parameters = (lsi_parameters_t*)parameters;

	uint64_t dpid = lsi_parameters->dpid;
	
	stringstream lsiName_ss;
	lsiName_ss << dpid;
	string lsiName = lsiName_ss.str();

	rofl::cparams socket_params = rofl::csocket::get_default_params(rofl::csocket::SOCKET_TYPE_PLAIN);
	socket_params.set_param(rofl::csocket::PARAM_KEY_REMOTE_HOSTNAME) = lsi_parameters->controllerAddress;
	socket_params.set_param(rofl::csocket::PARAM_KEY_REMOTE_PORT) = lsi_parameters->controllerPort; 
	socket_params.set_param(rofl::csocket::PARAM_KEY_DOMAIN) = string("inet"); 
		int ma_list[OF1X_MAX_FLOWTABLES] = { 0 };
	try
	{
		switch_manager::create_switch(OFVERSION, dpid,lsiName,NUM_TABLES,ma_list,RECONNECT_TIME,rofl::csocket::SOCKET_TYPE_PLAIN,socket_params);
	} catch (...) {
		ROFL_ERR("[xdpd]["PLUGIN_NAME"] Unable to create the LSI\n");
		throw;
	}	
	
	sem_post(&lsi_parameters->lsi_created);
	sem_wait(&lsi_parameters->ports_attached);

	sem_destroy(&lsi_parameters->ports_attached);

//	rofl::cioloop::run();
	ciosrv::run();

	try
	{
		switch_manager::destroy_switch(dpid);
	} catch (...) {
		ROFL_ERR("[xdpd]["PLUGIN_NAME"] Unable to destroy LSI\n");
		throw;
	}	
	
	pthread_exit(NULL);

	return NULL;
}	

list<string> NodeOrchestrator::discoverPhyPorts()
{
	list<string> availablePorts =  port_manager::list_available_port_names();
	list<string>::iterator port = availablePorts.begin();
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Number of ports available: %d\n", availablePorts.size());
	for(; port != availablePorts.end(); port++)
	{
		ROFL_INFO("[xdpd]["PLUGIN_NAME"]%s\n",(*port).c_str());	
	}
	
	return availablePorts;
}

pair<unsigned int, unsigned int> NodeOrchestrator::createVirtualLink(uint64_t dpid_a,uint64_t dpid_b)
{
	string name_port_a;
	string name_port_b;
	unsigned int port_a, port_b;
	port_manager::connect_switches(dpid_a, name_port_a, dpid_b, name_port_b);
	
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Virtual link created - %x:%s <-> %x:%s\n", dpid_a,name_port_a.c_str(),dpid_b,name_port_b.c_str());
	
	//FIXME: tmp code;
	if(last_ports_id.count(dpid_a) == 0)
		port_a = 1;
	else
		port_a = last_ports_id[dpid_a] + 1;
	last_ports_id[dpid_a] = port_a;
	
	if(last_ports_id.count(dpid_b) == 0)
		port_b = 1;
	else
		port_b = last_ports_id[dpid_b] + 1;
	last_ports_id[dpid_b] = port_b;
			
	return make_pair(port_a,port_b);
}

unsigned int NodeOrchestrator::createNfPort(uint64_t dpid, string NfName, string NfPortName, PexType type)
{
	unsigned int port_number = 0;

	pex_manager::create_pex_port(NfName, NfPortName,type);	
	port_manager::attach_port_to_switch(dpid, NfPortName, &port_number);
	
	//FIXME: tmp code
	last_ports_id[dpid] = port_number;
	
	port_manager::bring_up(NfPortName);
	
	ROFL_INFO("[xdpd]["PLUGIN_NAME"] Port '%s' attached to port %d of LSI '%x'\n",NfPortName.c_str(),port_number,dpid);	
	
	return port_number;
}

void NodeOrchestrator::destroyLSI(uint64_t dpid)
{
	assert(threads.count(dpid) != 0);

	pthread_kill(threads[dpid],SIGUSR1);

	threads.erase(threads.find(dpid));

}

