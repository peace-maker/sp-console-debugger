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
#include "commands.h"
#include "debugger.h"
#include <iostream>

using namespace SourcePawn;

const std::string
DebuggerCommand::GetMatch(const std::string command) {
  // Check if any of the command names start with the given command input.
  std::string best_match;
  for (auto& name : names_) {
    // Find the shortest alias that's matching the input.
    if (name.rfind(command) == 0 && (best_match.empty() || best_match.size() > name.size()))
      best_match = name;
  }
  return best_match;
}

void
DebuggerCommand::ShortHelp() {
  // TODO: Align description at consistent offset.
  std::cout << "\t" << names_[0] << "\t" << description_ << std::endl;
}

CommandResult
BacktraceCommand::Accept(const std::string command, std::vector<std::string> params) {
  fputs("Stack trace:", stdout);
  debugger_->DumpStack();
  return CR_StayCommandLoop;
}

CommandResult
BreakpointCommand::Accept(const std::string command, std::vector<std::string> params) {
  if (params.empty()) {
    debugger_->breakpoints().ListBreakpoints();
    return CR_StayCommandLoop;
  }
  
  std::string filename = debugger_->currentfile();
  std::string breakpoint_location = debugger_->breakpoints().ParseBreakpointLine(params[0], &filename);
  if (breakpoint_location.empty())
    return CR_StayCommandLoop;

  bool isTemporary = !command.rfind("tb", 0);

  Breakpoint *bp;
  // User specified a line number
  if (isdigit(breakpoint_location[0])) {
    bp = debugger_->breakpoints().AddBreakpoint(filename, strtol(breakpoint_location.c_str(), NULL, 10) - 1, isTemporary);
  }
  // User wants to add a breakpoint at the current location
  else if (breakpoint_location[0] == '.') {
    bp = debugger_->breakpoints().AddBreakpoint(filename, debugger_->lastline() - 1, isTemporary);
  }
  // User specified a function name
  else {
    bp = debugger_->breakpoints().AddBreakpoint(filename, breakpoint_location, isTemporary);
  }

  if (bp == nullptr) {
    fputs("Invalid breakpoint\n", stdout);
    return CR_StayCommandLoop;
  }
  
  uint32_t bpline = 0;
  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  debuginfo->LookupLine(bp->addr(), &bpline);
  printf("Set breakpoint %zu in file %s on line %d", debugger_->breakpoints().GetBreakpointCount(), SkipPath(filename.c_str()), bpline);
  if (bp->name() != nullptr)
    printf(" in function %s", bp->name());
  fputs("\n", stdout);
  return CR_StayCommandLoop;
}

bool
BreakpointCommand::LongHelp(const std::string command) {
  std::cout << "\tUse TBREAK for one-time breakpoints (may be abbreviated to TB)\n"
    "\tBREAK may be abbreviated to B\n\n"
    "\tBREAK\t\tlist all breakpoints\n"
    "\tBREAK n\t\tset a breakpoint at line \"n\"\n"
    "\tBREAK name:n\tset a breakpoint in file \"name\" at line \"n\"\n"
    "\tBREAK func\tset a breakpoint at function with name \"func\"\n"
    "\tBREAK .\t\tset a breakpoint at the current location\n";
  return true;
}

CommandResult
ClearBreakpointCommand::Accept(const std::string command, std::vector<std::string> params) {
  if (params.empty()) {
    std::cout << "\tInvalid syntax. Type \"? cbreak\" for help.\n";
    return CR_StayCommandLoop;
  }

  if (params[0] == "*") {
    size_t num_bps = debugger_->breakpoints().GetBreakpointCount();
    debugger_->breakpoints().ClearAllBreakpoints();
    std::cout << "\tCleared all " << num_bps << " breakpoints.\n";
  }
  else {
    int number = debugger_->breakpoints().FindBreakpoint(params[0]);
    if (number < 0 || !debugger_->breakpoints().ClearBreakpoint(number))
      std::cout << "\tUnknown breakpoint (or wrong syntax)\n";
    else
      std::cout << "\tCleared breakpoint " << number << ".\n";
  }
  return CR_StayCommandLoop;
}

