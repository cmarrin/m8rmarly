/*-------------------------------------------------------------------------
    This source file is a part of m8rscript
    For the latest info, see http:www.marrin.org/
    Copyright (c) 2018-2019, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Telnet.h"

#include "Value.h"

using namespace m8r;

Telnet::Telnet()
    : _stateMachine({ { { '\x01', '\xff'}, State::Ready } })
{
    _stateMachine.addState(State::Ready, []() { }, 
    {
          { '\x7f', State::Backspace }
        , { { ' ', '\x7e' }, State::AddChar }
        , { '\x03', State::Interrupt }
        , { '\n', State::Ready }
        , { '\r', State::SendLine }
        , { '\e', State::CSI }
        , { '\xff', State::IAC }
    });
    
    _stateMachine.addState(State::Backspace, [this]() { handleBackspace(); }, State::Ready);
    _stateMachine.addState(State::AddChar, [this]() { handleAddChar(); }, State::Ready);
    _stateMachine.addState(State::Interrupt, [this]() { handleInterrupt(); }, State::Ready);
    _stateMachine.addState(State::SendLine, [this]() { handleSendLine(); }, State::Ready);

    // CSI
    _stateMachine.addState(State::CSI, []() { },
    {
         { '[', State::CSIBracket }
    });

    _stateMachine.addState(State::CSIBracket, []() { },
    {
          { { '\x30', '\x3f' }, State::CSIParam }
        , { { '\x40', '\x7e' }, State::CSICommand }
    });

    _stateMachine.addState(State::CSIParam, []() { },
    {
           { { '\x40', '\x7e' }, State::CSICommand }
    });

    _stateMachine.addState(State::CSICommand, [this]() { handleCSICommand(); }, State::Ready);

    // IAC
    _stateMachine.addState(State::IAC, []() { },
    {
         { { '\xf0', '\xfe' }, State::IACVerb }
       , { '\xff', State::AddFF }
    });

    _stateMachine.addState(State::AddFF, [this]() { handleAddFF(); }, State::Ready);

    _stateMachine.addState(State::IACVerb, []() { },
    {
          { { '\x01', '\x7e' }, State::IACCommand }
    });

    _stateMachine.addState(State::IACCommand, [this]() { handleIACCommand(); }, State::Ready);
}

void Telnet::handleBackspace()
{
    if (!_line.empty() && _position > 0) {
        _position--;
        _line.erase(_line.begin() + _position);

        if (_position == _line.size()) {
            // At the end of line. Do the simple thing
            _toChannel = "\x08 \x08";
        } else {
            _toChannel = makeInputLine();
        }
    }    
}

void Telnet::handleAddChar()
{
    if (_position == _line.size()) {
        // At end of line, do the simple thing
        _line.push_back(_currentChar);
        _position++;
        _toChannel = _currentChar;
    } else {
        _line.insert(_line.begin() + _position, _currentChar);
        _position++;
        makeInputLine();
    }
}

void Telnet::handleAddFF()
{
    _line.push_back('\xff');
}

void Telnet::handleInterrupt()
{
    _toClient = '\x03';
}

void Telnet::handleCSICommand()
{
    // TODO: Handle Params, etc.
    switch(_currentChar) {
        case 'D':
            if (_position > 0) {
                _position--;
                _toChannel = "\e[D";
            }
            break;
    }
}

void Telnet::handleIACCommand()
{
    // TODO: Implement whatever is needed
}

void Telnet::handleSendLine()
{
    _toClient = String::join(_line);
    _line.clear();
    _position = 0;
    _toChannel = "\r\n";
}

Telnet::Action Telnet::receive(char fromChannel, String& toChannel, String& toClient)
{
    _currentChar = fromChannel;
    _stateMachine.sendInput(_currentChar);
    
    std::swap(toChannel, _toChannel);
    std::swap(toClient, _toClient);
    return Action::None;
}

String Telnet::makeInputLine()
{
    String s = "\e[1000D\e[0K";
    s += String::join(_line);
    s += "\e[1000D";
    if (_position) {
        s += "\e[";
        
        // TODO: Need to move all this string processing stuff to String class
        s += Value::toString(_position);
        s += "C";
    }
    return s;
}
