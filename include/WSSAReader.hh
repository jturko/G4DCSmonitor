#ifndef WSSAReader_hh
#define WSSAReader_hh 1

// Minimal, dependency-free reader for MCNP surface-source (.wssa) files.
//
// The record layout follows the read statements in
//   T. J. Kulesza, "MCNP Surface Source Write/Read File Format Primer",
//   LA-UR-24-29475 (2024), Appendix A.
//
// A Fortran "unformatted" record is stored as
//     [4-byte length][payload][4-byte length]
// with little-endian byte order.  This class streams the header, then
// exposes the particle records one at a time.  It deliberately contains no
// Geant4 or ROOT types so the same parser can be used by the simulation and
// by offline ROOT macros.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace wssa {

// ---------------------------------------------------------------------------
// Little-endian decoding helpers.  The machine is assumed to be IEEE-754.
// ---------------------------------------------------------------------------
inline uint32_t u32le(const unsigned char* p)
{
    return  static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) <<  8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

inline int32_t i32le(const unsigned char* p)
{
    return static_cast<int32_t>(u32le(p));
}

inline uint64_t u64le(const unsigned char* p)
{
    return  static_cast<uint64_t>(p[0])
         | (static_cast<uint64_t>(p[1]) <<  8)
         | (static_cast<uint64_t>(p[2]) << 16)
         | (static_cast<uint64_t>(p[3]) << 24)
         | (static_cast<uint64_t>(p[4]) << 32)
         | (static_cast<uint64_t>(p[5]) << 40)
         | (static_cast<uint64_t>(p[6]) << 48)
         | (static_cast<uint64_t>(p[7]) << 56);
}

inline int64_t i64le(const unsigned char* p)
{
    return static_cast<int64_t>(u64le(p));
}

inline double f64le(const unsigned char* p)
{
    const uint64_t u = u64le(p);
    double d;
    const uint16_t one = 1;
    const bool hostLittleEndian =
        (*reinterpret_cast<const unsigned char*>(&one) == 1);
    if (hostLittleEndian) {
        std::memcpy(&d, &u, sizeof(d));
    } else {
        const uint64_t v = __builtin_bswap64(u);
        std::memcpy(&d, &v, sizeof(d));
    }
    return d;
}

// ---------------------------------------------------------------------------
// Header metadata
// ---------------------------------------------------------------------------
struct Surface {
    int    jss   = 0;   // surface identifier
    int    jsq   = 0;   // extra integer in the kq != 0 layout
    int    type  = 0;   // MCNP surface-type code
    int    n     = 0;   // number of coefficients
    double coeff = 0.0; // the (single) coefficient
};

struct Header {
    std::string codeName;
    std::string version;
    std::string buildDate;
    std::string idtm;
    std::string probid;
    std::string aid;
    int         knod = 0;

    int64_t np   = 0;
    int64_t nrsw = 0;   // number of particle records
    int32_t nd   = 0;
    int32_t njss = 0;   // number of surface records
    int64_t nqsw = 0;

    int32_t nilw = 0;
    int32_t mipt = 0;
    int32_t kq   = 0;

    std::vector<Surface> surfaces;

    // Byte offset of the first particle record and its total size on disk
    // (4-byte prefix + payload + 4-byte trailer).  Filled by readHeader().
    int64_t particleStart       = 0;
    int64_t particleRecordBytes = 0;
};

// ---------------------------------------------------------------------------
// One particle record
// ---------------------------------------------------------------------------
struct Record {
    int64_t nps  = 0;   // history / source-point number (absolute value)
    double  b    = 0.0; // signed flag; only its sign is used (see w())
    double  wgt  = 0.0; // statistical weight
    double  erg  = 0.0; // energy [MeV]
    double  tme  = 0.0; // time
    double  x    = 0.0; // position [cm]
    double  y    = 0.0;
    double  z    = 0.0;
    double  u    = 0.0; // direction cosine, x
    double  v    = 0.0; // direction cosine, y
    double  c    = 0.0; // surface label (integer part is the surface id)
    double  cs   = 0.0; // used only by the 7-double symmetric record
    bool    symmetric = false;
    bool    valid     = false;

