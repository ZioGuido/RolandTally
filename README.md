# RolandTally
Roland V-8HD Tally control with Raspberry Pi

This project can be compiled on both Windows and on a Raspberry Pi. 
If compiled on Windows, should be launched from the command prompt. 
If the Roland V-8HD is connected, the console just logs the input channel that is currently on Program (Red tally light).
If compiled on a Rapberry Pi, you can attach one of those 8-relay boards that can be bought for cheap on Amazon, 
and control up to 8 light bulbs (or LEDs). 

I have tested it on a Raspberry Pi 3 that I had handy, but it should run on every model, just make sure that the pinout connections are made correctly (check the code).
Also, if launched with the -d argument, it can be ran as a daemon. Copy the executable in a directory with chmod 755 and run it at startup from a systemctl service,
so you have your own Roland Tally controller with less than $30.

See it in action:
https://www.youtube.com/watch?v=GsHVB5VyFAA

Guido.
