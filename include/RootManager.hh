#ifndef ROOTMANAGER_HH
#define ROOTMANAGER_HH

#include "TFile.h"
#include "THnSparse.h"
#include "TRandom3.h"
#include <mutex>
#include <atomic>
#include <string>
#include <memory>

class RootManager {
public:
    static RootManager& GetInstance() {
        static RootManager instance;
        return instance;
    }
    
    void Initialize(const std::string& filename, const std::string& histname);
    void SampleEvent(Double_t* values);
    bool IsInitialized() const { return fInitialized; }

    void SetFileNum(int num) { fFileNum = num;  }
    int  GetFileNum()        { return fFileNum; }

    // Cleanup method to call at end of program
    void Cleanup();

private:
    RootManager();
    ~RootManager();
    
    RootManager(const RootManager&) = delete;
    RootManager& operator=(const RootManager&) = delete;
    
    // Master histogram (not used directly in sampling)
    THnSparseD* fMasterSparse;
    std::atomic<bool> fInitialized;
    std::mutex fMutex;
    
    // Thread-local storage key (conceptual)
    static thread_local std::unique_ptr<THnSparseD> fThreadLocalSparse;
    static thread_local std::unique_ptr<TRandom3> fThreadLocalRandom;

    int fFileNum = -1;
};

#endif
