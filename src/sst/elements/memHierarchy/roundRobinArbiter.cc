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
// Author: Joongun Park
// Email: jpark3234@gatech.edu

#include <sst_config.h>
#include <sst/core/params.h>
#include <sst/core/simulation.h>
#include <sst/core/interfaces/stringEvent.h>
#include <sst/core/timeLord.h>

#include "memEvent.h"
#include "roundRobinArbiter.h"

using namespace SST;
using namespace SST::MemHierarchy;

RoundRobinArbiter::RoundRobinArbiter(ComponentId_t id, Params &params) : Component(id) {

    /* --------------- Output Class --------------- */
    out_ = new Output();
    out_->init("", params.find<int>("verbose", 1), 0, Output::STDOUT);

    /* init flags */
    init1 = 0;
    init2 = 0;
    init3 = 0;

    /* Pull out parameters that the arbiter keeps */
    lineSize_              = params.find<uint64_t>("cache_line_size", 64);
    arbitername_           = getParentComponentName();
    UnitAlgebra packetSize = UnitAlgebra(params.find<std::string>("min_packet_size", "8B")); //TODO: What to do with min_packet_size
    packetHeaderBytes      = packetSize.getRoundedValue();
    maxRequestsPerCycle_   = params.find<int>("max_requests_per_cycle",-1);
    overrideGroupID        = params.find<uint64_t>("overrideGroupID", 2);
    isCacheConnected = params.find<bool>("isCacheConnected", false);

    port1minAddr= params.find<uint64_t>("port1minAddr", 10);
    port2minAddr= params.find<uint64_t>("port2minAddr", 1024);
    port3minAddr= params.find<uint64_t>("port3minAddr", 2048);
    port1maxAddr= params.find<uint64_t>("port1maxAddr", 1023);
    port2maxAddr= params.find<uint64_t>("port2maxAddr", 2047);
    port3maxAddr= params.find<uint64_t>("port3maxAddr", 3071);

    allowedMinAddr= params.find<uint64_t>("allowedMinAddr", 0);
    allowedMaxAddr= params.find<uint64_t>("allowedMaxAddr", 4095);

    ilSize   = params.find<std::string>("interleave_size", "0B");
    ilStep   = params.find<std::string>("interleave_step", "0B");

    /* Create clock, deadlock timeout, etc. */
    createClock(params);

    /* Configure links */
    configureLinks(params, defaultTimeBase_); // Must be called after createClock so timeebase is initialized

    /* Register statistics */
    registerStatistics();
}

/**************************************/
/******** Statistics handling *********/
/**************************************/

void RoundRobinArbiter::recordIncomingRequest(MemEventBase* event) {
    // Default type is -1
    LatencyStat lat(timestamp_, event->getCmd(), -1);
    startTimes_.insert(std::make_pair(event->getID(), lat));
}

/**************************************************************************
 * Handlers for various links
 * linkUp/down -> handleEvent()
 **************************************************************************/

void RoundRobinArbiter::handleEventUpPort1(SST::Event * ev) {
    MemEventBase* event = static_cast<MemEventBase*>(ev);
    if (!clockIsOn_)
        turnClockOn();

    // Record the time at which requests arrive for latency statistics
//    if (CommandClassArr[(int)event->getCmd()] == CommandClass::Request && !CommandWriteback[(int)event->getCmd()])
        this->recordIncomingRequest(event);

    // TODO: Add statistics recording here
    // Record that an event was received
    if (MemEventTypeArr[(int)event->getCmd()] != MemEventType::Cache || event->queryFlag(MemEventBase::F_NONCACHEABLE)) {
    } else {
    }

    eventBufferFromLinkUpPort1_.push_back(event);
    stat_Buff[0]->addData(1);
    if (BuffMaxDepth[0] < eventBufferFromLinkUpPort1_.size())
    {
        stat_BuffMaxDepth[0]->addData(eventBufferFromLinkUpPort1_.size()-BuffMaxDepth[0]);
        BuffMaxDepth[0] = eventBufferFromLinkUpPort1_.size();
    }

    MemEvent * eventType = static_cast<MemEvent*>(event);
    if (eventType->isDataRequest()) {
        if (addrPortMap_.insert({std::make_pair(eventType->getBaseAddr(),event->getID()),PortNum::PORT1}).second == false)
            out_->fatal(CALL_INFO, -1, "%s, Failed to insert request into address port map.\n", getName().c_str());
    }
}


void RoundRobinArbiter::handleEventUpPort2(SST::Event * ev) {
    MemEventBase* event = static_cast<MemEventBase*>(ev);
    if (!clockIsOn_)
        turnClockOn();

    // Record the time at which requests arrive for latency statistics
//    if (CommandClassArr[(int)event->getCmd()] == CommandClass::Request && !CommandWriteback[(int)event->getCmd()])
        this->recordIncomingRequest(event);

    // Record that an event was received
    if (MemEventTypeArr[(int)event->getCmd()] != MemEventType::Cache || event->queryFlag(MemEventBase::F_NONCACHEABLE)) {
        //statSFUncacheRecv[(int)event->getCmd()]->addData(1);
    } else {
    // TODO: Anshuman: Uncomment this
        //statSFCacheRecv[(int)event->getCmd()]->addData(1);
    }

    eventBufferFromLinkUpPort2_.push_back(event);
    stat_Buff[1]->addData(1);
    if (BuffMaxDepth[1] < eventBufferFromLinkUpPort2_.size())
    {
        stat_BuffMaxDepth[1]->addData(eventBufferFromLinkUpPort2_.size()-BuffMaxDepth[1]);
        BuffMaxDepth[1] = eventBufferFromLinkUpPort2_.size();
    }

    MemEvent * eventType = static_cast<MemEvent*>(event);
    if (eventType->isDataRequest()) {
        if (addrPortMap_.insert({std::make_pair(eventType->getBaseAddr(),event->getID()),PortNum::PORT2}).second == false)
            out_->fatal(CALL_INFO, -1, "%s, Failed to insert request into address port map.\n", getName().c_str());
    }
}


