#pragma once
#ifndef __helpers_h__
#define __helpers_h__
#include <functional>
#include <memory>
#include "cJSON.h"


using unique_ptr_json = std::unique_ptr<cJSON, void(*)(cJSON*)>;
constexpr void(*json_delete)(cJSON* json) = [](cJSON* obj){ cJSON_Delete(obj); };
#define unique_json(x) unique_ptr_json(x, json_delete)

using unique_ptr_char = std::unique_ptr<char, void(*)(char*)>;
constexpr void(*char_delete)(char* var) = [](char* var) { delete var; };
#define unique_char(x) unique_ptr_char(x, char_delete)

#endif // __helpers_h__
