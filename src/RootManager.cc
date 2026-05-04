#include "RootManager.hh"
#include "G4Exception.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

// Thread-local storage definitions
thread_local std::unique_ptr<THnSparseD> RootManager::fThreadLocalSparse = nullptr;
thread_local std::unique_ptr<TRandom3> RootManager::fThreadLocalRandom = nullptr;

RootManager::RootManager() 
    : fMasterSparse(nullptr), 
      fInitialized(false) {
}

RootManager::~RootManager() {
    Cleanup();
}

void RootManager::Cleanup() {
    std::lock_guard<std::mutex> lock(fMutex);
    
    // Only clean up master objects
    if (fMasterSparse) {
        // IMPORTANT: Use Delete() instead of delete operator for ROOT objects
        fMasterSparse->Delete();
        fMasterSparse = nullptr;
    }
    
    fInitialized = false;
}

void RootManager::Initialize(const std::string& filename, const std::string& histname) {
    if (fInitialized) {
        G4cout << "RootManager already initialized" << G4endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(fMutex);
    
    try {
        G4cout << "Initializing RootManager with file: " << filename << G4endl;
        
        // Use smart pointer for automatic cleanup
        std::unique_ptr<TFile> rootFile(TFile::Open(filename.c_str(), "READ"));
        if (!rootFile || rootFile->IsZombie()) {
            G4ExceptionDescription desc;
            desc << "Cannot open ROOT file: " << filename;
            G4Exception("RootManager::Initialize", "RootFileError", 
                       FatalException, desc);
            return;
        }
        
        G4cout << "Successfully opened ROOT file" << G4endl;
        
        // Get the histogram - use Get() and then manually manage
        THnSparseD* hist = dynamic_cast<THnSparseD*>(rootFile->Get(histname.c_str()));
        if (!hist) {
            G4ExceptionDescription desc;
            desc << "Cannot find THnSparse histogram: " << histname;
            G4Exception("RootManager::Initialize", "HistogramError",
                       FatalException, desc);
            return;
        }
        
        // Clone the histogram to take ownership
        fMasterSparse = dynamic_cast<THnSparseD*>(hist->Clone());
        if (!fMasterSparse) {
            G4ExceptionDescription desc;
            desc << "Failed to clone THnSparse histogram";
            G4Exception("RootManager::Initialize", "HistogramCloneError",
                       FatalException, desc);
            return;
        }
        
        // Now we can close the file - our clone is independent
        rootFile->Close();
        
        G4cout << "Successfully cloned THnSparse histogram" << G4endl;
        
        fInitialized = true;
        
        G4cout << "RootManager initialization completed successfully" << G4endl;
        G4cout << "Histogram dimensions: " << fMasterSparse->GetNdimensions() << G4endl;
        
    } catch (std::exception& e) {
        G4ExceptionDescription desc;
        desc << "Exception during RootManager initialization: " << e.what();
        G4Exception("RootManager::Initialize", "RootInitError",
                   FatalException, desc);
    }
}

void RootManager::SampleEvent(Double_t* values) {
    if (!fInitialized) {
        G4ExceptionDescription desc;
        desc << "RootManager not initialized. Call Initialize() first.";
        G4Exception("RootManager::SampleEvent", "NotInitialized",
                   FatalException, desc);
        return;
    }
    
    // Initialize thread-local objects if needed
    if (!fThreadLocalSparse) {
        std::lock_guard<std::mutex> lock(fMutex);
        if (fMasterSparse) {
            fThreadLocalSparse.reset(dynamic_cast<THnSparseD*>(fMasterSparse->Clone()));
            fThreadLocalRandom.reset(new TRandom3());
            
            // Use thread-specific seed
            G4int threadId = G4Threading::G4GetThreadId();
            fThreadLocalRandom->SetSeed(threadId + 1); // +1 to avoid seed=0
        }
    }
    
    if (!fThreadLocalSparse || !fThreadLocalRandom) {
        G4Exception("RootManager::SampleEvent", "ThreadLocalInitError",
                   JustWarning, "Thread-local ROOT objects not initialized");
        return;
    }
    
    // Now sample without any locks - each thread has its own copy
    fThreadLocalSparse->GetRandom(values);
}
