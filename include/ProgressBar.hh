#ifndef PROGBAR_HH
#define PROGBAR_HH 1

#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <vector>

class ProgressBar {
  public:
    // Monotonic clock for elapsed / rate / remaining-time computations.
    // (system_clock is unsuitable here as it can jump backwards on NTP sync.)
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Seconds   = std::chrono::duration<double>;

    inline static std::atomic<uint64_t> gEvtNb{0};

    explicit ProgressBar(uint64_t max_events,
                         double   readout_interval_s = 1.0,
                         int      bar_width          = 40)
      : fMaxEvents     (max_events),
        fLastEvent     (0),
        fPrintInterval (readout_interval_s),
        fBarWidth      (bar_width),
        fN             (100),
        fRates         (fN, 0.0),
        fNextToReplace (0),
        fRateSum       (0.0),
        fNFilled       (0),
        fCounter       (0),
        fDone          (false)
    {
        const TimePoint now = Clock::now();
        fStartTime = now;
        fLastTime  = now;
    }

    ~ProgressBar() = default;

    void SetMaxEvents(uint64_t n)         { fMaxEvents     = n; }
    void SetPrintInterval(double sec)     { fPrintInterval = Seconds(sec); }
    void SetPrintInterval(int    sec)     { SetPrintInterval(static_cast<double>(sec)); }

    // Returns true iff the bar actually printed on this call.
    bool Print(uint64_t event)
    {
        if(fDone) return false;

        const TimePoint now      = Clock::now();
        const Seconds   sinceLast(now - fLastTime);
        const bool      finalEvt = (event == fMaxEvents - 1);

        if (sinceLast < fPrintInterval && !finalEvt) return false;

        // ----------------------------------------------------------------
        // Rate estimate.
        //   thisRate     : instantaneous (events / wall-second since last print)
        //   smoothedRate : running average of the last fN samples (O(1) update
        //                  via fRateSum / fNFilled).
        // For the very first 2 prints we don't yet have a meaningful sample
        // pool, so we fall back to the instantaneous value.
        // ----------------------------------------------------------------
        const double thisDt   = sinceLast.count();   // seconds (double)
        const double thisRate = (thisDt > 0.0)
                              ? static_cast<double>(event - fLastEvent) / thisDt
                              : 0.0;

        // O(1) circular buffer + running sum (replaces the O(N) sweep in Rate()).
        const double oldVal = fRates[fNextToReplace];
        if (oldVal > 0.0) { fRateSum -= oldVal;  --fNFilled; }
        fRates[fNextToReplace] = thisRate;
        if (thisRate > 0.0) { fRateSum += thisRate; ++fNFilled; }
        fNextToReplace = (fNextToReplace + 1) % fN;

        const double smoothed = (fNFilled > 0) ? (fRateSum / fNFilled) : 0.0;
        const double rate     = (fCounter < 2) ? thisRate : smoothed;

        // ----------------------------------------------------------------
        // Time accounting (all from the same monotonic 'now' snapshot, so
        // remaining + elapsed cannot drift relative to each other).
        // ----------------------------------------------------------------
        const uint64_t remainingEvts = (fMaxEvents > event) ? (fMaxEvents - event) : 0;
        const double   secLeft       = (rate > 0.0)
                                     ? static_cast<double>(remainingEvts) / rate
                                     : 0.0;
        const double   secElap       = Seconds(now - fStartTime).count();

        const int sL = static_cast<int>(secLeft);
        const int hL = sL / 3600, mL = (sL / 60) % 60, sLm = sL % 60;
        const int sE = static_cast<int>(secElap);
        const int hE = sE / 3600, mE = (sE / 60) % 60, sEm = sE % 60;

        // ----------------------------------------------------------------
        // Wall-clock ETA. Anchored on system_clock so it represents an
        // actual local time (steady_clock would give meaningless absolute
        // values on most platforms). If we don't yet have a positive rate,
        // we print "--" rather than a nonsense far-future date.
        // ----------------------------------------------------------------
        char etaBuf[32] = "--";
        if (rate > 0.0) {
            using SysClock = std::chrono::system_clock;
            const auto etaTp = SysClock::now()
                + std::chrono::duration_cast<SysClock::duration>(Seconds(secLeft));
            std::time_t etaTT = SysClock::to_time_t(etaTp);
            std::tm     etaTm{};
#if defined(_WIN32)
            localtime_s(&etaTm, &etaTT);
#else
            localtime_r(&etaTT, &etaTm);
#endif
            std::strftime(etaBuf, sizeof(etaBuf), "%Y-%m-%d %H:%M:%S", &etaTm);
        }

        // ----------------------------------------------------------------
        // Render the bar.
        // ----------------------------------------------------------------
        const double percent = static_cast<double>(event) / static_cast<double>(fMaxEvents);
        const int    nDone   = static_cast<int>(percent * fBarWidth);

        std::cout << "\33[2K\r  -> [";
        for (int i = 0; i < fBarWidth; ++i) std::cout.put(i < nDone ? '=' : ' ');

        if (!finalEvt) {
            char pctBuf[16];
            std::snprintf(pctBuf, sizeof(pctBuf), "%.1f", 100.0 * percent);

            std::cout << "] processing evt " << (event + 1) << " / " << fMaxEvents
                      << " (" << pctBuf << "%), "
                      << hL << "h " << mL << "m " << sLm << "s left @ "
                      << static_cast<int64_t>(rate) << " evts/s"
                      << ", " << hE << "h " << mE << "m " << sEm << "s elapsed"
                      << ", ETA: " << etaBuf
                      << std::flush;
        }

        fLastTime  = now;
        fLastEvent = event;
        ++fCounter;

        if (finalEvt) {
            const double avgRate = (secElap > 0.0)
                                 ? static_cast<double>(fMaxEvents) / secElap
                                 : 0.0;

            std::time_t finTT = std::chrono::system_clock::to_time_t(
                                    std::chrono::system_clock::now());
            std::tm finTm{};
#if defined(_WIN32)
            localtime_s(&finTm, &finTT);
#else
            localtime_r(&finTT, &finTm);
#endif
            char finBuf[32];
            std::strftime(finBuf, sizeof(finBuf), "%Y-%m-%d %H:%M:%S", &finTm);

            std::cout << "] all done at " << finBuf << "!" << std::endl
                      << "  -> average rate was " << avgRate << " evts/s" << std::endl;
            fDone = true;
        }

        return true;
    }

    // O(1) accessor (kept for API parity with the previous version).
    double Rate() const {
        return (fNFilled > 0) ? (fRateSum / fNFilled) : 0.0;
    }

  private:
    uint64_t  fMaxEvents;
    uint64_t  fLastEvent;

    TimePoint fStartTime;
    TimePoint fLastTime;
    Seconds   fPrintInterval;

    int       fBarWidth;

    // Rate-history ring buffer with running sum / fill count for O(1) avg.
    int                 fN;
    std::vector<double> fRates;
    int                 fNextToReplace;
    double              fRateSum;
    int                 fNFilled;
    bool                fDone;

    uint64_t  fCounter;
};

#endif