    // Third direction cosine.  The file stores only u and v; the sign of the
    // missing component is carried by b.
    double w() const
    {
        const double s = 1.0 - u*u - v*v;
        return std::copysign(std::sqrt(std::max(0.0, s)), b);
    }

    int surfaceId() const
    {
        return static_cast<int>(std::floor(c));
    }

    bool sameAs(const Record& o) const
    {
        return nps == o.nps && b == o.b && wgt == o.wgt && erg == o.erg &&
               tme == o.tme && x == o.x && y == o.y && z == o.z &&
               u == o.u && v == o.v && c == o.c && cs == o.cs &&
               symmetric == o.symmetric && valid == o.valid;
    }
};

// ---------------------------------------------------------------------------
// Streaming reader
// ---------------------------------------------------------------------------
class Reader {
  public:
    Reader() = default;
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    bool open(const std::string& path, std::string& err)
    {
        f_.open(path, std::ios::binary);
        if (!f_) {
            err = "cannot open '" + path + "'";
            return false;
        }
        buffer_.resize(kBufferSize);
        pos_ = len_ = 0;
        consumed_ = 0;
        hasPending_ = false;
        pending_.clear();
        return true;
    }

    void close() { f_.close(); }

    bool readHeader(std::string& err)
    {
        std::vector<unsigned char> p;

        // Record 0: 8-character format identifier.
        if (!readRecord(p, err)) { err = "cannot read record 0: " + err; return false; }
        if (p.size() != 8) {
            err = "record 0 has unexpected payload size";
            return false;
        }
        identifier_.assign(reinterpret_cast<const char*>(p.data()), p.size());

        // Record 1: code name, version, build date, two date strings, aid, knod.
        if (!readRecord(p, err)) { err = "cannot read record 1: " + err; return false; }
        if (p.size() < 191) {
            err = "record 1 has unexpected payload size";
            return false;
        }
        h_.codeName  = trim(p.data() +   0,  8);
        h_.version   = trim(p.data() +   8,  5);
        h_.buildDate = trim(p.data() +  13,  8);
        h_.idtm      = trim(p.data() +  21, 19);
        h_.probid    = trim(p.data() +  40, 19);
        h_.aid       = trim(p.data() +  59, 128);
        h_.knod      = i32le(p.data() + 187);

        // Record 2: np, nrsw, nd, njss, nqsw.
        if (!readRecord(p, err)) { err = "cannot read record 2: " + err; return false; }
        if (p.size() < 32) {
            err = "record 2 has unexpected payload size";
            return false;
        }
        h_.np   = i64le(p.data() +  0);
        h_.nrsw = i64le(p.data() +  8);
        h_.nd   = i32le(p.data() + 16);
        h_.njss = i32le(p.data() + 20);
        h_.nqsw = i64le(p.data() + 24);
        if (h_.nrsw < 0 || h_.njss < 0) {
            err = "negative record count in header";
            return false;
        }

        // Record 3: nilw, mipt, kq, then padding integers.
        if (!readRecord(p, err)) { err = "cannot read record 3: " + err; return false; }
        if (p.size() < 12) {
            err = "record 3 has unexpected payload size";
            return false;
        }
        h_.nilw = i32le(p.data() + 0);
        h_.mipt = i32le(p.data() + 4);
        h_.kq   = i32le(p.data() + 8);
        if (h_.nilw < 0) {
            err = "negative nilw in header";
            return false;
        }

        // Surface-description records.  kq selects whether an extra jsq
        // integer is present in each record.
        h_.surfaces.clear();
        h_.surfaces.reserve(static_cast<size_t>(h_.njss));
        for (int32_t i = 0; i < h_.njss; ++i) {
            if (!readRecord(p, err)) {
                err = "cannot read surface record: " + err;
                return false;
            }
            Surface s;
            if (h_.kq == 0) {
                if (p.size() < 20) { err = "short surface record"; return false; }
                s.jss   = i32le(p.data() +  0);
                s.type  = i32le(p.data() +  4);
                s.n     = i32le(p.data() +  8);
                s.coeff = f64le(p.data() + 12);
            } else {
                if (p.size() < 24) { err = "short surface record"; return false; }
                s.jss   = i32le(p.data() +  0);
                s.jsq   = i32le(p.data() +  4);
                s.type  = i32le(p.data() +  8);
                s.n     = i32le(p.data() + 12);
                s.coeff = f64le(p.data() + 16);
            }
            h_.surfaces.push_back(s);
        }

        // Selected-surface records; none in the files seen so far.
        for (int32_t i = 0; i < h_.nilw; ++i) {
            if (!readRecord(p, err)) {
                err = "cannot read selected-surface record: " + err;
                return false;
            }
        }

        // Summary record: skipped by its recorded length (it may include
        // 4-byte padding beyond the nominal field size).
        if (!readRecord(p, err)) {
            err = "cannot read summary record: " + err;
            return false;
        }

        // The next record is the first particle record.  Read it now so that
        // particleStart and particleRecordBytes can be reported, then keep it
        // pending for the first next() call.
        h_.particleStart = consumed_;
        hasPending_ = false;
        pending_.clear();
        if (h_.nrsw > 0) {
            if (!readRecord(pending_, err)) {
                err = "cannot read first particle record: " + err;
                return false;
            }
            h_.particleRecordBytes =
                static_cast<int64_t>(pending_.size()) + 8;  // + two markers
            hasPending_ = true;
        } else {
            h_.particleRecordBytes = 0;
        }

        return true;
    }

