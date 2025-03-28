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
// G4EventManager class implementation
//
// Author: M.Asai, SLAC
// Adding sub-event parallelism: M.Asai, JLAB
// --------------------------------------------------------------------

#include "G4EventManager.hh"
#include "G4ios.hh"
#include "G4EvManMessenger.hh"
#include "G4Event.hh"
#include "G4ParticleDefinition.hh"
#include "G4VTrackingManager.hh"
#include "G4UserEventAction.hh"
#include "G4UserStackingAction.hh"
#include "G4SDManager.hh"
#include "G4StateManager.hh"
#include "G4ApplicationState.hh"
#include "G4TransportationManager.hh"
#include "G4Navigator.hh"
#include "Randomize.hh"
#include "G4GlobalFastSimulationManager.hh"
#include "G4AutoLock.hh"

namespace {
 G4Mutex EventMgrMutex = G4MUTEX_INITIALIZER;
}

#include <unordered_set>

G4ThreadLocal G4EventManager* G4EventManager::fpEventManager = nullptr;

G4EventManager* G4EventManager::GetEventManager()
{
  return fpEventManager;
}

G4EventManager::G4EventManager()
{
  if(fpEventManager != nullptr)
  {
    G4Exception("G4EventManager::G4EventManager", "Event0001", FatalException,
                "G4EventManager::G4EventManager() has already been made.");
  }
  else
  {
    trackManager = new G4TrackingManager;
    transformer = new G4PrimaryTransformer;
    trackContainer = new G4StackManager;
    theMessenger = new G4EvManMessenger(this);
    sdManager = G4SDManager::GetSDMpointerIfExist();
    stateManager = G4StateManager::GetStateManager();
    fpEventManager = this;
  }
}

G4EventManager::~G4EventManager()
{
  delete trackContainer;
  delete transformer;
  delete trackManager;
  delete theMessenger;
  delete userEventAction;
  fpEventManager = nullptr;
}

