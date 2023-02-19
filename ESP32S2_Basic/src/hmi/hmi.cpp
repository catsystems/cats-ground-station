
#include "hmi.h"
#include "console.h"
#include "telemetry/telemetry.h"
#include "navigation.h"
#include <timeLib.h>

extern Telemetry link1;
extern Telemetry link2;

extern Navigation navigation;

void Hmi::begin(){
    upButton.begin();
    downButton.begin();
    leftButton.begin();
    rightButton.begin();
    centerButton.begin();

    okButton.begin();
    backButton.begin();

    recorder.begin();
    recorder.enable();

    window.begin();
    initialized = true;
    xTaskCreate(update, "task_hmi", 8196, this, 1, NULL);
}

void Hmi::fsm(){
    switch(state){
        case MENU:
            menu();
        break;

        case LIVE:
            live();
        break;

        case RECOVERY:
            recovery();
        break;

        case TESTING:
            testing();
        break;

        case DATA:
            data();
        break;

        case SENSORS:
            sensors();
        break;

        case SETTINGS:
            settings();
        break;

        default:
        break;
    }
}

/* MENU */

void Hmi::initMenu(){
    window.initMenu(menuIndex);
}

void Hmi::menu(){
    uint32_t oldIndex = menuIndex;
    if(rightButton.wasPressed() && (menuIndex % 3) < 2){
        menuIndex++;
    }
    
    if(leftButton.wasPressed() && (menuIndex % 3) > 0) {
        menuIndex--;
    }

    if(downButton.wasPressed() && menuIndex < 3) {
        menuIndex += 3;
    }

    if(upButton.wasPressed() && menuIndex > 2) {
        menuIndex -= 3;
    }

    if(menuIndex != oldIndex){
        window.updateMenu(menuIndex);
    }

    if(okButton.wasPressed() || centerButton.wasPressed()){
        state = (State)(menuIndex + 1);
        if(state == LIVE) {
            initLive();
        } else if(state == RECOVERY){
            initRecovery();
        } else if (state == TESTING) {
            initTesting();
        } else if (state == DATA) {
            initData();
        } else if (state == SENSORS) {
            initSensors();
        } else if (state == SETTINGS) {
            initSettings();
        }
    }
}


/* LIVE */

void Hmi::initLive(){
    window.initLive();
}

void Hmi::live(){

    if(boxWindow){
        /* Arming Window Mode */
        bool exit = false;

        if(link1.data.state() == 1 && enableTestMode) {
            exit = true;
        }

        if(okButton.wasPressed()){
            link2.disable();
            if(triggerTouchdown) {
                link1.triggerEvent(6);
                exit = true;
            } else {
                link1.triggerEvent(0);
            }
        }

        if(backButton.wasPressed() || exit){
            window.initLive();
            boxWindow = false;
            enableTestMode = false;
            triggerTouchdown = false;
        }
    } else {
        /* Normal Mode */
        bool updated = false;
        
        if(link1.data.isUpdated() && link1.info.isUpdated()){
            window.updateLive(&link1.data, &link1.info, 0);
            updated = true;
        } else if (link1.info.isUpdated()){
            window.updateLive(&link1.info, 0);
            updated = true;
        }

        if(link2.data.isUpdated() && link2.info.isUpdated()){
            if(link2.data.state() > 2){
                recorder.record(&link2.data.rxData);
                isLogging = true;
            } else {
                isLogging = false;
            }
            window.updateLive(&link2.data, &link2.info, 1);
            updated = true;
        } else if (link2.info.isUpdated()){
            window.updateLive(&link2.info, 1);
            updated = true;
        }

        if(updated) {
            window.refresh();
        }

        if(backButton.wasPressed()){
            state = MENU;
            window.initMenu(menuIndex);
        }

        if(rightButton.pressedFor(100) && link1.data.testingMode() && link1.data.state() == 0){
            window.initBox("Go to Testing?");
            boxWindow = true;
            enableTestMode = true;
        }

        if(rightButton.pressedFor(100) && link1.data.testingMode() && link1.data.state() == 1){
            window.initBox("Go to Touchdown?");
            boxWindow = true;
            triggerTouchdown = true;
        }
    }    

}

