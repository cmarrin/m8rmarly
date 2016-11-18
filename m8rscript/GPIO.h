/*-------------------------------------------------------------------------
This source file is a part of m8rscript

For the latest info, see http://www.marrin.org/

Copyright (c) 2016, Chris Marrin
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.
    
    - Redistributions in binary form must reproduce the above copyright 
    notice, this list of conditions and the following disclaimer in the 
    documentation and/or other materials provided with the distribution.
    
    - Neither the name of the <ORGANIZATION> nor the names of its 
    contributors may be used to endorse or promote products derived from 
    this software without specific prior written permission.
    
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.
-------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <functional>

namespace m8r {

class GPIO {
public:
    static constexpr uint8_t LED = 2;
    static constexpr uint8_t PinCount = 16;
    
    enum class PinMode { Output, OpenDrain, Input, Interrupt };
    enum class Trigger { None, RisingEdge, FallingEdge, BothEdges, Low, High };
    
    GPIO()
    {
    }
    virtual ~GPIO() { }
    
    virtual bool pinMode(uint8_t pin, PinMode mode, bool pullup = false)
    {
        if (pin > 16) {
            return false;
        }
        _pinState[pin] = { mode, pullup };
        return true;
    }
    
    virtual bool digitalRead(uint8_t pin) const = 0;
    virtual void digitalWrite(uint8_t pin, bool level) = 0;
    virtual void onInterrupt(uint8_t pin, Trigger, std::function<void(uint8_t pin)> = { }) = 0;
    
    void enableHeartbeat() { pinMode(LED, PinMode::Output); }
    void heartbeat(bool on)
    {
        if (_pinState[LED]._mode != PinMode::Output) {
            return;
        }
        
        // Generally the heartbeat is the inverse of the current state of the LED pin. But when turning
        // it off (which will be for a longer period of time) if the pin has changed state from when
        // we turned it on, we assume it is being used somewhere else, so we don't change it
        bool state = digitalRead(LED);
        if ((!on && (state ^ _heartbeatState)) || (on == _heartbeatState)) {
            _heartbeatState = on;
            return;
        }
        _heartbeatState = !state;
        digitalWrite(LED, _heartbeatState);
    }
    
private:
    struct PinState
    {
        PinState() { }
        PinState(PinMode mode, bool pullup) : _mode(mode), _pullup(pullup) { }
        PinMode _mode = PinMode::Input;
        bool _pullup = false;
    };
    PinState _pinState[PinCount];
    bool _heartbeatState = false;
};

}