void G4EventManager::DoProcessing(G4Event* anEvent,
  G4TrackVector* trackVector, G4bool IDhasAlreadySet)
{
  abortRequested = false;
  G4ApplicationState currentState = stateManager->GetCurrentState();
  if(currentState != G4State_GeomClosed)
  {
    G4Exception("G4EventManager::ProcessOneEvent", "Event0002", JustWarning,
           "IllegalState -- Geometry not closed: cannot process an event.");
    return;
  }
  currentEvent = anEvent;
  if(!subEventParaWorker) stateManager->SetNewState(G4State_EventProc);
  if(storetRandomNumberStatusToG4Event > 1)
  {
    std::ostringstream oss;
    CLHEP::HepRandom::saveFullState(oss);
    randomNumberStatusToG4Event = oss.str();
    currentEvent->SetRandomNumberStatusForProcessing(randomNumberStatusToG4Event); 
  }

  // Resetting Navigator has been moved to G4EventManager,
  // so that resetting is now done for every event.
  G4ThreeVector center(0,0,0);
  G4Navigator* navigator = G4TransportationManager::GetTransportationManager()
                         ->GetNavigatorForTracking();
  navigator->LocateGlobalPointAndSetup(center,nullptr,false);
                                                                                      
  G4Track* track = nullptr;
  G4TrackStatus istop = fAlive;

#ifdef G4VERBOSE
  if ( verboseLevel > 0 )
  {
    G4cout << "=====================================" << G4endl;
    G4cout << "  G4EventManager::ProcessOneEvent()  " << G4endl;
    if(trackVector!=nullptr) G4cout << "     for a sub-event" << G4endl;
    G4cout << "=====================================" << G4endl;
  }
#endif

  trackContainer->PrepareNewEvent(currentEvent);

#ifdef G4_STORE_TRAJECTORY
  trajectoryContainer = nullptr;
//  trajectoryContainer = currentEvent->GetTrajectoryContainer();
//  if(trajectoryContainer==nullptr) {
//    trajectoryContainer = new G4TrajectoryContainer;
//    currentEvent->SetTrajectoryContainer(trajectoryContainer);
//  }
#endif

  sdManager = G4SDManager::GetSDMpointerIfExist();
  if(sdManager != nullptr)
  { currentEvent->SetHCofThisEvent(sdManager->PrepareNewEvent()); }

  if(!subEventParaWorker && userEventAction != nullptr) userEventAction->BeginOfEventAction(currentEvent);

#ifdef G4VERBOSE
  if ( verboseLevel > 1 )
  {
    G4cout << currentEvent->GetNumberOfPrimaryVertex()
         << " vertices passed from G4Event." << G4endl;
  }
#endif

  if(trackVector!=nullptr) 
  {
    StackTracks(trackVector,IDhasAlreadySet);
  }
  if(!abortRequested)
  {
    StackTracks(transformer->GimmePrimaries(currentEvent,trackIDCounter), true);
  }

#ifdef G4VERBOSE
  if ( verboseLevel > 0 )
  {
    G4cout << trackContainer->GetNTotalTrack() << " primary tracks "
           << "are passed to the stack." << G4endl;
    G4cout << "!!!!!!! Now start processing an event !!!!!!!" << G4endl;
  }
#endif

  std::unordered_set<G4VTrackingManager *> trackingManagersToFlush;

  do
  {
    G4VTrajectory* previousTrajectory;
    while( (track=trackContainer->PopNextTrack(&previousTrajectory)) != nullptr )
    {                                        // Loop checking 12.28.2015 M.Asai

      const G4ParticleDefinition* partDef = track->GetParticleDefinition();
      G4VTrackingManager* particleTrackingManager = partDef->GetTrackingManager();

      if (particleTrackingManager != nullptr)
      {
#ifdef G4VERBOSE
        if ( verboseLevel > 1 )
        {
          G4cout << "Track " << track << " (trackID " << track->GetTrackID()
                 << ", parentID " << track->GetParentID()
                 << ") is handed over to custom TrackingManager." << G4endl;
        }
#endif

        particleTrackingManager->HandOverOneTrack(track);
        // The particle's tracking manager may either track immediately or
        // defer processing until FlushEvent is called. Thus, we must neither
        // check the track's status nor stack secondaries.

        // Remember this tracking manager to later call FlushEvent.
        trackingManagersToFlush.insert(particleTrackingManager);

      } else {
#ifdef G4VERBOSE
        if ( verboseLevel > 1 )
        {
          G4cout << "Track " << track << " (trackID " << track->GetTrackID()
                 << ", parentID " << track->GetParentID()
                 << ") is passed to G4TrackingManager." << G4endl;
        }
#endif

        tracking = true;
        trackManager->ProcessOneTrack( track );
        istop = track->GetTrackStatus();
        tracking = false;

#ifdef G4VERBOSE
        if ( verboseLevel > 0 )
        {
          G4cout << "Track (trackID " << track->GetTrackID()
             << ", parentID " << track->GetParentID()
             << ") is processed with stopping code " << istop << G4endl;
        }
#endif

        G4VTrajectory* aTrajectory = nullptr;
#ifdef G4_STORE_TRAJECTORY
        aTrajectory = trackManager->GimmeTrajectory();

        if(previousTrajectory != nullptr)
        {
          previousTrajectory->MergeTrajectory(aTrajectory);
          delete aTrajectory;
          aTrajectory = previousTrajectory;
        }
        if((aTrajectory != nullptr)&&(istop!=fStopButAlive)
           &&(istop!=fSuspend)&&(istop!=fSuspendAndWait))
        {
          if(trajectoryContainer == nullptr)
          {
            trajectoryContainer = new G4TrajectoryContainer;
            currentEvent->SetTrajectoryContainer(trajectoryContainer);
          }
          trajectoryContainer->insert(aTrajectory);
        }
#endif

        G4TrackVector* secondaries = trackManager->GimmeSecondaries();
        switch (istop)
        {
          case fStopButAlive:
          case fSuspend:
          case fSuspendAndWait:
            trackContainer->PushOneTrack( track, aTrajectory );
            StackTracks( secondaries );
            break;

          case fPostponeToNextEvent:
            trackContainer->PushOneTrack( track );
            StackTracks( secondaries );
            break;

          case fStopAndKill:
            StackTracks( secondaries );
            delete track;
            break;

          case fAlive:
            G4Exception("G4EventManager::DoProcessing", "Event004", JustWarning,
                "Illegal track status returned from G4TrackingManager."\
                " Continue with simulation.");
            break;

          case fKillTrackAndSecondaries:
            if( secondaries != nullptr )
            {
              for(auto & secondarie : *secondaries)
              { delete secondarie; }
              secondaries->clear();
            }
            delete track;
            break;
        }
      }
    }

    // Flush all tracking managers, which may have deferred processing until now.
    for (G4VTrackingManager *tm : trackingManagersToFlush)
    {
      tm->FlushEvent();
    }
    trackingManagersToFlush.clear();

    // flush any fast simulation models
    G4GlobalFastSimulationManager::GetGlobalFastSimulationManager()->Flush();

    // Check if flushing one of the tracking managers or a fast simulation model
    // stacked new secondaries.
  } while (trackContainer->GetNUrgentTrack() > 0);

#ifdef G4VERBOSE
  if ( verboseLevel > 0 )
  {
    G4cout << "NULL returned from G4StackManager." << G4endl;
    G4cout << "Terminate current event processing." << G4endl;
  }
#endif

  if(sdManager != nullptr)
  {
    sdManager->TerminateCurrentEvent(currentEvent->GetHCofThisEvent());
  }

//  In case of sub-event parallelism, an event may not be completed at
//  this point but results of sus-events may be merged later. Thus
//  userEventAction->EndOfEventAction() is invoked by G4RunManager
//  immediately prior to deleting the event.
  if(!subEventPara && (userEventAction != nullptr))
  {
    userEventAction->EndOfEventAction(currentEvent);
  }

  // Store remaining sub-events to the current event
  auto nses = trackContainer->GetNSubEventTypes();
  if(nses>0)
  {
#ifdef G4VERBOSE
    if ( verboseLevel > 2 )
    {
      G4cout<<"## End of processing an event --- "
            <<nses<<" sub-event types registered."<<G4endl;
    }
#endif
    for(std::size_t i=0;i<nses;i++)
    {
      auto ty = trackContainer->GetSubEventType(i);
      trackContainer->ReleaseSubEvent(ty);
    }
  }

  if(!subEventParaWorker) stateManager->SetNewState(G4State_GeomClosed);
  currentEvent = nullptr;
  abortRequested = false;
}

