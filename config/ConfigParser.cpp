/****************************************************************************
 * Copyright (C) 2016,2017 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ConfigReader.hpp"
#include "../utils/CPStringTools.hpp"
#include "./ConfigParser.hpp"
#include "./ConfigValues.hpp"

ConfigParser::ConfigParser(std::string configData){
    this->content = configData;
    this->contentLines = CPStringTools::StringSplit(content, "\n");

    if(contentLines.empty())
		return;

    //remove the comments and make everything uppercase
    for(u32 i = 0; i < contentLines.size(); i++){
        std::vector<std::string> comments = CPStringTools::StringSplit(contentLines[i], "//");
        if(!comments.empty()){
            contentLines[i] = comments[0];
        }
        //we want everything uppercase
        std::transform(contentLines[i].begin(), contentLines[i].end(),contentLines[i].begin(), ::toupper);
    }

    //remove empty lines
    std::vector<std::string> contentline2;
    for(u32 i = 0; i < contentLines.size(); i++){
        if(strlen(contentLines[i].c_str()) > 0){
           contentline2.push_back(contentLines[i]);
        }
    }
    contentLines = contentline2;
    Init();
}

ConfigParser::~ConfigParser(){

}

PARSE_TYPE ConfigParser::getType(){
    return type_b;
}

void ConfigParser::setType(PARSE_TYPE newType){
    this->type_b = newType;
}

u16 ConfigParser::getSlot(){
    return this->slot_b;
}

void ConfigParser::setSlot(u16 newSlot){
    this->slot_b = newSlot;
}

bool ConfigParser::Init(){
    if(contentLines.size() == 0){
        log_printf("ConfigParser::Init(): Files seems to be empty. Make sure to have a proper header\n");
        return false;
    }
    const char * line = contentLines[0].c_str();
    int len = strlen(line);
    std::string identify;
    if(line[0] == '[' && line[len-1] == ']'){
        identify = contentLines[0].substr(1,len-2);
    }else{
        log_printf("ConfigParser::Init(): Not a proper config file!\n");
        return false;
    }

    if(identify.compare("GAMEPAD") == 0){
        if(HID_DEBUG) log_printf("Its a gamepad config file!\n");
        setSlot(gGamePadSlot);
        setType(PARSE_GAMEPAD);
    }else if(identify.compare("MOUSE") == 0){
        if(HID_DEBUG) log_printf("Its a mouse config file!\n");
        setSlot(gMouseSlot);
        setType(PARSE_MOUSE);
        this->vid = HID_MOUSE_VID;
        this->pid = HID_MOUSE_PID;
    }else if(identify.compare("KEYBOARD") == 0){
        if(HID_DEBUG) log_printf("Its a keyboard config file!\n");
        setSlot(gHID_SLOT_KEYBOARD);
        setType(PARSE_KEYBOARD);
        this->vid = HID_KEYBOARD_VID;
        this->pid = HID_KEYBOARD_PID;
    }else{
        if(HID_DEBUG) log_printf("Its a controller config file!\n");

        setSlot(getSlotController(identify));
        setType(PARSE_CONTROLLER);
    }

    if(getSlot() == HID_INVALID_SLOT){
        return false;
    }

    ConfigReader::increaseNumberOfLoadedFiles();

    return true;
}

void ConfigParser::parseSingleLine(std::string line){
    std::vector<std::string> cur_values = CPStringTools::StringSplit(line,"=");
    if(cur_values.size() != 2){
        if(HID_DEBUG || cur_values.size() > 2) log_printf("Not a valid key=pair line %s\n",line.c_str());
        return;
    }else{
        u16 hid_slot = getSlot();

        if(HID_DEBUG) log_printf("leftpart = \"%s\" \n",cur_values[0].c_str());
        if(HID_DEBUG) log_printf("rightpart = _%s_ \n",cur_values[1].c_str());
        int keyslot =  -1;

        if(HID_DEBUG) log_printf("Checking single value\n");
        if(getType() == PARSE_GAMEPAD /*|| getType() == PARSE_KEYBOARD*/){
            keyslot = ConfigValues::getKeySlotGamePad(cur_values[0]);
        }else if(getType() == PARSE_MOUSE){
            keyslot = ConfigValues::getKeySlotMouse(cur_values[0]);
        }else{
            keyslot = ConfigValues::getKeySlotDefaultSingleValue(cur_values[0]);
        }
        if(keyslot != -1){
            if(HID_DEBUG) log_printf("Its a single value\n");
            long rightValue = -1;
            bool valueSet = false;
            if(cur_values[0].compare("DPAD_MODE") == 0){
                const u8 * values_ = NULL;
                if((values_ = ConfigValues::getValuesStickPreset(cur_values[1])) != NULL){
                    if(values_[STICK_CONF_MAGIC_VERSION] != STICK_CONF_MAGIC_VALUE)
                    if(HID_DEBUG) log_printf("Settings preset DPAD MODE and Mask\n");
                    config_controller[hid_slot][CONTRPS_DPAD_MODE][0] = CONTROLLER_PATCHER_VALUE_SET;
                    config_controller[hid_slot][CONTRPS_DPAD_MODE][1] = values_[CONTRDPAD_MODE];
                    if(values_[CONTRDPAD_MASK] != 0x00){
                        config_controller[hid_slot][CONTRPS_DPAD_MASK][0] = CONTROLLER_PATCHER_VALUE_SET;
                        config_controller[hid_slot][CONTRPS_DPAD_MASK][1] = values_[CONTRDPAD_MASK];
                    }
                    valueSet = true;
                }
            }

            if(!valueSet){
                if(getType() == PARSE_KEYBOARD){
                    if((rightValue = ConfigValues::getPresetValuesKeyboard(cur_values[1]))!= -1){
                        if(HID_DEBUG) log_printf("Used pre-defined Keyboard! \"%s\" is %d\n",cur_values[1].c_str(),rightValue);
                    }else{
                        if(HID_DEBUG) log_printf("I need to parse %s\n",cur_values[1].c_str());
                        char * ptr;
                        rightValue = strtol(cur_values[1].c_str(),&ptr,16);
                    }
                }else{
                    rightValue = ConfigValues::getPresetValue(cur_values[1]);

                    if(getType() == PARSE_MOUSE){ //No parsing for the mouse
                         if(rightValue == -1){
                             if(HID_DEBUG) log_printf("Invalid mouse value, lets skip it %s\n",cur_values[1].c_str());
                             return;
                         }
                    }else{
                        if(rightValue == -1){
                            if(HID_DEBUG) log_printf("I need to parse %s\n",cur_values[1].c_str());
                            char * ptr;
                            rightValue = strtol(cur_values[1].c_str(),&ptr,16);
                        }
                    }
                }
                if(HID_DEBUG) log_printf("Setting value to %d\n",rightValue);

                config_controller[hid_slot][keyslot][0] = CONTROLLER_PATCHER_VALUE_SET;
                config_controller[hid_slot][keyslot][1] = rightValue;
            }
        }else{
            if(HID_DEBUG) log_printf("Check pair value\n");
            keyslot = ConfigValues::getKeySlotDefaultPairedValue(cur_values[0]);
            if(keyslot != -1){
                if(HID_DEBUG) log_printf("Its a  pair value\n");

                if(!ConfigValues::getInstance()->setIfValueIsAControllerPreset(cur_values[1],getSlot(),keyslot)){
                    if(HID_DEBUG) log_printf("And its no preset\n");
                    std::vector<std::string> rightvalues = CPStringTools::StringSplit(cur_values[1],",");

                    if(rightvalues.size() != 2){
                        log_printf("ConfigParser::parseSingleLine: %d instead of 2 key=values pairs in line\n",rightvalues.size());
                        return;
                    }

                    char * ptr;
                    long firstValue = strtol(rightvalues[0].c_str(),&ptr,16);
                    long secondValue = strtol(rightvalues[1].c_str(),&ptr,16);
                    config_controller[hid_slot][keyslot][0] = firstValue;
                    config_controller[hid_slot][keyslot][1] = secondValue;

                    if(HID_DEBUG) log_printf("Set %02X,%02X\n",firstValue,secondValue);
                }else{
                    if(HID_DEBUG) log_printf("Found preset value!!\n");
                }
            }else{
                log_printf("ConfigParser::parseSingleLine: The setting \"%s\" is unknown!\n",cur_values[0].c_str());
            }
        }
    }
}

