FROM ubuntu:19.10

COPY target/brewblox-gcc /target/brewblox-gcc
WORKDIR /target/brewblox-gcc

RUN chmod +x brewblox && touch device_key.der server_key.der eeprom.bin

# status page
EXPOSE 80
# brewblox protocol
EXPOSE 8332
# debug port
EXPOSE 8333
# mdns
EXPOSE 5353/udp
# websocket display emulation
EXPOSE 7376

CMD ./brewblox --device_id=123456789012345678901234
