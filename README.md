# transit-tracker
This is a personal transit tracker project for CTA buses and trains.

Status: v1 complete!

[Video of v1](./documentation/video.mov)

## Requirements

**Hardware**

2 64x32 LED matrices

Power supply

Connector (right now need the special one for the matrix, soon just a USB-C)

You also need to get the PCB. Using the gerber files in this repo, you can get the PCB manufactured at your preferred PCB manufacturer (I used PCBWay).

## Installing/Building

(These instructions reflect the set-up process as of August 24, 2026.)

### Getting API Keys

Before you start connecting or building anything, you need to have API keys for CTA trains and buses. You can get those here:

Buses: https://ctabustracker.com/account

Trains: https://www.transitchicago.com/developers/traintrackerapply/

Once you have your keys, pull this repo. Inside the `include` folder, make a `secrets.h` file and place your keys in there, like this:

```
#ifndef SECRETS_H
#define SECRETS_H

#define TRAIN_TRACKER_API_KEY "Your key here"
#define BUS_TRACKER_API_KEY "Your keky here"

#endif
```

Now you're ready to build

### Building the tracker

1. Connect LED matrices together with HUB75 cable

2. Connect the PCB to a computer and flash the PCB

   1. Open VS Code, navigate to the project folder, and run idf.py flash

3. Set the tracker configuration

   1. Run idf.py menuconfig from the project folder

   2. In the menuconfig menu, navigate as follows:

      1. Component config->WiFi STA Configuration

         1. Check connect using wifi
         2. Set WiFi SSID and password

      2. Component config->HUB75 RGB LED Matrix Driver

         1. Go to Board Preset and check Manual Pin Configuration

         2. Go to Panel Settings and set the following settings:

            1. Panel Width: 64 (pixels)
            2. Panel Height: 32 (pixels)
            3. Minimum Refresh Rate: 60 (Hz)
            4. Default Brightness: 128

         3. Go to Multi Panel Layout and set the following settings:

            1. Layout Rows: 1
            2. Layour Columns: 2
            3. Layout Type (Horizonal (Single Row))

         4. Go to Pin Configuration and set it as follows:

            ```
            4 - R1
            5 - G1
            6 - B1
            7 - R2
            15 - G2
            16 - B2
            17 - A
            18 - B
            14 - C
            13 - D
            -1 - E
            11 - LAT
            10 - OE
            9 - CLK
            ```

      3. Component config->Transit Tracker Configuration

         1. Check connect using wifi
         2. Check enable bus tracking if you want to track buses
         3. Check enable train tracking if you want to track trains
         4. Hit enter on bus routes to track, and enter the numbers of the buses you want to track
            1. i.e., if you want to track the 1 and the 7, type in `1,7` with no spaces just like shown
         5. Hit enter on bus stop IDs to track, and enter the number of the stop IDs you want to track
            1. i.e., if you want to track stop 68 and stop 1583, type in `68,1583` with no spaces as shown
         6. Hit enter on "Max number of bus predictions to receive," and enter the maximum number of bus predictions you want to display
            1. I recommend 6 or 9
         7. Hit enter on train stop IDs to track, and enter the number of the stop IDs you want to track
            1. i.e., if you want to track the Roosevelt and the Jackson stops, type in `41400,40070` with no spaces as shown
         8. Hit enter on "Max number of train predictions to receive," and enter the maximum number of train predictions you want to display
            1. I recommend 6 or 9

4. Connect the PCB to the matrices with the HUB75

   1. Note: the HUB75 header in v1 of the board is wired wrong, so you have to pull the header pins to the opposite side of the board for it to work. I plan to update the board so this is no longer an issue.

5. Connect the matrices' power leads to the screw terminals on the PCB

   1. Power is closest to the HUB75 header
   2. GND is the other terminal

6. Print the enclosure

   1. Files in 3dprinting folder of this repo

7. Screw PCB to the thin part of the enclosure

   1. There are little holes for screws to go through on both the thin part of the enclosure and the PCB
   2. With v1 of the enclosure/board, the PCB ends up a little tilted when you do this. This is something I plan to fix with the next version

8. Place both parts of the enclosure on top of the LED matrices, and screw them together using the screws that came with the LED matrices

9. Connect the train tracker to wall power using a USB cable

10. Watch your trains/buses go!

### Libraries/Skills used

C, C++, ESP IDF/FreeRTOS, LVGL, PCB Design, CAD (Autodesk Fusion), 3D Printing

