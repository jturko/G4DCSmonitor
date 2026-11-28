// compare_g4_wssa_rate.C -- sum p1 surface weights per PID and write a CSV.
//
// Manifest line: source|baseFuel|nEvts|lambda|path
//   source    free label, e.g. gamma_662keV
//   baseFuel  p1 base-fuel index (0..83)
//   nEvts     number of generated p1 primaries, from the file name
//   lambda    estimated source rate per assembly [1/s]
//   path      p1 ROOT file with a 'surfaceFlux' tree
//
// Output columns:
//   source,base,nEvts,lambda,path,entries,sumW_gamma,sumW_neutron,
//   r_gamma,r_neutron
//
// Usage:
//   root -l -b -q 'scripts/wssa/compare_g4_wssa_rate.C("manifest.txt","out.csv")'

#include <ROOT/RDataFrame.hxx>
#include <TSystem.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(s);
    while (std::getline(ss, item, sep)) out.push_back(item);
    return out;
}

} // namespace

void compare_g4_wssa_rate(const char* manifest, const char* outcsv)
{
    ROOT::EnableImplicitMT(2);

    std::ifstream in(manifest);
    if (!in) {
        std::cerr << "compare_g4_wssa_rate: cannot open manifest " << manifest << std::endl;
        gSystem->Exit(1);
    }
    std::ofstream out(outcsv);
    if (!out) {
        std::cerr << "compare_g4_wssa_rate: cannot open output " << outcsv << std::endl;
        gSystem->Exit(1);
    }

    out << "source,base,nEvts,lambda,path,entries,sumW_gamma,sumW_neutron,"
           "r_gamma,r_neutron\n";

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto f = split(line, '|');
        if (f.size() != 5) {
            std::cerr << "compare_g4_wssa_rate: bad manifest line: " << line << std::endl;
            continue;
        }
        const std::string& source = f[0];
        const std::string& base   = f[1];
        const long long nEvts     = std::stoll(f[2]);
        const double lambda       = std::stod(f[3]);
        const std::string& path   = f[4];

        std::cout << "[" << source << " base " << base << "] " << path << std::endl;

        try {
            ROOT::RDataFrame df("surfaceFlux", path);
            auto n  = df.Count();
            auto wg = df.Filter("pid==22").Sum<double>("weight");
            auto wn = df.Filter("pid==2112").Sum<double>("weight");

            const double sumWg = *wg;
            const double sumWn = *wn;
            const double rg = (nEvts > 0) ? sumWg / static_cast<double>(nEvts) : 0.0;
            const double rn = (nEvts > 0) ? sumWn / static_cast<double>(nEvts) : 0.0;

            out << source << ',' << base << ',' << nEvts << ',' << lambda << ','
                << path << ',' << *n << ',' << sumWg << ',' << sumWn << ','
                << rg << ',' << rn << '\n';
            out.flush();

            std::cout << "  entries=" << *n << " sumW_gamma=" << sumWg
                      << " sumW_neutron=" << sumWn << " r_gamma=" << rg
                      << " r_neutron=" << rn << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "  ERROR: " << e.what() << std::endl;
            out << source << ',' << base << ',' << nEvts << ',' << lambda << ','
                << path << ",ERROR,ERROR,ERROR,ERROR,ERROR\n";
            out.flush();
        }
    }
}
