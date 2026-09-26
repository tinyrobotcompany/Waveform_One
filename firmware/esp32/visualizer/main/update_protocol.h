#pragma once
#include <charconv>
#include <string>
#include <string_view>
namespace update {
struct Storage {
    virtual ~Storage() = default;
    virtual bool begin(unsigned size, const char* sha256) = 0;
    virtual bool write(const unsigned char* data, unsigned size) = 0;
    virtual bool finish() = 0;
    virtual void abort() = 0;
};
inline bool number(std::string_view s, unsigned& value) {
    auto r=std::from_chars(s.data(),s.data()+s.size(),value);
    return !s.empty() && r.ec==std::errc{} && r.ptr==s.data()+s.size();
}
inline int hex(char c) {return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
class Transfer {
    Storage& storage_; bool active_=false; unsigned size_=0, offset_=0;
public:
    explicit Transfer(Storage& storage):storage_(storage){}
    bool active() const {return active_;}
    void abort(){if(active_)storage_.abort();active_=false;}
    std::string handle(std::string_view line) {
        if(line=="WFU ABORT"){abort();return "WFU ABORTED";}
        if(line.substr(0,10)=="WFU BEGIN "){
            if(active_)return "WFU ERR BUSY";
            line.remove_prefix(10);auto space=line.find(' ');unsigned size;
            if(space==std::string_view::npos || !number(line.substr(0,space),size) || !size || size>3*1024*1024)return "WFU ERR SIZE";
            auto hash=line.substr(space+1);
            if(hash.size()!=64)return "WFU ERR HASH";
            for(char c:hash)if(hex(c)<0)return "WFU ERR HASH";
            if(!storage_.begin(size,std::string(hash).c_str()))return "WFU ERR BEGIN";
            active_=true;size_=size;offset_=0;return "WFU READY 0";
        }
        if(!active_)return "WFU ERR INACTIVE";
        if(line=="WFU END"){
            if(offset_!=size_)return "WFU ERR INCOMPLETE";
            const bool ok=storage_.finish(); active_=false;
            return ok?"WFU STAGED":"WFU ERR IMAGE";
        }
        if(line.substr(0,9)!="WFU DATA ")return "WFU ERR COMMAND";
        line.remove_prefix(9);auto space=line.find(' ');unsigned offset;
        if(space==std::string_view::npos || !number(line.substr(0,space),offset) || offset!=offset_)return "WFU ERR OFFSET";
        auto text=line.substr(space+1);
        if(text.empty() || text.size()%2 || text.size()>48 || text.size()/2>size_-offset_)return "WFU ERR DATA";
        unsigned char bytes[24];
        for(unsigned i=0;i<text.size()/2;i++){
            int high=hex(text[2*i]),low=hex(text[2*i+1]);
            if(high<0||low<0)return "WFU ERR DATA";
            bytes[i]=static_cast<unsigned char>((high<<4)|low);
        }
        if(!storage_.write(bytes,text.size()/2)){abort();return "WFU ERR WRITE";}
        offset_+=text.size()/2;return "WFU READY "+std::to_string(offset_);
    }
};
}
