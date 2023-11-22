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

// TODO: Anshuman: update it later
#ifndef ROUND_ROBIN_ARBITER_H_
#define ROUND_ROBIN_ARBITER_H_

#include <queue>
#include <map>
#include <string>
#include <sstream>

#include <sst/core/event.h>
#include <sst/core/sst_types.h>
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/timeConverter.h>
#include <sst/core/output.h>

#include "sst/elements/memHierarchy/memTypes.h"
#include "sst/elements/memHierarchy/util.h"
#include "sst/elements/memHierarchy/memLinkBase.h"

namespace SST { namespace MemHierarchy {

using namespace std;

/*
 * Component: memHierarchy.RoundRobinArbiter
 *
 */

class RoundRobinArbiter : public SST::Component {
public:
    typedef enum {PORT1, PORT2, PORT3, OUTPORT} PortNum;

    /* Element Library Info */
    SST_ELI_REGISTER_COMPONENT(RoundRobinArbiter, "memHierarchy", "RoundRobinArbiter", SST_ELI_ELEMENT_VERSION(1,0,0), "Round Robin Arbiter", COMPONENT_CATEGORY_MEMORY)

    SST_ELI_DOCUMENT_PARAMS(
            /* Required */
            {"port1minAddr",            "(uint) Minimum value of the address serviced by Port 1. Specify in MB.", "10"},
            {"port1maxAddr",            "(uint) Maximum value of the address serviced by Port 1. Specify in MB.", "1023"},
            {"port2minAddr",            "(uint) Minimum value of the address serviced by Port 2. Specify in MB.", "1024"},
            {"port2maxAddr",            "(uint) Maximum value of the address serviced by Port 2. Specify in MB.", "2047"},
            {"port3minAddr",            "(uint) Minimum value of the address serviced by Port 3. Specify in MB.", "2048"},
            {"port3maxAddr",            "(uint) Maximum value of the address serviced by Port 3. Specify in MB.", "3071"},
            {"min_packet_size",         "(string) Number of bytes in a request/response not including payload (e.g., addr + cmd). Specify in B.", "8B"},
            {"cache_line_size",         "(uint) Size of a cache line (aka cache block) in bytes.", "64"})

    SST_ELI_DOCUMENT_PORTS(
	    // TODO: Anshuman: What are split networks?
            {"port1",                   "Network link port to one of the component on the CPU side",			      {"memHierarchy.MemRtrEvent"} },
            {"port2",                   "Network link port to one of the component on the CPU side",			      {"memHierarchy.MemRtrEvent"} },
            {"port3",                   "Network link port to one of the component on the CPU side",			      {"memHierarchy.MemRtrEvent"} },
            {"directory",               "Network link port to directory; doubles as request network port for split networks", {"memHierarchy.MemRtrEvent"} })

    SST_ELI_DOCUMENT_STATISTICS(
	    // TODO: Anshuman: decide between events and counts
            {"TotalSnoopsReceived",     "Total number of snoops received from the directory", "events", 1},
            {"TotalSnoopsFiltered",     "Total number of snoops filtered from the directory", "events", 1},
            {"TotalEventsReceived",     "", "events", 1},
            {"TotalEventsReplayed",     "", "events", 1},
            {"EventsPushedQueue_1",     "", "events", 1},
            {"EventsPushedQueue_2",     "", "events", 1},
            {"EventsPushedQueue_3",     "", "events", 1},
            {"EventsPushedQueue_4",     "", "events", 1},
            {"EventsPushedQueue_5",     "", "events", 1},
            {"EventsPushedQueue_6",     "", "events", 1},
            {"EventsPushedQueue_7",     "", "events", 1},
            {"EventsPushedQueue_8",     "", "events", 1},
            {"EventsPushedQueue_9",     "", "events", 1},
            {"EventsPushedQueue_10",     "", "events", 1},
            {"EventsPushedQueue_11",     "", "events", 1},
            {"EventsPushedQueue_12",     "", "events", 1},
            {"EventsPushedQueue_13",     "", "events", 1},
            {"EventsPushedQueue_14",     "", "events", 1},
            {"EventsPushedQueue_15",     "", "events", 1},
            {"EventsPushedQueue_16",     "", "events", 1},
            {"MaxDepthPushedQueue_1",     "", "events", 1},
            {"MaxDepthPushedQueue_2",     "", "events", 1},
            {"MaxDepthPushedQueue_3",     "", "events", 1},
            {"MaxDepthPushedQueue_4",     "", "events", 1},
            {"MaxDepthPushedQueue_5",     "", "events", 1},
            {"MaxDepthPushedQueue_6",     "", "events", 1},
            {"MaxDepthPushedQueue_7",     "", "events", 1},
            {"MaxDepthPushedQueue_8",     "", "events", 1},
            {"MaxDepthPushedQueue_9",     "", "events", 1},
            {"MaxDepthPushedQueue_10",     "", "events", 1},
            {"MaxDepthPushedQueue_11",     "", "events", 1},
            {"MaxDepthPushedQueue_12",     "", "events", 1},
            {"MaxDepthPushedQueue_13",     "", "events", 1},
            {"MaxDepthPushedQueue_14",     "", "events", 1},
            {"MaxDepthPushedQueue_15",     "", "events", 1},
            {"MaxDepthPushedQueue_16",     "", "events", 1},
            {"default_stat",            "Default statistic. If not 0 then a statistic is missing", "", 1})

