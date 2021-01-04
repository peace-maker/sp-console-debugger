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
  active_(false),
  is_breakpoint_(false),
  breakpoints_(this)
{
  commands_.push_back(std::make_shared<BacktraceCommand>(this));
  commands_.push_back(std::make_shared<BreakpointCommand>(this));
  commands_.push_back(std::make_shared<ClearBreakpointCommand>(this));
  commands_.push_back(std::make_shared<ContinueCommand>(this));
  commands_.push_back(std::make_shared<FilesCommand>(this));
  commands_.push_back(std::make_shared<FunctionsCommand>(this));
  commands_.push_back(std::make_shared<NextCommand>(this));
  commands_.push_back(std::make_shared<PositionCommand>(this));
  commands_.push_back(std::make_shared<QuitCommand>(this));
  commands_.push_back(std::make_shared<StepCommand>(this));
}

bool
Debugger::Initialize()
{
  if (!breakpoints_.Initialize())
    return false;

  if (!watch_table_.init())
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
  ClearAllWatches();
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
    if (!selected_first_scripted && frames->IsScriptedFrame()) {
      selected_frame_ = frame_count_;
      selected_first_scripted = true;
    }
  }

  context_->DestroyFrameIterator(frames);

  // Show where we've stopped.
  PrintCurrentPosition();

  // Print all watched variables now.
  ListWatches();

  std::string line, command;
  std::vector<std::string> params;
  for (;;) {
    // Show a debugger prompt.
    std::cout << "dbg> ";

    // Read debugger command.
    std::getline(std::cin, line);

    // Repeat the last command, if no new command was given.
    if (line.empty()) {
      line = lastcommand;
    }
    lastcommand.clear();

    // Extract the first word from the string.
    std::istringstream iss(line);
    iss >> command;
    if (command.empty()) {
      ListCommands("");
      continue;
    }

    // Optional params start after the command.
    params = std::vector<std::string>(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>());

    // Handle inbuilt help command.
    if (!stricmp(command.c_str(), "?") || !stricmp(command.c_str(), "help")) {
      ListCommands(!params.empty() ? params[0] : "");
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
    
#if 0
    /*else if (!stricmp(command, "f") || !stricmp(command, "frame")) {
      HandleFrameCmd(params);
    }*/
    else if (!stricmp(command, "print") || !stricmp(command, "p")) {
      HandleVariableDisplayCmd(params);
    }
    else if (!stricmp(command, "set")) {
      HandleSetVariableCmd(params);
    }
    // Change display format of symbol
    /*else if (!stricmp(command, "type")) {
      HandleDisplayFormatChangeCmd(params);
    }*/
    else if (!stricmp(command, "w") || !stricmp(command, "watch")) {
      HandleWatchCmd(params);
    }
    else if (!stricmp(command, "cw") || !stricmp(command, "cwatch")) {
      HandleClearWatchCmd(params);
    }
    else if (!stricmp(command, "x") || !stricmp(command, "examine")) {
      HandleDumpMemoryCmd(command, params);
    }
    else {
      printf("\tInvalid command \"%s\", use \"?\" to view all commands\n", command);
    }
#endif
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

#if 0
  if (!stricmp(command, "cw") || !stricmp(command, "cwatch")) {
    printf("\tCWATCH may be abbreviated to CW\n\n"
      "\tCWATCH n\tremove watch number \"n\"\n"
      "\tCWATCH var\tremove watch from \"var\"\n"
      "\tCWATCH *\tremove all watches\n");
  }
  else if (!stricmp(command, "p") || !stricmp(command, "print")) {
    printf("\tPRINT may be abbreviated to P\n\n"
      "\tPRINT\t\tdisplay all local variables that are currently in scope\n"
      "\tPRINT *\tdisplay all variables that are currently in scope including global variables\n"
      "\tPRINT var\tdisplay the value of variable \"var\"\n"
      "\tPRINT var[i]\tdisplay the value of array element \"var[i]\"\n");
  }
  /*else if (!stricmp(command, "f") || !stricmp(command, "frame")) {
    printf("\tFRAME may be abbreviated to F\n\n");
    printf("\tFRAME n\tselect frame n and show/change local variables in that function\n");
  }*/
  else if (!stricmp(command, "set")) {
    printf("\tSET var=value\t\tset variable \"var\" to the numeric value \"value\"\n"
      "\tSET var[i]=value\tset array item \"var\" to a numeric value\n"
      "\tSET var=\"value\"\t\tset string variable \"var\" to string \"value\"\n");
  }
  /*else if (!stricmp(command, "type")) {
    printf("\tTYPE var STRING\t\tdisplay \"var\" as string\n"
      "\tTYPE var STD\t\tset default display format (decimal integer)\n"
      "\tTYPE var HEX\t\tset hexadecimal integer format\n"
      "\tTYPE var FLOAT\t\tset floating point format\n");
  }*/
  else if (!stricmp(command, "watch") || !stricmp(command, "w")) {
    printf("\tWATCH may be abbreviated to W\n\n"
      "\tWATCH var\tset a new watch at variable \"var\"\n");
  }
  else if (!stricmp(command, "examine") || !stricmp(command, "x")) {
    printf("\tX/FMT ADDRESS\texamine plugin memory at \"ADDRESS\"\n"
      "\tADDRESS is an expression for the memory address to examine.\n"
      "\tFMT is a repeat count followed by a format letter and a size letter.\n"
      "\t\tFormat letters are o(octal), x(hex), d(decimal), u(unsigned decimal),\n"
      "\t\t\tf(float), c(char) and s(string).\n"
      "\t\tSize letters are b(byte), h(halfword), w(word).\n\n"
      "\t\tThe specified number of objects of the specified size are printed\n"
      "\t\taccording to the format.\n\n"
      //"\tDefaults for format and size letters are those previously used.\n"
      //"\tDefault count is 1.  Default address is following last thing printed\n"
      //"\twith this command or \"disp\".\n"
    );
  }
  else {
    printf("\tCW(atch)\tremove a \"watchpoint\"\n"
      "\tP(rint)\t\tdisplay the value of a variable, list variables\n"
      //"\tF(rame)\t\tSelect a frame from the back trace to operate on\n"
      "\tSET\t\tset a variable to a value\n"
      //"\tTYPE\t\tset the \"display type\" of a symbol\n"
      "\tW(atch)\t\tset a \"watchpoint\" on a variable\n"
      "\tX\t\teXamine plugin memory: x/FMT ADDRESS\n"
      "\n\tUse \"? <command name>\" to view more information on a command\n");
  }
#endif
}

void
Debugger::HandleVariableDisplayCmd(char *params)
{
  uint32_t idx[sDIMEN_MAX];
  memset(idx, 0, sizeof(idx));
  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();

  IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(cip_);
  if (*params == '\0' || *params == '*') {
    // Display all variables that are in scope
    while (!symbol_iterator->Done()) {
      const IDebugSymbol* sym = symbol_iterator->Next();

      // Skip global variables in this list.
      if (*params != '*' && sym->scope() == Global)
        continue;

      // Print the name and address
      printf("%s\t<%#8x>\t", ScopeToString(sym->scope()), (sym->scope() == Local || sym->scope() == Argument) ? frm_ + sym->address() : sym->address());
      if (sym->name() != nullptr) {
        printf("%s\t", sym->name());
      }

      // Print the value.
      DisplayVariable(sym, idx, 0);
      fputs("\n", stdout);
    }
  }
  // Display a single variable with the given name.
  else {
    char *indexptr = strchr(params, '[');
    char *behindname = nullptr;

    // Parse all [x][y] dimensions
    int dim = 0;
    while (indexptr != nullptr && dim < sDIMEN_MAX) {
      if (behindname == nullptr)
        behindname = indexptr;

      indexptr++;
      idx[dim++] = atoi(indexptr);
      indexptr = strchr(indexptr, '[');
    }

    // End the string before the first [ temporarily, 
    // so GetVariable only looks for the variable name.
    if (behindname != nullptr)
      *behindname = '\0';

    // find the symbol with the smallest scope
    const IDebugSymbol *sym = FindDebugSymbol(params, cip_, symbol_iterator);
    if (sym) {
      // Add the [ back again
      if (behindname != nullptr)
        *behindname = '[';

      // Print variable address and name.
      printf("%s\t<%#8x>\t%s\t", ScopeToString(sym->scope()), (sym->scope() == Local || sym->scope() == Argument) ? frm_ + sym->address() : sym->address(), params);
      // Print variable value.
      DisplayVariable(sym, idx, dim);
      fputs("\n", stdout);
    }
    else {
      fputs("\tSymbol not found, or not a variable\n", stdout);
    }
  }
  debuginfo->DestroySymbolIterator(symbol_iterator);
}

void
Debugger::HandleSetVariableCmd(char *params)
{
  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
  IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(cip_);

  // TODO: Allow float assignments.
  // See if this is an array assignment. Only supports single dimensional arrays.
  char varname[32], strvalue[1024];
  strvalue[0] = '\0';
  uint32_t index;
  cell_t value;
  if (sscanf(params, " %31[^[ ][%d] = %d", varname, &index, &value) != 3) {
    index = 0;
    // Normal variable number assign
    if (sscanf(params, " %31[^= ] = %d", varname, &value) != 2) {
      // String assign
      if (sscanf(params, " %31[^= ] = \"%1023[^\"]\"", varname, strvalue) != 2) {
        varname[0] = '\0';
        strvalue[0] = '\0';
      }
    }
  }

  if (varname[0] != '\0') {
    // Find the symbol within the given range with the smallest scope.
    const IDebugSymbol *sym = FindDebugSymbol(varname, cip_, symbol_iterator);
    if (sym) {
      // User gave an integer as value
      if (strvalue[0] == '\0') {
        if (SetSymbolValue(sym, index, value)) {
          if (index > 0)
            printf("%s[%d] set to %d\n", varname, index, value);
          else
            printf("%s set to %d\n", varname, value);
        }
        else {
          if (index > 0)
            printf("Failed to set %s[%d] to %d\n", varname, index, value);
          else
            printf("Failed to set %s to %d\n", varname, value);
        }
      }
      // We have a string as value
      else {
        if (!sym->type()->isArray()
          || sym->type()->dimcount() != 1) {
          printf("%s is not a string.\n", varname);
        }
        else {
          if (SetSymbolString(sym, strvalue))
            printf("%s set to \"%s\"\n", varname, strvalue);
          else
            printf("Failed to set %s to \"%s\"\n", varname, strvalue);
        }
      }
    }
    else {
      fputs("Symbol not found or not a variable\n", stdout);
    }
  }
  else {
    fputs("Invalid syntax for \"set\". Type \"? set\".\n", stdout);
  }
  debuginfo->DestroySymbolIterator(symbol_iterator);
}

void
Debugger::HandleWatchCmd(char *params)
{
  if (strlen(params) == 0) {
    fputs("Missing variable name\n", stdout);
    return;
  }
  // List watched variables right away after adding one.
  if (AddWatch(params))
    ListWatches();
  else
    fputs("Invalid watch\n", stdout);
}

void
Debugger::HandleClearWatchCmd(char *params)
{
  if (strlen(params) == 0) {
    fputs("Missing variable name\n", stdout);
    return;
  }

  // Asterix just removes all watched variables.
  if (*params == '*') {
    ClearAllWatches();
  }
  else if (isdigit(*params)) {
    // Delete watch by index
    if (!ClearWatch(atoi(params)))
      fputs("Bad watch number\n", stdout);
  }
  else {
    if (!ClearWatch(params))
      fputs("Variable not watched\n", stdout);
  }
  ListWatches();
}

void
Debugger::HandleDumpMemoryCmd(char *command, char *params)
{
	// Mimic GDB's |x| command.
	char* fmt = command + 1;

	// Just "x" is invalid.
	if (strlen(params) == 0) {
		fputs("Missing address.\n", stdout);
		return;
	}

	// Format is x/[count][format][size] <address>
	// We require a slash.
	if (*fmt != '/') {
		fputs("Bad format specifier.\n", stdout);
		return;
	}
	fmt++;

	char* count_str = fmt;
	// Skip count number
	while (isdigit(*fmt)) {
		fmt++;
	}

	// Default count is 1.
	int count = 1;
	// Parse [count] number. The amount of stuff to display.
	if (count_str != fmt) {
		count = atoi(count_str);
		if (count <= 0) {
			fputs("Invalid count.\n", stdout);
			return;
		}
	}

	// Format letters are o(octal), x(hex), d(decimal), u(unsigned decimal)
	// t(binary), f(float), a(address), i(instruction), c(char) and s(string).
	char* format = fmt++;
	if (*format != 'o' && *format != 'x' && *format != 'd' &&
		*format != 'u' && *format != 'f' && *format != 'c' &&
		*format != 's') {
		printf("Invalid format letter '%c'.\n", *format);
		return;
	}

	// Size letters are b(byte), h(halfword), w(word).
	char size_ltr = *fmt;

	unsigned int size;
	unsigned int line_break;
	unsigned int mask;
	switch (size_ltr) {
	case 'b':
		size = 1;
		line_break = 8;
		mask = 0x000000ff;
		break;
	case 'h':
		size = 2;
		line_break = 8;
		mask = 0x0000ffff;
		break;
	case 'w':
		// Default size is a word, if none was given.
	case '\0':
		size = 4;
		line_break = 4;
		mask = 0xffffffff;
		break;
	default:
		printf("Invalid size letter '%c'.\n", size_ltr);
		return;
	}

	// Skip the size letter.
	if (size_ltr != '\0')
		fmt++;

	if (*fmt) {
		fputs("Invalid output format string.\n", stdout);
		return;
	}

	// Parse address.
	// We support 4 "magic" addresses:
	// $cip: instruction pointer
	// $sp: stack pointer
	// $hp: heap pointer
	// $frm: frame pointer
	cell_t address = 0;
	if (*params == '$') {
		if (!stricmp(params, "$cip")) {
			address = cip_;
		}
		// TODO: adjust for selected frame like frm_.
		/*else if (!stricmp(params, "$sp")) {
			address = selected_context_->sp();
		}
		else if (!stricmp(params, "$hp")) {
			address = selected_context_->hp();
		}*/
		else if (!stricmp(params, "$frm")) {
			address = frm_;
		}
		else {
			printf("Unknown address %s.\n", params);
			return;
		}
	}
	// This is a raw address.
	else {
		address = (cell_t)strtol(params, NULL, 0);
	}

	// Make sure we just read the plugin's memory.
	if (selected_context_->LocalToPhysAddr(address, nullptr) != SP_ERROR_NONE) {
		fputs("Address out of plugin's bounds.\n", stdout);
		return;
	}

	// Print the memory
	// Create a format string for the desired output format.
	char fmt_string[16];
	switch (*format) {
	case 'd':
	case 'u':
		ke::SafeSprintf(fmt_string, sizeof(fmt_string), "%%%d%c", size * 2, *format);
		break;
	case 'o':
		ke::SafeSprintf(fmt_string, sizeof(fmt_string), "0%%0%d%c", size * 2, *format);
		break;
	case 'x':
		ke::SafeSprintf(fmt_string, sizeof(fmt_string), "0x%%0%d%c", size * 2, *format);
		break;
	case 's':
		ke::SafeStrcpy(fmt_string, sizeof(fmt_string), "\"%s\"");
		break;
	case 'c':
		ke::SafeStrcpy(fmt_string, sizeof(fmt_string), "'%c'");
		break;
	case 'f':
		ke::SafeStrcpy(fmt_string, sizeof(fmt_string), "%.2f");
		break;
	default:
		return;
	}

	// Put |count| blocks of formated data on the console.
	cell_t *data;
	for (int i = 0; i < count; i++) {

		// Stop when reading out of bounds.
		// Get the data pointer we want to print.
		if (selected_context_->LocalToPhysAddr(address, &data) != SP_ERROR_NONE)
			break;

		// Put |line_break| blocks in one line.
		if (i % line_break == 0) {
			if (i > 0)
				fputs("\n", stdout);
			printf("0x%x: ", address);
		}

		// Print the data according to the specified format identifer.
		switch (*format) {
		case 'f':
			printf(fmt_string, sp_ctof(*data));
			break;
		case 'd':
		case 'u':
		case 'o':
		case 'x':
			printf(fmt_string, *data & mask);
			break;
		case 's':
			printf(fmt_string, (char*)data);
			break;
		default:
			printf(fmt_string, *data);
			break;
		}

		fputs("  ", stdout);

		// Move to the next address based on the size;
		address += size;
	}

	fputs("\n", stdout);
}



bool
Debugger::AddWatch(const char* symname)
{
  WatchTable::Insert i = watch_table_.findForAdd(symname);
  if (i.found())
    return false;
  watch_table_.add(i, symname);
  return true;
}

bool
Debugger::ClearWatch(const char* symname)
{
  WatchTable::Result r = watch_table_.find(symname);
  if (!r.found())
    return false;
  watch_table_.remove(r);
  return true;
}

bool
Debugger::ClearWatch(uint32_t num)
{
  if (num < 1 || num > watch_table_.elements())
    return false;

  uint32_t index = 1;
  for (WatchTable::iterator iter = WatchTable::iterator(&watch_table_); !iter.empty(); iter.next()) {
    if (num == index++) {
      iter.erase();
      break;
    }
  }
  return true;
}

void
Debugger::ClearAllWatches()
{
  watch_table_.clear();
}

void
Debugger::ListWatches()
{
  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
  IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(cip_);

  uint32_t num = 1;
  std::string symname;
  int dim;
  uint32_t idx[sDIMEN_MAX];
  const char *indexptr;
  char *behindname = nullptr;
  const IDebugSymbol *sym;
  for (WatchTable::iterator iter = WatchTable::iterator(&watch_table_); !iter.empty(); iter.next()) {
    symname = *iter;

    indexptr = strchr(symname.c_str(), '[');
    behindname = nullptr;
    dim = 0;
    memset(idx, 0, sizeof(idx));
    // Parse all [x][y] dimensions
    while (indexptr != nullptr && dim < sDIMEN_MAX) {
      if (behindname == nullptr)
        behindname = (char *)indexptr;

      indexptr++;
      idx[dim++] = atoi(indexptr);
      indexptr = strchr(indexptr, '[');
    }

    // End the string before the first [ temporarily, 
    // so FindDebugSymbol only looks for the variable name.
    if (behindname != nullptr)
      *behindname = '\0';

    // find the symbol with the smallest scope
    symbol_iterator->Reset();
    sym = FindDebugSymbol(symname.c_str(), cip_, symbol_iterator);
    if (sym) {
      // Add the [ back again
      if (behindname != nullptr)
        *behindname = '[';

      printf("%d  %-12s ", num++, symname.c_str());
      DisplayVariable(sym, idx, dim);
      printf("\n");
    }
    else {
      printf("%d  %-12s (not in scope)\n", num++, symname.c_str());
    }
  }
  debuginfo->DestroySymbolIterator(symbol_iterator);
}

const IDebugSymbol *
Debugger::FindDebugSymbol(const char* name, cell_t scopeaddr, IDebugSymbolIterator* symbol_iterator)
{
  cell_t codestart = 0, codeend = 0;
  const IDebugSymbol* matching_symbol = nullptr;

  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
  while (!symbol_iterator->Done()) {
    const IDebugSymbol* sym = nullptr;
    while (!symbol_iterator->Done()) {
      sym = symbol_iterator->Next();
      // find (next) matching variable
      if (strcmp(sym->name(), name) == 0)
        break;
    }

    // check the range, keep a pointer to the symbol with the smallest range
    if (strcmp(sym->name(), name) == 0 &&
       ((codestart == 0 && codeend == 0) ||
       (sym->codestart() >= codestart &&
        sym->codeend() <= codeend)))
    {
      matching_symbol = sym;
      codestart = sym->codestart();
      codeend = sym->codeend();
    }
  }
  return matching_symbol;
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

  /*if (selected_frame_ > 0)
    printf("\tframe: %d", selected_frame_);*/

  fputs("\n", stdout);
}

const char *
Debugger::ScopeToString(SymbolScope scope)
{
  static const char *scope_names[] = { "glb", "loc", "sta", "arg" };
  if (scope < 0 || scope >= sizeof(scope_names) / sizeof(scope_names[0]))
    return "unk";
  return scope_names[scope];
}

bool
Debugger::GetSymbolValue(const IDebugSymbol *sym, uint32_t index, cell_t* value)
{
  // Tried to index a non-array.
  if (index > 0 && !sym->type()->isArray())
    return false;

  // Index out of bounds.
  if (sym->type()->dimcount() > 0 && index >= sym->type()->dimension(0))
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(sym, &addr))
    return false;

  // Resolve index into array.
  cell_t *vptr;
  if (selected_context_->LocalToPhysAddr(addr + index * sizeof(cell_t), &vptr) != SP_ERROR_NONE)
    return false;

  if (vptr != nullptr)
    *value = *vptr;

  return vptr != nullptr;
}

