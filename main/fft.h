#pragma once
#include <array>
#include <complex>
#include <helpers.h>


namespace fft{
    constexpr const unsigned int array_size = 1024;
    using Array = std::array<std::complex<float>,array_size>;

    Array& getArray();
    void zero_vector(Array& vec);
    void transform(Array& vec);
    void transformRadix2(Array& vec);

    unique_ptr_json toJson(Array& vec);
    Array* fromJson(cJSON* json);
}