    /*********************************************************************************
     * Event handlers - one per event type
     * Handlers return whether the event was accepted (true) or rejected (false)
     *********************************************************************************/
    virtual bool handleCMD(MemEventBase * event, PortNum srcPort);
    //virtual bool handleCMD(MemEvent * event, PortNum srcPort);

    /*********************************************************************************
     * Send outgoing events
     *********************************************************************************/

    /* Send commands when their timestamp expires. Return whether queue is empty or not */
    virtual bool sendOutgoingEvents();

    ///* Forward an event using memory address to locate a destination. */
    virtual void forwardByAddress(MemEventBase * event, Cycle_t ts, PortNum srcPort);    // ts specifies the send time

    /*********************************************************************************
     * Initialization/finish functions used by parent
     *********************************************************************************/

    /*
     * Get the InitCoherenceEvent
     * Event contains information about the protocol configuration and so is generated by coherence managers
     */
    MemEventInitCoherence * getInitCoherenceEvent();

    /*********************************************************************************
     * Miscellaneous functions used by parent
     *********************************************************************************/

    /* For clock handling = parent updates timestamp when the clock is re-enabled */
    void updateTimestamp(uint64_t newTS) { timestamp_ = newTS; }

    /* Check whether the event queues are empty/subcomponent is doing anything */
    bool checkIdle();

/* Class definition */

    /** Constructor for RoundRobinArbiter Component */
    RoundRobinArbiter(ComponentId_t id, Params &params);
    ~RoundRobinArbiter() { }

    /** Component API - pre- and post-simulation */
    virtual void init(unsigned int);
    virtual void setup(void);
    virtual void finish(void);

    /** Component API - debug and fatal */
    void printStatus(Output & out); // Called on SIGUSR2
    void emergencyShutdown();       // Called on output.fatal(), SIGINT/SIGTERM

    /* Arbiter name - used for identifying where events came from/are going to */
    std::string arbitername_;

    /* Cache parameters that are often needed by coherence managers */
    uint64_t lineSize_;

    // TODO: Anshuman: Are these needed?
    struct LatencyStat{
        uint64_t time;
        Command cmd;
        int missType;
        LatencyStat(uint64_t t, Command c, int m) : time(t), cmd(c), missType(m) { }
    };

    std::map<SST::Event::id_type, LatencyStat> startTimes_;

    typedef std::pair<uint64_t, int> id_type;
    std::map<std::pair<Addr,id_type>, PortNum>     addrPortMap_;
    std::map<std::pair<Addr,id_type>, std::string> addrDestMap_; // Used to figure out the destination of a response event

    virtual void recordIncomingRequest(MemEventBase* event);

    /* Response structure - used for outgoing event queues */
    struct Response {
        MemEventBase* event;    // Event to send
        uint64_t deliveryTime;  // Time this event can be sent
        uint64_t size;          // Size of event (for bandwidth accounting)
    };

private:
    /** RoundRobinArbiter factory methods **************************************************/

    // Create clock
    void createCSRsnoopFilterTable(Params &params);

    // Create clock
    void createClock(Params &params);

    // Statistic initialization
    void registerStatistics();

    // Configure links
    void configureLinks(Params &params, TimeConverter* tc);

    // Handle incoming events -> prepare to process
    void handleEventUpPort1(SST::Event *event);
    void handleEventUpPort2(SST::Event *event);
    void handleEventUpPort3(SST::Event *event);
    void handleEventDown(SST::Event *event);

    // Process events
    bool processEvent(MemEventBase * ev, PortNum srcPort);

    // Clock handler
    bool clockTick(Cycle_t time);

    // Clock helpers - turn clock on & off
    void turnClockOn();
    void turnClockOff();