bool ConfigParser::resetConfig(){
    int slot = getSlot();
    if(slot >= gHIDMaxDevices) return false;
    for(int j = (CONTRPS_PID+1);j< CONTRPS_MAX_VALUE;j++){
        config_controller[slot][j][0] = CONTROLLER_PATCHER_INVALIDVALUE;
        config_controller[slot][j][1] = CONTROLLER_PATCHER_INVALIDVALUE;
    }
    return true;
}

int  ConfigParser::getSlotController(std::string identify){
    if(HID_DEBUG) log_printf("Getting Controller Slot\n");

    std::vector<std::string> values = CPStringTools::StringSplit(identify,",");

    if(values.size() != 2){
        log_printf("ConfigParser::getSlotController: You need to provide a VID and PID. e.g. \"[vid=0x451,pid=0x152]\". (%s)\n",identify.c_str());
        return HID_INVALID_SLOT;
    }

    int vid = getValueFromKeyValue(values[0],"VID","=");
    if(vid < 0){
        return HID_INVALID_SLOT;
    }
    int pid = getValueFromKeyValue(values[1],"PID","=");
    if(pid < 0){
        return HID_INVALID_SLOT;
    }
     if(HID_DEBUG) log_printf("VID: %04x PID: %04x\n",vid,pid);

    this->vid = vid;
    this->pid = pid;
    DeviceInfo deviceinfo;
    memset(&deviceinfo,0,sizeof(deviceinfo));
    deviceinfo.vidpid.vid = vid;
    deviceinfo.vidpid.pid = pid;
    int result = ControllerPatcherUtils::getDeviceInfoFromVidPid(&deviceinfo);
    int slot = deviceinfo.slotdata.deviceslot;
    int hid = 0;
    if(result < 0){
         if(HID_DEBUG) log_printf("Its a new controller, lets save it\n");

        HIDSlotData slotdata;
        ControllerPatcherUtils::getNextSlotData(&slotdata);

        slot = slotdata.deviceslot;
        hid = slotdata.hidmask;

        if(slot >= gHIDMaxDevices){
            log_printf("ConfigParser::getSlotController: We don't a space for a new controller, please delete .inis\n");
            return HID_INVALID_SLOT;
        }
        if(HID_DEBUG) log_printf("Got new slot! slot: %d hid %s .. Lets registrate it!\n",slot,CPStringTools::byte_to_binary(hid));
        config_controller[slot][CONTRPS_VID][0] = (vid & 0xFF00) >> 8;
        config_controller[slot][CONTRPS_VID][1] = (vid & 0x00FF);
        config_controller[slot][CONTRPS_PID][0] = (pid & 0xFF00) >> 8;
        config_controller[slot][CONTRPS_PID][1] = (pid & 0x00FF);

        u16 used_vid = config_controller[slot][CONTRPS_VID][0] * 0x100 + config_controller[slot][CONTRPS_VID][1];
        u16 used_pid = config_controller[slot][CONTRPS_PID][0] * 0x100 + config_controller[slot][CONTRPS_PID][1];

        if(HID_DEBUG) log_printf("Saved vid: %04X pid: %04X\n",used_vid,used_pid);

        config_controller_hidmask[slot] =  hid;
        if(HID_DEBUG) log_printf("Saved the hid\n");

    }else{
        if(slot < gHIDMaxDevices){
            hid = config_controller_hidmask[slot];
            if(HID_DEBUG) log_printf(">>>>>> found slot %d (hid:%s). Modifing existing data <<<<<<<<\n",slot,CPStringTools::byte_to_binary(hid));
            if(HID_DEBUG) log_printf("We already have data of this controller, lets modify it\n");
        }else{
            log_printf("ConfigParser::getSlotController: Something really odd happend to the slots. %d is bigger then max (%d)\n",slot,gHIDMaxDevices);
            return HID_INVALID_SLOT;
        }
    }

    if(HID_DEBUG) log_printf("using slot: %d hid %s\n",slot,CPStringTools::byte_to_binary(hid));
    return slot;
}

