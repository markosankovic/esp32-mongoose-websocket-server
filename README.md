# esp32-mongoose-websocket-server

Configure SSID for wireless network, Password and WebSocket Server Port in "PERK Configuration":

    $ make menuconfig

Save configuration, build and run:

    $ make flash monitor

Use WebSocket Client e.g. [Simple WebSocket Client](https://chrome.google.com/webstore/detail/simple-websocket-client/pfdhoblngboilpfeibdedpjgfnlcodoo?hl=en) and connect to the assigned private IP address. See `$ make flash monitor` output:

    I (3630) event: sta ip: 192.168.0.17, mask: 255.255.255.0, gw: 192.168.0.1

Example of WebSocket Server Address:

    ws://192.168.0.17:7123
