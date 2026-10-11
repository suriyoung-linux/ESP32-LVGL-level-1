#pragma once

#include <cmath>
#include <limits>

namespace ldr {

// วงจร: Vs -- R15 -- ADC -- (R19 ขนานกับ LDR) -- GND
// กระแสผ่าน LDR = (Vs - V) / R15 - V / R19 และ R_LDR = V / I_LDR
// คืน NaN เมื่อแรงดันอยู่นอกช่วงที่ใช้ประมาณค่าได้ แทนการหารด้วยศูนย์
inline float estimateResistanceOhms(float voltage, float supplyVoltage,
                                    float pullupOhms, float pulldownOhms,
                                    float minimumVoltage)
{
    const float unavailable = std::numeric_limits<float>::quiet_NaN();
    if (!std::isfinite(voltage) || !std::isfinite(supplyVoltage)
        || !std::isfinite(pullupOhms) || !std::isfinite(pulldownOhms)
        || !std::isfinite(minimumVoltage) || minimumVoltage < 0.0f
        || voltage <= 0.0f || voltage < minimumVoltage || voltage >= supplyVoltage
        || pullupOhms <= 0.0f || pulldownOhms <= 0.0f) {
        return unavailable;
    }

    const float ldrCurrent = (supplyVoltage - voltage) / pullupOhms - voltage / pulldownOhms;
    if (ldrCurrent <= 0.0f) {
        return unavailable;
    }

    const float resistance = voltage / ldrCurrent;
    return std::isfinite(resistance) ? resistance : unavailable;
}

}  // namespace ldr
