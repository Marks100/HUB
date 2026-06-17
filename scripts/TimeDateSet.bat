@echo off

REM set the title
title=%~n0

echo displaying all available com ports...
mode 
SET /P COMPORT="Please enter the desired com portT, eg 1 for COM 1, 37 for COM37 :  

echo Setting Time and date over the serial port

REM setup the properties of the serial port
mode COM37 BAUD=115200 PARITY=n DATA=8 >nul

REM echo the current time over the serial port ( 37 )
echo TIME:%TIME% >\\.\COM%COMPORT%
echo TIME:%TIME%\r >Time.txt

echo "Time has been set to %TIME%"
PING localhost -n 2 >NUL

REM echo the current date over the serial port ( 37 )
echo DATE:%DATE% >\\.\COM%COMPORT%
::echo DATE:%DATE%\r >Date.txt

echo "Date has been set to %DATE%"
echo :)

::DEL Time.txt & DEL Date.txt

TIMEOUT /t 10


REM EOF
:END