G4SubEvent* G4EventManager::PopSubEvent(G4int ty)
{
  G4AutoLock lock(&EventMgrMutex);
  if(currentEvent==nullptr) return nullptr;
  return currentEvent->PopSubEvent(ty);
}

void G4EventManager::TerminateSubEvent(const G4SubEvent* se,const G4Event* evt)
{
  G4AutoLock lock(&EventMgrMutex);
  auto ev = se->GetEvent();
  ev->MergeSubEventResults(evt);
  if(!subEventParaWorker && userEventAction!=nullptr) userEventAction->MergeSubEvent(ev,evt);
#ifdef G4VERBOSE
  // Capture this here because termination will delete subevent...
  G4int seType = se->GetSubEventType();
#endif
  ev->TerminateSubEvent(const_cast<G4SubEvent*>(se));
#ifdef G4VERBOSE
  if ( verboseLevel > 1 )
  {
    G4cout << "A sub-event of type " << seType
           << " is merged to the event " << ev->GetEventID() << G4endl;
    if(ev->GetNumberOfRemainingSubEvents()>0)
    {
      G4cout << " ---- This event still has " << ev->GetNumberOfRemainingSubEvents()
             << " sub-events to be processed." << G4endl;
    }
    else
    { G4cout << " ---- This event has no more sub-event remaining." << G4endl; }
  }
#endif
}

G4int G4EventManager::StoreSubEvent(G4Event* evt, G4int& subEvtType, G4SubEvent* se)
{
  G4AutoLock lock(&EventMgrMutex);
  if(evt != currentEvent) {
    G4Exception("G4EventManager::StoreSubEvent","SubEvt1011", FatalException,
                "StoreSubEvent is invoked with a G4Event that is not the current event. PANIC!");
  }
  return evt->StoreSubEvent(subEvtType,se);
}

