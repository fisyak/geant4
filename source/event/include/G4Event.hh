//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
// G4Event
//
// Class description:
//
// This is the class which represents an event. A G4Event is constructed and
// deleted by G4RunManager (or its derived class). When a G4Event object is
// passed to G4EventManager, G4Event must have one or more primary verteces
// and primary particle(s) associated to the verteces as an input of
// simulating an event.
// G4Event has trajectories, hits collections, and/or digi collections. 

// Author: M.Asai, SLAC
// Adding sub-event : M.Asai, JLAB
// --------------------------------------------------------------------
#ifndef G4Event_hh
#define G4Event_hh 1

#include <set>
#include <map>

#include "globals.hh"
#include "evtdefs.hh"
#include "G4Allocator.hh"
#include "G4PrimaryVertex.hh"
#include "G4HCofThisEvent.hh"
#include "G4DCofThisEvent.hh"
#include "G4TrajectoryContainer.hh"
#include "G4VUserEventInformation.hh"

class G4VHitsCollection;
class G4SubEvent;

class G4Event 
{
  public:
    G4Event() = default;
    explicit G4Event(G4int evID);
   ~G4Event();

    G4Event(const G4Event &) = delete;
    G4Event& operator=(const G4Event &) = delete;

    inline void *operator new(std::size_t);
    inline void operator delete(void* anEvent);

    G4bool operator==(const G4Event& right) const;
    G4bool operator!=(const G4Event& right) const;

    void Print() const;
      // Print the event ID (starts with zero and increments by one) to G4cout.
    void Draw() const;
      // Invoke Draw() methods of all stored trajectories, hits, and digits.
      // For hits and digits, Draw() methods of the concrete classes must be
      // implemented. Otherwise nothing will be drawn.

    inline void SetEventID(G4int i)
      { eventID =  i; }
    inline void SetHCofThisEvent(G4HCofThisEvent* value)
      { HC = value; }
    inline void SetDCofThisEvent(G4DCofThisEvent* value)
      { DC = value; }
    inline void SetTrajectoryContainer(G4TrajectoryContainer* value)
      { trajectoryContainer = value; }
    inline void SetEventAborted()
      { eventAborted = true; }
    inline void SetRandomNumberStatus(G4String& st)
      {
        randomNumberStatus = new G4String(st);
        validRandomNumberStatus = true;
      }
    inline void SetRandomNumberStatusForProcessing(G4String& st)
      {
        randomNumberStatusForProcessing = new G4String(st);
        validRandomNumberStatusForProcessing = true;
      }
    inline void KeepTheEvent(G4bool vl=true) const
      { keepTheEvent = vl; }
    inline G4bool KeepTheEventFlag() const
      { return keepTheEvent; }
    inline G4bool ToBeKept() const
      {
        /* TODO (PHASE-II): consider grips for subevents
        { return keepTheEvent || grips>0 || GetNumberOfRemainingSubEvents()>0; }
        */
        return keepTheEvent || GetNumberOfRemainingSubEvents()>0;
      }
    inline void KeepForPostProcessing() const
      { ++grips; }
    inline void PostProcessingFinished() const
      {
        --grips;
        if (grips<0)
        {
          G4Exception("G4Event::Release()", "EVENT91001", JustWarning,
                      "Number of grips is negative. This cannot be correct.");
        }
      }
    inline G4int GetNumberOfGrips() const
      { return grips; }

    inline G4int GetEventID() const
      { return eventID; }

    inline void AddPrimaryVertex(G4PrimaryVertex* aPrimaryVertex)
      {
        //  This method sets a new primary vertex. This method must be invoked 
        // exclusively by G4VPrimaryGenerator concrete class.

        if( thePrimaryVertex == nullptr )
        { thePrimaryVertex = aPrimaryVertex; }
        else
        { thePrimaryVertex->SetNext( aPrimaryVertex ); }
        ++numberOfPrimaryVertex;
      }

    inline G4int GetNumberOfPrimaryVertex() const
      { return numberOfPrimaryVertex; }
      // Returns number of primary verteces the G4Event object has.

    inline G4PrimaryVertex* GetPrimaryVertex(G4int i=0)  const
      { 
        if( i == 0 )
        { return thePrimaryVertex; }
        if( i > 0 && i < numberOfPrimaryVertex )
        {
          G4PrimaryVertex* primaryVertex = thePrimaryVertex;
          for( G4int j=0; j<i; ++j )
          {
            if( primaryVertex == nullptr ) return nullptr; 
            primaryVertex = primaryVertex->GetNext();
          }
          return primaryVertex;
        }
        
        return nullptr;
      }
      // Returns i-th primary vertex of the event.

    inline G4HCofThisEvent* GetHCofThisEvent()  const
      { return HC; }
    inline G4DCofThisEvent* GetDCofThisEvent()  const
      { return DC; }
    inline G4TrajectoryContainer* GetTrajectoryContainer() const
      { return trajectoryContainer; }
      //  These three methods return the pointers to the G4HCofThisEvent
      // (hits collections of this event), G4DCofThisEvent (digi collections
      // of this event), and G4TrajectoryContainer (trajectory coonainer),
      // respectively.

    inline G4bool IsAborted() const { return eventAborted; }
      //  Return a boolean which indicates the event has been aborted and thus
      // it should not be used for analysis.