void RoundRobinArbiter::handleEventUpPort3(SST::Event * ev) {
    MemEventBase* event = static_cast<MemEventBase*>(ev);
    if (!clockIsOn_)
        turnClockOn();

    // Record the time at which requests arrive for latency statistics
//    if (CommandClassArr[(int)event->getCmd()] == CommandClass::Request && !CommandWriteback[(int)event->getCmd()])
        this->recordIncomingRequest(event);

    // Record that an event was received
    if (MemEventTypeArr[(int)event->getCmd()] != MemEventType::Cache || event->queryFlag(MemEventBase::F_NONCACHEABLE)) {
        //statSFUncacheRecv[(int)event->getCmd()]->addData(1);
    } else {
    // TODO: Anshuman: Uncomment this
        //statSFCacheRecv[(int)event->getCmd()]->addData(1);
    }

    // TODO: Anshuman: Add support for prefetch if needed. Removed as of now.
    eventBufferFromLinkUpPort3_.push_back(event);

    // Update stat
    stat_Buff[2]->addData(1);
    if (BuffMaxDepth[2] < eventBufferFromLinkUpPort3_.size())
    {
        stat_BuffMaxDepth[2]->addData(eventBufferFromLinkUpPort3_.size()-BuffMaxDepth[2]);
        BuffMaxDepth[2] = eventBufferFromLinkUpPort3_.size();
    }

    MemEvent * eventType = static_cast<MemEvent*>(event);
    if (eventType->isDataRequest()) {
        if (addrPortMap_.insert({std::make_pair(eventType->getBaseAddr(),event->getID()),PortNum::PORT3}).second == false)
            out_->fatal(CALL_INFO, -1, "%s, Failed to insert request into address port map.\n", getName().c_str());
    }
}

/* Handle incoming event on the memNIC links */
void RoundRobinArbiter::handleEventDown(SST::Event * ev) {
    MemEventBase* event = static_cast<MemEventBase*>(ev);
    if (!clockIsOn_)
        turnClockOn();

    // Record the time at which requests arrive for latency statistics
//    if (CommandClassArr[(int)event->getCmd()] == CommandClass::Request && !CommandWriteback[(int)event->getCmd()])
        this->recordIncomingRequest(event);

    // Record that an event was received
    if (MemEventTypeArr[(int)event->getCmd()] != MemEventType::Cache || event->queryFlag(MemEventBase::F_NONCACHEABLE)) {
        //statSFUncacheRecv[(int)event->getCmd()]->addData(1);
    } else {
        //statSFCacheRecv[(int)event->getCmd()]->addData(1);
    }

    // TODO: Anshuman: Add support for prefetch if needed. Removed as of now.
    eventBufferFromLinkDown_.push_back(event);

    // update stat
    stat_Buff[3]->addData(1);
    if (BuffMaxDepth[3] < eventBufferFromLinkDown_.size())
    {
        stat_BuffMaxDepth[3]->addData(eventBufferFromLinkDown_.size()-BuffMaxDepth[3]);
        BuffMaxDepth[3] = eventBufferFromLinkDown_.size();
    }

    MemEvent * eventType = static_cast<MemEvent*>(event);
    if (eventType->isDataRequest()) {
        if (addrPortMap_.insert({std::make_pair(eventType->getBaseAddr(),event->getID()),PortNum::OUTPORT}).second == false)
            out_->fatal(CALL_INFO, -1, "%s, Failed to insert request into address port map.\n", getName().c_str());
    }
}

/**************************************************************************
 * Clock handler and management
 **************************************************************************/

/* Clock handler */
bool RoundRobinArbiter::clockTick(Cycle_t time) {
    timestamp_++;

    // Drain any outgoing messages
    bool idle = this->sendOutgoingEvents();

    /* become idle only when all the connected components are idle */
    if (linkUpPort1_)
        idle &= linkUpPort1_->clock();
    if (linkUpPort2_)
        idle &= linkUpPort2_->clock();
    if (linkUpPort3_)
        idle &= linkUpPort3_->clock();
    if (linkDown_)
        idle &= linkDown_->clock();

    int accepted = 0;
    std::list<MemEventBase*>::iterator itport1, itport2, itport3;
    std::list<MemEventBase*>::iterator it;

    it = eventBufferFromLinkUpPort1_.begin();
    while (it != eventBufferFromLinkUpPort1_.end()) {
        idle = false;
//        printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//             getCurrentSimCycle(), timestamp_, getName().c_str(), (*it)->getVerboseString().c_str());
        if (accepted == maxRequestsPerCycle_)
            break;
        Command cmd = (*it)->getCmd();
        if (processEvent(*it, PortNum::PORT1)) {
            accepted++;
            it = eventBufferFromLinkUpPort1_.erase(it);
        } else {
            it++;
            printf("%s a rejected event \n", __func__);
        }
    }

    it = eventBufferFromLinkUpPort2_.begin();
    while (it != eventBufferFromLinkUpPort2_.end()) {
        idle = false;
//        printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//             getCurrentSimCycle(), timestamp_, getName().c_str(), (*it)->getVerboseString().c_str());
        if (accepted == maxRequestsPerCycle_)
            break;
        Command cmd = (*it)->getCmd();
        if (processEvent(*it, PortNum::PORT2)) {
            accepted++;
            it = eventBufferFromLinkUpPort2_.erase(it);
        } else {
            it++;
            printf("%s a rejected event \n", __func__);
        }
    }

    it = eventBufferFromLinkUpPort3_.begin();
    while (it != eventBufferFromLinkUpPort3_.end()) {
        idle = false;
//        printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//             getCurrentSimCycle(), timestamp_, getName().c_str(), (*it)->getVerboseString().c_str());
        if (accepted == maxRequestsPerCycle_)
            break;
        Command cmd = (*it)->getCmd();
        if (processEvent(*it, PortNum::PORT3)) {
              accepted++;
            it = eventBufferFromLinkUpPort3_.erase(it);
        } else {
            it++;
            printf("%s a rejected event \n", __func__);
        }
    }

    it = eventBufferFromLinkDown_.begin();
    while (it != eventBufferFromLinkDown_.end()) {
        idle = false;
//        printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//             getCurrentSimCycle(), timestamp_, getName().c_str(), (*it)->getVerboseString().c_str());
        if (accepted == maxRequestsPerCycle_)
            break;
        Command cmd = (*it)->getCmd();
        if (processEvent(*it, PortNum::OUTPORT)) {
            accepted++;
            it = eventBufferFromLinkDown_.erase(it);
        } else {
            it++;
            printf("%s a rejected event \n", __func__);
        }
    }

    idle &= eventBufferFromLinkUpPort1_.empty();
    idle &= eventBufferFromLinkUpPort2_.empty();
    idle &= eventBufferFromLinkUpPort3_.empty();
    idle &= eventBufferFromLinkDown_.empty();

    //printf("Tick \n", __func__);
    if (idle && clockIsOn_) {
//        turnClockOff();
//        return true;
    }
    return false;
}

void RoundRobinArbiter::turnClockOn() {
    Cycle_t time = reregisterClock(defaultTimeBase_, clockHandler_);
    timestamp_ = time - 1;
    this->updateTimestamp(timestamp_);
    int64_t cyclesOff = timestamp_ - lastActiveClockCycle_;
    clockIsOn_ = true;
}

void RoundRobinArbiter::turnClockOff() {
    printf("%s turning clock OFF at cycle %" PRIu64 ", timestamp %" PRIu64 ", ns %" PRIu64 "\n", this->getName().c_str(), getCurrentSimCycle(), timestamp_, getCurrentSimTimeNano());
    clockIsOn_ = false;
    lastActiveClockCycle_ = timestamp_;
}

