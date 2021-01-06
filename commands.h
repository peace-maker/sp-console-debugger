/**
* vim: set ts=4 :
* =============================================================================
* SourceMod Console Debugger Extension
* Copyright (C) 2018-2021 Peace-Maker  All rights reserved.
* =============================================================================
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License, version 3.0, as published by the
* Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*
* As a special exception, AlliedModders LLC gives you permission to link the
* code of this program (as well as its derivative works) to "Half-Life 2," the
* "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
* by the Valve Corporation.  You must obey the GNU General Public License in
* all respects for all other code used.  Additionally, AlliedModders LLC grants
* this exception to all derivative works.  AlliedModders LLC defines further
* exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
* or <http://www.sourcemod.net/license.php>.
*
* Version: $Id$
*/
#ifndef _INCLUDE_DEBUGGER_COMMAND_H
#define _INCLUDE_DEBUGGER_COMMAND_H

#include <string>
#include <vector>

class Debugger;

enum CommandResult {
  CR_StayCommandLoop,
  CR_LeaveCommandLoop
};

class DebuggerCommand {
public:
  DebuggerCommand(Debugger* debugger, std::vector<std::string> names, const std::string description) : debugger_(debugger), names_(names), description_(description), match_start_only_(false) {}
  virtual ~DebuggerCommand() {}
  virtual const std::string GetMatch(const std::string& command);
  virtual CommandResult Accept(const std::string& command, const std::string& params) = 0;
  virtual void ShortHelp();
  virtual bool LongHelp(const std::string& command) {
    return false;
  }

protected:
  Debugger *debugger_;
  std::vector<std::string> names_;
  std::string description_;
  bool match_start_only_;
};

class BacktraceCommand : public DebuggerCommand {
public:
  BacktraceCommand(Debugger* debugger) : DebuggerCommand(debugger, { "backtrace", "bt" }, "display the stack trace") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class BreakpointCommand : public DebuggerCommand {
public:
  BreakpointCommand(Debugger* debugger) : DebuggerCommand(debugger, {"break", "tbreak", "b"}, "set breakpoint at line number or function name") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class ClearBreakpointCommand : public DebuggerCommand {
public:
  ClearBreakpointCommand(Debugger* debugger) : DebuggerCommand(debugger, { "cbreak" }, "remove breakpoint") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class ClearWatchVariableCommand : public DebuggerCommand {
public:
  ClearWatchVariableCommand(Debugger* debugger) : DebuggerCommand(debugger, { "cwatch" }, "remove a \"watchpoint\"") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class ContinueCommand : public DebuggerCommand {
public:
  ContinueCommand(Debugger* debugger) : DebuggerCommand(debugger, { "continue", "c" }, "run program (until breakpoint)") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class ExamineMemoryCommand : public DebuggerCommand {
public:
  ExamineMemoryCommand(Debugger* debugger) : DebuggerCommand(debugger, { "x" }, "eXamine plugin memory: x/FMT ADDRESS") {
    // Allow fuzzy matching of command like "x/32xw".
    match_start_only_ = true;
  }
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class FilesCommand : public DebuggerCommand {
public:
  FilesCommand(Debugger* debugger) : DebuggerCommand(debugger, { "files" }, "list all files that this program is composed off") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class FrameCommand : public DebuggerCommand {
public:
  FrameCommand(Debugger* debugger) : DebuggerCommand(debugger, { "frame", "f" }, "select a frame from the back trace to operate on") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class FunctionsCommand : public DebuggerCommand {
public:
  FunctionsCommand(Debugger* debugger) : DebuggerCommand(debugger, { "funcs", "functions" }, "display functions") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class NextCommand : public DebuggerCommand {
public:
  NextCommand(Debugger* debugger) : DebuggerCommand(debugger, { "next" }, "run until next line, step over functions") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class PositionCommand : public DebuggerCommand {
public:
  PositionCommand(Debugger* debugger) : DebuggerCommand(debugger, { "position" }, "show current file and line") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class PrintVariableCommand : public DebuggerCommand {
public:
  PrintVariableCommand(Debugger* debugger) : DebuggerCommand(debugger, { "print", "p" }, "display the value of a variable, list variables") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class QuitCommand : public DebuggerCommand {
public:
  QuitCommand(Debugger* debugger) : DebuggerCommand(debugger, { "quit", "exit" }, "exit debugger") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class SetVariableCommand : public DebuggerCommand {
public:
  SetVariableCommand(Debugger* debugger) : DebuggerCommand(debugger, { "set" }, "set a variable to a value") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

class StepCommand : public DebuggerCommand {
public:
  StepCommand(Debugger* debugger) : DebuggerCommand(debugger, { "step", "s" }, "single step, step into functions") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
};

class WatchVariableCommand : public DebuggerCommand {
public:
  WatchVariableCommand(Debugger* debugger) : DebuggerCommand(debugger, { "watch" }, "set a \"watchpoint\" on a variable") {}
  virtual CommandResult Accept(const std::string& command, const std::string& params);
  virtual bool LongHelp(const std::string& command);
};

#endif // _INCLUDE_DEBUGGER_COMMAND_H
