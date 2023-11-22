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


#ifndef _H_SST_MIRANDA_RANDOM_GEN
#define _H_SST_MIRANDA_RANDOM_GEN

#include <sst/elements/miranda/mirandaGenerator.h>
#include <sst/core/output.h>
#include <sst/core/rng/rng.h>

#include <queue>

using namespace SST::RNG;

namespace SST {
namespace Miranda {

class CustomRandomGenerator : public RequestGenerator {

public:
	CustomRandomGenerator( ComponentId_t id, Params& params );
        void build(Params& params);
	~CustomRandomGenerator();
	void generate(MirandaRequestQueue<GeneratorRequest*>* q);
	bool isFinished();
	void completed();

	SST_ELI_REGISTER_SUBCOMPONENT_DERIVED(
                CustomRandomGenerator,
                "miranda",
                "CustomRandomGenerator",
                SST_ELI_ELEMENT_VERSION(1,0,0),
                "Creates a random stream of accesses to/from memory",
                SST::Miranda::RequestGenerator
        )

	SST_ELI_DOCUMENT_PARAMS(
		{ "verbose",          			"Sets the verbosity output of the generator", "0" },
   	 	{ "seed_a",           "Sets the seed-a for the random generator", "11" },
    		{ "seed_b",           "Sets the seed-b for the random generator", "31" },
    		{ "count",            			"Count for number of items being requested", "1024" },
    		{ "length",           			"Length of requests", "8" },
    		{ "write_percentage",      		"Percentage of write operations", "50" },
    		{ "max_address",      			"Maximum address allowed for generation", "16384" },
    		{ "avoid_addr_range_min_value",      	"Minimum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_min_value1",      	"Minimum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_min_value2",      	"Minimum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_min_value3",      	"Minimum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_max_value",      	"Maximum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_max_value1",      	"Maximum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_max_value2",      	"Maximum address of the address range that should not be generated", "0" },
    		{ "avoid_addr_range_max_value3",      	"Maximum address of the address range that should not be generated", "0" },
    		{ "issue_op_fences",  			"Issue operation fences, \"yes\" or \"no\", default is yes", "yes" }
        )
private:
	uint64_t seed_a;
	uint64_t seed_b;
	uint64_t reqLength;
	uint64_t maxAddr;
	uint64_t addrRangeMaxVal;
	uint64_t addrRangeMaxVal1;
	uint64_t addrRangeMaxVal2;
	uint64_t addrRangeMaxVal3;
	uint64_t addrRangeMinVal;
	uint64_t addrRangeMinVal1;
	uint64_t addrRangeMinVal2;
	uint64_t addrRangeMinVal3;
	uint64_t issueCount;
	uint64_t writePercentage;
    uint64_t option;
	bool issueOpFences;
	Random* rng;
	Output*  out;

};

}
}

#endif
