/*
 * Copyright (C) 2020 Hendrik van Essen
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "Arduino.h"

#include "LiquidCrystalWired.h"

/******************************* PUBLIC METHODS *******************************/
LiquidCrystalWired::LiquidCrystalWired(
        uint8_t rowCount, uint8_t colCount, FontSize fontSize, BitMode bitMode) {

    _rowCount = rowCount;
    _colCount = colCount;
    _fontSize = fontSize;
    _bitMode = bitMode;
}

void LiquidCrystalWired::begin(uint8_t deviceAddress, TwoWire *wire) {

    uint8_t _functionSet = 0;

    _deviceAddress = deviceAddress;

    _wire = wire;
    _wire->begin();

    // configure bit mode
    if (_bitMode == BITMODE_8_BIT) {
        _functionSet |= (1 << BIT_FUNCTION_SET_BITMODE);
    }

    // configure line count
    if (_rowCount >= 2) {
        _functionSet |= (1 << BIT_FUNCTION_SET_LINECOUNT);
    }

    // configure character size
    if (_fontSize == FONT_SIZE_5x10) {
        _functionSet |= (1 << BIT_FUNCTION_SET_FONTSIZE);
    }

    // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    // according to datasheet, we need at least 40ms after power rises above 2.7V
    // before sending commands. Arduino can turn on way before 4.5V so we'll wait 50 ms
    delay(50);

    // this is according to the hitachi HD44780 datasheet
    // page 45 figure 23

    // Send function set command sequence
    command(CMD_FUNCTION_SET | _functionSet);
    delay(5);

    // second try
    command(CMD_FUNCTION_SET | _functionSet);
    delayMicroseconds(500);

    // third go
    command(CMD_FUNCTION_SET | _functionSet);

    turnOn();
    clear();
    setTextInsertionMode(LEFT_TO_RIGHT);
}

void LiquidCrystalWired::turnOn() {

    _currDisplayControl |= (1 << BIT_DISPLAY_CONTROL_DISPLAY);
    command(CMD_DISPLAY_CONTROL | _currDisplayControl);
}

void LiquidCrystalWired::turnOff() {

    _currDisplayControl &= ~(1 << BIT_DISPLAY_CONTROL_DISPLAY);
    command(CMD_DISPLAY_CONTROL | _currDisplayControl);
}

void LiquidCrystalWired::clear() {

    command(CMD_CLEAR_DISPLAY);

    // max execution time of 1.52 ms
    delayMicroseconds(1700);
}

void LiquidCrystalWired::returnHome() {

    command(CMD_RETURN_HOME);

    // max execution time of 1.52 ms
    delayMicroseconds(1700);
}

void LiquidCrystalWired::setAutoScrollEnabled(bool enabled) {

    if (enabled) {
        _currEntryModeSet |= (1 << BIT_ENTRY_MODE_AUTOINCREMENT);
    }
    else {
        _currEntryModeSet &= ~(1 << BIT_ENTRY_MODE_AUTOINCREMENT);
    }

    command(CMD_ENTRY_MODE_SET | _currEntryModeSet);
}

void LiquidCrystalWired::setCursorBlinkingEnabled(bool enabled) {

    if (enabled) {
        _currDisplayControl |= (1 << BIT_DISPLAY_CONTROL_CURSOR_BLINKING);
    }
    else {
        _currDisplayControl &= ~(1 << BIT_DISPLAY_CONTROL_CURSOR_BLINKING);
    }

    command(CMD_DISPLAY_CONTROL | _currDisplayControl);
}

void LiquidCrystalWired::setCursorVisible(bool visible) {

    if (visible) {
        _currDisplayControl |= (1 << BIT_DISPLAY_CONTROL_CURSOR);
    }
    else {
        _currDisplayControl &= ~(1 << BIT_DISPLAY_CONTROL_CURSOR);
    }

    command(CMD_DISPLAY_CONTROL | _currDisplayControl);
}

void LiquidCrystalWired::setCursorPosition(uint8_t row, uint8_t col) {

    col = ((row == 0) ? (col | 0x80) : (col | 0xc0));
    uint8_t data[3] = { 0x80, col };

    deviceWrite(data, 2);
}

void LiquidCrystalWired::setTextInsertionMode(TextInsertionMode mode) {

    if (mode == RIGHT_TO_LEFT) {
        _currEntryModeSet &= ~(1 << BIT_ENTRY_MODE_INCREMENT);
    }
    else {
        _currEntryModeSet |= (1 << BIT_ENTRY_MODE_INCREMENT);
    }

    command(CMD_ENTRY_MODE_SET | _currEntryModeSet);
}

void LiquidCrystalWired::moveCursorLeft() {

    uint8_t cmd = CMD_CURSOR_DISPLAY_SHIFT;
    cmd &= ~(1 << BIT_CURSOR_DISPLAY_SHIFT_DIRECTION);

    command(cmd);
}

