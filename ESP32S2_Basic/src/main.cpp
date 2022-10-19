/******************************************************************************
* file    main.cpp
*******************************************************************************
* brief   Main Program
*******************************************************************************
* author  Florian Baumgartner
* version 1.0
* date    2022-08-02
*******************************************************************************
* MIT License
*
* Copyright (c) 2022 Crelin - Florian Baumgartner
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell          
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
******************************************************************************/

#include <Arduino.h>
#include "console.h"
#include "utils.h"
#include "telemetry/telemetry.h"
#include "hmi/hmi.h"
#include "logging/recorder.h"
#include "navigation.h"


Utils utils;
Hmi hmi("/logs");

Telemetry link1(Serial, 8, 9);
Telemetry link2(Serial1, 11, 12);

Navigation navigation;

void setup()
{

  pinMode(21, INPUT);

  console.begin();

  if(!utils.begin(0, "DRIVE"))
  {
    console.error.println("[MAIN] Could not initialize utilities");
  }

  link1.begin();
  link2.begin();

  

  navigation.setPointA(47.236777221226646, 8.819492881367166);
  navigation.setPointB(47.236777221226646, 8.819492881367166);

  hmi.begin();
}

bool ini = false;
void loop()
{ 

  if(millis() > 5000 && !ini)
  {
    ini = true;
    link1.setLinkPhrase("cats");
    link2.setLinkPhrase("cats");

    navigation.begin();
  }

  if(link2.location.isUpdated()){
    navigation.setPointA(link2.location.lat(), link2.location.lon());
  }

  if(link1.data.lat() != 0 && link1.data.lon() != 0){
    navigation.setPointB(link1.data.lat(), link1.data.lon());
  }

  delay(100);
}