    /** RoundRobinArbiter structures *******************************************************/
    MemLinkBase* 	linkUpPort1_;                   // link manager up (towards element)
    MemLinkBase* 	linkUpPort2_;                   // link manager up (towards element)
    MemLinkBase* 	linkUpPort3_;                   // link manager up (towards element)
    MemLinkBase* 	linkDown_;                      // link manager down (towards directory)

    /** States *****************************************************************/
    uint64_t            timestamp_;
    int 	        maxRequestsPerCycle_;

    MemRegion           regionPort1_;                   // Memory region handled by this arbiter
    MemRegion           regionPort2_;                   // Memory region handled by this arbiter
    MemRegion           regionPort3_;                   // Memory region handled by this arbiter
    MemRegion           regionPort4_;                   // Memory region handled by this arbiter

    /* init marks */
    int init1;
    int init2;
    int init3;

    /* Group ID of Port 4 towards NIC */
    int 		overrideGroupID;

    /* If there is cache between CPU and arbiter */
    bool        isCacheConnected;

    /** Address spaces for the 3 uplink ports **********************************/
    uint64_t		port1minAddr;
    uint64_t		port2minAddr;
    uint64_t		port3minAddr;
    uint64_t		port1maxAddr;
    uint64_t		port2maxAddr;
    uint64_t		port3maxAddr;

    /** Address spaces allowed to access the 3 uplink ports **********************************/
    uint64_t		allowedMinAddr;
    uint64_t		allowedMaxAddr;

    /** Memory interleaving size **********************************/
    string ilSize;
    string ilStep;

    /** Response Queues *****************************************************************/
    list<Response>      outgoingEventQueueDownPort1_;
    list<Response>      outgoingEventQueueDownPort2_;
    list<Response>      outgoingEventQueueDownPort3_;
    list<Response>      outgoingEventQueueUpPort1FromOutPort_;
    list<Response>      outgoingEventQueueUpPort1FromPort2_;
    list<Response>      outgoingEventQueueUpPort1FromPort3_;
    list<Response>      outgoingEventQueueUpPort2FromOutPort_;
    list<Response>      outgoingEventQueueUpPort2FromPort1_;
    list<Response>      outgoingEventQueueUpPort2FromPort3_;
    list<Response>      outgoingEventQueueUpPort3FromOutPort_;
    list<Response>      outgoingEventQueueUpPort3FromPort1_;
    list<Response>      outgoingEventQueueUpPort3FromPort2_;

    /** Output and debug *******************************************************/
    Output*             out_;

    /** Clocks *****************************************************************/
    Clock::Handler<RoundRobinArbiter>*  clockHandler_;
    TimeConverter*                      defaultTimeBase_;
    bool                                clockIsOn_;     // Whether clock is on or off ... Not used currently
    SimTime_t                           lastActiveClockCycle_;  // Cycle we turned the clock off at - for re-syncing stats

    /** Event Buffers *****************************************************************/
    std::list<MemEventBase*>            eventBufferFromLinkUpPort1_; // Buffer for registering the events
    std::list<MemEventBase*>            eventBufferFromLinkUpPort2_; // Buffer for registering the events
    std::list<MemEventBase*>            eventBufferFromLinkUpPort3_; // Buffer for registering the events
    std::list<MemEventBase*>            eventBufferFromLinkDown_;    // Buffer for registering the events

    // Event counts
    // TODO: Anshuman: Add stat support for arbiter later
    Statistic<uint64_t>* stat_RecvEvents;
    Statistic<uint64_t>* stat_RetryEvents;
    Statistic<uint64_t>* stat_Buff[16];
    Statistic<uint64_t>* stat_BuffMaxDepth[16];
    uint64_t BuffMaxDepth[16];
    Statistic<uint64_t>* statSFCacheRecv[(int)Command::LAST_CMD];

    /* Add a new event to the outgoing command queue towards memory */
    virtual void addToOutgoingQueueDown(Response& resp, PortNum srcPort, PortNum destPort);

    /* Add a new event to the outgoing command queue towards the CPU */
    virtual void addToOutgoingQueueUp(Response& resp, PortNum srcPort, PortNum destPort);

protected:

    /* Forward a message to a lower memory level (towards memory) */
    uint64_t packetHeaderBytes;
    uint64_t forwardMessage(MemEventBase * event, unsigned int requestSize, uint64_t baseTime, PortNum srcPort, vector<uint8_t>* data, Command fwdCmd = Command::LAST_CMD);
    //uint64_t forwardMessage(MemEvent * event, unsigned int requestSize, uint64_t baseTime, PortNum srcPort, vector<uint8_t>* data, Command fwdCmd = Command::LAST_CMD);

};

}}
#endif // ROUND_ROBIN_ARBITER_H_