bool ConfigParser::parseIni(){
    if(getSlot() == HID_INVALID_SLOT){
        log_printf("ConfigParser::parseIni: Couldn't parse file. Not a valid slot. Probably broken config. Or you tried to have more than %d devices\n",getType(),gHIDMaxDevices);
    }

    if(HID_DEBUG) log_printf("Parsing content, type %d\n",getType());

    int start = 1;
    if(contentLines[1].compare("[IGNOREDEFAULT]") == 0){
        resetConfig();
        if(HID_DEBUG) log_printf("Overwriting existing settings of this device\n");
        start++;
    }

    for(u32 i = start; i < contentLines.size(); i++){
        if(HID_DEBUG) log_printf("line %d: \"%s\" \n",(i+1),contentLines[i].c_str());
        parseSingleLine(contentLines[i]);
    }

    if(HID_DEBUG) log_printf("Parsing of the file is done.\n");
    return true;
}

int ConfigParser::getValueFromKeyValue(std::string value_pair,std::string expectedKey,std::string delimiter){
    std::vector<std::string> string_value = CPStringTools::StringSplit(value_pair,delimiter);
    if(string_value.size() != 2){
        if(HID_DEBUG || string_value.size() > 2) log_printf("Not a valid key=pair line %s\n",value_pair.c_str());
        return -1;
    }
    if(string_value[0].compare(expectedKey) != 0){
        log_printf("ConfigParser::getValueFromKeyValue: Key part not %s, its %s",expectedKey.c_str(),string_value[0].c_str());
        return -1;
    }
    char * ptr;
    int value = strtol(string_value[1].c_str(),&ptr,16);

    return value;
}