void LiquidCrystalWired::moveCursorRight() {

    uint8_t cmd = CMD_CURSOR_DISPLAY_SHIFT;
    cmd |= (1 << BIT_CURSOR_DISPLAY_SHIFT_DIRECTION);

    command(cmd);
}

void LiquidCrystalWired::scrollDisplayLeft() {

    uint8_t cmd = CMD_CURSOR_DISPLAY_SHIFT;
    cmd |= (1 << BIT_CURSOR_DISPLAY_SHIFT_SELECTION);
    cmd &= ~(1 << BIT_CURSOR_DISPLAY_SHIFT_DIRECTION);

    command(cmd);
}

void LiquidCrystalWired::scrollDisplayRight() {

    uint8_t cmd = CMD_CURSOR_DISPLAY_SHIFT;
    cmd |= (1 << BIT_CURSOR_DISPLAY_SHIFT_SELECTION);
    cmd |= (1 << BIT_CURSOR_DISPLAY_SHIFT_DIRECTION);

    command(cmd);
}

void LiquidCrystalWired::setCustomSymbol(
        CustomSymbol customSymbol, uint8_t charmap[]) {

    uint8_t location = customSymbol;
    command(CMD_SET_CGRAM_ADDR | (location << 3));

    uint8_t data[9];
    data[0] = 0x40;
    for (int i = 1; i < 9; i++) {
        data[i] = charmap[i - 1];
    }
    deviceWrite(data, 9);
}

void LiquidCrystalWired::printCustomSymbol(CustomSymbol customSymbol) {
    write((byte) customSymbol);
}

void LiquidCrystalWired::setProgressBarEnabled(bool enabled) {

    _progressBarEnabled = enabled;

    if (enabled) {
        initProgressBar(_rowCount - 1);
    }
}

void LiquidCrystalWired::setProgressBarRow(uint8_t row) {
    _progressBarRow = row;
}

void LiquidCrystalWired::setProgress(float progress) {

    if (!_progressBarEnabled) {
        return;
    }

    int barCount = _colCount * 5;

    if (progress < 0) {
        progress = 0;
    }
    else if (progress > 100) {
        progress = 100;
    }

    int progressBarCount = barCount * (progress / 100);

    int fullBarCount = progressBarCount / 5;
    int remainderBarCount = progressBarCount % 5;

    setCursorPosition(_progressBarRow, 0);

    for (int i = 0; i < fullBarCount; i++) {
        printCustomSymbol(CUSTOM_SYMBOL_8);
    }

    uint8_t blankCount = _colCount - fullBarCount;

    switch (remainderBarCount) {

        case 1:
            printCustomSymbol(CUSTOM_SYMBOL_4);
            blankCount--;
            break;

        case 2:
            printCustomSymbol(CUSTOM_SYMBOL_5);
            blankCount--;
            break;

        case 3:
            printCustomSymbol(CUSTOM_SYMBOL_6);
            blankCount--;
            break;

        case 4:
            printCustomSymbol(CUSTOM_SYMBOL_7);
            blankCount--;
            break;
    }

    // clear the rest of the line, so it appears as empty part of the progressbar
    for (int i = 0; i < blankCount; i++) {
        print(" ");
    }
}

inline size_t LiquidCrystalWired::write(uint8_t value) {

    uint8_t data[3] = { 0x40, value };
    deviceWrite(data, 2);

    // assume success
    return 1;
}

/****************************** PRIVATE METHODS *******************************/
inline void LiquidCrystalWired::command(uint8_t value) {
    uint8_t data[3] = { 0x80, value };
    deviceWrite(data, 2);

    // max execution time for most commands of 37 µs
    delayMicroseconds(50);
}

void LiquidCrystalWired::deviceWrite(uint8_t *data, uint8_t len) {

    _wire->beginTransmission(_deviceAddress);

    for (int i = 0; i < len; i++) {
        _wire->write(data[i]);
        delay(5);
    }

    _wire->endTransmission();
}

void LiquidCrystalWired::initProgressBar(uint8_t row) {

    _progressBarRow = row;

    // if autoscroll was used, the progress bar would be displayed incorrectly
    setAutoScrollEnabled(false);

    // undo any scrolling
    returnHome();

    // progress bar should increase from left to right
    setTextInsertionMode(LEFT_TO_RIGHT);

    setCustomSymbol(CUSTOM_SYMBOL_4, customCharProgressBar1);
    setCustomSymbol(CUSTOM_SYMBOL_5, customCharProgressBar2);
    setCustomSymbol(CUSTOM_SYMBOL_6, customCharProgressBar3);
    setCustomSymbol(CUSTOM_SYMBOL_7, customCharProgressBar4);
    setCustomSymbol(CUSTOM_SYMBOL_8, customCharProgressBar5);
}
