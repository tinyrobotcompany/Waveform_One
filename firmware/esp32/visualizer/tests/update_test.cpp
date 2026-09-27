#include <cassert>
#include <string>
#include "update_protocol.h"
struct Storage : update::Storage {
    bool aborted=false, valid=true; unsigned bytes=0;
    bool begin(unsigned, const char*) override {bytes=0;return true;}
    bool write(const unsigned char*, unsigned n) override {bytes+=n;return true;}
    bool finish() override {return valid;}
    void abort() override {aborted=true;}
};
int main(){
    Storage storage; update::Transfer transfer(storage);
    std::string begin="WFU BEGIN 4 "+std::string(64,'a');
    assert(transfer.handle(begin)=="WFU READY 0");
    assert(transfer.handle(begin)=="WFU ERR BUSY");
    assert(transfer.handle("WFU DATA 1 0102")=="WFU ERR OFFSET");
    assert(transfer.handle("WFU DATA 0 xx")=="WFU ERR DATA");
    assert(transfer.handle("WFU DATA 0 0102")=="WFU READY 2");
    assert(transfer.handle("WFU END")=="WFU ERR INCOMPLETE");
    assert(transfer.handle("WFU DATA 2 030405")=="WFU ERR DATA");
    assert(transfer.handle("WFU DATA 2 0304")=="WFU READY 4");
    storage.valid=false;
    assert(transfer.handle("WFU END")=="WFU ERR IMAGE");
    assert(!transfer.active());
    assert(transfer.handle(begin)=="WFU READY 0");
    assert(transfer.handle("WFU ABORT")=="WFU ABORTED");
    assert(storage.aborted);
    storage.valid=true;
    assert(transfer.handle(begin)=="WFU READY 0");
    assert(transfer.handle("WFU DATA 0 01020304")=="WFU READY 4");
    assert(transfer.handle("WFU END")=="WFU STAGED");
    assert(transfer.handle("WFU DATA 4 00")=="WFU ERR INACTIVE");
}
