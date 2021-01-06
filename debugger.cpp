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
#include "debugger.h"
#include "commands.h"
#include "breakpoints.h"
#include "symbols.h"
#include <amtl/am-string.h>
#include <ctype.h>
#include <iterator>
#include <iostream>
#include <sstream>

using namespace SourcePawn;

Debugger::Debugger(IPluginContext *context)
  : context_(context),
  runmode_(RUNNING),
  lastfrm_(0),
  lastline_(-1),
  currentfile_(nullptr),
  currentfunction_(nullptr),
  is_breakpoint_(false),
  active_(false),
  breakpoints_(this),
  symbols_(this)
{
  commands_.push_back(std::make_shared<BacktraceCommand>(this));
  commands_.push_back(std::make_shared<BreakpointCommand>(this));
  commands_.push_back(std::make_shared<ClearBreakpointCommand>(this));
  commands_.push_back(std::make_shared<ClearWatchVariableCommand>(this));
  commands_.push_back(std::make_shared<ContinueCommand>(this));
  commands_.push_back(std::make_shared<FilesCommand>(this));
  commands_.push_back(std::make_shared<FrameCommand>(this));
  commands_.push_back(std::make_shared<FunctionsCommand>(this));
  commands_.push_back(std::make_shared<NextCommand>(this));
  commands_.push_back(std::make_shared<PositionCommand>(this));
  commands_.push_back(std::make_shared<PrintVariableCommand>(this));
  commands_.push_back(std::make_shared<QuitCommand>(this));
  commands_.push_back(std::make_shared<SetVariableCommand>(this));
  commands_.push_back(std::make_shared<StepCommand>(this));
  commands_.push_back(std::make_shared<ExamineMemoryCommand>(this));
  commands_.push_back(std::make_shared<WatchVariableCommand>(this));
}

bool
Debugger::Initialize()
{
  if (!breakpoints_.Initialize())
    return false;

  if (!symbols_.Initialize())
    return false;

  return true;
}

void
Debugger::Activate()
{
  active_ = true;
}

void
Debugger::Deactivate()
{
  active_ = false;

  breakpoints_.ClearAllBreakpoints();
  symbols_.ClearAllWatches();
  SetRunmode(RUNNING);
}

SourcePawn::IPluginDebugInfo*
Debugger::GetDebugInfo() const
{
  return selected_context_->GetRuntime()->GetDebugInfo();
}

void
Debugger::HandleInput(cell_t cip, cell_t frm, bool isBp)
{
  // Remember which command was executed last,
  // so you don't have to type it again if you
  // want to repeat it.
  static std::string lastcommand = "";

  // Reset the state.
  IFrameIterator *frames = context_->CreateFrameIterator();
  frame_count_ = 0;
  selected_frame_ = 0;
  cip_ = cip;
  frm_ = frm;
  selected_context_ = context_;
  is_breakpoint_ = isBp;

  // Count the frames
  // Select first scripted frame, if it's not frame 0
  bool selected_first_scripted = false;
  for (; !frames->Done(); frames->Next(), frame_count_++) {
    if (frames->IsInternalFrame())
      continue;

    if (!selected_first_scripted && frames->IsScriptedFrame()) {
      selected_frame_ = frame_count_;
      selected_first_scripted = true;
    }
  }

  context_->DestroyFrameIterator(frames);

  // Show where we've stopped.
  PrintCurrentPosition();

  // Print all watched variables now.
  symbols_.ListWatches();

  std::string line, command, params;
  for (;;) {
    // Show a debugger prompt.
    std::cout << "dbg> ";

    // Read debugger command.
    if (!std::getline(std::cin, line)) {
      // Ctrl+C? Not sure what's the best behavior here.
      SetRunmode(RUNNING);
      std::cout << std::endl;
      break;
    }
    line = trimString(line);

    // Repeat the last command, if no new command was given.
    if (line.empty()) {
      line = lastcommand;
    }
    lastcommand.clear();

    // Split the line into "<command> <params...>"
    size_t pos = line.find_first_of(" ");
    if (pos == std::string::npos) {
      command = line;
      params = "";
    }
    else {
      // Extract the first word from the string.
      command = line.substr(0, pos);
      // Optional params start after the command.
      params = line.substr(pos + 1);
      params = trimString(params);
    }
    
    if (command.empty()) {
      ListCommands("");
      continue;
    }

    // Handle inbuilt help command.
    if (!stricmp(command.c_str(), "?") || !stricmp(command.c_str(), "help")) {
      ListCommands(params);
      continue;
    }

    std::shared_ptr<DebuggerCommand> matched_cmd = ResolveCommandString(command);
    if (!matched_cmd) {
      continue;
    }
    
    lastcommand = line;
    if (matched_cmd->Accept(command, params) == CR_LeaveCommandLoop) {
      return;
    }
    
    /*
    // Change display format of symbol
    else if (!stricmp(command, "type")) {
      HandleDisplayFormatChangeCmd(params);
    }*/
  }
}

