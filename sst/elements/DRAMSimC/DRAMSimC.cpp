// Copyright 2009-2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2013, Sandia Corporation
// All rights reserved.
// 
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>
#include "sst/core/serialization/element.h"
#include <DRAMSimC.h>
#include <sstream> // for stringstream() so I don't have to use atoi()

#define DBG( fmt, args... ) \
    m_dbg.write( "%s():%d: "fmt, __FUNCTION__, __LINE__, ##args)

DRAMSimC::DRAMSimC( ComponentId_t id, Params_t& params ) :
    IntrospectedComponent( id ),
    m_printStats("no"),
    m_dbg( *new Log< DRAMSIMC_DBG >( "DRAMSimC::", false ) ),
    m_log( *new Log< >( "INFO DRAMSimC: ", false ) )
{
    std::string frequency = "2.2 GHz";
    std::string systemIniFilename = "ini/system.ini";
    std::string deviceIniFilename = "";
    unsigned megsOfMemory = 4096;

    if ( params.find( "info" ) != params.end() ) {
        if ( params[ "info" ].compare( "yes" ) == 0 ) {
            m_log.enable();
        }
    }

    if ( params.find( "debug" ) != params.end() ) {
        if ( params[ "debug" ].compare( "yes" ) == 0 ) {
            m_dbg.enable();
        }
    }

    if ( params.find( "clock" ) != params.end() ) {
      frequency = params["clock"];
    }

    DBG("new id=%lu\n",id);

    m_memChan = new memChan_t( *this, params, "bus" );

    std::string     pwdString = "";

    Params_t::iterator it = params.begin();
    while( it != params.end() ) {
        DBG("key=%s value=%s\n",
        it->first.c_str(),it->second.c_str());
        if ( ! it->first.compare("deviceini") ) {
            deviceIniFilename = it->second;
        }
        if ( ! it->first.compare("systemini") ) {
            systemIniFilename = it->second;
        }
        if ( ! it->first.compare("pwd") ) {
            pwdString = it->second;
        }
        if ( ! it->first.compare("printStats") ) {
            m_printStats = it->second;
        }
        if ( ! it->first.compare("megsOfMemory") ) {
            stringstream(it->second) >> megsOfMemory;
        }

        ++it;
    }

    DBG("pwd %s\n",pwdString.c_str());

    deviceIniFilename = pwdString + "/" + deviceIniFilename;
    systemIniFilename = pwdString + "/" + systemIniFilename;

    m_log.write("device ini %s\n",deviceIniFilename.c_str());
    m_log.write("system ini %s\n",systemIniFilename.c_str());

    m_log.write("freq %s\n",frequency.c_str());
//     ClockHandler_t* handler;
//     handler = new EventHandler< DRAMSimC, bool, Cycle_t >
//                                         ( this, &DRAMSimC::clock );

//     TimeConverter* tc = registerClock( frequency, handler );
    TimeConverter* tc = registerClock( frequency, new Clock::Handler<DRAMSimC>(this, &DRAMSimC::clock) );
    m_log.write("period %ld\n",tc->getFactor());

    m_memorySystem = new MultiChannelMemorySystem(/*0,*/ deviceIniFilename,
                    systemIniFilename, "", "", megsOfMemory);
    if ( ! m_memorySystem ) {
      _abort(DRAMSimC,"MemorySystem() failed\n");
    }

    Callback< DRAMSimC, void, uint, uint64_t,uint64_t >* readDataCB;
    Callback< DRAMSimC, void, uint, uint64_t,uint64_t >* writeDataCB;

    readDataCB = new Callback< DRAMSimC, void, uint, uint64_t,uint64_t >
      (this, &DRAMSimC::readData);

    writeDataCB = new Callback< DRAMSimC, void, uint, uint64_t,uint64_t >
      (this, &DRAMSimC::writeData);
    m_memorySystem->RegisterCallbacks( readDataCB,writeDataCB, NULL);

    //make the following energy information monitored by introspector
    /*registerMonitorInt("dram_backgroundEnergy");
    registerMonitorInt("dram_burstEnergy");
    registerMonitorInt("dram_actpreEnergy");
    registerMonitorInt("dram_refreshEnergy");*/
}

void DRAMSimC::finish() 
{
  /*vector< uint64_t > &v = m_memorySystem->memoryController->backgroundEnergy;
  for (int i = 0; i < v.size(); ++i) {
    printf("DRAM: Background Energy %llu\n", 
	   m_memorySystem->memoryController->backgroundEnergy[i]);
    printf("DRAM: Burst Energy %llu\n", 
	   m_memorySystem->memoryController->burstEnergy[i]);
    printf("DRAM: ACT/PRE Energy %llu\n", 
	   m_memorySystem->memoryController->actpreEnergy[i]);
    printf("DRAM: Refresh Energy %llu\n", 
	   m_memorySystem->memoryController->refreshEnergy[i]);
  }*/
  
  m_memorySystem->printStats();
//  return 0;
}

void DRAMSimC::readData(uint id, uint64_t addr, uint64_t clockcycle)
{
    DBG("id=%d addr=%#lx clock=%lu\n",id,addr,clockcycle);
    memChan_t::event_t* event = new memChan_t::event_t;
    event->addr = addr;
    event->reqType = memChan_t::event_t::READ; 
    event->msgType = memChan_t::event_t::RESPONSE; 
    if ( ! m_memChan->send( event ) ) {
        _abort(DRAMSimC::readData,"m_memChan->send failed\n");
    }
}

void DRAMSimC::writeData(uint id, uint64_t addr, uint64_t clockcycle)
{
    DBG("id=%d addr=%#lx clock=%lu\n",id,addr,clockcycle);
    memChan_t::event_t* event = new memChan_t::event_t;
    event->addr = addr;
    event->reqType = memChan_t::event_t::WRITE; 
    event->msgType = memChan_t::event_t::RESPONSE; 
    if( ! m_memChan->send(event) ) {
        _abort(DRAMSimC::writeData, "m_memChan->send failed\n");
    }
}

#if 0
extern int badCheat;
#endif

bool DRAMSimC::clock( Cycle_t current )
{
#if 0
  // simplest way to reset for now
  if (badCheat) {
    {
      vector< uint64_t > &v = m_memorySystem->memoryController->backgroundEnergy;
      for (vector< uint64_t >::iterator i = v.begin(); i != v.end(); ++i) {
	*i = 0;
      }
    }

    {
      vector< uint64_t > &v = m_memorySystem->memoryController->burstEnergy;
      for (vector< uint64_t >::iterator i = v.begin(); i != v.end(); ++i) {
	*i = 0;
      }
    }

    {
      vector< uint64_t > &v = m_memorySystem->memoryController->actpreEnergy;
      for (vector< uint64_t >::iterator i = v.begin(); i != v.end(); ++i) {
	*i = 0;
      }
    }

    {
      vector< uint64_t > &v = m_memorySystem->memoryController->refreshEnergy;
      for (vector< uint64_t >::iterator i = v.begin(); i != v.end(); ++i) {
	*i = 0;
      }
    }
    badCheat = 0;
    printf("DRAMSim reset stats\n");
  }   
#endif

    m_memorySystem->update();

    memChan_t::event_t* event;
    while ( m_memChan->recv( &event ) ) 
    {
        DBG("got an event %x\n", event);
        
        TransactionType transType = 
                            convertType( event->reqType );
        DBG("transType=%d addr=%#lx\n", transType, event->addr );
        m_transQ.push_back( Transaction( transType, event->addr, NULL));
	delete event;
    }

    int ret = 1; 
    while ( ! m_transQ.empty() && ret ) {
        if (  ( ret = m_memorySystem->addTransaction( m_transQ.front() ) ) )
        {
	        DBG("addTransaction succeeded %#x\n", m_transQ.front().address);
	        m_transQ.pop_front();
        } else {
	        DBG("addTransaction failed\n");
        }
    }
    return false;
}

/*uint64_t DRAMSimC::getIntData(int dataID, int index)
{ 
	vector< uint64_t > &v = m_memorySystem->memoryController->backgroundEnergy;
  	assert( index <= v.size());

	switch(dataID)
	{ 
	    case dram_backgroundEnergy:	    
		return (m_memorySystem->memoryController->backgroundEnergy[index]);
		break;
	    case dram_burstEnergy: 	   
		return (m_memorySystem->memoryController->burstEnergy[index]);
		break;
	    case dram_actpreEnergy: 	    
		return (m_memorySystem->memoryController->actpreEnergy[index]);
		break;
	    case dram_refreshEnergy:
		return (m_memorySystem->memoryController->refreshEnergy[index]);
		break;
	    default:
		return (0);
		break;	
	}
}*/

extern "C" {
DRAMSimC* DRAMSimCAllocComponent( SST::ComponentId_t id,  
                                    SST::Component::Params_t& params )
{
    return new DRAMSimC( id, params );
}
}

