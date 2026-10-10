// Exercise the production fetch routine through fake ESP-IDF network I/O.
#include "recognition_http.h"
#include <cassert>

int main()
{
    using namespace recognition_http;
    auto fetch_body = [&](int64_t declared, std::string body, bool expected) {
        test_http = {}; test_http.declared = declared; test_http.body = body;
        Response response;
        assert(fetch("https://example.test/body", nullptr, 8, response) == expected);
        assert(test_http.closed == 1 && test_http.cleaned == 1);
        if (expected) {
            assert(response.size == body.size());
            assert(std::string(response.bytes.get(), response.size) == body);
            assert(response.bytes.get()[response.size] == '\0');
        }
    };
    fetch_body(8, "12345678", true);
    // IDF returns zero for chunked or absent Content-Length, not a negative.
    fetch_body(0, "12345678", true);
    fetch_body(0, "123456789", false);
    assert(test_http.offset == 9); // One excess byte proves the bounded rejection.
    fetch_body(9, "123456789", false); assert(test_http.reads == 0);
    fetch_body(-1, "valid", false); assert(test_http.reads == 0);
    fetch_body(-ESP_ERR_HTTP_EAGAIN, "valid", false); assert(test_http.reads == 0);
    test_http = {}; test_http.body = "truncated"; test_http.complete = false;
    { Response response; assert(!fetch("https://example.test/", nullptr, 32, response)); }
    test_http = {}; test_http.body = "valid"; test_http.eagain = 1;
    { Response response; assert(fetch("https://example.test/", "{}", 8, response)); }
    test_http = {}; test_http.read_error = true;
    { Response response; assert(!fetch("https://example.test/", nullptr, 8, response)); }
    test_http = {}; test_http.eagain = 10; test_http.read_duration_us = 10000000;
    { Response response; assert(!fetch("https://example.test/", nullptr, 8, response)); }
    assert(test_http.reads == 3);
    test_http = {}; test_http.status = 429; test_http.retry_after = "180";
    { Response response; assert(!fetch("https://example.test/", nullptr, 8, response));
      assert(response.retry_after_us == 180000000 && test_http.reads == 0); }
    assert(test_allocations.empty());
}