/* RECOVERY */

void Hmi::initRecovery(){
    window.initRecovery();
}

void Hmi::recovery(){
    
    EarthPoint3D a;
    EarthPoint3D b;

    a = navigation.getPointA();
    b = navigation.getPointB();
    if(a.lat && a.lon && b.lat && b.lon){
        //window.updateRecovery(a, b, navigation.getHorizontalDistance(), navigation.getBearing());
        
    } else {
        //window.updateRecovery(a, b);
    }

    if(navigation.isUpdated()){
        window.updateRecovery(&navigation);
    }
    
    if(backButton.wasPressed()){
        state = MENU;
        window.initMenu(menuIndex);
    }
}

/* TESTING */

void Hmi::initTesting(){
    window.initTesting();
}

void Hmi::testing() {

    if(boxWindow){

        bool exit = false;
        if(okButton.wasPressed()){
            link1.triggerEvent(testingIndex + 1);
            exit = true;
        }

        if(backButton.wasPressed() || exit){
            window.initTestingReady();
            window.updateTesting(testingIndex);
            boxWindow = false;
        }
        return;
    }

    if(backButton.wasPressed()){
        state = MENU;
        if(testingState >= WAIT_FOR_START) {
            link1.exitTesting();
            link2.enable();
        }
        testingState = DISCLAIMER;
        
        window.initMenu(menuIndex);
    }

    switch(testingState){
        case DISCLAIMER: {
            if(okButton.wasPressed()) {
                bool connected = false; 
                if((link1.data.getLastUpdateTime() + 1000) > xTaskGetTickCount()){
                    connected = true; 
                }
                
                window.initTestingConfirmed(connected, link1.data.testingMode());
                if(connected) {
                    testingState = CAN_START;
                } else {
                    testingState = CAN_NOT_START;
                }
            }
        } break;

        case CAN_START: {
            if(okButton.wasPressed()) {
                // Disable Link2
                link2.disable();
                link1.enterTesting();
                
                window.initTestingWait();

                startTestingTime = xTaskGetTickCount();
                testingState = WAIT_FOR_START;
            }
        } break;

        case WAIT_FOR_START: {
            static uint32_t counter = 0;
            if(link1.data.isUpdated()) {
                // In testing mode state indicates if we sucessfully started the mode
                if((link1.data.getLastUpdateTime() + 200) > xTaskGetTickCount()){
                    counter++;
                    if(link1.data.state() == 1 && counter > 5) {
                        window.initTestingReady();
                        window.updateTesting(0);
                        testingIndex = 0;
                        testingState = STARTED;
                        counter = 0;
                    }
                } else {
                    counter = 0;
                }
            } 
            if((startTestingTime + 10000) < xTaskGetTickCount()) {
                    link1.disable();
                    window.initTestingFailed();
                    testingState = FAILED;
                    counter = 0;
            }
        } break;

        case CAN_NOT_START: {

        } break;

        case FAILED: {

        } break;

        case STARTED: {
            uint32_t oldIndex = testingIndex;
            if(upButton.wasPressed() && (testingIndex % 4) > 0) {
                testingIndex--;
            } else if(downButton.wasPressed() && (testingIndex % 4) < 3) {
                testingIndex++;
            } else if (rightButton.wasPressed() && (testingIndex < 4)){
                testingIndex += 4; 
            } else if (leftButton.wasPressed() && (testingIndex > 3)) {
                testingIndex -= 4;
            }

            if(link1.data.isUpdated()) {
                if(link1.data.state() != 1) {
                    testingState = FAILED;
                    link1.exitTesting();
                    window.initTestingLost();
                }
            } 
            
            if ((link1.data.getLastUpdateTime() + 1000) < xTaskGetTickCount()){
                testingState = FAILED;
                link1.exitTesting();
                window.initTestingLost();
            }

            if(oldIndex != testingIndex) {
                window.updateTesting(testingIndex);
            }

            if(okButton.wasPressed()) {
                window.initTestingBox(testingIndex);
                boxWindow = true;
            }
        } break;

        default:
        break;
    }
}

