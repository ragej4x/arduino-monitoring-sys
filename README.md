<h1>CIRCUIT CONNECTION</h1>

![alt text](http://url/to/img.png)


DTS ES :
   pin A1
   5.0vcc
   GRD


TEMP SEN:
   pin 2
   5.0vcc
   GRDs

   5.1k or 10k  ohm resistor between pin pin2 and 5v vcc 
   dont overpass the 10k limit = abnormal output



PH TDS:
   pin A0 - Po
   5.0vcc   
   GRD


  +3.3V
        |
    [Pump +]
        |
[Collector of 2n2222]
        |
    [Pump −]
        |
[Emitter of 2n2222]
        |
    Arduino Pin 13,12,8,7,4(OUTPUT)
        |
    GND



        _______
       |       |
       | 2N2222|
       |_______|
        | | |
        E B C

Alternatives - BC547 if ever wlang 2n2222 


Pump + is powered from 3.3V pin of the Arduino.

Pump − is controlled via the 2n2222 transistor, which is connected to Arduino pin 13,12,8,7,4.

Arduino Pin 2 controls the transistor's base to switch the pump on/off.

Arduino GND is connected to the emitter of the 2n2222 and pump's −.




Pump Power (+) Connection:

Connect the Pump's + pin to the 5V pin (or 3.3V pin) on the Arduino, depending on your pump's rating.

Pump Ground (−) Connection:

Connect the Pump's − pin to the Collector of the 2n2222 transistor.

The Emitter of the 2n2222 will be connected to Arduino GND.

Base of 2n2222:

The Base of the 2n2222 is connected to Arduino Pin 2 — this controls when the pump is switched on or off.
