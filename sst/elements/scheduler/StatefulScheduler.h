// Copyright 2011 Sandia Corporation. Under the terms                          
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.             
// Government retains certain rights in this software.                         
//                                                                             
// Copyright (c) 2011, Sandia Corporation                                      
// All rights reserved.                                                        
//                                                                             
// This file is part of the SST software package. For license                  
// information, see the LICENSE file in the top level directory of the         
// distribution.                                                               

#ifndef __StatefulSCHEDULER_H__
#define __StatefulSCHEDULER_H__

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include "Scheduler.h"
#include "misc.h"

#include <stdio.h>

using namespace std;

class SchedChange{
  protected:
    unsigned long time;
    SchedChange* partner;
  public:
    bool isEnd;
    Job* j;
    SchedChange(unsigned long intime, Job* inj, bool end, SchedChange* inpartner = NULL){
      if(inpartner == NULL && !end)
        error("Schedchange beginning not given partner");
      partner = inpartner;
      time = intime;
      j = inj;
      isEnd = end;
    }
    //need to implement some sort of comparison function for SchedChange

    const unsigned long getTime(){
      return time;
    }

    int freeProcChange() {
      return isEnd ? j->getProcsNeeded(): -j->getProcsNeeded();
    }

    char* toString(){
      char* retval = new char[100];
      sprintf(retval, "%ld : start Job %ld", time, j->getJobNum());
      return retval;
    }
    SchedChange* getPartner(){
      return partner;
    }
    void print(){
      if(!isEnd)
        printf("%s Scheduled from %ld to %ld\n", j->toString().c_str(), time, partner->getTime());
      else
        printf("%s Scheduled until %ld\n", j->toString().c_str(), time);
    }

};

class SCComparator{
  public:
    bool operator()(SchedChange* const& first, SchedChange* const& second)
    {
      //needs to be checked for right direction
      if(first->getTime() != second->getTime())
        return first->getTime() < second->getTime(); 
      if(first->isEnd && !second->isEnd)
        return false;
      if(!first->isEnd && second->isEnd)
        return true;
      if(first->j->getEstimatedRunningTime() != second->j->getEstimatedRunningTime())
        return first->j->getEstimatedRunningTime() < second->j->getEstimatedRunningTime();
      return first->j->getJobNum() < second->j->getJobNum() ;
    }
};

class StatefulScheduler : public Scheduler {
  private:
    static const int numCompTableEntries;
    enum ComparatorType {  //to represent type of JobComparator
      FIFO = 0,
      LARGEFIRST = 1,
      SMALLFIRST = 2,
      LONGFIRST = 3,
      SHORTFIRST = 4,
        BETTERFIT = 5
    };
    struct compTableEntry {
      ComparatorType val;
      string name;
    };
    static const compTableEntry compTable[6];

    string compSetupInfo;
    set<SchedChange*, SCComparator> *estSched;
    unsigned long findTime(set<SchedChange*, SCComparator> *sched, Job* Job, unsigned long time);
    
    int numProcs;
    int freeProcs;

  public:
    void jobArrives(Job* j, unsigned long time, Machine* mach);
    void jobFinishes(Job* j, unsigned long time, Machine* mach);

    //Make????
    void reset();
    unsigned long scheduleJob(Job* job, unsigned long time);
    unsigned long zeroCase(set<SchedChange*, SCComparator> *sched, Job* filler, unsigned long time);
    AllocInfo* tryToStart(Allocator* alloc, unsigned long time, Machine* mach, Statistics* stats);
    string getSetupInfo(bool comment);
    void printPlan();
    void done(){
      heart->done(); 
    }
    void removeJob(Job* j, unsigned long time);

    class JobComparator : public binary_function<Job*,Job*,bool> {
      public:
        static JobComparator* Make(string typeName);  //return NULL if name is invalid
        static void printComparatorList(ostream& out);  //print list of possible comparators
        bool operator()(Job*& j1, Job*& j2);
        bool operator()(Job* const& j1, Job* const& j2);
        string toString();
      private:
        JobComparator(ComparatorType type);
        ComparatorType type;
    };