    // Read the next particle record in file order.
    bool next(Record& rec, std::string& err)
    {
        const std::vector<unsigned char>* p = nullptr;
        if (hasPending_) {
            p = &pending_;
            hasPending_ = false;
        } else {
            if (!readRecord(scratch_, err)) return false;
            p = &scratch_;
        }
        return parseParticle(*p, rec, err);
    }

    // Random access by record index.  Valid only when all particle records
    // have the same size (true for the files in use, all 11-double records).
    bool readAt(int64_t index, Record& rec, std::string& err)
    {
        if (index < 0 || index >= h_.nrsw) {
            err = "record index out of range";
            return false;
        }
        if (h_.particleRecordBytes <= 0) {
            err = "particle record size unavailable";
            return false;
        }
        const int64_t off =
            h_.particleStart + index * h_.particleRecordBytes;
        if (!seek(off)) {
            err = "seek failed";
            return false;
        }
        return next(rec, err);
    }

    const Header& header() const { return h_; }
    const std::string& identifier() const { return identifier_; }

  private:
    static constexpr std::size_t kBufferSize = 1u << 20;  // 1 MiB

    std::ifstream               f_;
    std::vector<unsigned char>  buffer_;
    std::size_t                 pos_ = 0;
    std::size_t                 len_ = 0;
    int64_t                     consumed_ = 0;

    Header                      h_;
    std::string                 identifier_;
    std::vector<unsigned char>  pending_;
    std::vector<unsigned char>  scratch_;
    bool                        hasPending_ = false;

    static std::string trim(const unsigned char* p, std::size_t n)
    {
        std::string s(reinterpret_cast<const char*>(p), n);
        std::size_t b = 0;
        while (b < s.size() &&
               (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' ||
                s[b] == '\n' || s[b] == '\0'))
            ++b;
        std::size_t e = s.size();
        while (e > b &&
               (s[e-1] == ' ' || s[e-1] == '\t' || s[e-1] == '\r' ||
                s[e-1] == '\n' || s[e-1] == '\0'))
            --e;
        return s.substr(b, e - b);
    }

