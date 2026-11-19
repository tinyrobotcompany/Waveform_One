#include "setup_guide.h"
#include <cassert>
using setup_guide::Guide;
using setup_guide::Network;
int main() {
    Guide first;
    assert(!first.visible() && !first.qr_ready());
    first.network(Network::Unconfigured);
    assert(first.visible() && !first.qr_ready());
    first.dismiss(); first.network(Network::Unconfigured);
    assert(!first.visible()); // No repeated takeover after dismissal.
    first.open(); first.choose_wifi();
    first.network(Network::Ready);
    assert(!first.visible()); // IP acquisition is not proof of a saved password.
    first.joined(false);
    assert(!first.visible());
    first.joined(true);
    assert(first.visible() && first.qr_ready());
    first.network(Network::Reconnecting);
    assert(first.visible() && !first.qr_ready() && first.reconnecting());
    first.network(Network::Ready);
    assert(first.visible() && first.qr_ready());
    first.dismiss(); first.network(Network::Ready);
    assert(!first.visible()); // Rotation, pairing and rejoin do not reopen guide.
    Guide saved;
    saved.network(Network::Reconnecting); saved.network(Network::Ready);
    assert(!saved.visible()); // Existing setup is not interrupted at boot.
    saved.network(Network::Unconfigured);
    assert(!saved.visible()); // A later Wi-Fi failure must not cover its error UI.
    saved.network(Network::Ready);
    saved.open(); saved.choose_wifi(); saved.network(Network::Reconnecting);
    saved.joined(true);
    assert(!saved.visible());
    saved.network(Network::Ready);
    assert(saved.visible()); // Either callback order is supported.
    saved.choose_wifi(); saved.dismiss(); saved.joined(true);
    assert(!saved.visible());
}
