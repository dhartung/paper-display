# Paper-Display

This project aims to transform the LilyGo EPD47, a development board based on an ESP32 with an e-paper display, into a display that renders
images server side. The goal is for the device to send a request to a server that responds with an image and a sleep time. The device will
then enter deep sleep mode for the specified duration before repeating the process.

The clients request also includes an `image_id` parameter, which allows the server to react on the currently displayed image. This enables the server
either send a partial image or no image at all. The schema for the server response is outlined in [Schema.md](docs/Schema.md).

## Getting Started

To get this code running, you first need to create a configuration file. Please rename copy the file `epaper_config.example.h` into `epaper_config.h` and fill in the required values.

This project uses [platform.io](https://platformio.org/). Download the Tool for your platform or IDE of choice. Then you can run:
```bash
pio run build
```
To build and upload the code to your device of choice.


## Troubleshooting

If you are unable to patch or access the device due to its deep sleep state, you can hold down the STR_IO0 button while connecting a USB-C
cable. This will force the device into flash mode, allowing you to make necessary changes and resume normal operation.

## Examples

An example implementation of a server providing images in the required schema can be found in the [paper-image-server](https://github.com/dhartung/paper-image-server) repository.

## Acknowledgements

The code for this project is heavily inspired by the following sources:
* [LilyGo EPD47 GitHub Page](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47)
* [LilyGo EPD-4-7 OWM Weather Display GitHub Page](https://github.com/Xinyuan-LilyGO/LilyGo-EPD-4-7-OWM-Weather-Display)