void RoundRobinArbiter::createClock(Params &params) {
    /* Create clock */
    bool found;
    std::string frequency = params.find<std::string>("arbiter_frequency", "", found);
    if (!found)
        out_->fatal(CALL_INFO, -1, "%s, Param not specified: frequency - snoop filter frequency.\n", getName().c_str());

    clockHandler_       = new Clock::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::clockTick);
    defaultTimeBase_    = registerClock(frequency, clockHandler_);

    clockIsOn_ = true;
    timestamp_ = 0;
    lastActiveClockCycle_ = 0;

}

/**************************************************************************
 * Event processing
 **************************************************************************/

/*
 * Main function for processing events
 * - Dispatches events to appropriate handlers
 *   Returns: whether event was accepted/can be popped off event queue
 */

bool RoundRobinArbiter::processEvent(MemEventBase* ev, PortNum srcPort) {

    MemEvent * event = static_cast<MemEvent*>(ev);

//    printf("Before : %s 0x%x\n", __func__, event->getAddr());
//    Addr rtaddr = ev->getRoutingAddress();
//    event->setBaseAddr(rtaddr);
//    event->setAddr(rtaddr);
//    printf("After : %s 0x%x\n", __func__, event->getAddr());
    
    bool accepted = false;

    switch(srcPort) {
        case PortNum::PORT1   : accepted = this->handleCMD(ev, PortNum::PORT1); break;
        case PortNum::PORT2   : accepted = this->handleCMD(ev, PortNum::PORT2); break;
        case PortNum::PORT3   : accepted = this->handleCMD(ev, PortNum::PORT3); break;
        case PortNum::OUTPORT : accepted = this->handleCMD(ev, PortNum::OUTPORT); break;
//        case PortNum::PORT1   : accepted = this->handleCMD(event, PortNum::PORT1); break;
//        case PortNum::PORT2   : accepted = this->handleCMD(event, PortNum::PORT2); break;
//        case PortNum::PORT3   : accepted = this->handleCMD(event, PortNum::PORT3); break;
//        case PortNum::OUTPORT : accepted = this->handleCMD(event, PortNum::OUTPORT); break;
        default               : stat_RetryEvents->addData(1); return false;
    }

    // MSHR conflit which stalls the CPU occurs without below code.
    // We manually make PutAck for MSHR because of the problem in some configurations.
    // TODO: Need to fix this problem by chaning topology in config file.
//    Command cmd = ev->getCmd();
//    switch (cmd) {
//        case Command::PutS:
//        case Command::PutM:
//        case Command::PutE:
//        case Command::PutX:
//            Addr addr = ev->getRoutingAddress();
//            PortNum destPort;
//            Response fwdReq = {ev, timestamp_, packetHeaderBytes + ev->getPayloadSize()};
//
//            if ((addr >= port1minAddr) && (addr <port1maxAddr)){
//                destPort = PortNum::PORT1;
//            } else if ((addr >= port2minAddr) && (addr <port2maxAddr)){
//                destPort = PortNum::PORT2;
//            } else if ((addr >= port3minAddr) && (addr <port3maxAddr)){
//                destPort = PortNum::PORT3;
//            } else{
//                destPort = PortNum::OUTPORT;
//            }
//
//            switch(srcPort) {
//                case PortNum::PORT1   : linkUpPort1_->send(ev->clone()->makeResponse()); break;
//                case PortNum::PORT2   : linkUpPort2_->send(ev->clone()->makeResponse()); break;
//                case PortNum::PORT3   : linkUpPort3_->send(ev->clone()->makeResponse()); break;
//                case PortNum::OUTPORT : linkDown_->send(ev->clone()->makeResponse()); break;
//            }
//            break;
//    }
    return accepted;
}

/**************************************************************************
 * Simulation flow
 * - init: coordinate protocols/configuration between components
 * - setup
 * - finish
 * - printStatus - called on SIGUSR2 and emergencyShutdown
 * - emergenyShutdown - called on fatal()
 **************************************************************************/

void RoundRobinArbiter::init(unsigned int phase) {

    linkUpPort1_->init(phase);
    linkUpPort2_->init(phase);
    linkUpPort3_->init(phase);
    linkDown_->init(phase);

    if (!phase) {
//        linkUpPort1_->sendUntimedData(this->getInitCoherenceEvent());
//        linkUpPort2_->sendUntimedData(this->getInitCoherenceEvent());
//        linkUpPort3_->sendUntimedData(this->getInitCoherenceEvent());
//        linkDown_->sendUntimedData(this->getInitCoherenceEvent());
    }

    while (MemEventInit * memEvent = linkUpPort1_->recvUntimedData()) {
        init1=1;
        if (memEvent->getCmd() == Command::NULLCMD) {
//            if (memEvent->getInitCmd() == MemEventInit::InitCommand::Endpoint) {
//                MemEventInit * mEv = memEvent->clone();
//                mEv->setSrc(getName());
//                linkDown_->sendUntimedData(mEv);
//            }
        } else {
            MemEventInit * mEv = memEvent->clone();
            mEv->setSrc(getName());
            linkDown_->sendUntimedData(mEv, false);
        }
        delete memEvent;
    }

    if (init1){
        init1=0;
        while (MemEventInit * memEvent = linkDown_->recvUntimedData()) {
            if (memEvent && memEvent->getCmd() == Command::NULLCMD) {
//	            if (memEvent->getInitCmd() == MemEventInit::InitCommand::Endpoint) {
//	                MemEventInit * mEv = memEvent->clone();
//	                mEv->setSrc(getName());
//	                linkUpPort1_->sendUntimedData(mEv);
//	            }
			}
            delete memEvent;
        }
    }

    while (MemEventInit * memEvent = linkUpPort2_->recvUntimedData()) {
        init2=1;
        if (memEvent->getCmd() == Command::NULLCMD) {
//            if (memEvent->getInitCmd() == MemEventInit::InitCommand::Endpoint) {
//                MemEventInit * mEv = memEvent->clone();
//                mEv->setSrc(getName());
//                linkDown_->sendUntimedData(mEv);
//            }
        } else {
            MemEventInit * mEv = memEvent->clone();
            mEv->setSrc(getName());
            linkDown_->sendUntimedData(mEv, false);
        }
        delete memEvent;
    }

    if (init2){
        init2=0;
        while (MemEventInit * memEvent = linkDown_->recvUntimedData()) {
            if (memEvent && memEvent->getCmd() == Command::NULLCMD){
//                if (memEvent->getInitCmd() == MemEventInit::InitCommand::Endpoint) {
//                    MemEventInit * mEv = memEvent->clone();
//                    mEv->setSrc(getName());
//                    linkUpPort2_->sendUntimedData(mEv);
//                }
            }
            delete memEvent;
        }
    }

    while (MemEventInit * memEvent = linkUpPort3_->recvUntimedData()) {
        init3=0;
        if (memEvent->getCmd() == Command::NULLCMD) {
//            if (memEvent->getInitCmd() == MemEventInit::InitCommand::Endpoint) {
//                MemEventInit * mEv = memEvent->clone();
//                mEv->setSrc(getName());
//                linkDown_->sendUntimedData(mEv);
//            }
        } else {
            MemEventInit * mEv = memEvent->clone();
            mEv->setSrc(getName());
            linkDown_->sendUntimedData(mEv, false);
        }
        delete memEvent;
    }

    if (init3){
        init3=0;
        while (MemEventInit * memEvent = linkDown_->recvUntimedData()) {
            if (memEvent && memEvent->getCmd() == Command::NULLCMD) {
//	            if (memEvent->getInitCmd() == MemEventInit::InitCommand::Endpoint) {
//	                MemEventInit * mEv = memEvent->clone();
//	                mEv->setSrc(getName());
//	                linkUpPort3_->sendUntimedData(mEv);
//	            }
			}
            delete memEvent;
        }
    }
}

