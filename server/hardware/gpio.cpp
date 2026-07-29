//==============================================================================
// gpio.cpp
// Lectura de pines GPIO: mmap (RPi) o simulacion (x86)
// Licencia: MIT
//==============================================================================

#include "gpio.h"
#include <cstring>
#include <iostream>

#if defined(__arm__) || defined(__aarch64__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#define GPIO_MMAP_AVAILABLE 1
#endif

GPIOReader::GPIOReader() {}
GPIOReader::~GPIOReader() { close(); }

bool GPIOReader::init() {
#if GPIO_MMAP_AVAILABLE
    fd_ = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (fd_ < 0) {
        std::cerr << "[GPIO] Failed to open /dev/gpiomem: " << strerror(errno) << std::endl;
        std::cerr << "[GPIO] Falling back to simulation mode" << std::endl;
        return init_simulation();
    }
    gpio_map_ = (volatile uint32_t*)mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE,
                                         MAP_SHARED, fd_, 0);
    if (gpio_map_ == MAP_FAILED) {
        std::cerr << "[GPIO] mmap failed: " << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return init_simulation();
    }
    std::cout << "[GPIO] mmap OK at " << (void*)gpio_map_ << std::endl;
    for (int p = 0; p < 28; p++) {
        int reg = p / 10, bit = (p % 10) * 3;
        gpio_map_[reg] &= ~(7 << bit);  // todas como input
    }
    return true;
#else
    return init_simulation();
#endif
}

uint32_t GPIOReader::read_all() {
#if GPIO_MMAP_AVAILABLE
    if (gpio_map_) return gpio_map_[13];  // GPLEV0 register
#endif
    return simulate_read();
}

bool GPIOReader::is_simulation() const { return simulation_mode_; }

std::string GPIOReader::mode_string() const {
    return simulation_mode_ ? "SIMULATION" : "HW GPIO mmap";
}

bool GPIOReader::init_simulation() {
    simulation_mode_ = true;
    sim_counter_ = 0;
    std::cout << "[GPIO] Simulation mode" << std::endl;
    return true;
}

uint32_t GPIOReader::simulate_read() {
    sim_counter_++;
    uint32_t v = 0;
    if ((sim_counter_ / 250) % 2 == 0)   v |= (1 << 2);   // 1 MHz
    if ((sim_counter_ / 500) % 2 == 0)   v |= (1 << 3);   // 500 kHz
    if ((sim_counter_ % 7) < 4)          v |= (1 << 4);   // random
    if ((sim_counter_ % 2000) < 1500)    v |= (1 << 5);   // 75% duty
    if ((sim_counter_ / 1000) % 2 == 0)  v |= (1 << 6);   // 250 kHz
    if ((sim_counter_ / 2500) % 2 == 0)  v |= (1 << 7);   // 100 kHz
    v |= (1 << 8);                                         // always high
    if ((sim_counter_ % 10) < 5)         v |= (1 << 10);  // UART-like
    if ((sim_counter_ % 1000) < 500)     v |= (1 << 11);  // 50% duty
    if ((sim_counter_ / 100) % 2 == 0)   v |= (1 << 17);  // SPI CLK
    if (((sim_counter_ / 200) % 8) & 1)  v |= (1 << 22);  // SPI MOSI
    if ((sim_counter_ / 5000) % 5000 < 4900) v |= (1 << 23); // SPI CS
    if ((sim_counter_ / 600) % 2 == 0)   v |= (1 << 24);  // I2C SDA
    if ((sim_counter_ % 600) < 300)      v |= (1 << 27);  // I2C SCL
    if ((sim_counter_ % 4340) < 2170)    v |= (1 << 14);  // UART TX
    if ((sim_counter_ % 4340) < 2170)    v |= (1 << 15);  // UART RX
    return v;
}

void GPIOReader::close() {
#if GPIO_MMAP_AVAILABLE
    if (gpio_map_ && gpio_map_ != MAP_FAILED)
        munmap((void*)gpio_map_, 0x1000);
    if (fd_ >= 0) ::close(fd_);
#endif
    fd_ = -1;
    gpio_map_ = nullptr;
}
