#include "IRSender.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "TimeMeasurement.h"

esp_err_t IRSender::initialize(gpio_num_t gpio_pin, uint8_t address, bool invert/* = false */, bool MSB_first /* = true */, uint8_t prescaler /* = 1 */) {
    int needed_size = snprintf(&_name[0], sizeof(_name), "IRSender Address 0x%x", address);
    if(needed_size > sizeof(_name)){
        if(_log_enabled)
            ESP_LOGE("IRSender", "Name could not be set properly - Needed name size: %d", needed_size);
        _name[0] = 0; // clear string by writing null character at the beginning
        return ESP_FAIL;
    }
    _gpio_pin = gpio_pin;
    _address = address;
    _invert = invert;
    _MSB_first = MSB_first;
    _prescaler = prescaler;

    gpio_reset_pin(_gpio_pin);
    gpio_set_direction(_gpio_pin, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(_gpio_pin);
    gpio_pulldown_en(_gpio_pin);
    gpio_set_level(_gpio_pin, 0);


    // esp_timer_create_args_t timer_args;
    // timer_args.callback = &IRSender::_timer_callback;
    // timer_args.arg = this;
    // timer_args.dispatch_method = ESP_TIMER_TASK;
    // timer_args.name = "IRSender timer";
    // timer_args.skip_unhandled_events = false;

    // ESP_ERROR_CHECK(esp_timer_init());
    // ESP_ERROR_CHECK(esp_timer_create(&timer_args, &_ir_sender_timer));
    if(_log_enabled)
        ESP_LOGI(_name, "Has been initialized");
    _initialized = true;
    return ESP_OK;
}

// void IRSender::_start_timer(){
//     ESP_ERROR_CHECK(esp_timer_start_periodic(_ir_sender_timer, _timer_period)); // us
// }

// void IRSender::_stop_timer(){
//     ESP_ERROR_CHECK(esp_timer_stop(_ir_sender_timer));
// }

// void IRSender::_timer_callback(void* arg){
//     IRSender* me = static_cast<IRSender*>(arg);
//     ESP_LOGI("IRSender", "Timer callback from %s", me->_name);
// }

bool IRSender::_checkIsInitialized() const {
    if(_initialized) return true;
    ESP_LOGE("IRSender", "Is not initialized properly");
    return false;
}

uint8_t reverse_bit_order(uint8_t data){
    uint8_t result = 0;
    for(uint8_t i=0; i<8; ++i){
        if(data & 1 << i){
            result |= 1 << (7-i);
        }
    }
    return result;
}

uint8_t bitinvert(uint8_t data){
    uint8_t result = 0;
    for(uint8_t i=0; i<8; ++i){
        if(!(data & 1 << i)){
            result |= 1 << i;
        }
    }
    return result;
}

// time in us
void ir_carrier(gpio_num_t pin, uint32_t time){
    uint16_t i = 0;
    uint16_t n_periods = time / 26;
    // if(n_periods > 0){
    //     ESP_ERROR_CHECK(gpio_set_level(pin, 1));
    //     esp_rom_delay_us(4);

    //     ESP_ERROR_CHECK(gpio_set_level(pin, 0));
    //     esp_rom_delay_us(10);
    // }
    const uint32_t wait_time = 13;
    const uint32_t first_wait_time = 6;

    gpio_set_level(pin, 1);
    esp_rom_delay_us(first_wait_time);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(wait_time);

    for(i=1; i < n_periods; ++i){
        gpio_set_level(pin, 1);
        esp_rom_delay_us(wait_time);

        gpio_set_level(pin, 0);
        esp_rom_delay_us(wait_time);
    }
}

void write_nec(gpio_num_t pin, uint8_t data, uint8_t prescaler = 1){
    uint32_t wait_time = 562 * prescaler;
    uint32_t wait_time_1 = wait_time * 3 * prescaler;
    for(uint8_t i = 0; i<8; ++i){
        bool value = data & ( 1 << i);
        ir_carrier(pin, wait_time);
        if(value){
            esp_rom_delay_us(wait_time_1);
        }
        else{
            esp_rom_delay_us(wait_time);
        }
    }
}

esp_err_t IRSender::send_data(uint8_t data, uint8_t repeat /* = 0 */) {
    if(!_checkIsInitialized()) return false;

    const uint8_t log_data = data;

    if(_log_enabled)
        ESP_LOGI(_name, "Sending data: 0x%x", log_data);

    uint8_t address = 0;
    if(_MSB_first){
        address = reverse_bit_order(_address);
    }
    else{
        address = _address;
    }
    uint8_t address_inverted = bitinvert(address);

    if(_log_enabled)
        ESP_LOGI(_name, "Converted address to 0x%x, (inv: 0x%x)", address, address_inverted);

    if(_invert)
        data = !data;

    if(_MSB_first){
        // nec sends lsb first
        data = reverse_bit_order(data);
    }
    uint8_t data_inverted = bitinvert(data);

    if(_log_enabled)
        ESP_LOGI(_name, "Converted data to 0x%x, (inv: 0x%x)", data, data_inverted);

    // portDISABLE_INTERRUPTS();

    if(_log_enabled)
        ESP_LOGI(_name, "Disabled interrupts - Start transmission");

    // start transmission
    ir_carrier(_gpio_pin, 9'000);
    esp_rom_delay_us(4'500*_prescaler);

    // send address
    write_nec(_gpio_pin, address, _prescaler);

    // send inverse of address
    write_nec(_gpio_pin, address_inverted, _prescaler);

    // send data
    write_nec(_gpio_pin, data, _prescaler);

    // send inverse of data
    write_nec(_gpio_pin, data_inverted, _prescaler);

    // end transmission
    ir_carrier(_gpio_pin, 562);

    // portENABLE_INTERRUPTS();

    if(_log_enabled)
        ESP_LOGI(_name, "End transmission - Enable interrupts");

    for(uint8_t i = 0; i<repeat; ++i){

        if(_log_enabled)
            ESP_LOGI(_name, "Repeat %d of %d", i, repeat);
        if(i == 0){
            esp_rom_delay_us(40'500*_prescaler);
        }
        else{
            esp_rom_delay_us(96'200*_prescaler);
        }
        // portDISABLE_INTERRUPTS();
        // send repeat signal
        ir_carrier(_gpio_pin, 9000);
        esp_rom_delay_us(2'250*_prescaler);
        ir_carrier(_gpio_pin, 562);
        // portENABLE_INTERRUPTS();
    }

    if(_log_enabled)
        ESP_LOGI(_name, "Done - Sent data: 0x%x", log_data);

    return true;
}

// time in us
esp_err_t IRSender::carrier(uint32_t time, uint8_t prescaler /* = 1 */){
    if(!_checkIsInitialized()) return false;
    portDISABLE_INTERRUPTS();
    ir_carrier(_gpio_pin, time);
    portENABLE_INTERRUPTS();
    return ESP_OK;
}

const char* IRSender::name() const {
    if(!_checkIsInitialized()) return nullptr;
    return _name;
}

void IRSender::enable_log(bool value) {
    _log_enabled = value;
}
