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
//
//

#ifndef G4PSTermination_h
#define G4PSTermination_h 1

#include "G4VPrimitiveScorer.hh"
#include "G4THitsMap.hh"

//////////////////////////////////////////////////////////////////////////////////
// (Description)
//   This is a primitive scorer class for scoring Number of Steps in the cell.
//
// Created: 2007-2-2  Tsukasa ASO, Akinori Kimura.
// 2010-07-22   Introduce Unit specification.
//
///////////////////////////////////////////////////////////////////////////////

class G4PSTermination : public G4VPrimitiveScorer
{
 public:
  G4PSTermination(const G4String& name, G4int depth = 0);
  ~G4PSTermination() override = default;

  inline void Weighted(G4bool flg = true) { weighted = flg; }
  // Multiply track weight
  
  void Initialize(G4HCofThisEvent*) override;
  void clear() override;
  void PrintAll() override;

  virtual void SetUnit(const G4String& unit);

 protected:
  G4bool ProcessHits(G4Step*, G4TouchableHistory*) override;

 private:
  G4int HCID{-1};
  G4THitsMap<G4double>* EvtMap{nullptr};
  G4bool weighted{false};
};

#endif