/* DATA */

void Hmi::initData(){
    window.initData();
}

void Hmi::data() {
    if(backButton.wasPressed()){
        state = MENU;
        window.initMenu(menuIndex);
    }
}

/* SENSORS */

void Hmi::initSensors(){
    window.initSesnors();
}

void Hmi::sensors() {
    if(backButton.wasPressed()){
        state = MENU;
        window.initMenu(menuIndex);
    }
}

/* SETTINGS */

void Hmi::initSettings(){
    settingSubMenu = 0;
    settingIndex = -1;
    window.initSettings(settingSubMenu);
}

void Hmi::settings(){
    static bool keyboardActive = false;
    static bool configChanged = false;
    static int32_t i = 0;
    if(keyboardActive){
        if(rightButton.wasPressed() || rightButton.pressedFor(500)){
            
            if(i != 9 && i != 19 && i != 28 && i != 37) {
                i++;
                window.updateKeyboard(keyboardString, i);
            }   
        }
        if(leftButton.wasPressed() || leftButton.pressedFor(500)){
            if(i != -1 && i != 0 && i != 10 && i != 20 && i != 29) {
                i--;
                window.updateKeyboard(keyboardString, i);
            }   
        }
        if(downButton.wasPressed() || downButton.pressedFor(500)){
            if (i == -1){
                i = 7;
                window.updateKeyboard(keyboardString, i);
            } else if (i < 15){
                i += 10;
                window.updateKeyboard(keyboardString, i);
            } else if (i < 29){
                i += 9;
                window.updateKeyboard(keyboardString, i);
            }  
        }
        if(upButton.wasPressed() || upButton.pressedFor(500)){
            if(i > 9){
                if(i < 25){
                    i -= 10;
                    window.updateKeyboard(keyboardString, i);
                } else if (i < 38){
                    i -= 9;
                    window.updateKeyboard(keyboardString, i);
                }
            } else {
                i = -1;
                window.updateKeyboard(keyboardString, i);
            } 
        }
        if(okButton.wasPressed() || okButton.pressedFor(500)){
            if(i == 29) { // shift
                window.updateKeyboard(keyboardString, i, true);
            } else if (i == 37) { // enter
                memcpy((char*)settingsTable[settingSubMenu][settingIndex].dataPtr, keyboardString, 8);
                window.initSettings(settingSubMenu);
                configChanged = true;
                window.updateSettings(settingIndex);
                keyboardActive = false;
            } else {
                window.updateKeyboard(keyboardString, i, true);
            }
        }

        if(backButton.wasPressed()){
            memcpy((char*)settingsTable[settingSubMenu][settingIndex].dataPtr, keyboardString, 8);
            window.initSettings(settingSubMenu);
            configChanged = true;
            window.updateSettings(settingIndex);
            keyboardActive = false;
        }


    } else {
        if(settingIndex == -1){
        if(rightButton.wasPressed() && settingSubMenu < 1){
        settingSubMenu++;
        window.initSettings(settingSubMenu);
        }

        if(leftButton.wasPressed() && settingSubMenu > 0){
            settingSubMenu--;
            window.initSettings(settingSubMenu);
        }
        } else {

            if(settingsTable[settingSubMenu][settingIndex].type == NUMBER){
                if((rightButton.wasPressed() || rightButton.pressedFor(500)) && \
                    *(int16_t*)settingsTable[settingSubMenu][settingIndex].dataPtr < \
                    settingsTable[settingSubMenu][settingIndex].config.minmax.max) {
                        (*(int16_t*)settingsTable[settingSubMenu][settingIndex].dataPtr)++;
                        configChanged = true;
                        window.updateSettings(settingIndex);
                }
                if((leftButton.wasPressed() || leftButton.pressedFor(500)) && \
                    *(int16_t*)settingsTable[settingSubMenu][settingIndex].dataPtr > \
                    settingsTable[settingSubMenu][settingIndex].config.minmax.min) {
                        (*(int16_t*)settingsTable[settingSubMenu][settingIndex].dataPtr)--;
                        configChanged = true;
                        window.updateSettings(settingIndex);
                }
            }

            if(settingsTable[settingSubMenu][settingIndex].type == TOGGLE){
                if(rightButton.wasPressed() && *(bool*)settingsTable[settingSubMenu][settingIndex].dataPtr == false) {
                    (*(bool*)settingsTable[settingSubMenu][settingIndex].dataPtr) = true;
                    configChanged = true;
                    window.updateSettings(settingIndex);
                }
                if(leftButton.wasPressed() && *(bool*)settingsTable[settingSubMenu][settingIndex].dataPtr == true) {
                    (*(bool*)settingsTable[settingSubMenu][settingIndex].dataPtr) = false;
                    configChanged = true;
                    window.updateSettings(settingIndex);
                }
            }

            if(settingsTable[settingSubMenu][settingIndex].type == STRING){
                if(okButton.wasPressed()){
                    memcpy(keyboardString, (char*)settingsTable[settingSubMenu][settingIndex].dataPtr, 8);
                    
                    window.initKeyboard(keyboardString, settingsTable[settingSubMenu][settingIndex].config.stringLength);
                    keyboardActive = true;
                }
            }
        }

        if(downButton.wasPressed() && settingIndex < settingsTableValueCount[settingSubMenu]-1){
            settingIndex++;
            configChanged = true;
            window.updateSettings(settingIndex);
        }

        if(upButton.wasPressed() && settingIndex > -1){
            settingIndex--;
            configChanged = true;
            window.updateSettings(settingIndex);
        }

        

        if(backButton.wasPressed()){
            state = MENU;
            if(configChanged) {
                configChanged = false;
                link1.setLinkPhrase(systemConfig.config.linkPhrase1, 8);
                link2.setLinkPhrase(systemConfig.config.linkPhrase2, 8);
                link1.setTestingPhrase(systemConfig.config.testingPhrase, 8);
                systemConfig.save();
                console.log.println("Save config");
            }
            window.initMenu(menuIndex);
        }
    }
    
}

