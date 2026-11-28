// ICRP74H10.hh
//
// Fluence-to-ambient-dose-equivalent coefficients H*(10) from ICRP
// Publication 74 / ICRU Report 57, in pSv cm^2 per incident particle.
//
// The tabulated values are the ICRP 74 H*(10) data distributed with OpenMC
// (openmc/data/dose/icrp74/{photons,neutrons}_H10.txt), which reproduces the
// published tables and uses the corrigendum values.  The source data are kept
// here as plain arrays so the ROOT reader has no Python/OpenMC dependency.
//
// Interpolation is log-log in energy, which is the standard way to interpolate
// positive fluence-to-dose functions.  Below/above the tabulated range the
// nearest endpoint is used; the WSSA energy range is inside the tables.

#ifndef ICRP74_H10_HH
#define ICRP74_H10_HH 1

#include <cmath>
#include <cstddef>

namespace icrp74h10 {

enum Particle { kPhoton = 0, kNeutron = 1 };

inline const double* energies(Particle p)
{
    static const double photonE[] = {
        0.01, 0.015, 0.02, 0.03, 0.04, 0.05, 0.06, 0.08, 0.1, 0.15,
        0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0,
        4.0, 5.0, 6.0, 8.0, 10.0
    };
    static const double neutronE[] = {
        1e-9, 1e-8, 2.53e-8, 1e-7, 2e-7, 5e-7, 1e-6, 2e-6, 5e-6, 1e-5,
        2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3, 1e-2, 2e-2,
        3e-2, 5e-2, 7e-2, 1e-1, 1.5e-1, 2e-1, 3e-1, 5e-1, 7e-1, 9e-1,
        1.0, 1.2, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,
        10.0, 12.0, 14.0, 15.0, 16.0, 18.0, 20.0
    };
    return (p == kPhoton) ? photonE : neutronE;
}

inline const double* coefficients(Particle p)
{
    static const double photonH[] = {
        0.061, 0.83, 1.05, 0.81, 0.64, 0.55, 0.51, 0.53, 0.61, 0.89,
        1.20, 1.80, 2.38, 2.93, 3.44, 4.38, 5.20, 6.90, 8.60, 11.1,
        13.4, 15.5, 17.6, 21.6, 25.6
    };
    static const double neutronH[] = {
        6.6, 9.0, 10.6, 12.9, 13.5, 13.6, 13.3, 12.9, 12.0, 11.3,
        10.6, 9.9, 9.4, 8.9, 8.3, 7.9, 7.7, 8.0, 10.5, 16.6,
        23.7, 41.1, 60.0, 88.0, 132.0, 170.0, 233.0, 322.0, 375.0, 400.0,
        416.0, 425.0, 420.0, 412.0, 408.0, 405.0, 400.0, 405.0, 409.0, 420.0,
        440.0, 480.0, 520.0, 540.0, 555.0, 570.0, 600.0
    };
    return (p == kPhoton) ? photonH : neutronH;
}

inline std::size_t size(Particle p)
{
    return (p == kPhoton) ? 25u : 47u;
}

inline double h10(double energyMeV, Particle p)
{
    const double* e = energies(p);
    const double* h = coefficients(p);
    const std::size_t n = size(p);
    if (!(energyMeV > 0.0)) return 0.0;
    if (energyMeV <= e[0]) return h[0];
    if (energyMeV >= e[n - 1]) return h[n - 1];

    std::size_t i = 1;
    while (i < n && energyMeV > e[i]) ++i;
    const double e0 = e[i - 1];
    const double e1 = e[i];
    const double h0 = h[i - 1];
    const double h1 = h[i];
    if (h0 <= 0.0 || h1 <= 0.0) return 0.0;
    const double t = (std::log(energyMeV) - std::log(e0)) /
                     (std::log(e1) - std::log(e0));
    return std::exp(std::log(h0) + t * (std::log(h1) - std::log(h0)));
}

} // namespace icrp74h10

#endif