void RoundRobinArbiter::setup() {
    linkUpPort1_->setup();
    linkUpPort2_->setup();
    linkUpPort3_->setup();
//    linkDown_->setup();
}


void RoundRobinArbiter::finish() {
    linkUpPort1_->finish();
    linkUpPort2_->finish();
    linkUpPort3_->finish();
    linkDown_->finish();
}


// TODO: Anshuman: Complete this
void RoundRobinArbiter::printStatus(Output &out) {
    out.output("MemHierarchy::RoundRobinArbiter %s\n", getName().c_str());
    out.output("  Clock is %s. Last active cycle: %" PRIu64 "\n", clockIsOn_ ? "on" : "off", timestamp_);
    //out.output("  Events in queues: Retry = %zu, Event = %zu, Prefetch = %zu\n", retryBuffer_.size(), eventBuffer_.size(), prefetchBuffer_.size());
    //if (mshr_) {
    //    out.output("  MSHR Status:\n");
    //    mshr_->printStatus(out);
    //}
    //if (linkUp_ && linkUp_ != linkDown_) {
    //    out.output("  Up link status: ");
    //    linkUp_->printStatus(out);
    //    out.output("  Down link status: ");
    //} else {
    //    out.output("  Link status: ");
    //}
    //if (linkDown_) linkDown_->printStatus(out);

    out.output(" CSR Snoop filter \n");
    this->printStatus(out);
    out.output("End MemHierarchy::RoundRobinArbiter\n\n");
}

// TODO: Anshuman: Complete this
void RoundRobinArbiter::emergencyShutdown() {
    //if (out_->getVerboseLevel() > 1) {
    //    if (out_->getOutputLocation() == Output::STDOUT)
    //        out_->setOutputLocation(Output::STDERR);
    //    printStatus(*out_);
    //    if (linkUp_ && linkUp_ != linkDown_) {
    //        out_->output("  Checking for unreceived events on up link: \n");
    //        linkUp_->emergencyShutdownDebug(*out_);
    //        out_->output("  Checking for unreceived events on down link: \n");
    //    } else {
    //        out_->output("  Checking for unreceived events on link: \n");
    //    }
    //    linkDown_->emergencyShutdownDebug(*out_);
    //}
}


void RoundRobinArbiter::configureLinks(Params &params, TimeConverter* tc) {

    linkUpPort1_ = loadUserSubComponent<MemLinkBase>("port1link", ComponentInfo::SHARE_NONE, tc);
    if (linkUpPort1_)
        linkUpPort1_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventUpPort1));

    linkUpPort2_ = loadUserSubComponent<MemLinkBase>("port2link", ComponentInfo::SHARE_NONE, tc);
    if (linkUpPort2_)
        linkUpPort2_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventUpPort2));

    linkUpPort3_ = loadUserSubComponent<MemLinkBase>("port3link", ComponentInfo::SHARE_NONE, tc);
    if (linkUpPort3_)
        linkUpPort3_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventUpPort3));

    linkDown_ = loadUserSubComponent<MemLinkBase>("memlink", ComponentInfo::SHARE_NONE, tc);
    if (linkDown_)
        linkDown_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventDown));

    // For linkDown_
    regionPort4_.start = port1minAddr;
    regionPort4_.end = port3maxAddr;
    regionPort4_.interleaveSize = 0;
    regionPort4_.interleaveStep = 0;

    regionPort1_.start = port1minAddr;
    regionPort1_.end = port1maxAddr;
    regionPort1_.interleaveSize = 0;
    regionPort1_.interleaveStep = 0;

    regionPort2_.start = port2minAddr;
    regionPort2_.end = port2maxAddr;
    regionPort2_.interleaveSize = 0;
    regionPort2_.interleaveStep = 0;

    regionPort3_.start = port3minAddr;
    regionPort3_.end = port3maxAddr;
    regionPort3_.interleaveSize = 0;
    regionPort3_.interleaveStep = 0;

    std::string opalNode = params.find<std::string>("node", "0");
    std::string opalShMem = params.find<std::string>("shared_memory", "0");
    std::string opalSize = params.find<std::string>("local_memory_size", "0");

    Params nicParams = params.get_scoped_params("memNIC" );
    nicParams.insert("node", opalNode);
    nicParams.insert("shared_memory", opalShMem);
    nicParams.insert("local_memory_size", opalSize);

    Params port1link = params.get_scoped_params("port1link");
    port1link.insert("port", "port1");
    port1link.insert("node", opalNode);
    port1link.insert("shared_memory", opalShMem);
    port1link.insert("local_memory_size", opalSize);

    Params port2link = params.get_scoped_params("port2link");
    port2link.insert("port", "port2");
    port2link.insert("node", opalNode);
    port2link.insert("shared_memory", opalShMem);
    port2link.insert("local_memory_size", opalSize);

    Params port3link = params.get_scoped_params("port3link");
    port3link.insert("port", "port3");
    port3link.insert("node", opalNode);
    port3link.insert("shared_memory", opalShMem);
    port3link.insert("local_memory_size", opalSize);

    bool found;

    if (overrideGroupID == 2) {
        nicParams.find<std::string>("group", "", found);
        if (!found) nicParams.insert("group", "2"); // TODO: Group value taken from directory controller file
        int cl = nicParams.find<int>("group");
        nicParams.insert("sources", "2,3", false);
        nicParams.insert("destinations", "2,3", false);
    } else if (overrideGroupID == 3) {
        nicParams.find<std::string>("group", "", found);
        if (!found) nicParams.insert("group", "3"); // TODO: Group value taken from directory controller file
        int cl = nicParams.find<int>("group");
        nicParams.insert("sources", "2,3", false);
        nicParams.insert("destinations", "2", false);
    } else {
        out_->fatal(CALL_INFO, -1, "%s, ERROR: Invalid GroupID.\n", getName().c_str());
    }

    nicParams.find<std::string>("port", "", found);
    if (!found) nicParams.insert("port", "directory");

