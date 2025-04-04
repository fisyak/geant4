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
/// \file ExGflashMessenger.hh
/// \brief Definition of the ExGflashMessenger class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef ExGflashMessenger_h
#define ExGflashMessenger_h 1

#include "G4UImessenger.hh"
#include "globals.hh"

class ExGflashDetectorConstruction;
class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;
class G4UIcmdWith3Vector;
class G4UIcmdWithADoubleAndUnit;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class ExGflashMessenger : public G4UImessenger
{
  public:
    ExGflashMessenger(ExGflashDetectorConstruction*);
    ~ExGflashMessenger() override;

    void SetNewValue(G4UIcommand*, G4String) override;

  private:
    ExGflashDetectorConstruction* fDetector;

    G4UIdirectory* fExGflashDir;
    G4UIcmdWithAnInteger* fVerbose;
    G4UIdirectory* fDetDir;
    G4UIcmdWithAString* fMaterCmd;
    G4UIcmdWith3Vector* fLBinCmd;
    G4UIcmdWith3Vector* fRBinCmd;
    G4UIcmdWithAnInteger* fNbCrysCmd;
    G4UIcmdWithADoubleAndUnit* fWCrysCmd;
    G4UIcmdWithADoubleAndUnit* fLCrysCmd;
};

#endif
