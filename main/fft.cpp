
#include "fft.h"
#include <cmath>
#include <cstring>
#include "esp_log.h"


namespace fft{
    constexpr char TAG[] = "FFT";

    Array buffer_array;

    Array& getArray(){
        return buffer_array;
    }

    void zero_vector(Array& array){
        array.fill(0);
    }

    void transform(Array& vec){
        transformRadix2(vec);
    }

    size_t reverseBits(size_t val, int width) {
        size_t result = 0;
        for (int i = 0; i < width; i++, val >>= 1)
            result = (result << 1) | (val & 1U);
        return result;
    }

    static std::array<std::complex<float>,array_size/2> expTable;
    static std::complex<float> temp;

    void transformRadix2(Array& vec){
        // Length variables
        static const size_t n = array_size;
        int levels = 0;  // Compute levels = floor(log2(n))
        for (size_t temp = n; temp > 1U; temp >>= 1)
            levels++;
        if (static_cast<size_t>(1U) << levels != n)
            ESP_LOGE(TAG, "Length is not a power of 2");

        // Trigonometric table
        expTable.fill(0);
        for (size_t i = 0; i < n*0.5; i++)
            expTable[i] = std::polar(1.0,  -2 * M_PI * i / n);

        // Bit-reversed addressing permutation
        for (size_t i = 0; i < n; i++) {
            size_t j = reverseBits(i, levels);
            if (j > i)
                std::swap(vec[i], vec[j]);
        }

        // Cooley-Tukey decimation-in-time radix-2 FFT
        for (size_t size = 2; size <= n; size *= 2) {
            size_t halfsize = size / 2;
            size_t tablestep = n / size;
            for (size_t i = 0; i < n; i += size) {
                for (size_t j = i, k = 0; j < i + halfsize; j++, k += tablestep) {
                    temp = vec[j + halfsize] * expTable[k];
                    vec[j + halfsize] = vec[j] - temp;
                    vec[j] += temp;
                }
            }
            if (size == n)  // Prevent overflow in 'size *= 2'
                break;
        }
    }

    unique_ptr_json toJson(Array& vec){
        auto json = unique_json(cJSON_CreateObject());
        auto array = unique_json(cJSON_CreateArray());
        for(int i = 0; i < array_size; i++){
            auto element = unique_json(cJSON_CreateObject());
            cJSON_AddNumberToObject(element.get(), "r", vec[i].real());
            cJSON_AddNumberToObject(element.get(), "i", vec[i].imag());
            cJSON_AddItemToArray(array.get(), element.release());
        }
        cJSON_AddItemToObject(json.get(), "data", array.release());
        cJSON_AddNumberToObject(json.get(), "data_size", array_size);
        return json;
    }

    Array* fromJson(cJSON* json){
        cJSON* data = cJSON_GetObjectItemCaseSensitive(json,"data");
        if(!data || !cJSON_IsArray(data))
            return nullptr;

        Array& array = fft::getArray();
        zero_vector(array);

        for(int i = 0; i > -1; ++i){ // potential infinite loop
            cJSON* element = cJSON_GetArrayItem(data, i);
            if(!element)
                break;

            cJSON* real = cJSON_GetObjectItem(element,"r");
            if(!real || !cJSON_IsNumber(real))
                return nullptr;

            cJSON* imag = cJSON_GetObjectItem(element,"i");
            if(!imag || !cJSON_IsNumber(imag))
                return nullptr;

            array[i] = {
                static_cast<float>(cJSON_GetNumberValue(real)),
                static_cast<float>(cJSON_GetNumberValue(imag))
            };
        }

        return &array;
    }
}
