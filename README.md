# LCD-Coy-Fish

## Table of Contents
- [Description](#Description)
- [Prerequisites](#Prerequisites)
- [How to Install](#How-to-Install)
    - [Wiring](#Wiring)
    - [Uploading the Code](#Uploading-the-Code)
    - [Controls](#Controls)
- [FAQ](#FAQ)
- [Helpful Links](#Helpful-Links)

<br/><br/>

## Description
An example of what can be done with my [LCD Animation Library][4] for Arduino.

Using an Arduino Liquid Crystal Display (LCD) and a joystick, the player is able to swim around the screen like a coy fish.

**Some "features":**
- You can variate speed with joystick position
- You can spawn a *worm*
- You can *eat* the worm
- If you don't touch the joystick, the fish *swims by itself*
- If you spawn a worm when the fish is swimming by itself, it will *chase and eat the worm* 

See it for yourself in the video below!

[![](https://img.youtube.com/vi/PHDyf8hoNdM/maxresdefault.jpg)](https://youtu.be/PHDyf8hoNdM)

<br/><br/>

## Prerequisites
In order to run the program, you must have access to the following:
- [Arduino Microcontroller][1]
- [Arduino LCD and Joystick][2]
- [Arduino IDE][3]
- [LCD Animation Library][4]

<br/><br/>

## How to Use
### Wiring
The wiring portion uses [this tutorial for LCD][5] and [this tutorial for the joystick controller][6]. Note, the potentiometer is required because it allows you to regulate the brightness of the screen. For exact wiring, see the diagram below.

![](./images/img1.jpg)

<br/>

### Uploading the Code
After you have connected the LCD and joystick to your Arduino, you can upload and run the code.
1. If you haven't already, [install the LCD Animation Library][7].
2. Download the repository.
```bash
git clone https://github.com/Vladnet47/LCD-Coy-Fish.git
```
3. Open ***src/CoyFish/CoyFish.ino*** using the Arduino IDE.
4. Select the correct ***Board*** and ***Port*** under ***Tools***.
5. Upload the sketch to your board.

<br/>

### Controls
There are two main controls to the game.
1. Move the joystick to swim with the fish.
2. Click the joystick to spawn a worm in a random location.

<br/><br/>

## FAQ
#### Why aren't there more fish?
The Arduino LCD only permits eight custom characters. The fish utilizes six and the worm utilizes two (at most). There simply is not enough onboard memory on the display, which is truly a shame, because it would be a simple addition with the LCD Animation Library.

## Helpful Links
[Purchase Arduino Microcontroller][1]

[Purchase Arduino LCD and Joystick][2]

[Wire Arduino LCD][5]

[Wire Arduino Joystick][6]

[Arduino IDE][3]

[LCD Animation Library][4]


[1]: https://store.arduino.cc/usa/mega-2560-r3
[2]: https://www.amazon.com/ELEGOO-Upgraded-Tutorial-Compatible-MEGA2560/dp/B01MG49ZQ5/ref=pd_sbs_147_img_1/137-9343435-9781537?_encoding=UTF8&pd_rd_i=B01MG49ZQ5&pd_rd_r=015e7469-253e-44bf-992d-c821a0dbdd71&pd_rd_w=QBO6t&pd_rd_wg=AVDBZ&pf_rd_p=5cfcfe89-300f-47d2-b1ad-a4e27203a02a&pf_rd_r=69BPHVW21DAD759CPNC9&psc=1&refRID=69BPHVW21DAD759CPNC9
[3]: https://www.arduino.cc/en/main/software
[4]: https://github.com/Vladnet47/LCD-Animation-Library
[5]: https://howtomechatronics.com/tutorials/arduino/lcd-tutorial/
[6]: https://www.brainy-bits.com/arduino-joystick-tutorial/
[7]: https://github.com/Vladnet47/LCD-Animation-Library#Installation
