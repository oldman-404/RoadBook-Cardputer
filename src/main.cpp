#include <M5Cardputer.h>

#include "App.h"

App app;

void setup()
{
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5Cardputer.begin(cfg, true);

    app.setup();
}

void loop()
{
    app.loop();
}