std::shared_ptr<DebuggerCommand>
Debugger::ResolveCommandString(const std::string command)
{
  if (command.empty()) {
    return nullptr;
  }

  std::vector<std::shared_ptr<DebuggerCommand>> matched_cmds;
  std::string match;
  for (auto& cmd : commands_) {
    match = cmd->GetMatch(command);
    if (match.empty())
      continue;

    // Early exit for complete matches.
    if (match.size() == command.size())
      return cmd;

    matched_cmds.push_back(cmd);
  }

  if (matched_cmds.empty()) {
    std::cout << "\tInvalid command \"" << command << "\", use \"?\" to view all commands\n";
    return nullptr;
  }

  if (matched_cmds.size() > 1) {
    std::cout << "\tAmbiguous command \"" << command << "\", need more characters\n\t";
    for (auto& cmd : matched_cmds) {
      std::cout << "\"" << cmd->GetMatch(command) << "\", ";
    }
    std::cout << "\n";
    return nullptr;
  }
  return matched_cmds[0];
}

void
Debugger::ListCommands(const std::string command)
{
  if (command.empty() || command == "?" || !stricmp(command.c_str(), "help")) {
    std::cout << "At the prompt, you can type debug commands. For example, the word \"step\" is a\n"
      "command to execute a single line in the source code. The commands that you will\n"
      "use most frequently may be abbreviated to a single letter: instead of the full\n"
      "word \"step\", you can also type the letter \"s\" followed by the enter key.\n\n"
      "Available commands:\n";

    // Print a short description for every available command.
    for (auto& cmd : commands_) {
      cmd->ShortHelp();
    }
    std::cout << "\n\tUse \"? <command name>\" to view more information on a command\n";
    return;
  }

  std::cout << "Options for command \"" << command << "\":\n";

  std::shared_ptr<DebuggerCommand> matched_cmd = ResolveCommandString(command);
  // Ambiguous or invalid command. ResolveCommandString printed an error.
  if (!matched_cmd)
    return;

  if (!matched_cmd->LongHelp(command)) {
    std::cout << "\tno additional information\n";
  }

  /*if (!stricmp(command, "type")) {
    printf("\tTYPE var STRING\t\tdisplay \"var\" as string\n"
      "\tTYPE var STD\t\tset default display format (decimal integer)\n"
      "\tTYPE var HEX\t\tset hexadecimal integer format\n"
      "\tTYPE var FLOAT\t\tset floating point format\n");
  }
  else {
    printf(
      "\tTYPE\t\tset the \"display type\" of a symbol\n"
      "\n\tUse \"? <command name>\" to view more information on a command\n");
  }*/
}

void
Debugger::PrintCurrentPosition()
{
  if (!is_breakpoint_)
    printf("STOP");
  else
    printf("BREAK");

  // Print file, function, line and selected frame.
  printf(" at line %d", lastline_);

  if (currentfile_)
    printf(" in %s", SkipPath(currentfile_));

  if (currentfunction_)
    printf(" in %s", currentfunction_);

  if (selected_frame_ > 0)
    printf("\tframe: %d", selected_frame_);

  fputs("\n", stdout);
}

void
Debugger::DumpStack()
{
  IFrameIterator *frames = context_->CreateFrameIterator();

  uint32_t index = 0;
  for (; !frames->Done(); frames->Next(), index++) {

    if (frames->IsInternalFrame())
      continue;

    if (index == selected_frame_) {
      fputs("->", stdout);
    }
    else {
      fputs("  ", stdout);
    }

    const char *name = frames->FunctionName();
    if (!name) {
      name = "<unknown function>";
    }

    if (frames->IsNativeFrame()) {
      fprintf(stdout, "[%d] %s\n", index, name);
      continue;
    }

    if (frames->IsScriptedFrame()) {
      const char *file = frames->FilePath();
      if (!file)
        file = "<unknown>";
      fprintf(stdout, "[%d] Line %d, %s::%s\n", index, frames->LineNumber(), SkipPath(file), name);
    }
  }
  context_->DestroyFrameIterator(frames);
}

std::string
Debugger::FindFileByPartialName(const std::string partialname)
{
  // the user may have given a partial filename (e.g. without a path), so
  // walk through all files to find a match.
  IPluginDebugInfo *debuginfo = context_->GetRuntime()->GetDebugInfo();
  size_t len = partialname.size();
  size_t offs;
  const char *filename;
  for (uint32_t i = 0; i < debuginfo->NumFiles(); i++) {
    filename = debuginfo->GetFileName(i);
    offs = strlen(filename) - len;
    if (offs >= 0 &&
      !strncmp(filename + offs, partialname.c_str(), len))
    {
      return filename;
    }
  }
  return nullptr;
}