void Hmi::update(void *pvParameter){
    Hmi* ref = (Hmi*)pvParameter;

    ref->window.logo();

    vTaskDelay(2000);

    int oldUsbStatus = 0;
    ref->window.initBar();
    ref->initMenu();

    uint32_t barUpdate = millis();
    bool timeValid = false;

    while(ref->initialized){
        TickType_t task_last_tick = xTaskGetTickCount();

        ref->fsm();

        if(link1.data.isUpdated()){
            //ref->window.updateBar(link1.data.ts());
        }

        if(millis() - barUpdate >= 1000){
            barUpdate = millis();
            float voltage = analogRead(18)*0.00059154929;
            if(link2.time.isUpdated()){
                setTime(link2.time.hour(), link2.time.minute(), link2.time.second(),0,0,0);
                adjustTime(systemConfig.config.timeZoneOffset * 3600);
                timeValid = true;
            }
            ref->window.updateBar(voltage, digitalRead(21), ref->isLogging, link2.location.isValid(), timeValid);
        }

        
        
        
        ref->upButton.read();
        ref->downButton.read();
        ref->leftButton.read();
        ref->rightButton.read();
        ref->centerButton.read();

        ref->okButton.read();
        ref->backButton.read();
        vTaskDelayUntil(&task_last_tick, (const TickType_t) 1000 / 50);
    }
}