void G4EventManager::StackTracks(G4TrackVector* trackVector,
                                 G4bool IDhasAlreadySet)
{
  if( trackVector != nullptr )
  {
    if( trackVector->empty() ) return;
    for( auto newTrack : *trackVector )
    {
      ++trackIDCounter;
      if(!IDhasAlreadySet)
      {
        newTrack->SetTrackID( trackIDCounter );
        if(newTrack->GetDynamicParticle()->GetPrimaryParticle() != nullptr)
        {
          auto* pp
            = (G4PrimaryParticle*)(newTrack->GetDynamicParticle()->GetPrimaryParticle());
          pp->SetTrackID(trackIDCounter);
        }
      }
      newTrack->SetOriginTouchableHandle(newTrack->GetTouchableHandle());
      trackContainer->PushOneTrack( newTrack );
#ifdef G4VERBOSE
      if ( verboseLevel > 1 )
      {
        G4cout << "A new track " << newTrack 
               << " (trackID " << newTrack->GetTrackID()
               << ", parentID " << newTrack->GetParentID() 
               << ") is passed to G4StackManager." << G4endl;
      }
#endif
    }
    trackVector->clear();
  }
}

void G4EventManager::SetUserAction(G4UserEventAction* userAction)
{
  userEventAction = userAction;
  if(userEventAction != nullptr)
  {
    userEventAction->SetEventManager(this);
  }
}

void G4EventManager::SetUserAction(G4UserStackingAction* userAction)
{
  userStackingAction = userAction;
  trackContainer->SetUserStackingAction(userAction);
}

void G4EventManager::SetUserAction(G4UserTrackingAction* userAction)
{
  userTrackingAction = userAction;
  trackManager->SetUserAction(userAction);
}

void G4EventManager::SetUserAction(G4UserSteppingAction* userAction)
{
  userSteppingAction = userAction;
  trackManager->SetUserAction(userAction);
}

void G4EventManager::ProcessOneEvent(G4Event* anEvent)
{
  trackIDCounter = 0;
  DoProcessing(anEvent);
}

void G4EventManager::ProcessOneEvent(G4TrackVector* trackVector,
                                     G4Event* anEvent)
{
  static G4ThreadLocal G4String* randStat = nullptr;
  if (randStat == nullptr) randStat = new G4String;
  G4bool tempEvent = false;
  G4bool newEvent = false;
  if(anEvent == nullptr)
  {
    anEvent = new G4Event();
    tempEvent = true;
    newEvent = true;
  } else {
    if(evID_inSubEv != anEvent->GetEventID()) {
      evID_inSubEv = anEvent->GetEventID();
      newEvent = true;
    }
  }
  if(newEvent) trackIDCounter = 0;

  if (storetRandomNumberStatusToG4Event==1
   || storetRandomNumberStatusToG4Event==3)
  {
    std::ostringstream oss;
    CLHEP::HepRandom::saveFullState(oss);
    (*randStat) = oss.str();
    anEvent->SetRandomNumberStatus(*randStat);
  }
  DoProcessing(anEvent,trackVector,false);
  if(tempEvent) { delete anEvent; }
}

void G4EventManager::SetUserInformation(G4VUserEventInformation* anInfo)
{ 
  G4ApplicationState currentState = stateManager->GetCurrentState();
  if(currentState != G4State_EventProc || currentEvent == nullptr)
  {
    G4Exception("G4EventManager::SetUserInformation",
                "Event0003", JustWarning,
                "G4VUserEventInformation cannot be set because of absence "\
                "of G4Event.");
    return;
  }
  
  currentEvent->SetUserInformation(anInfo);
}

G4VUserEventInformation* G4EventManager::GetUserInformation()
{ 
  G4ApplicationState currentState = stateManager->GetCurrentState();
  if(currentState != G4State_EventProc || currentEvent == nullptr)
  {
    return nullptr;
  }
  
  return currentEvent->GetUserInformation();
}

void G4EventManager::KeepTheCurrentEvent()
{
  if(currentEvent != nullptr)  { currentEvent->KeepTheEvent(); }
}

void G4EventManager::AbortCurrentEvent()
{
  abortRequested = true;
  trackContainer->clear();
  if(tracking) trackManager->EventAborted();
}