    //MANAGERS:******************************************************
    class Manager{
      protected:
        StatefulScheduler* scheduler;
      public:
        virtual void arrival(Job* j, unsigned long time) = 0;
        virtual void start(Job* j, unsigned long time) = 0;
        virtual void tryToStart(unsigned long time) = 0;
        virtual void printPlan() = 0;
        virtual void onTimeFinish(Job* j, unsigned long time) = 0;
        virtual void reset() = 0;
        virtual void done() = 0;
        virtual void earlyFinish(Job* j, unsigned long time) = 0;
        virtual void removeJob(Job* j, unsigned long time) = 0;
        virtual string getString() = 0;
        void compress(unsigned long time) ;
    };

    class ConservativeManager : public Manager{
      public:
        ConservativeManager(StatefulScheduler* inscheduler){
          scheduler = inscheduler;
        }
        void earlyFinish(Job* j, unsigned long time){
          compress(time);
        }
        void removeJob(Job* j, unsigned long time){
          compress(time);
        }
        void arrival(Job* j, unsigned long time){
        }
        void start(Job* j, unsigned long time){
        }
        void tryToStart(unsigned long time){
        }
        void printPlan(){
        }
        void onTimeFinish(Job* j, unsigned long time){
        }
        void reset(){
        }
        void done(){
        }
        string getString();
    };

    class PrioritizeCompressionManager : public Manager{
      protected:
        set<Job*, JobComparator>* backfill;
        int fillTimes;
        int* numSBF;
      public:
          PrioritizeCompressionManager(StatefulScheduler* inscheduler, JobComparator* comp, int infillTimes);
          void reset();
          void arrival(Job* j, unsigned long time){
            backfill->insert(j);
          }
          void start(Job *j, unsigned long time){
            backfill->erase(j);
          }
          void printPlan();
          void done();
          void earlyFinish(Job* j, unsigned long time);
          void tryToStart(unsigned long time);
          void removeJob(Job* j, unsigned long time);
          void onTimeFinish(Job* j, unsigned long time);
        string getString();
    };

    class DelayedCompressionManager : public Manager{
      protected:
        set<Job*, JobComparator>* backfill;
      public:
        DelayedCompressionManager(StatefulScheduler* inscheduler, JobComparator* comp);
        void reset();
        void arrival(Job* j, unsigned long time);
        void start(Job* j, unsigned long time){
          backfill->erase(j);
        }
        void tryToStart(unsigned long time);
        void printPlan();
        void done();
        void earlyFinish(Job *j, unsigned long time);
        void fill(unsigned long time);
        void removeJob(Job* j, unsigned long time);
        void onTimeFinish(Job* j, unsigned long time);
        string getString();
      private:
        int results;
    };

    class EvenLessManager : public Manager{
      protected:
        set<Job*, JobComparator>* backfill;
        set<SchedChange*, SCComparator> *guarantee;
        map<Job*, SchedChange*, JobComparator> *guarJobToEvents;
        int bftimes;
      public:
        EvenLessManager(StatefulScheduler* inscheduler, JobComparator* comp, int infillTimes);
        void deepCopy(set<SchedChange*, SCComparator> *from, set<SchedChange*, SCComparator> *to, map<Job*, SchedChange*, JobComparator> *toJ);
        void backfillfunc(unsigned long time);
        void arrival(Job* j, unsigned long time);
        void start(Job* j, unsigned long time);
        void tryToStart(unsigned long time){};
        void printPlan();
        void done(){};
        void earlyFinish(Job* j, unsigned long time);
        void onTimeFinish(Job* j, unsigned long time);
        void fill(unsigned long time);
        void removeJob(Job* j, unsigned long time);
        void reset();
        string getString();
      private:
        int results;
    };
    //MANAGERS OVER***************************************************************

    map<Job*, SchedChange*, JobComparator>* jobToEvents;

    StatefulScheduler(int numprocs, JobComparator* comp, bool dummy);
    StatefulScheduler(int numprocs, JobComparator* comp, int fillTimes);
    StatefulScheduler(int numprocs, JobComparator* comp);
    StatefulScheduler(int numprocs, JobComparator* comp, int fillTimes, bool dummy);
    StatefulScheduler(JobComparator* comp);

  protected:
    Manager *heart;
};




#endif
