// Copyright 2009-2018 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2018, NTESS
// All rights reserved.
// 
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef SST_SCHEDULER_NODECOMPONENT_H
#define SST_SCHEDULER_NODECOMPONENT_H

#include <sst/core/element.h>
#include <sst/core/event.h>
#include <sst/core/sst_types.h>
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/timeConverter.h>
#include "events/FaultEvent.h"
#include "events/JobKillEvent.h"

#include "linkBuilder.h"

#include <vector>
#include <fstream>

namespace SST {
    namespace Scheduler {

        class nodeComponent : public SST::Component, public virtual linkChanger {
            friend class linkBuilder;

            public:

            nodeComponent(SST::ComponentId_t id, SST::Params& params);
            void setup();
            void finish() {}

            virtual void addLink(SST::Link * link, enum linkTypes type);
            virtual void rmLink(SST::Link * link, enum linkTypes type);
            virtual void disconnectYourself();
            virtual std::string getType();
            virtual std::string getID();

            bool doDetailedNetworkSim; //NetworkSim: variable that protects the original functionality without detailed network sim 

            private:
		unsigned short int * yumyumFaultRand48State;
		unsigned short int * yumyumErrorLogRand48State;
		unsigned short int * yumyumErrorLatencyRand48State;
		unsigned short int * yumyumErrorCorrectionRand48State;
		unsigned short int * yumyumJobKillRand48State;

		bool faultsActivated;

            nodeComponent();  // for serialization only
            nodeComponent(const nodeComponent&); // do not implement
            void operator=(const nodeComponent&); // do not implement

            void handleEvent(SST::Event* ev);
            void handleSelfEvent(SST::Event* ev);
            void handleFaultEvent(SST::Event* ev);
            bool canCorrectError(FaultEvent* error);
            unsigned int genFaultLatency( std::string faultName );

            void handleJobKillEvent(JobKillEvent * killEvent);

            void sendNextFault(std::string faultType);
            void logError(FaultEvent* faultEvent);
            void logFault(FaultEvent* faultEvent);

            int jobNum;
            int nodeNum;

            SST::Link * Scheduler;
            SST::Link * failureInjector;
            SST::Link * SelfLink;
            SST::Link * FaultLink;

            std::vector<SST::Link*> ParentFaultLinks;
            std::vector<SST::Link*> ChildFaultLinks;
            SST::Link* Builder;

            std::map<std::string, float> Faults;
            std::map<std::string, std::pair<unsigned int, unsigned int> > FaultLatencyBounds;
            std::map<std::string, float> errorCorrectionProbability;
            std::map<std::string, float> errorLogProbability;
            std::map<std::string, float> jobKillProbability;
            std::map<int, int> killedJobs;        // jobs that had to be killed, but haven't finished yet.  used to keep track of this node's state
            std::string faultLogFileName;
            std::string errorLogFileName;

            std::string ID;
            std::string nodeType;

        };

    }
}
#endif /* _NODECOMPONENT_H */
