/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OF_ENDPOINT_H
#define OF_ENDPOINT_H 

#include <map>
#include <vector>
#include <string>
#include <iostream>

#include <pthread.h>
#include <stdio.h>
#include <limits.h>
#include <endian.h>
#include <string.h>
#include <time.h>

#include <stdexcept>

#include <rofl/common/crofbase.h>
#include <rofl/datapath/afa/afa.h>
//#include "openflow_switch.h"

/**
* @file of_endpoint.h
* @author Marc Sune<marc.sune (at) bisdn.de>
* @author Andreas Koepsel<andreas.koepsel (at) bisdn.de>
* 
* @brief OpenFlow endpoint abstraction. 
*/

using namespace rofl;

namespace xdpd {

//Fwd declaration
class openflow_switch;

/**
* @brief of_endpoint is an OpenFlow version-agnostic abstraction of the OF agent.
* @ingroup cmm_of
*
* @description An OpenFlow endpoint is not a single connection endpoint, but rather the agent which
* manages both ACTIVE and PASSIVE mode connections
*/
class of_endpoint : public crofbase {

public:

	of_endpoint(cofhello_elem_versionbitmap const& versionbitmap = cofhello_elem_versionbitmap()) : crofbase(versionbitmap), sw(NULL) {};

	virtual ~of_endpoint() {};

	/*
	* Port notifications
	*/

	virtual rofl_result_t notify_port_attachment(const switch_port_t* port)=0;
	
	virtual rofl_result_t notify_port_detachment(const switch_port_t* port)=0;

	virtual rofl_result_t notify_port_status_changed(const switch_port_t* port)=0;



protected:
	
	//Switch to which the endpoint belongs to
	openflow_switch* sw;	
};

}// namespace rofl

#endif /* OF_ENDPOINT_H_ */
