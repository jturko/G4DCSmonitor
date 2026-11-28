#include "../../include/WSSAReader.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

void validate_wssa_directions(const char* path, long long limit = 1000000)
{
    wssa::Reader reader;
    std::string err;
    if (!reader.open(path, err) || !reader.readHeader(err)) {
        std::cerr << "ERROR: " << err << "\n";
        return;
    }

    long long nSide = 0, nTop = 0, nOther = 0;
    long long sideOut = 0, sideIn = 0, sideTan = 0;
    long long topUp = 0, topDown = 0;
    long long badNorm = 0;
    long long missing = 0;

    std::map<long long, int> npsCount;
    wssa::Record r;
    for (long long i = 0; i < limit; ++i) {
        if (!reader.next(r, err)) {
            missing = limit - i;
            break;
        }
        const double rho = std::hypot(r.x, r.y) * 10.0;
        const double zmm = r.z * 10.0;
        const double w = r.w();
        const double norm = r.u*r.u + r.v*r.v + w*w;
        if (std::fabs(norm - 1.0) > 1.0e-5) ++badNorm;
        ++npsCount[std::llabs(r.nps)];

        if (zmm > 3900.0) {
            ++nTop;
            if (w > 0.0) ++topUp;
            else ++topDown;
        } else if (rho > 1200.0) {
            ++nSide;
            const double radial = r.u * r.x + r.v * r.y;
            if (radial > 1.0e-9) ++sideOut;
            else if (radial < -1.0e-9) ++sideIn;
            else ++sideTan;
        } else {
            ++nOther;
        }
    }

    int repeated = 0;
    for (const auto& kv : npsCount) if (kv.second > 1) ++repeated;

    std::cout << "file             : " << path << "\n"
              << "header nrsw      : " << reader.header().nrsw << "\n"
              << "processed        : " << (limit - missing) << "\n"
              << "side             : " << nSide << "\n"
              << "  radial outward : " << sideOut << "\n"
              << "  radial inward  : " << sideIn << "\n"
              << "  tangential     : " << sideTan << "\n"
              << "top              : " << nTop << "\n"
              << "  +z outward     : " << topUp << "\n"
              << "  -z inward      : " << topDown << "\n"
              << "other            : " << nOther << "\n"
              << "bad |u|^2        : " << badNorm << "\n"
              << "repeated nps     : " << repeated << "\n";
}