bool
Debugger::SetSymbolValue(const IDebugSymbol *sym, uint32_t index, cell_t value)
{
  // Tried to index a non-array.
  if (index > 0 && !sym->type()->isArray())
    return false;

  // Index out of bounds.
  if (sym->type()->dimcount() > 0 && index >= sym->type()->dimension(0))
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(sym, &addr))
    return false;

  // Resolve index into array.
  cell_t *vptr;
  if (selected_context_->LocalToPhysAddr(addr + index * sizeof(cell_t), &vptr) != SP_ERROR_NONE)
    return false;

  if (vptr != nullptr)
    *vptr = value;

  return vptr != nullptr;
}

const char*
Debugger::GetSymbolString(const IDebugSymbol *sym)
{
  // Make sure we're dealing with a one-dimensional array.
  if (!sym->type()->isArray() || sym->type()->dimcount() != 1)
    return nullptr;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(sym, &addr))
    return false;

  char *str;
  if (selected_context_->LocalToStringNULL(addr, &str) != SP_ERROR_NONE)
    return nullptr;
  return str;
}

bool
Debugger::SetSymbolString(const IDebugSymbol *sym, const char* value)
{
  // Make sure we're dealing with a one-dimensional array.
  if (!sym->type()->isArray() || sym->type()->dimcount() != 1)
    return nullptr;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(sym, &addr))
    return false;

  return selected_context_->StringToLocalUTF8(addr, sym->type()->dimension(0), value, NULL) == SP_ERROR_NONE;
}

