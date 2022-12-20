#pragma once
#ifndef __json_helpers_h__
#define __json_helpers_h__
#include <functional>
#include <memory>
#include "cJSON.h"


using unique_ptr_json = std::unique_ptr<cJSON, void(*)(cJSON*)>;
constexpr void(*json_delete)(cJSON*) = [](cJSON* json){cJSON_Delete(json);};

#endif // __json_helpers_h__
