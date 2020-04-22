FROM node:12-alpine

WORKDIR /app

RUN apk add --no-cache make gcc g++ linux-headers eudev-dev python bash usbutils curl \
  && apk add --no-cache --repository http://dl-3.alpinelinux.org/alpine/edge/testing/ dfu-util \
  && npm install serialport --build-from-source --unsafe-perm \
  && npm install particle-cli --unsafe-perm \
  && npm install particle-usb \
  && apk del make gcc g++ linux-headers

COPY particle /usr/local/bin/particle

ENTRYPOINT [ "/bin/bash" ]