bool
Debugger::GetEffectiveSymbolAddress(const IDebugSymbol *sym, cell_t *address)
{
  cell_t base = sym->address();
  // addresses of local vars are relative to the frame
  if (sym->scope() == Local || sym->scope() == Argument)
    base += frm_;

  // a reference
  cell_t *addr;
  if (sym->type()->isReference()) {
    if (selected_context_->LocalToPhysAddr(base, &addr) != SP_ERROR_NONE)
      return false;

    assert(addr != nullptr);
    base = *addr;
  }

  *address = base;
  return true;
}

void
Debugger::PrintValue(const ISymbolType* type, long value)
{
  if (type->isFloat()) {
    printf("%f", sp_ctof(value));
  }
  else if (type->isBoolean()) {
    switch (value)
    {
    case 0:
      fputs("false", stdout);
      break;
    case 1:
      fputs("true", stdout);
      break;
    default:
      printf("%ld (false)", value);
      break;
    }
  }
  else {
    printf("%ld", value);
  }
  /*case DISP_HEX:
    printf("%lx", value);
    break;*/
}

void
Debugger::DisplayVariable(const IDebugSymbol *sym, uint32_t index[], uint32_t idxlevel)
{
  assert(index != nullptr);

  // first check whether the variable is visible at all
  if (cip_ < sym->codestart() || cip_ > sym->codeend()) {
    fputs("(not in scope)", stdout);
    return;
  }

  if (sym->type()->isArray()) {
    // Check whether any of the indices are out of range
    uint32_t dim;
    for (dim = 0; dim < idxlevel && dim < sym->type()->dimcount(); dim++) {
      if (sym->type()->dimension(dim) > 0 && index[dim] >= sym->type()->dimension(dim))
        break;
    }
    if (dim < idxlevel) {
      fputs("(index out of range)", stdout);
      return;
    }
  }

  cell_t value;
  if (sym->type()->isEnumStruct()) {
    uint32_t idx[sDIMEN_MAX];
    memset(idx, 0, sizeof(idx));
    fputs("{", stdout);
    for (size_t i = 0; i < sym->type()->esfieldcount(); i++) {
      if (i > 0)
        fputs(", ", stdout);
    
      const IEnumStructField* field = sym->type()->esfield(i);
      printf("%s: ", field->name());
      if (field->type()->isArray())
        fputs("(array)", stdout);
      else {
        if (GetSymbolValue(sym, field->offset(), &value))
          PrintValue(field->type(), value);
        else
          fputs("?", stdout);
      }
    }
    fputs("}", stdout);
  }
  // Print first dimension of array
  else if (sym->type()->isArray() && idxlevel == 0)
  {
    // Print string
    if (sym->type()->isString()) {
      const char *str = GetSymbolString(sym);
      if (str != nullptr)
        printf("\"%s\"", str); // TODO: truncate to 40 chars
      else
        fputs("NULL_STRING", stdout);
    }
    // Print one-dimensional array
    else if (sym->type()->dimcount() == 1) {
      uint32_t len = sym->type()->dimension(0);
      // Only print the first 5 elements
      if (len > 5)
        len = 5;
      else if (len == 0)
        len = 1; // unknown array length, assume at least 1 element

      fputs("{", stdout);
      uint32_t i;
      for (i = 0; i < len; i++) {
        if (i > 0)
          fputs(",", stdout);
        if (GetSymbolValue(sym, i, &value))
          PrintValue(sym->type(), value);
        else
          fputs("?", stdout);
      }
      if (len < sym->type()->dimension(0) || sym->type()->dimension(0) == 0)
        fputs(",...", stdout);
      fputs("}", stdout);
    }
    // Not supported..
    else {
      fputs("(multi-dimensional array)", stdout);
    }
  }
  else if (!sym->type()->isArray() && idxlevel > 0) {
    // index used on a non-array
    fputs("(invalid index, not an array)", stdout);
  }
  else {
    // simple variable, or indexed array element
    assert(idxlevel > 0 || index[0] == 0); // index should be zero if non-array
    uint32_t dim = 0;
    cell_t base = 0;
    if (idxlevel > 0) {
      for (; dim < idxlevel - 1; dim++) {
        base += index[dim];
        if (!GetSymbolValue(sym, base, &value))
          break;
        base += value / sizeof(cell_t);
      }
    }

    if (GetSymbolValue(sym, base + index[dim], &value) &&
      sym->type()->dimcount() == idxlevel)
      PrintValue(sym->type(), value);
    else if (sym->type()->dimcount() != idxlevel)
      fputs("(invalid number of dimensions)", stdout);
    else
      fputs("?", stdout);
  }
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