//    printf("Interleave Size %s\n", ilSize.c_str());
//    printf("Interleave Step %s\n", ilStep.c_str());

//    nicParams.insert("memNIC.interleave_size", ilSize, false);
//    nicParams.insert("memNIC.interleave_step", ilStep, false);

    // Configure low link
    linkDown_ = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemNIC", "memlink", 0, ComponentInfo::INSERT_STATS | ComponentInfo::SHARE_PORTS, nicParams, tc);
    linkDown_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventDown));
//    linkDown_->setRegion(regionPort4_);

    linkUpPort1_ = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemLink", "port1link", 0, ComponentInfo::INSERT_STATS | ComponentInfo::SHARE_PORTS, port1link, tc);
    linkUpPort1_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventUpPort1));

    linkUpPort2_ = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemLink", "port2link", 0, ComponentInfo::INSERT_STATS | ComponentInfo::SHARE_PORTS, port2link, tc);
    linkUpPort2_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventUpPort2));

    linkUpPort3_ = loadAnonymousSubComponent<MemLinkBase>("memHierarchy.MemLink", "port3link", 0, ComponentInfo::INSERT_STATS | ComponentInfo::SHARE_PORTS, port3link, tc);
    linkUpPort3_->setRecvHandler(new Event::Handler<RoundRobinArbiter>(this, &RoundRobinArbiter::handleEventUpPort3));
}

/*******************************************************************************
 * Send events
 *******************************************************************************/

/* Send commands when their timestampe expires. Return whether queue is empty or not. */
bool RoundRobinArbiter::sendOutgoingEvents() {
    // Update timestamp
    timestamp_++;
    bool idle = true;
	
	if(startTimes_.size() >= 2) asm("ud2");
	SST::Event::id_type firstEventID = startTimes_.begin()->first;

    while (!(outgoingEventQueueDownPort1_.empty() && outgoingEventQueueDownPort2_.empty() && outgoingEventQueueDownPort3_.empty())){
        if (outgoingEventQueueDownPort1_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueDownPort1_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkDown_->send(outgoingEvent);
	                outgoingEventQueueDownPort1_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueDownPort2_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueDownPort2_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkDown_->send(outgoingEvent);
	                outgoingEventQueueDownPort2_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueDownPort3_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueDownPort3_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkDown_->send(outgoingEvent);
	                outgoingEventQueueDownPort3_.pop_front();
	                idle = false;
				}
        }
    }

    while (!(outgoingEventQueueUpPort1FromOutPort_.empty() && outgoingEventQueueUpPort1FromPort2_.empty() && outgoingEventQueueUpPort1FromPort3_.empty())){
        if (outgoingEventQueueUpPort1FromOutPort_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort1FromOutPort_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort1_->send(outgoingEvent);
	                outgoingEventQueueUpPort1FromOutPort_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueUpPort1FromPort2_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort1FromPort2_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort1_->send(outgoingEvent);
	                outgoingEventQueueUpPort1FromPort2_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueUpPort1FromPort3_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort1FromPort3_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort1_->send(outgoingEvent);
	                outgoingEventQueueUpPort1FromPort3_.pop_front();
	                idle = false;
				}
        }
    }


    while (!(outgoingEventQueueUpPort2FromOutPort_.empty() && outgoingEventQueueUpPort2FromPort1_.empty() && outgoingEventQueueUpPort2FromPort3_.empty())){
        if (outgoingEventQueueUpPort2FromOutPort_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort2FromOutPort_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort2_->send(outgoingEvent);
	                outgoingEventQueueUpPort2FromOutPort_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueUpPort2FromPort1_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort2FromPort1_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort2_->send(outgoingEvent);
	                outgoingEventQueueUpPort2FromPort1_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueUpPort2FromPort3_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort2FromPort3_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort2_->send(outgoingEvent);
	                outgoingEventQueueUpPort2FromPort3_.pop_front();
	                idle = false;
				}
        }
    }

    while (!(outgoingEventQueueUpPort3FromOutPort_.empty() && outgoingEventQueueUpPort3FromPort2_.empty() && outgoingEventQueueUpPort3FromPort1_.empty())){
        if (outgoingEventQueueUpPort3FromOutPort_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort3FromOutPort_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort3_->send(outgoingEvent);
	                outgoingEventQueueUpPort3FromOutPort_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueUpPort3FromPort2_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort3FromPort2_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort3_->send(outgoingEvent);
	                outgoingEventQueueUpPort3FromPort2_.pop_front();
	                idle = false;
				}
        }
        if (outgoingEventQueueUpPort3FromPort1_.size() > 0) {
                MemEventBase *outgoingEvent = outgoingEventQueueUpPort3FromPort1_.front().event;
				if(outgoingEvent->getID() == firstEventID)
				{
					startTimes_.erase(firstEventID);
	                linkUpPort3_->send(outgoingEvent);
	                outgoingEventQueueUpPort3FromPort1_.pop_front();
	                idle = false;
				}
        }
    }
    return idle && checkIdle();
}

bool RoundRobinArbiter::checkIdle() {
    return outgoingEventQueueDownPort1_.empty() && outgoingEventQueueDownPort2_.empty() && outgoingEventQueueDownPort3_.empty() &&
           outgoingEventQueueUpPort1FromOutPort_.empty() && outgoingEventQueueUpPort1FromPort2_.empty() && outgoingEventQueueUpPort1FromPort3_.empty() &&
           outgoingEventQueueUpPort2FromOutPort_.empty() && outgoingEventQueueUpPort2FromPort3_.empty() && outgoingEventQueueUpPort2FromPort1_.empty() &&
           outgoingEventQueueUpPort3FromOutPort_.empty() && outgoingEventQueueUpPort3FromPort2_.empty() && outgoingEventQueueUpPort3FromPort1_.empty();
}


bool RoundRobinArbiter::handleCMD(MemEventBase * event, PortNum srcPort) {
//bool RoundRobinArbiter::handleCMD(MemEvent * event, PortNum srcPort) {
    uint64_t sendTime = 0;
    stat_RecvEvents->addData(1);
    sendTime = forwardMessage(event, lineSize_, 0, srcPort, nullptr);
    return true;
}


