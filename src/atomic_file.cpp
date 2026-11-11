#include "atomic_file.hpp"
#include <unistd.h>
namespace ps {
bool atomicWrite(const std::string& path,const std::function<bool(FILE*)>& write) {
    std::string temp=path+".tmp.XXXXXX";
    const int fd=mkstemp(temp.data()); if(fd<0) return false;
    FILE* out=fdopen(fd,"w");
    if(!out) { close(fd); std::remove(temp.c_str()); return false; }
    bool ok=write(out);
    if(std::fflush(out)!=0||fsync(fd)!=0) ok=false;
    if(std::fclose(out)!=0) ok=false;
    if(ok) ok=std::rename(temp.c_str(),path.c_str())==0;
    if(!ok) std::remove(temp.c_str());
    return ok;
}
}
