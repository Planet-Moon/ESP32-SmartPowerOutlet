#pragma once

namespace ntc{
constexpr float ntcTemperature(const float& R, const float& B, const float& T0, const float& R0);

float readTemperature();

int readAdc();
}