uint64_t RoundRobinArbiter::forwardMessage(MemEventBase * event, unsigned int requestSize, uint64_t baseTime, PortNum srcPort, vector<uint8_t>* data, Command fwdCmd) {
//uint64_t RoundRobinArbiter::forwardMessage(MemEvent * event, unsigned int requestSize, uint64_t baseTime, PortNum srcPort, vector<uint8_t>* data, Command fwdCmd) {

    MemEvent * eventType = static_cast<MemEvent*>(event);
    if (eventType->isDataRequest()) {
//        addrDestMap_.insert({std::make_pair(event->getAddr(),event->getID()),event->getSrc()});
        addrDestMap_.insert({std::make_pair(event->getRoutingAddress(),event->getID()),event->getSrc()});
    }

    /* Create event to be forwarded */
//    MemEvent* forwardEvent;
//    forwardEvent = new MemEvent(*event);

    MemEvent* forwardEvent;
    forwardEvent = new MemEvent(*eventType);
    Addr rtaddr = event->getRoutingAddress();
    forwardEvent->setBaseAddr(rtaddr);
    forwardEvent->setAddr(rtaddr);

    if (fwdCmd != Command::LAST_CMD) {
        forwardEvent->setCmd(fwdCmd);
    }

    if (data == nullptr) forwardEvent->setPayload(0, nullptr);

    forwardEvent->setSize(requestSize);

    if (data != nullptr) forwardEvent->setPayload(*data);

    /* Determine latency in cycles */
    uint64_t deliveryTime;
//    if (baseTime < timestamp_) baseTime = timestamp_;
//
//    if (event->queryFlag(MemEvent::F_NONCACHEABLE)) {
//        forwardEvent->setFlag(MemEvent::F_NONCACHEABLE);
//        deliveryTime = timestamp_+1;
//    } else {
//        deliveryTime = baseTime+1;
//    }

//    printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//         getCurrentSimCycle(), timestamp_, getName().c_str(), forwardEvent->getVerboseString().c_str());
//    forwardByAddress(forwardEvent, deliveryTime, srcPort);
    forwardByAddress(event, timestamp_, srcPort);

    return deliveryTime;
}

void RoundRobinArbiter::forwardByAddress(MemEventBase * event, Cycle_t ts, PortNum srcPort) {
    event->setSrc(getName());

    std::string dst;
    PortNum destPort = PortNum::OUTPORT;

    Addr addr = event->getRoutingAddress();

    /*** Perform security check here. ***/
//    if ((addr < allowedMinAddr) || (addr >= allowedMaxAddr))
//        out_->fatal(CALL_INFO, -1, "%s, ERROR. Accessing 0x%x address is not allowed.\n", getName().c_str(), addr);

    MemEvent * eventType = static_cast<MemEvent*>(event);
    Command cmd = event->getCmd();

    if (eventType->isDataRequest()) {
        if ((addr >= port1minAddr) && (addr <= port1maxAddr)){
            dst = linkUpPort1_->findTargetDestination(event->getRoutingAddress());
            destPort = PortNum::PORT1;
            if (dst != "") {event->setDst(dst);}
            Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
            addToOutgoingQueueUp(fwdReq, srcPort, destPort);
        } else if ((addr >= port2minAddr) && (addr <= port2maxAddr)){
            dst = linkUpPort2_->findTargetDestination(event->getRoutingAddress());
            destPort = PortNum::PORT2;
            if (dst != "") {event->setDst(dst);}
            Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
            addToOutgoingQueueUp(fwdReq, srcPort, destPort);
        } else if ((addr >= port3minAddr) && (addr <= port3maxAddr)){
            dst = linkUpPort3_->findTargetDestination(event->getRoutingAddress());
            destPort = PortNum::PORT3;
            if (dst != "") {event->setDst(dst);}
            Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
            addToOutgoingQueueUp(fwdReq, srcPort, destPort);
        } else{
            dst = linkDown_->findTargetDestination(event->getRoutingAddress());
            destPort = PortNum::OUTPORT;
            if (dst != "") {event->setDst(dst);}
            Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
            addToOutgoingQueueDown(fwdReq, srcPort, destPort);
        }

        if (dst == "")
        {
            out_->fatal(CALL_INFO, -1, "%s, ERROR. Destination not found.\n", getName().c_str());
        }
    }
    else if (eventType->isResponse()) {
        int i;
        std::map<std::pair<Addr,id_type>,PortNum>::iterator it;
        it = addrPortMap_.find(std::make_pair(addr,event->getResponseToID()));

        // Means no request in PortMap
        if (it != addrPortMap_.end())
        {
            destPort = it->second;
            addrPortMap_.erase(it);

            // Test omitting AckPut
//            Command cmd = event->getCmd();
//            switch (cmd) {
//                case Command::AckPut:
//                    return;
//            }

            switch(destPort) {
                case PortNum::PORT1  : dst = linkUpPort1_->findTargetDestination(event->getRoutingAddress()); break;
                case PortNum::PORT2  : dst = linkUpPort2_->findTargetDestination(event->getRoutingAddress()); break;
                case PortNum::PORT3  : dst = linkUpPort3_->findTargetDestination(event->getRoutingAddress()); break;
                case PortNum::OUTPORT: {
                std::map<std::pair<Addr,id_type>,std::string>::iterator it2 = addrDestMap_.find(std::make_pair(addr,event->getResponseToID()));
                if (it2 != addrDestMap_.end()) {
                    dst = it2->second;
                    addrDestMap_.erase(it2);
                }
                else {
                    out_->fatal(CALL_INFO, -1, "%s, ERROR. Destination not found.\n", getName().c_str());
                }
                break;
                }
                default:
                    out_->fatal(CALL_INFO, -1, "%s, ERROR. Destination not found.\n", getName().c_str());break;
            }
        } else {
//    printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//         getCurrentSimCycle(), timestamp_, getName().c_str(), event->getVerboseString().c_str());
            // Test omitting FetchXResp
            Command cmd = event->getCmd();
            switch (cmd) {
                case Command::FetchXResp:
                case Command::FetchResp:
//				default:
                    event->setDst("dirctrl0");
        			Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
        			addToOutgoingQueueUp(fwdReq, PortNum::OUTPORT, PortNum::PORT1);
                    return;
            }
            out_->fatal(CALL_INFO, -1, "%s, ERROR. Address not found.\n", getName().c_str());
        }

        if (dst != "") event->setDst(dst);

        if (dst == "")
        {
            out_->fatal(CALL_INFO, -1, "%s, ERROR. Destination not found.\n", getName().c_str());
        }

        Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
        if (destPort != PortNum::OUTPORT) addToOutgoingQueueUp(fwdReq, srcPort, destPort);
        else addToOutgoingQueueDown(fwdReq, srcPort, destPort);
    }
    else {
//    printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//         getCurrentSimCycle(), timestamp_, getName().c_str(), event->getVerboseString().c_str());

			dst = linkDown_->findTargetDestination(event->getRoutingAddress());
			if(dst == "")
				dst ="node_os.cache";
			//dst ="node0.cpu0.l2cache";
            event->setDst(dst);
			event->setSrc(getName());
            destPort = PortNum::OUTPORT;
            Response fwdReq = {event, ts, packetHeaderBytes + event->getPayloadSize()};
            addToOutgoingQueueDown(fwdReq, srcPort, destPort);
//        }

//        if (eventType->isWriteback())
//            printf("Writeback 0x%x\n",addr);
//        else if (eventType->isRoutedByAddress())
//            printf("RoutedByAddress 0x%x\n",addr);
//        else if (eventType->isPrefetch())
//            printf("Prefetch 0x%x\n",addr);
//        else
//            printf("Unknown Event type 0x%x\n",addr);

        // TODO: Need to figure out what to do for these packets.
        //out_->fatal(CALL_INFO, -1, "%s, ERROR. Event is neither a Request, nor a Response.\n", getName().c_str());
    }

//    printf("E: %-20" PRIu64 " %-20" PRIu64 " %-20s Event:New     (%s)\n",
//         getCurrentSimCycle(), timestamp_, getName().c_str(), event->getVerboseString().c_str());

}


