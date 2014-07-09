/**
* This is a unit test that must check the proper 
* funcionality of datapacket_storage
*
*/

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>

#include <stdio.h>
#include <unistd.h>
#include <rofl/common/cmemory.h>
#include <rofl/datapath/pipeline/common/large_types.h>
#include "io/packet_classifiers/c_pktclassifier/headers/cpc_tcp.h"


using namespace std;

class ChecksumTestCase : public CppUnit::TestFixture{
	CPPUNIT_TEST_SUITE(ChecksumTestCase);
	CPPUNIT_TEST(test_rfc1071_checksum);
	CPPUNIT_TEST_SUITE_END();
	
	void test_rfc1071_checksum(void);
	
public:
	void setUp(void);
	void tearDown(void);
};

void ChecksumTestCase::setUp(){
	fprintf(stderr,"<%s:%d> ************** Set up ************\n",__func__,__LINE__);
}

void ChecksumTestCase::tearDown(){
	fprintf(stderr,"<%s:%d> ************** Tear Down ************\n",__func__,__LINE__);
}

/* Tests */
void ChecksumTestCase::test_rfc1071_checksum(void )
{
	fprintf(stderr,"<%s:%d> ************** Test Checksum RFC1071 ************\n",__func__,__LINE__);

	uint32_t ip_src 	= 0xa1a2a3a4;
	uint32_t ip_dst 	= 0xb1b2b3b4;
	uint16_t ip_proto	= 6;
	uint16_t length		= 32;

	rofl::cmemory mem(2*sizeof(uint32_t) + 2*sizeof(uint16_t) + length);
	
	uint32_t* p_ip_src 		= (uint32_t*)&mem[0];
	uint32_t* p_ip_dst 		= (uint32_t*)&mem[4];
	uint16_t* p_ip_proto 	= (uint16_t*)&mem[8];
	uint16_t* p_length 		= (uint16_t*)&mem[10];

	*p_ip_src 	= htobe32(ip_src);
	*p_ip_dst 	= htobe32(ip_dst);
	*p_ip_proto	= htobe16(ip_proto);
	*p_length 	= htobe16(length);

	for (unsigned int i = 0; i < length; i++) {
		mem[12+i] = i;
	}

	cpc_tcp_hdr_t* hdr = (cpc_tcp_hdr_t*)&mem[12];
	hdr->checksum = 0x0000;

	std::cerr << mem;

	uint16_t sum = ietf_calc_checksum(mem.somem(), mem.memlen());

	std::cerr << "RFC1071 sum: 0x" << std::hex << (unsigned int)sum << std::dec << std::endl;

	tcpv4_calc_checksum((void*)&mem[12], ip_src, ip_dst, ip_proto, length);

	std::cerr << "tcpv4_calc_checksum() sum: " << std::hex <<
			(unsigned int)(hdr->checksum) << std::dec << std::endl;

	std::cerr << mem;

	CPPUNIT_ASSERT(true);
}

/*
* Test MAIN
*/
int main( int argc, char* argv[] )
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(ChecksumTestCase::suite()); // Add the top suite to the test runner
	runner.setOutputter(
			new CppUnit::CompilerOutputter(&runner.result(), std::cerr));

	// Run the test and don't wait a key if post build check.
	bool wasSuccessful = runner.run( "" );
	
	std::cerr<<"************** Test finished ************"<<std::endl;

	// Return error code 1 if the one of test failed.
	return wasSuccessful ? 0 : 1;
}
