#pragma once

namespace setup_guide {
enum class Network { Starting, Unconfigured, Reconnecting, Ready };
class Guide {
public:
    void network(Network value) {
        network_ = value;
        if (!presented_ && value != Network::Starting) {
            if (value == Network::Unconfigured) open();
            else presented_ = true; // Existing setup must not become first-run later.
        }
        finish_join();
    }
    void open() { presented_ = visible_ = true; }
    void dismiss() { visible_ = awaiting_join_ = saved_ = false; }
    // Closing Settings is not the guide's explicit "Done / later" action.
    void settings_closed() { visible_ = false; }
    void choose_wifi() { visible_ = false; awaiting_join_ = true; saved_ = false; }
    void joined(bool saved) { saved_ = saved; finish_join(); }
    bool visible() const { return visible_; }
    bool qr_ready() const { return network_ == Network::Ready; }
    bool reconnecting() const { return network_ == Network::Reconnecting; }
private:
    void finish_join() {
        if (awaiting_join_ && saved_ && qr_ready()) {
            open(); awaiting_join_ = saved_ = false;
        }
    }
    Network network_ = Network::Starting;
    bool presented_ = false, visible_ = false, awaiting_join_ = false, saved_ = false;
};
}
