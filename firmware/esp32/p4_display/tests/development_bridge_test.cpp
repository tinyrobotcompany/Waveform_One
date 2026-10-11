#include "development_bridge_policy.h"
#include <cassert>
#include <string>
int main() {
    const std::string token(64, 'a');
    assert(development_bridge::authorize(token, token, 100, 100));
    assert(development_bridge::authorize(token, token, 100, 600000099));
    assert(!development_bridge::authorize(token, token, 100, 600000100));
    assert(!development_bridge::authorize(token, token, 100, 99));
    assert(!development_bridge::authorize(token, token.substr(1), 100, 100));
    assert(!development_bridge::authorize(token, token + "\n", 100, 100));
    for (unsigned i = 0; i < 64; ++i) { auto wrong = token; wrong[i] = 'b'; assert(!development_bridge::authorize(token, wrong, 100, 100)); }
}