    inline void SetUserInformation(G4VUserEventInformation* anInfo)
      { userInfo = anInfo; }
    inline G4VUserEventInformation* GetUserInformation() const
      { return userInfo; }
      //  Set and Get method of G4VUserEventInformation

    inline const G4String& GetRandomNumberStatus() const 
      {
        if(!validRandomNumberStatus)
        { G4Exception(
              "G4Event::GetRandomNumberStatus","Event0701",JustWarning,
              "Random number status is not available for this event."); }
        return *randomNumberStatus;
      }
    inline const G4String& GetRandomNumberStatusForProcessing() const 
      {
        if(!validRandomNumberStatusForProcessing)
        { G4Exception(
              "G4Event::GetRandomNumberStatusForProcessing","Event0702",
              JustWarning,
              "Random number status is not available for this event."); }
        return *randomNumberStatusForProcessing;
      }

  private:

    // event ID
    G4int eventID = 0;      

    // PrimaryVertex
    G4PrimaryVertex* thePrimaryVertex = nullptr;
    G4int numberOfPrimaryVertex = 0;

    // HitsCollection
    G4HCofThisEvent* HC = nullptr;

    // DigiCollection
    G4DCofThisEvent* DC = nullptr;

    // TrajectoryContainer
    G4TrajectoryContainer* trajectoryContainer = nullptr;

    // Boolean flag which shall be set to true if the event is aborted and 
    // thus the containing information is not to be used.
    G4bool eventAborted = false;

    // UserEventInformation (optional)
    G4VUserEventInformation* userInfo = nullptr;

    // Initial random number engine status before primary particle generation
    G4String* randomNumberStatus = nullptr;
    G4bool validRandomNumberStatus = false;

    // Initial random number engine status before event processing
    G4String* randomNumberStatusForProcessing = nullptr;
    G4bool validRandomNumberStatusForProcessing = false;

    // Flag to keep the event until the end of run
    mutable G4bool keepTheEvent = false;
    mutable G4int grips = 0;

  //========================= for sub-event parallelism
  // following methods should be used only within the master thread

  public:
    G4SubEvent* PopSubEvent(G4int);
      // This method is to be invoked from G4SubEvtRunManager. 
      // SpawnSubEvent() is internally invoked.
    G4int TerminateSubEvent(G4SubEvent*);
      // This method is to be invoked from G4SubEvtRunManager once a sub-event is
      // fully processed by a worker thread.

    G4int StoreSubEvent(G4int,G4SubEvent*);
      // This method is used by G4EventManager to store all the remeining sub-event 
      // object at the end of processing the event.

    G4int SpawnSubEvent(G4SubEvent*);
      // Registering sub-event when it is sent to a worker thread.
    G4int GetNumberOfRemainingSubEvents() const;
      // Number of sub-events that are either still waiting to be processed by worker
      // threads or sent to worker threads but not yet completed
    inline G4int GetNumberOfCompletedSubEvent() const
    { return (G4int)fSubEventGarbageBin.size(); }

    void MergeSubEventResults(const G4Event* se);

  private:
  // These containers are for sub-event objects.
  // - fSubEvtStackMap stores sub-events that are yet to be sent to the worker thread
  //   it stores sub-events sorted separately to sub-event types
  // - fSubEvtVector stores sub-events that are sent to worther thread for processing
  // - fSubEventGarbageBin stores sub-events that have already been processed by worker thread
  //   these sub-events are deleted at the time the G4Event of the master thread is deleted
  //   - TODO (PHASE-II): we may need to consider better way to delete sub-events stored in this garbase bin
  //     at earlier timing to reduce amount of memory used for storing these garbages
    std::map<G4int,std::set<G4SubEvent*>*> fSubEvtStackMap;
    std::set<G4SubEvent*> fSubEvtVector;
    std::set<G4SubEvent*> fSubEventGarbageBin;

  // motherEvent pointer is non-null for an event created in the worker
  // thread of sub-event parallelism.
  // This object and all the contained objects must be deleted by the
  // responsible worker thread.
  private:
    G4Event* motherEvent = nullptr;
    G4int subEventType = -1;

  public:
    void FlagAsSubEvent(G4Event* me, G4int ty)
    {
      motherEvent = me;
      subEventType = ty;
    }
    inline G4Event* GetMotherEvent() const
    { return motherEvent; }
    inline G4int GetSubEventType() const
    { return subEventType; }

  private:
    mutable G4bool scoresRecorded = false;
    mutable G4bool eventCompleted = false;
  public:
    void ScoresRecorded() const { scoresRecorded = true; }
    G4bool ScoresAlreadyRecorded() const { return scoresRecorded; }
    void EventCompleted() const { eventCompleted = true; }
    G4bool IsEventCompleted() const { return eventCompleted; }
};

extern G4EVENT_DLL G4Allocator<G4Event>*& anEventAllocator();

inline void* G4Event::operator new(std::size_t)
{ 
  if (anEventAllocator() == nullptr) 
  {
    anEventAllocator() = new G4Allocator<G4Event>;
  }
  return (void*)anEventAllocator()->MallocSingle();
}

inline void G4Event::operator delete(void* anEvent)
{
  anEventAllocator()->FreeSingle((G4Event*)anEvent);
}

#endif
