[![Build Status](https://travis-ci.org/BrewPi/firmware.svg?branch=feature%2Ftravis-ci)](https://travis-ci.org/BrewPi/firmware)

This is the main source code repository  for the firmware on the BrewPi brewing temperature controller.


## Getting started
End users will not have to compile the firmware themselves.

We provide pre-compiled binaries [in releases](https://github.com/BrewPi/firmware/releases).


## Updating your controller
Our update script (part of [brewpi-tools](https://github.com/elcojacobs/brewpi-tools)) automatically downloads the latest release to flash to your controller.

To update your controller, the brewpi script and the web interface, you will generally just run:
```
cd ~/brewpi-tools
sudo python updater.py
```

You can also upload to your controller from the BrewPi web interface. For the Spark Core, this requires that you already have a version of BrewPi running on it. If not, read how to flash via DFU below.


## Building the firmware for the Brewpi Spark
If you want to make your own changes to the firmware, follow these steps:

- in the firmwarwe repo, it is recommended to change to the "develop" branch: `git checkout develop`

Then browse to `platform/spark/` in the `firmware` repo and run make:

```
cd platform/spark
make
```

To build for the photon, use

```
cd platform/spark
make PLATFORM=photon
```


This will build the binary to the file `platform/spark/target/brewpi.bin`. You can upload your new binary via the BrewPi web interface.

## Flashing the firmware via DFU
If uploading firmware via the web interface fails, you can flash new firmware to your Spark Core with dfu-util. Please refer to this [guide on our community forum](https://community.brewpi.com/t/flashing-the-core-without-the-web-interface-fresh-core-or-in-case-of-emergency/).

You can also build the firmware and flash directly by running `make program-dfu` from `platform/spark`.


## Changelog
Please see our GitHub release for the change log


## License
Unless stated elsewhere, file headers or otherwise, all files herein are licensed under an GPLv3 license. For more information, please read the LICENSE file.


## Contribute
Contributions to our firmware are very welcome. Please contact us first via our [community forum](https://community.brewpi.com/) to discuss what you want to code to make sure that it aligns with our road map.

Please send pull requests against the develop branch. We can only accept your pull request if you have signed our [Contributor License Agreement (CLA)](http://www.brewpi.com/cla/).

## Controlbox

Controlbox is the framework that we are using to build the next version of brewpi. 

It's still work in progress - checkout the [readme](./app/cbox/readme.md) for details.