    bool refill()
    {
        if (!f_) return false;
        f_.read(reinterpret_cast<char*>(buffer_.data()),
                static_cast<std::streamsize>(buffer_.size()));
        const std::streamsize got = f_.gcount();
        pos_ = 0;
        len_ = static_cast<std::size_t>(got > 0 ? got : 0);
        return len_ > 0;
    }

    bool readRaw(void* dst, std::size_t n)
    {
        unsigned char* out = static_cast<unsigned char*>(dst);
        while (n > 0) {
            if (pos_ == len_) {
                if (!refill()) return false;
            }
            const std::size_t take = std::min(n, len_ - pos_);
            std::memcpy(out, buffer_.data() + pos_, take);
            pos_ += take;
            out  += take;
            n    -= take;
            consumed_ += static_cast<int64_t>(take);
        }
        return true;
    }

    bool skipRaw(std::size_t n)
    {
        while (n > 0) {
            if (pos_ == len_) {
                if (!refill()) return false;
            }
            const std::size_t take = std::min(n, len_ - pos_);
            pos_ += take;
            n    -= take;
            consumed_ += static_cast<int64_t>(take);
        }
        return true;
    }

    bool readRecord(std::vector<unsigned char>& p, std::string& err)
    {
        unsigned char marker[4];
        if (!readRaw(marker, 4)) {
            err = "unexpected end of file";
            return false;
        }
        const int32_t n = i32le(marker);
        if (n < 0 || n > (1 << 28)) {
            err = "invalid record length";
            return false;
        }
        p.resize(static_cast<std::size_t>(n));
        if (n > 0 && !readRaw(p.data(), static_cast<std::size_t>(n))) {
            err = "truncated payload";
            return false;
        }
        unsigned char trailer[4];
        if (!readRaw(trailer, 4)) {
            err = "truncated trailer";
            return false;
        }
        if (i32le(trailer) != n) {
            err = "record marker mismatch";
            return false;
        }
        return true;
    }

    bool seek(int64_t off)
    {
        f_.clear();
        f_.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        if (!f_) return false;
        pos_ = len_ = 0;
        consumed_ = off;
        hasPending_ = false;
        pending_.clear();
        return true;
    }

    static bool parseParticle(const std::vector<unsigned char>& p,
                              Record& rec, std::string& err)
    {
        rec = Record();
        const std::size_t n = p.size();
        if (n == 11 * 8) {
            const unsigned char* q = p.data();
            const int64_t nps = static_cast<int64_t>(f64le(q + 0 * 8));
            rec.nps  = nps < 0 ? -nps : nps;
            rec.b    = f64le(q +  1 * 8);
            rec.wgt  = f64le(q +  2 * 8);
            rec.erg  = f64le(q +  3 * 8);
            rec.tme  = f64le(q +  4 * 8);
            rec.x    = f64le(q +  5 * 8);
            rec.y    = f64le(q +  6 * 8);
            rec.z    = f64le(q +  7 * 8);
            rec.u    = f64le(q +  8 * 8);
            rec.v    = f64le(q +  9 * 8);
            rec.c    = f64le(q + 10 * 8);
            rec.symmetric = false;
            rec.valid = true;
            return true;
        }
        if (n == 7 * 8) {
            const unsigned char* q = p.data();
            const int64_t nps = static_cast<int64_t>(f64le(q + 0 * 8));
            rec.nps  = nps < 0 ? -nps : nps;
            rec.b    = f64le(q + 1 * 8);
            rec.wgt  = f64le(q + 2 * 8);
            rec.erg  = f64le(q + 3 * 8);
            rec.tme  = f64le(q + 4 * 8);
            rec.cs   = f64le(q + 5 * 8);
            rec.c    = f64le(q + 6 * 8);
            rec.symmetric = true;
            rec.valid = true;
            return true;
        }
        err = "unknown particle-record length";
        return false;
    }
};

} // namespace wssa

#endif // WSSAReader_hh