bool
ClearBreakpointCommand::LongHelp(const std::string command) {
  std::cout << "\tCBREAK may be abbreviated to CB\n\n"
    "\tCBREAK n\tremove breakpoint number \"n\"\n"
    "\tCBREAK *\tremove all breakpoints\n";
  return true;
}

CommandResult
ContinueCommand::Accept(const std::string command, std::vector<std::string> params) {
  if (!params.empty()) {
    // "continue func" runs until the function returns.
    if (!stricmp(params[0].c_str(), "func")) {
      debugger_->SetRunmode(STEPOUT);
      return CR_LeaveCommandLoop;
    }

    // There is a parameter given -> run until that line!
    std::string filename = debugger_->currentfile();
    // ParseBreakpointLine prints an error.
    std::string breakpoint_location = debugger_->breakpoints().ParseBreakpointLine(params[0], &filename);
    if (breakpoint_location.empty())
      return CR_StayCommandLoop;

    Breakpoint *bp = nullptr;
    // User specified a line number. Add a breakpoint.
    if (isdigit(breakpoint_location[0])) {
      bp = debugger_->breakpoints().AddBreakpoint(filename, strtol(breakpoint_location.c_str(), NULL, 10) - 1, true);
    }

    if (bp == nullptr) {
      fputs("Invalid format or bad breakpoint address. Type \"? continue\" for help.\n", stdout);
      return CR_StayCommandLoop;
    }

    uint32_t bpline = 0;
    IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
    debuginfo->LookupLine(bp->addr(), &bpline);
    printf("Running until line %d in file %s.\n", bpline, SkipPath(filename.c_str()));
  }

  debugger_->SetRunmode(RUNNING);
  // Return true, to break out of the debugger shell 
  // and continue execution of the plugin.
  return CR_LeaveCommandLoop;
}

bool
ContinueCommand::LongHelp(const std::string command) {
  std::cout << "\tCONTINUE may be abbreviated to C\n\n"
    "\tCONTINUE\t\trun until the next breakpoint or program termination\n"
    "\tCONTINUE n\t\trun until line number \"n\"\n"
    "\tCONTINUE name:n\trun until line number \"n\" in file \"name\"\n"
    "\tCONTINUE func\t\trun until the current function returns (\"step out\")\n";
  return true;
}

CommandResult
FilesCommand::Accept(const std::string command, std::vector<std::string> params) {
  fputs("Source files:\n", stdout);
  // Browse through the file table
  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  for (unsigned int i = 0; i < debuginfo->NumFiles(); i++) {
    if (debuginfo->GetFileName(i) != nullptr) {
      printf("%s\n", debuginfo->GetFileName(i));
    }
  }
  return CR_StayCommandLoop;
}

CommandResult
FunctionsCommand::Accept(const std::string command, std::vector<std::string> params) {
  fputs("Listing functions:\n", stdout);

  // Run through all functions with a name and 
  // print it including the filename where it's defined.
  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  const char *functionname;
  const char *filename;
  for (size_t i = 0; i < debuginfo->NumFunctions(); i++) {
    functionname = debuginfo->GetFunctionName(i, &filename);
    if (functionname != nullptr)
      printf("%s", functionname);
    if (filename != nullptr) {
      printf("\t(%s)", SkipPath(filename));
    }
    fputs("\n", stdout);
  }
  return CR_StayCommandLoop;
}

CommandResult
NextCommand::Accept(const std::string command, std::vector<std::string> params) {
  debugger_->SetRunmode(STEPOVER);
  return CR_LeaveCommandLoop;
}

CommandResult
PositionCommand::Accept(const std::string command, std::vector<std::string> params) {
  debugger_->PrintCurrentPosition();
  return CR_StayCommandLoop;
}

CommandResult
QuitCommand::Accept(const std::string command, std::vector<std::string> params) {
  fputs("Clearing all breakpoints. Running normally.\n", stdout);
  debugger_->Deactivate();
  return CR_LeaveCommandLoop;
}

CommandResult
StepCommand::Accept(const std::string command, std::vector<std::string> params) {
  debugger_->SetRunmode(STEPPING);
  return CR_LeaveCommandLoop;
}
