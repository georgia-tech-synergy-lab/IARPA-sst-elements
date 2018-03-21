class Tlb : public Unit {
    std::string m_prefix;
    const char* prefix() { return m_prefix.c_str(); }

    struct Entry {
        enum Type { Load, Store } type;
        Entry( MemReq* req, Callback callback, Type type  ) : req(req), callback(callback), type(type) {} 
        MemReq* req;
        Callback callback;
    };
    
  public:
    Tlb( SimpleMemoryModel& model, Output& dbg, int id, Unit* load, Unit* store, int size, int pageSize, int tlbMissLat_ns,
            int numWalkers, int maxStores, int maxLoads ) :
        Unit( model, dbg ), m_curPid(-1), m_cache( size ), m_cacheSize( size), m_pageMask( ~(pageSize - 1) ),
        m_load(load), 
        m_store(store),
        m_storeBlocked( false ), 
        m_loadBlocked( false ), 
        m_numWalkers(numWalkers), 
        m_tlbMissLat_ns( tlbMissLat_ns ),
        m_maxPendingStores(maxStores),
        m_maxPendingLoads(maxLoads),
        m_numPendingStores(0),
        m_numPendingLoads(0),
        m_pendingWalks(0),
        m_blockedStoreSrc(NULL)
    {
        m_prefix = "@t:" + std::to_string(id) + ":SimpleMemoryModel::Tlb::@p():@l ";

        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"tlbSize=%d, pageMask=%#" PRIx64 ", numWalkers=%d\n",
                        size, m_pageMask, numWalkers );

        // we need to start with a full cache, what's in it doesn't mater
        for ( unsigned i = 0; i < size; i++ ) {
            m_cache.insert( ( i + 1 ) );
        }
    }

    void resume( UnitBase* unit ) {
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"\n");

        if ( unit == m_store ) {
            m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"store unblocked\n");
            assert( m_storeBlocked );
            m_storeBlocked = false;
            while( ! m_storeBlocked && ! m_readyStores.empty() ) {
                m_storeBlocked = passUpStore( m_readyStores.front() );
                m_readyStores.pop_front();
            }
        }

        if ( unit == m_load ) {
            m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"load unblocked\n");
            assert( m_loadBlocked );
            m_loadBlocked= false;
            while( ! m_loadBlocked && ! m_readyLoads.empty() ) {
                m_loadBlocked = passUpLoad( m_readyLoads.front() );
                m_readyLoads.pop_front();
            }
        }
    }

    bool storeCB( UnitBase* src, MemReq* req, Callback callback ) {

        Hermes::Vaddr addr = getPageAddr(req->addr);
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"req Addr %#" PRIx64 ", page Addr %#" PRIx64 "\n", addr, req->addr );

        if ( lookup( req->pid, addr ) ) {
            if ( m_storeBlocked ) {
                m_readyStores.push_back( new Entry( req, callback, Entry::Store ) );
                ++m_numPendingStores;
            } else {
                m_storeBlocked = m_store->storeCB( this, req, callback );
            }
        } else {
            tlbMiss( new Entry( req, callback, Entry::Store ) );
            ++m_numPendingStores;
        }

        if ( m_numPendingStores == m_maxPendingStores ) {
            m_blockedStoreSrc = src;
            m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"blocking source\n" );
            return true;
        } else { 
            return false;
        }
    }

    bool load( UnitBase* src, MemReq* req, Callback callback ) {
        Hermes::Vaddr addr = getPageAddr(req->addr);
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"%#" PRIx64 " %#" PRIx64 "\n", addr, req->addr );

        if ( lookup( req->pid, addr ) ) {
            if ( m_loadBlocked ) {
                m_readyLoads.push_back( new Entry( req, callback, Entry::Load ) );
                ++m_numPendingLoads;
            } else {
                m_loadBlocked = m_load->load( this, req, callback );
            }
        } else {
            tlbMiss( new Entry( req, callback, Entry::Load ) );
            ++m_numPendingLoads;
        }

        if ( m_numPendingLoads == m_maxPendingLoads ) {
            m_blockedLoadSrc = src;
            m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"blocking source\n" );
            return true;
        } else { 
            return false;
        }
    }

  private:

    void tlbMiss( Entry* entry ) {
        if ( tlbMiss2( entry ) ) {
            m_blockedByTLB.push_back(entry);
        }
    }

    bool tlbMiss2( Entry* entry ) {
        Hermes::Vaddr addr = getPageAddr(entry->req->addr);
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"addr=%#" PRIx64 "\n",addr);

        bool retval = false;
        if ( m_pendingWalkList.find(addr) != m_pendingWalkList.end() ) {
            m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"add pending, page addr=%#" PRIx64 "\n",addr);
            m_pendingWalkList[addr].push_back(entry);
        } else if ( m_pendingWalks < m_numWalkers ) {
            m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"walk, page addr=%#" PRIx64 "\n",addr);
            walk( entry->req->pid, addr, std::bind(&Tlb::processMissComplete, this, addr ) );
            m_pendingWalkList[addr].push_back(entry);
            ++m_pendingWalks;
        } else {
            retval = true;
        }
        return retval;
    }

    void processBlockedByTLB() {
        while ( ! m_blockedByTLB.empty() ) {
            if ( tlbMiss2( m_blockedByTLB.front() ) ) {
                break;
            } else {
                m_blockedByTLB.pop_front();
            }
        }
    }

    void processMissComplete( uint64_t addr ) {
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"page addr=%#" PRIx64 "\n",addr);
        --m_pendingWalks;

        while ( ! m_pendingWalkList[addr].empty() ) {

            Entry* entry = m_pendingWalkList[addr].front();
            m_pendingWalkList[addr].pop_front();

            if ( entry->type == Entry::Store ) {
                if ( m_storeBlocked ) {
                    m_readyStores.push_back( entry );
                    m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"store blocked, page addr=%#" PRIx64 "\n",addr);
                } else {
                    m_storeBlocked = passUpStore( entry );
                }
            } else {
                if ( m_loadBlocked ) {
                    m_readyLoads.push_back( entry );
                    m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"load blocked, page addr=%#" PRIx64 "\n",addr);
                } else {
                    m_loadBlocked = passUpLoad( entry );
                }
            }
        }

        m_pendingWalkList.erase(addr);
        processBlockedByTLB();
    }

    bool passUpLoad( Entry* entry ) {
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK," req addr=%#" PRIx64 "\n",entry->req->addr);
        bool retval = m_load->load( this, entry->req, entry->callback );
        delete entry;

        if ( m_numPendingLoads-- == m_maxPendingLoads ) {
            assert( m_blockedLoadSrc );
            m_model.schedResume( 0, m_blockedLoadSrc );
            m_blockedLoadSrc = NULL;
        }
        return retval;
    }

    bool passUpStore( Entry* entry ) {
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"req addr=%#" PRIx64 "\n",entry->req->addr);
        bool retval = m_store->storeCB( this, entry->req, entry->callback );
        delete entry;

        if ( m_numPendingStores-- == m_maxPendingStores ) {
            assert( m_blockedStoreSrc );
            m_model.schedResume( 0, m_blockedStoreSrc );
            m_blockedStoreSrc = NULL;
        }
        return retval;
    }


    void walk( int pid, uint64_t addr, Callback callback ) {
        Hermes::Vaddr evictAddr = m_cache.evict();
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"pid=%d addr=%#" PRIx64 " evictAddr=%#" PRIx64 "\n",pid, addr, evictAddr );
        m_model.schedCallback( m_tlbMissLat_ns, 
            [=](){               
                m_cache.insert( addr );
                callback();
            }
        );
    }

    bool lookup( int pid, uint64_t addr  ) {
        if ( m_cacheSize == 0 ) { return true; }

        assert ( -1 == m_curPid || pid == m_curPid ); 

        bool retval;
        if ( m_cache.isValid( addr) ) {
            m_cache.updateAge( addr );
            retval = true;
        } else {
            retval = false;
        }
        m_dbg.verbosePrefix(prefix(),CALL_INFO,1,TLB_MASK,"pid=%d addr=%#" PRIx64 " %s\n",pid, addr, retval ? "hit":"miss" );
        return retval;
    }

    Hermes::Vaddr getPageAddr(Hermes::Vaddr addr ) {
        return addr & m_pageMask;
    } 

    Unit* m_load;
    Unit* m_store;
    int m_cacheSize;
    int m_maxPendingStores;
    int m_maxPendingLoads;
    int m_numPendingStores;
    int m_numPendingLoads;
    int m_numWalkers;
    int m_pendingWalk;
    int m_tlbMissLat_ns;

    bool m_storeBlocked;
    bool m_loadBlocked;

    UnitBase* m_blockedStoreSrc;
    UnitBase* m_blockedLoadSrc;

    int m_pendingWalks;
    std::deque<Entry*> m_readyLoads;
    std::deque<Entry*> m_blockedByTLB;
    std::deque<Entry*> m_readyStores;
    std::unordered_map< Hermes::Vaddr, std::deque<Entry*> > m_pendingWalkList;

    int                 m_curPid;
    uint64_t            m_pageMask;
    Cache               m_cache;
};