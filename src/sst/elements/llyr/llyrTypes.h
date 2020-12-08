// Copyright 2009-2020 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2020, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#ifndef _LLYR_TYPES
#define _LLYR_TYPES

#include <bitset>
#include <sst/core/interfaces/simpleMem.h>

#define Bit_Length 64
typedef std::bitset< Bit_Length > LlyrData;

using namespace SST::Interfaces;

namespace SST {
namespace Llyr {

// forward declaration of LSQueue
class LSQueue;

// data type to pass between Llyr, mapper, and PEs
typedef struct alignas(64) {
    LSQueue*    lsqueue_;
    SimpleMem*  mem_interface_;

    uint32_t    verbosity_;
    uint16_t    queueDepth_;
    uint16_t    arith_latency_;
    uint16_t    int_latency_;
    uint16_t    fp_latency_;
    uint16_t    fp_mul_Latency_;
    uint16_t    fp_div_Latency_;
} LlyrConfig;

typedef enum {
    ANY,
    LD,
    LD_ST,
    ST,
    ANY_LOGIC = 0x40,
    SLL,
    SLR,
    ROL,
    ROR,
    ANY_INT = 0x80,
    ADD,
    SUB,
    MUL,
    DIV,
    ANY_FP = 0xC0,
    FPADD,
    FPSUB,
    FPMUL,
    FPDIV,
    DUMMY = 0xFF,
    BUFFER,
    OTHER
} opType;

}//Llyr
}//SST

#endif