void RoundRobinArbiter::addToOutgoingQueueUp(Response& resp, PortNum srcPort, PortNum destPort) {
    list<Response>::reverse_iterator rit;

    switch(destPort) {
        case PortNum::PORT1 :
				//Directly send
//                linkUpPort1_->send(resp.event);
        switch(srcPort) {
                case PortNum::PORT2 :
                    for (rit = outgoingEventQueueUpPort1FromPort2_.rbegin(); rit != outgoingEventQueueUpPort1FromPort2_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort1FromPort2_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[7]->addData(1);
                    if (BuffMaxDepth[7] < outgoingEventQueueUpPort1FromPort2_.size())
                    {
                        stat_BuffMaxDepth[7]->addData(outgoingEventQueueUpPort1FromPort2_.size()-BuffMaxDepth[7]);
                        BuffMaxDepth[7] = outgoingEventQueueUpPort1FromPort2_.size();
                    }
                    break;
                case PortNum::PORT3 :
                    for (rit = outgoingEventQueueUpPort1FromPort3_.rbegin(); rit != outgoingEventQueueUpPort1FromPort3_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort1FromPort3_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[9]->addData(1);
                    if (BuffMaxDepth[9] < outgoingEventQueueUpPort1FromPort3_.size())
                    {
                        stat_BuffMaxDepth[9]->addData(outgoingEventQueueUpPort1FromPort3_.size()-BuffMaxDepth[9]);
                        BuffMaxDepth[9] = outgoingEventQueueUpPort1FromPort3_.size();
                    }
                    break;
                case PortNum::OUTPORT :
                    for (rit = outgoingEventQueueUpPort1FromOutPort_.rbegin(); rit != outgoingEventQueueUpPort1FromOutPort_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort1FromOutPort_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[8]->addData(1);
                    if (BuffMaxDepth[8] < outgoingEventQueueUpPort1FromOutPort_.size())
                    {
                        stat_BuffMaxDepth[8]->addData(outgoingEventQueueUpPort1FromOutPort_.size()-BuffMaxDepth[8]);
                        BuffMaxDepth[8] = outgoingEventQueueUpPort1FromOutPort_.size();
                    }
                    break;
                default:
            out_->fatal(CALL_INFO, -1, "%s, ERROR. Invalid Port.\n", getName().c_str());break;
        }
            break;

        case PortNum::PORT2 :
				//Directly send
//                linkUpPort2_->send(resp.event);
        switch(srcPort) {
                case PortNum::PORT1 :
                    for (rit = outgoingEventQueueUpPort2FromPort1_.rbegin(); rit != outgoingEventQueueUpPort2FromPort1_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort2FromPort1_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[10]->addData(1);
                    if (BuffMaxDepth[10] < outgoingEventQueueUpPort2FromPort1_.size())
                    {
                        stat_BuffMaxDepth[10]->addData(outgoingEventQueueUpPort2FromPort1_.size()-BuffMaxDepth[10]);
                        BuffMaxDepth[10] = outgoingEventQueueUpPort2FromPort1_.size();
                    }
                    break;
                case PortNum::PORT3 :
                    for (rit = outgoingEventQueueUpPort2FromPort3_.rbegin(); rit != outgoingEventQueueUpPort2FromPort3_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort2FromPort3_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[12]->addData(1);
                    if (BuffMaxDepth[12] < outgoingEventQueueUpPort2FromPort3_.size())
                    {
                        stat_BuffMaxDepth[12]->addData(outgoingEventQueueUpPort2FromPort3_.size()-BuffMaxDepth[12]);
                        BuffMaxDepth[12] = outgoingEventQueueUpPort2FromPort3_.size();
                    }
                    break;
                case PortNum::OUTPORT :
                    for (rit = outgoingEventQueueUpPort2FromOutPort_.rbegin(); rit != outgoingEventQueueUpPort2FromOutPort_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort2FromOutPort_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[11]->addData(1);
                    if (BuffMaxDepth[11] < outgoingEventQueueUpPort2FromOutPort_.size())
                    {
                        stat_BuffMaxDepth[11]->addData(outgoingEventQueueUpPort2FromOutPort_.size()-BuffMaxDepth[11]);
                        BuffMaxDepth[11] = outgoingEventQueueUpPort2FromOutPort_.size();
                    }
                    break;
                default:
            out_->fatal(CALL_INFO, -1, "%s, ERROR. Invalid Port.\n", getName().c_str());break;
        }
            break;

        case PortNum::PORT3 :
				//Directly send
//                linkUpPort3_->send(resp.event);
        switch(srcPort) {
                case PortNum::PORT2 :
                    for (rit = outgoingEventQueueUpPort3FromPort2_.rbegin(); rit != outgoingEventQueueUpPort3FromPort2_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort3FromPort2_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[15]->addData(1);
                    if (BuffMaxDepth[15] < outgoingEventQueueUpPort3FromPort2_.size())
                    {
                        stat_BuffMaxDepth[15]->addData(outgoingEventQueueUpPort3FromPort2_.size()-BuffMaxDepth[15]);
                        BuffMaxDepth[15] = outgoingEventQueueUpPort3FromPort2_.size();
                    }
                    break;
                case PortNum::PORT1 :
                    for (rit = outgoingEventQueueUpPort3FromPort1_.rbegin(); rit != outgoingEventQueueUpPort3FromPort1_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort3FromPort1_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[13]->addData(1);
                    if (BuffMaxDepth[13] < outgoingEventQueueUpPort3FromPort1_.size())
                    {
                        stat_BuffMaxDepth[13]->addData(outgoingEventQueueUpPort3FromPort1_.size()-BuffMaxDepth[13]);
                        BuffMaxDepth[13] = outgoingEventQueueUpPort3FromPort1_.size();
                    }
                    break;
                case PortNum::OUTPORT :
                    for (rit = outgoingEventQueueUpPort3FromOutPort_.rbegin(); rit != outgoingEventQueueUpPort3FromOutPort_.rend(); rit++) {
//                        if (resp.deliveryTime >= (*rit).deliveryTime) break;
//                        if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
                    }
                    outgoingEventQueueUpPort3FromOutPort_.insert(rit.base(), resp);
    
                    // update stat
                    stat_Buff[14]->addData(1);
                    if (BuffMaxDepth[14] < outgoingEventQueueUpPort3FromOutPort_.size())
                    {
                        stat_BuffMaxDepth[14]->addData(outgoingEventQueueUpPort3FromOutPort_.size()-BuffMaxDepth[14]);
                        BuffMaxDepth[14] = outgoingEventQueueUpPort3FromOutPort_.size();
                    }
                    break;
                default:
            out_->fatal(CALL_INFO, -1, "%s, ERROR. Invalid Port.\n", getName().c_str());break;
        }
            break;

    default:
         out_->fatal(CALL_INFO, -1, "%s, ERROR. Invalid Port.\n", getName().c_str());break;

    }
}

void RoundRobinArbiter::addToOutgoingQueueDown(Response& resp, PortNum srcPort, PortNum destPort) {
    list<Response>::reverse_iterator rit;

    if (srcPort == PortNum::PORT1) {
        for (rit = outgoingEventQueueDownPort1_.rbegin(); rit != outgoingEventQueueDownPort1_.rend(); rit++) {
//            if (resp.deliveryTime >= (*rit).deliveryTime) break;
//            if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
        }
        outgoingEventQueueDownPort1_.insert(rit.base(), resp);


        // update stat
        stat_Buff[4]->addData(1);
        if (BuffMaxDepth[4] < outgoingEventQueueDownPort1_.size())
        {
            stat_BuffMaxDepth[4]->addData(outgoingEventQueueDownPort1_.size()-BuffMaxDepth[4]);
            BuffMaxDepth[4] = outgoingEventQueueDownPort1_.size();
        }
    } else if (srcPort == PortNum::PORT2) {
        for (rit = outgoingEventQueueDownPort2_.rbegin(); rit != outgoingEventQueueDownPort2_.rend(); rit++) {
//            if (resp.deliveryTime >= (*rit).deliveryTime) break;
//            if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
        }
        outgoingEventQueueDownPort2_.insert(rit.base(), resp);

        // update stat
        stat_Buff[5]->addData(1);
        if (BuffMaxDepth[5] < outgoingEventQueueDownPort2_.size())
        {
            stat_BuffMaxDepth[5]->addData(outgoingEventQueueDownPort2_.size()-BuffMaxDepth[5]);
            BuffMaxDepth[5] = outgoingEventQueueDownPort2_.size();
        }
    } else if (srcPort == PortNum::PORT3) {
        for (rit = outgoingEventQueueDownPort3_.rbegin(); rit != outgoingEventQueueDownPort3_.rend(); rit++) {
//            if (resp.deliveryTime >= (*rit).deliveryTime) break;
//            if (resp.event->getRoutingAddress() == (*rit).event->getRoutingAddress()) break;
        }
        outgoingEventQueueDownPort3_.insert(rit.base(), resp);

        // update stat
        stat_Buff[6]->addData(1);
        if (BuffMaxDepth[6] < outgoingEventQueueDownPort3_.size())
        {
            stat_BuffMaxDepth[6]->addData(outgoingEventQueueDownPort3_.size()-BuffMaxDepth[6]);
            BuffMaxDepth[6] = outgoingEventQueueDownPort3_.size();
        }
    } else {
        out_->fatal(CALL_INFO, -1, "%s, ERROR. Invalid Port. srcPort: %d\n", getName().c_str(), srcPort);
    }
}

void RoundRobinArbiter::registerStatistics() {
    Statistic<uint64_t>* def_stat = registerStatistic<uint64_t>("default_stat");
    for (int i = 0; i < 16; i++) {
        stat_Buff[i] = registerStatistic<uint64_t>("EventsPushedQueue_"+std::to_string(i+1));
        stat_BuffMaxDepth[i] = registerStatistic<uint64_t>("MaxDepthPushedQueue_"+std::to_string(i+1));
        BuffMaxDepth[i] = 0;
    }

    stat_RecvEvents  = registerStatistic<uint64_t>("TotalEventsReceived");
    stat_RetryEvents = registerStatistic<uint64_t>("TotalEventsReplayed");

    //statSFUncacheRecv[(int)Command::Put]      = registerStatistic<uint64_t>("Put_uncache_recv");
    //statSFUncacheRecv[(int)Command::Get]      = registerStatistic<uint64_t>("Get_uncache_recv");
    //statSFUncacheRecv[(int)Command::AckMove]  = registerStatistic<uint64_t>("AckMove_uncache_recv");
    //statSFUncacheRecv[(int)Command::GetS]     = registerStatistic<uint64_t>("GetS_uncache_recv");
    //statSFUncacheRecv[(int)Command::Write]    = registerStatistic<uint64_t>("Write_uncache_recv");
    //statSFUncacheRecv[(int)Command::GetSX]    = registerStatistic<uint64_t>("GetSX_uncache_recv");
    //statSFUncacheRecv[(int)Command::GetSResp] = registerStatistic<uint64_t>("GetSResp_uncache_recv");
    //statSFUncacheRecv[(int)Command::WriteResp]  = registerStatistic<uint64_t>("WriteResp_uncache_recv");
    //statSFUncacheRecv[(int)Command::CustomReq]  = registerStatistic<uint64_t>("CustomReq_uncache_recv");
    //statSFUncacheRecv[(int)Command::CustomResp] = registerStatistic<uint64_t>("CustomResp_uncache_recv");
    //statSFUncacheRecv[(int)Command::CustomAck]  = registerStatistic<uint64_t>("CustomAck_uncache_recv");

    //// Valid cache commands depend on coherence manager
    //std::set<Command> validrecv = coherenceMgr_->getValidReceiveEvents();

    //for (std::set<Command>::iterator it = validrecv.begin(); it != validrecv.end(); it++) {
    //    std::string stat = CommandString[(int)(*it)];
    //    stat.append("_recv");
    //    statSFCacheRecv[(int)(*it)] = registerStatistic<uint64_t>(stat);
    //}

    //statMSHROccupancy               = registerStatistic<uint64_t>("MSHR_occupancy");
    //statBankConflicts               = registerStatistic<uint64_t>("Bank_conflicts");
}


/*******************************************************************************
 * Initialization/finish functions used by parent
 *******************************************************************************/

MemEventInitCoherence * RoundRobinArbiter::getInitCoherenceEvent() {
    if (isCacheConnected)
        return new MemEventInitCoherence(getName(), Endpoint::Directory, false /* inclusive */, true /* sends WBAck */, false /* expects WBAck */, lineSize_, true /* tracks block presence */);
//        return new MemEventInitCoherence(getName(), Endpoint::Directory, true /* inclusive */, false /* sends WBAck */, true /* expects WBAck */, lineSize_, true /* tracks block presence */);
    else
        return new MemEventInitCoherence(getName(), Endpoint::Directory, true /* inclusive */, false /* sends WBAck */, false /* expects WBAck */, lineSize_, false /* tracks block presence */);
}

