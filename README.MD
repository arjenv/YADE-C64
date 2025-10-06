C64-YADE

- Lean and mean Cassette player for the C64. Based on ESP8266 (NodeMCU).
- Can handle TAP and PRG files in both normal speed as fastload mode.
- 3rd mode: Autofastload. Loads PRG files without loading a turbo loader first.
      Even faster than turbospeed.
- Saves program to TAP format, both normal speed as turbo speed.
- Upload and download files through a web-interface to/from FLASH
- No need for SD card
- Telnet (for status information)
- WifiManager. For your first login connect to the NODEMCU AP and set your credentials. 




October 2025:

Added 'Save' functionality.
Can now save a TAP file to the flash. User needs to make sure there is enough space.
TAP files, especially in the normal speed mode are bulky.




September 2025

YADE

Emulates a cassette player for C64.
Requires very little components.
Login through a website interface on your phone/PC/Tablet.
Simply upload C64 software via this IF.
No need for an SD card.

I have a few NodeMCU's lying around and thought it would be a good base for
a cassette player for the C64. And yes I do know there are other more elaborate
versions.

Loads .TAP and .PRG files in both normal mode as fastload mode.
      --> In fastload you's have to load a fastloader first.
          Just like the real world :-)

And there's an extra Auto_turbo mode. Autoturbo only works with .PRG files.
The autoturbo mode just requires a 'load' command.
It will 1st load a very basic fastloader, runs it and loads the .PRG file
afterward. It is faster then the normal fastload programs.

WIFI: Works via your home WIFI network. Starts in a WiFimanager.(192.168.4.1)
Set your phone's Wifi to connect to the YADE AP and set IP address etc.
The software will save these values in config.json

TELNET: Open a telnet session to this IP address to see some debug output.
        try 'h' for some simple help statements.
RS232: For more Debug statements connect an RS232 line to TX and GND to your
(USB) COM port.
BEWARE: I DID NOT DARE CONNECT BOTH THE C64 5V AND A USB CABLE AT THE SAME TIME.
Although I think it is save since both 5V's are separated via a diode, I
did not connect both the C64 5V and an USB cable from the PC. Instead I used
another NodeMCU to connect to the setup and the PC as an interface. (In fact I
use another NodeMCU with another telnet IP so there is no wired connection to
PC).

Use at your own risk. If you did connect both and it works pls notify me.





Schematic: The LED is 'luxury' so I could track the motor voltage during
this project. You do need, however, some sort of resistor divider from
Motor (6V) to D1 (NodeMCU) as a voltage > 3v3 will destroy it.
Try 3.3k and 3.3k. Or if you want the LED as well make it a green one since
green uses some 2V when lit. Of course the 1k+250 Ohm can be replaced with a
1.2k resistor.
The LVL block is a bidirectional voltage converter from 5 to 3V3. Readily
available in the electronics shop.

The 'Save' option is not yet implemented.

YADE is an acronym. Can you guess where it stands for? Let me know!
