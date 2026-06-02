//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef PrimaryGeneratorMessenger_h
#define PrimaryGeneratorMessenger_h 1

#include "G4UImessenger.hh"
#include "globals.hh"

class PrimaryGeneratorAction;
class G4UIdirectory;
class G4UIcmdWithAnInteger;
class G4UIcmdWith3Vector;
class G4UIcmdWithoutParameter;
class G4UIcmdWithAString; 
class G4UIcmdWithADouble;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithABool;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class PrimaryGeneratorMessenger: public G4UImessenger
{
    public:
        PrimaryGeneratorMessenger(PrimaryGeneratorAction*);
        virtual ~PrimaryGeneratorMessenger();

        void SetNewValue(G4UIcommand*, G4String);

    private:
        PrimaryGeneratorAction*     fPrimaryGeneratorAction = nullptr;

        G4UIdirectory*              fDir = nullptr;

        G4UIcmdWithAString*         fSourceModeCmd;   

        G4UIcmdWithAnInteger* fCaskNumCmd = nullptr;
        G4UIcmdWithAnInteger* fFuelNumCmd = nullptr;
        
        G4UIcmdWithADoubleAndUnit * fCLYCBoundingRadiusCmd = nullptr;

        G4UIcmdWithABool*           fUseWattCmd  = nullptr;
        G4UIcmdWithADoubleAndUnit*  fWattACmd    = nullptr;
        G4UIcmdWithADouble*         fWattBCmd    = nullptr;

        G4UIcmdWithAString* fSurfaceSourceFileCmd = nullptr;
        G4UIcmdWithAnInteger* fSurfaceSourcePidCmd = nullptr;
        G4UIcmdWithAnInteger* fSurfaceSourceMaxEntriesCmd = nullptr;

};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif

