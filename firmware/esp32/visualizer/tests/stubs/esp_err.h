#pragma once
#include <cassert>
constexpr int ESP_OK=0, ESP_ERR_NO_MEM=-1;
#define ESP_ERROR_CHECK(value) assert((value)==ESP_OK)
