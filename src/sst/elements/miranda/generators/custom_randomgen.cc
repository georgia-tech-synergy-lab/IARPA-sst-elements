// Copyright 2009-2021 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2021, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.
//
// Author: Anshuman
// Email: anshuman23307@gmail.com


#include <sst_config.h>
#include <sst/core/params.h>
#include <sst/core/rng/marsaglia.h>
#include <sst/elements/miranda/generators/custom_randomgen.h>
#include <time.h>
#include <cstdlib>
#include <stdlib.h>

using namespace SST::Miranda;
using namespace std;


CustomRandomGenerator::CustomRandomGenerator( ComponentId_t id, Params& params ) :
	RequestGenerator(id, params) {
            build(params);
        }

void CustomRandomGenerator::build(Params& params) {
	const uint32_t verbose = params.find<uint32_t>("verbose", 0);

	out = new Output("CustomRandomGenerator[@p:@l]: ", verbose, 0, Output::STDOUT);

    seed_a              = params.find<uint64_t>("seed_a", 11);
    seed_b              = params.find<uint64_t>("seed_b", 31);
	issueCount          = params.find<uint64_t>("count", 1000);
	reqLength           = params.find<uint64_t>("length", 8);
	maxAddr             = params.find<uint64_t>("max_address", 524288);
	addrRangeMaxVal     = params.find<uint64_t>("avoid_addr_range_max_value", 0);
	addrRangeMaxVal1    = params.find<uint64_t>("avoid_addr_range_max_value1", 0);
	addrRangeMaxVal2    = params.find<uint64_t>("avoid_addr_range_max_value2", 0);
	addrRangeMaxVal3    = params.find<uint64_t>("avoid_addr_range_max_value3", 0);
	addrRangeMinVal     = params.find<uint64_t>("avoid_addr_range_min_value", 0);
	addrRangeMinVal1    = params.find<uint64_t>("avoid_addr_range_min_value1", 0);
	addrRangeMinVal2    = params.find<uint64_t>("avoid_addr_range_min_value2", 0);
	addrRangeMinVal3    = params.find<uint64_t>("avoid_addr_range_min_value3", 0);

	writePercentage    = params.find<uint64_t>("write_percentage", 50);

	option    = params.find<uint64_t>("option", 1);

	rng = new MarsagliaRNG(seed_a, seed_b);

	out->verbose(CALL_INFO, 1, 0, "Will issue %" PRIu64 " operations\n", issueCount);
	out->verbose(CALL_INFO, 1, 0, "Request lengths: %" PRIu64 " bytes\n", reqLength);
	out->verbose(CALL_INFO, 1, 0, "Maximum address: %" PRIu64 "\n", maxAddr);
	out->verbose(CALL_INFO, 1, 0, "WritePercentage: %" PRIu64 "\n", writePercentage);

	issueOpFences = params.find<std::string>("issue_op_fences", "yes") == "yes";

}

CustomRandomGenerator::~CustomRandomGenerator() {
	delete out;
	delete rng;
}

void CustomRandomGenerator::generate(MirandaRequestQueue<GeneratorRequest*>* q) {
	out->verbose(CALL_INFO, 4, 0, "Generating next request number: %" PRIu64 "\n", issueCount);

	uint64_t addr;
	uint64_t rand_addr;

	do {
	    rand_addr = rng->generateNextUInt64() % (maxAddr * 1024); // Generating address in the KB granularity
	    addr = rand_addr * 1024;
	} while (
		(rand_addr >= addrRangeMinVal * 1024) && (rand_addr < addrRangeMaxVal * 1024) ||
		(rand_addr >= addrRangeMinVal1 * 1024) && (rand_addr < addrRangeMaxVal1 * 1024) ||
		(rand_addr >= addrRangeMinVal2 * 1024) && (rand_addr < addrRangeMaxVal2 * 1024) ||
		(rand_addr >= addrRangeMinVal3 * 1024) && (rand_addr < addrRangeMaxVal3 * 1024)
		);

	const double op_decide = rng->nextUniform();

	out->verbose(CALL_INFO, 4, 0, "addrRangeMinVal: %" PRIu64 "\n", addrRangeMinVal);
	out->verbose(CALL_INFO, 4, 0, "addrRangeMaxVal: %" PRIu64 "\n", addrRangeMaxVal);
	out->verbose(CALL_INFO, 4, 0, "Generating next request number: %" PRIu64 "\n", issueCount);
	out->verbose(CALL_INFO, 4, 0, "CustomRandomGenerator::maxAddr: %" PRIu64 "\n", maxAddr);
	out->verbose(CALL_INFO, 4, 0, "CustomRandomGenerator::rand_addr: %" PRIu64 "\n", rand_addr);
	out->verbose(CALL_INFO, 4, 0, "CustomRandomGenerator::addr: %" PRIu64 "\n", addr);
	out->verbose(CALL_INFO, 4, 0, "CustomRandomGenerator::op_decide: %f\n", op_decide);

    MemoryOpRequest* readAddr = new MemoryOpRequest(addr, reqLength, READ);
    MemoryOpRequest* writeAddr = new MemoryOpRequest(addr, reqLength, WRITE);

	if(option == 1) // READ only
	{
    	q->push_back(readAddr);
	}
	else if(option == 2) // WRITE only
	{
    	q->push_back(writeAddr);
	}
	else if(option == 3) // WRITE after READ
	{
	    writeAddr->addDependency(readAddr->getRequestID());
	    q->push_back(readAddr);
    	q->push_back(writeAddr);
	}
	else if(option == 4) // READ after WRITE 
	{
    	q->push_back(writeAddr);
	    q->push_back(readAddr);
	}
	else if(option == 5) // Randomly mixed 
	{
		q->push_back(new MemoryOpRequest(addr, reqLength, ((op_decide*100 < writePercentage) ? READ : WRITE)));
	}

	if (issueOpFences) {
	    q->push_back(new FenceOpRequest());
	}

	issueCount--;
}

bool CustomRandomGenerator::isFinished() {
	return (issueCount == 0);
}

void CustomRandomGenerator::completed() {

}
