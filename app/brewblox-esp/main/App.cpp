#include "App.h"
#include "network/Ethernet.h"
#include "network/Wifi.h"
#include "network/server.hpp"
#include "network/wifi_creds.h"
#include <asio.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <driver/gpio.h>
#include <nvs_flash.h>
#pragma GCC diagnostic pop

#include "PCA9571.hpp"
#include "hal/hal_i2c.h"

using namespace std::chrono;
using tcp = asio::ip::tcp;

App::App()
{
    nvs_flash_init();
    gpio_install_isr_service(0);
    esp_event_loop_create_default();
}

App::~App()
{
    esp_event_loop_delete_default();
    gpio_uninstall_isr_service();
    nvs_flash_deinit();
}

void App::start()
{
    init();
}

void App::init()
{
    hal_i2c_master_init();
    PCA9571 io_expander;
    io_expander.write_io(0xF8);

    esp_netif_init();

    auto& ethernet = get_ethernet();
    ethernet.set_host_name("brewblox_wired");
    ethernet.start();

    auto& wifi = get_wifi();
    wifi.set_host_name("brewblox_wifi");
    wifi.set_auto_connect(true);
    wifi.set_ap_credentials(WIFI_SSID, WIFI_PASSWORD);
    wifi.connect_to_ap();

    asio::io_context io_context;
    server srv(io_context, tcp::endpoint(tcp::v4(), 81));
    io_context.run();
}