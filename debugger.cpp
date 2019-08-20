/**
* vim: set ts=4 :
* =============================================================================
* SourceMod Console Debugger Extension
* Copyright (C) 2016-2018 AlliedModders LLC.  All rights reserved.
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
#include <amtl/am-string.h>
#include <ctype.h>

using namespace SourcePawn;

Debugger::Debugger(IPluginContext *context)
  : context_(context),
  runmode_(RUNNING),
  lastfrm_(0),
  lastline_(-1),
  currentfile_(""),
  active_(false)
{
}

bool
Debugger::Initialize()
{
  if (!breakpoint_map_.init())
    return false;

  if (!watch_table_.init())
    return false;

  return true;
}

bool
Debugger::active() const
{
  return active_;
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

  ClearAllBreakpoints();
  ClearAllWatches();
  SetRunmode(RUNNING);
}

Runmode
Debugger::runmode() const
{
  return runmode_;
}

void
Debugger::SetRunmode(Runmode runmode)
{
  runmode_ = runmode;
}

cell_t
Debugger::lastframe() const
{
  return lastfrm_;
}

void
Debugger::SetLastFrame(cell_t lastfrm)
{
  lastfrm_ = lastfrm;
}

uint32_t
Debugger::lastline() const
{
  return lastline_;
}

void
Debugger::SetLastLine(uint32_t line)
{
  lastline_ = line;
}

uint32_t
Debugger::breakcount() const
{
  return breakcount_;
}

void
Debugger::SetBreakCount(uint32_t breakcount)
{
  breakcount_ = breakcount;
}

const char *
Debugger::currentfile() const
{
  return currentfile_;
}

void
Debugger::SetCurrentFile(const char *file)
{
  currentfile_ = file;
}

size_t
Debugger::GetBreakpointCount() const
{
  return breakpoint_map_.elements();
}

void
Debugger::HandleInput(cell_t cip, cell_t frm, bool isBp)
{
  // Remember which command was executed last,
  // so you don't have to type it again if you
  // want to repeat it.
  // Only |step| and |next| can be repeated like that.
  static char lastcommand[32] = "";

  // Reset the state.
  IFrameIterator *frames = context_->CreateFrameIterator();
  frame_count_ = 0;
  selected_frame_ = 0;
  cip_ = cip;
  frm_ = frm;
  selected_context_ = context_;

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

  if (!isBp)
    printf("STOP at line %d in %s\n", lastline_, SkipPath(currentfile_));
  else
    printf("BREAK at line %d in %s\n", lastline_, SkipPath(currentfile_));

  // Print all watched variables now.
  ListWatches();

  char line[512], command[32];
  int result;
  char *params;
  for (;;) {
    // Read debugger command
    fgets(line, sizeof(line), stdin);

    // strip newline character, plus leading or trailing white space
    TrimString(line);

    // Repeat the last command, if no new command was given.
    if (strlen(line) == 0) {
      ke::SafeStrcpy(line, sizeof(line), lastcommand);
    }
    lastcommand[0] = '\0';

    // Extract the first word from the string.
    result = sscanf(line, "%31s", command);
    if (result <= 0) {
      ListCommands(nullptr);
      continue;
    }

    // Optional params start after the command.
    params = strchr(line, ' ');
    params = (params != nullptr) ? SkipWhitespace(params) : (char*)"";

    if (!stricmp(command, "?")) {
      HandleHelpCmd(line);
    }
    else if (!stricmp(command, "quit")) {
      HandleQuitCmd();
      return;
    }
    else if (!stricmp(command, "g") || !stricmp(command, "go")) {
      bool exitConsole = HandleGoCmd(params);
      if (exitConsole)
        return;
    }
    else if (!stricmp(command, "s") || !stricmp(command, "step")) {
      ke::SafeStrcpy(lastcommand, sizeof(lastcommand), "s");
      SetRunmode(STEPPING);
      return;
    }
    else if (!stricmp(command, "n") || !stricmp(command, "next")) {
      ke::SafeStrcpy(lastcommand, sizeof(lastcommand), "n");
      SetRunmode(STEPOVER);
      return;
    }
    /*else if (!stricmp(command, "funcs")) {
      HandleFunctionListCmd();
    }*/
    else if (!stricmp(command, "bt") || !stricmp(command, "backtrace")) {
      printf("Stack trace:\n");
      DumpStack();
    }
    /*else if (!stricmp(command, "f") || !stricmp(command, "frame")) {
      HandleFrameCmd(params);
    }*/
    else if (!stricmp(command, "break") || !stricmp(command, "tbreak")) {
      HandleBreakpointCmd(command, params);
    }
    else if (!stricmp(command, "cbreak")) {
      HandleClearBreakpointCmd(params);
    }
    else if (!stricmp(command, "disp") || !stricmp(command, "d")) {
      HandleVariableDisplayCmd(params);
    }
    else if (!stricmp(command, "set")) {
      HandleSetVariableCmd(params);
    }
    else if (!stricmp(command, "files")) {
      HandleFilesListCmd();
    }
    // Change display format of symbol
    /*else if (!stricmp(command, "type")) {
      HandleDisplayFormatChangeCmd(params);
    }*/
    else if (!stricmp(command, "pos")) {
      HandlePrintPositionCmd();
    }
    else if (!stricmp(command, "w") || !stricmp(command, "watch")) {
      HandleWatchCmd(params);
    }
    else if (!stricmp(command, "cw") || !stricmp(command, "cwatch")) {
      HandleClearWatchCmd(params);
    }
    else if (command[0] == 'x' || command[0] == 'X') {
      HandleDumpMemoryCmd(command, params);
    }
    else {
      printf("\tInvalid command \"%s\", use \"?\" to view all commands\n", command);
    }
  }
}

void
Debugger::ListCommands(const char *command)
{
  if (!command)
    command = "";

  if (strlen(command) == 0 || !strcmp(command, "?")) {
    printf("At the prompt, you can type debug commands. For example, the word \"step\" is a\n"
      "command to execute a single line in the source code. The commands that you will\n"
      "use most frequently may be abbreviated to a single letter: instead of the full\n"
      "word \"step\", you can also type the letter \"s\" followed by the enter key.\n\n"
      "Available commands:\n");
  }
  else {
    printf("Options for command \"%s\":\n", command);
  }

  if (!stricmp(command, "break") || !stricmp(command, "tbreak")) {
    printf("\tUse TBREAK for one-time breakpoints\n\n"
      "\tBREAK\t\tlist all breakpoints\n"
      "\tBREAK n\t\tset a breakpoint at line \"n\"\n"
      "\tBREAK name:n\tset a breakpoint in file \"name\" at line \"n\"\n"
      "\tBREAK func\tset a breakpoint at function with name \"func\"\n"
      "\tBREAK .\t\tset a breakpoint at the current location\n");
  }
  else if (!stricmp(command, "cbreak")) {
    printf("\tCBREAK n\tremove breakpoint number \"n\"\n"
      "\tCBREAK *\tremove all breakpoints\n");
  }
  else if (!stricmp(command, "cw") || !stricmp(command, "cwatch")) {
    printf("\tCWATCH may be abbreviated to CW\n\n"
      "\tCWATCH n\tremove watch number \"n\"\n"
      "\tCWATCH var\tremove watch from \"var\"\n"
      "\tCWATCH *\tremove all watches\n");
  }
  else if (!stricmp(command, "d") || !stricmp(command, "disp")) {
    printf("\tDISP may be abbreviated to D\n\n"
      "\tDISP\t\tdisplay all variables that are currently in scope\n"
      "\tDISP var\tdisplay the value of variable \"var\"\n"
      "\tDISP var[i]\tdisplay the value of array element \"var[i]\"\n");
  }
  /*else if (!stricmp(command, "f") || !stricmp(command, "frame")) {
    printf("\tFRAME may be abbreviated to F\n\n");
    printf("\tFRAME n\tselect frame n and show/change local variables in that function\n");
  }*/
  else if (!stricmp(command, "g") || !stricmp(command, "go")) {
    printf("\tGO may be abbreviated to G\n\n"
      "\tGO\t\trun until the next breakpoint or program termination\n"
      "\tGO n\t\trun until line number \"n\"\n"
      "\tGO name:n\trun until line number \"n\" in file \"name\"\n"
      "\tGO func\t\trun until the current function returns (\"step out\")\n");
  }
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
  else if (!stricmp(command, "x")) {
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
  else if (!stricmp(command, "n") || !stricmp(command, "next") ||
    !stricmp(command, "quit") || !stricmp(command, "pos") ||
    !stricmp(command, "s") || !stricmp(command, "step") ||
    !stricmp(command, "files"))
  {
    printf("\tno additional information\n");
  }
  else {
    printf("\tB(ack)T(race)\t\tdisplay the stack trace\n"
      "\tBREAK\t\tset breakpoint at line number or function name\n"
      "\tCBREAK\t\tremove breakpoint\n"
      "\tCW(atch)\tremove a \"watchpoint\"\n"
      "\tD(isp)\t\tdisplay the value of a variable, list variables\n"
      "\tFILES\t\tlist all files that this program is composed off\n"
      //"\tF(rame)\t\tSelect a frame from the back trace to operate on\n"
      //"\tFUNCS\t\tdisplay functions\n"
      "\tG(o)\t\trun program (until breakpoint)\n"
      "\tN(ext)\t\tRun until next line, step over functions\n"
      "\tPOS\t\tShow current file and line\n"
      "\tQUIT\t\texit debugger\n"
      "\tSET\t\tSet a variable to a value\n"
      "\tS(tep)\t\tsingle step, step into functions\n"
      //"\tTYPE\t\tset the \"display type\" of a symbol\n"
      "\tW(atch)\t\tset a \"watchpoint\" on a variable\n"
      "\tX\t\texamine plugin memory: x/FMT ADDRESS\n"
      "\n\tUse \"? <command name>\" to view more information on a command\n");
  }
}

void
Debugger::HandleHelpCmd(const char *line)
{
  char command[32];
  // See if there is a command specified after the "?".
  int result = sscanf(line, "%*s %31s", command);
  // Display general or special help for the command.
  ListCommands(result == 1 ? command : nullptr);
}

void
Debugger::HandleQuitCmd()
{
  fputs("Clearing all breakpoints. Running normally.\n", stdout);
  Deactivate();
}

bool
Debugger::HandleGoCmd(char *params)
{
  // "go func" runs until the function returns.
  if (!stricmp(params, "func")) {
    SetRunmode(STEPOUT);
    return true;
  }

  // There is a parameter given -> run until that line!
  if (*params != '\0') {
    const char *filename = currentfile_;
    // ParseBreakpointLine prints an error.
    params = ParseBreakpointLine(params, &filename);
    if (params == nullptr)
      return false;

    Breakpoint *bp = nullptr;
    // User specified a line number. Add a breakpoint.
    if (isdigit(*params)) {
      bp = AddBreakpoint(filename, strtol(params, NULL, 10) - 1, true);
    }

    if (bp == nullptr) {
      fputs("Invalid format or bad breakpoint address. Type \"? go\" for help.\n", stdout);
      return false;
    }

    uint32_t bpline = 0;
    IPluginDebugInfo *debuginfo = context_->GetRuntime()->GetDebugInfo();
    debuginfo->LookupLine(bp->addr(), &bpline);
    printf("Running until line %d in file %s.\n", bpline, SkipPath(filename));
  }

  SetRunmode(RUNNING);
  // Return true, to break out of the debugger shell 
  // and continue execution of the plugin.
  return true;
}

void
Debugger::HandleBreakpointCmd(char *command, char *params)
{
  if (*params == '\0') {
    ListBreakpoints();
  }
  else {
    const char *filename = currentfile_;
    params = ParseBreakpointLine(params, &filename);
    if (params == nullptr)
      return;

    Breakpoint *bp;
    // User specified a line number
    if (isdigit(*params)) {
      bp = AddBreakpoint(filename, strtol(params, NULL, 10) - 1, !stricmp(command, "tbreak"));
    }
    // User wants to add a breakpoint at the current location
    else if (*params == '.') {
      bp = AddBreakpoint(filename, lastline_ - 1, !stricmp(command, "tbreak"));
    }
    // User specified a function name
    else {
      bp = AddBreakpoint(filename, params, !stricmp(command, "tbreak"));
    }

    if (bp == nullptr) {
      fputs("Invalid breakpoint\n", stdout);
    }
    else {
      uint32_t bpline = 0;
      IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
      debuginfo->LookupLine(bp->addr(), &bpline);
      printf("Set breakpoint %zu in file %s on line %d", breakpoint_map_.elements(), SkipPath(filename), bpline);
      if (bp->name() != nullptr)
        printf(" in function %s", bp->name());
      fputs("\n", stdout);
    }
  }
}

void
Debugger::HandleClearBreakpointCmd(char *params)
{
  if (*params == '*') {
    size_t num_bps = breakpoint_map_.elements();
    // clear all breakpoints
    ClearAllBreakpoints();
    printf("\tCleared all %zu breakpoints.\n", num_bps);
  }
  else {
    int number = FindBreakpoint(params);
    if (number < 0 || !ClearBreakpoint(number))
      fputs("\tUnknown breakpoint (or wrong syntax)\n", stdout);
    else
      printf("\tCleared breakpoint %d\n", number);
  }
}

void
Debugger::HandleVariableDisplayCmd(char *params)
{
  uint32_t idx[sDIMEN_MAX];
  memset(idx, 0, sizeof(idx));
  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();

  IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(cip_);
  if (*params == '\0') {
    // Display all variables that are in scope
    while (!symbol_iterator->Done()) {
      IDebugSymbol* sym = symbol_iterator->Next();

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
    IDebugSymbol *sym = FindDebugSymbol(params, cip_, symbol_iterator);
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
    IDebugSymbol *sym = FindDebugSymbol(varname, cip_, symbol_iterator);
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
        if (!sym->type().isArray()
          || sym->type().dimcount() != 1) {
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
Debugger::HandleFilesListCmd()
{
  fputs("Source files:\n", stdout);
  // Browse through the file table
  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
  for (unsigned int i = 0; i < debuginfo->NumFiles(); i++) {
    if (debuginfo->GetFileName(i) != nullptr) {
      printf("%s\n", debuginfo->GetFileName(i));
    }
  }
}

void
Debugger::HandlePrintPositionCmd()
{
  // Print file, function, line and selected frame.
  printf("\tfile: %s", SkipPath(currentfile_));

  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
  const char *function = nullptr;
  if (debuginfo->LookupFunction(cip_, &function) == SP_ERROR_NONE)
    printf("\tfunction: %s", function);

  printf("\tline: %d", lastline_);

  /*if (selected_frame_ > 0)
    printf("\tframe: %d", selected_frame_);*/

  fputs("\n", stdout);
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

// Breakpoint handling
bool
Debugger::CheckBreakpoint(cell_t cip)
{
  // See if there's a break point on the current instruction.
  BreakpointMap::Result result = breakpoint_map_.find(cip);
  if (!result.found())
    return false;

  // Remove the temporary breakpoint
  if (result->value->temporary()) {
    ClearBreakpoint(result->value);
  }

  return true;
}

Breakpoint *
Debugger::AddBreakpoint(const char* file, uint32_t line, bool temporary)
{
  const char *targetfile = FindFileByPartialName(file);
  if (targetfile == nullptr)
    targetfile = currentfile_;

  IPluginDebugInfo *debuginfo = context_->GetRuntime()->GetDebugInfo();
  // Are there that many lines in the file?
  uint32_t addr;
  if (debuginfo->LookupLineAddress(line, targetfile, &addr) != SP_ERROR_NONE)
    return nullptr;

  Breakpoint *bp;
  {
    // See if there's already a breakpoint in place here.
    BreakpointMap::Insert p = breakpoint_map_.findForAdd(addr);
    if (p.found())
      return p->value;

    bp = new Breakpoint(debuginfo, addr, nullptr, temporary);
    breakpoint_map_.add(p, addr, bp);
  }

  return bp;
}

Breakpoint *
Debugger::AddBreakpoint(const char* file, const char *function, bool temporary)
{
  const char *targetfile = FindFileByPartialName(file);
  if (targetfile == nullptr)
    return nullptr;

  IPluginDebugInfo *debuginfo = context_->GetRuntime()->GetDebugInfo();
  // Is there a function named like that in the file?
  uint32_t addr;
  if (debuginfo->LookupFunctionAddress(function, targetfile, &addr) != SP_ERROR_NONE)
    return nullptr;

  Breakpoint *bp;
  {
    // See if there's already a breakpoint in place here.
    BreakpointMap::Insert p = breakpoint_map_.findForAdd(addr);
    if (p.found())
      return p->value;

    const char *realname = nullptr;
    debuginfo->LookupFunction(addr, &realname);

    bp = new Breakpoint(debuginfo, addr, realname, temporary);
    breakpoint_map_.add(p, addr, bp);
  }

  return bp;
}

bool
Debugger::ClearBreakpoint(int number)
{
  if (number <= 0)
    return false;

  int i = 0;
  for (BreakpointMap::iterator iter = breakpoint_map_.iter(); !iter.empty(); iter.next()) {
    if (++i == number) {
      iter.erase();
      return true;
    }
  }
  return false;
}

bool
Debugger::ClearBreakpoint(Breakpoint * bp)
{
  BreakpointMap::Result res = breakpoint_map_.find(bp->addr());
  if (!res.found())
    return false;

  breakpoint_map_.remove(res);
  return true;
}

void
Debugger::ClearAllBreakpoints()
{
  breakpoint_map_.clear();
}

int
Debugger::FindBreakpoint(char *breakpoint)
{
  breakpoint = SkipWhitespace(breakpoint);

  // check if a filename precedes the breakpoint location
  char *sep = strrchr(breakpoint, ':');
  if (sep == nullptr && isdigit(*breakpoint))
    return atoi(breakpoint);

  const char *filename = currentfile_;
  if (sep != nullptr) {
    /* the user may have given a partial filename (e.g. without a path), so
    * walk through all files to find a match
    */
    *sep = '\0';
    filename = FindFileByPartialName(breakpoint);
    if (filename == nullptr)
      return -1;

    // Skip past colon
    breakpoint = sep + 1;
  }

  breakpoint = SkipWhitespace(breakpoint);
  Breakpoint *bp;
  const char *fname;
  uint32_t line;
  uint32_t number = 0;
  IPluginDebugInfo *debuginfo = context_->GetRuntime()->GetDebugInfo();
  for (BreakpointMap::iterator iter = breakpoint_map_.iter(); !iter.empty(); iter.next()) {
    bp = iter->value;
    if (debuginfo->LookupFile(bp->addr(), &fname) != SP_ERROR_NONE)
      fname = nullptr;
    number++;

    // Correct file?
    if (fname != nullptr && !strcmp(fname, filename)) {
      // A function breakpoint
      if (bp->name() != nullptr && !strcmp(breakpoint, bp->name()))
        return number;

      // Line breakpoint
      if (debuginfo->LookupLine(bp->addr(), &line) == SP_ERROR_NONE &&
        line == strtoul(breakpoint, NULL, 10) - 1)
        return number;
    }
  }
  return -1;
}

void
Debugger::ListBreakpoints()
{
  Breakpoint *bp;
  uint32_t line;
  const char *filename;
  uint32_t number = 0;
  for (BreakpointMap::iterator iter = breakpoint_map_.iter(); !iter.empty(); iter.next()) {
    bp = iter->value;

    printf("%2d  ", ++number);
    line = bp->line();
    if (line > 0) {
      printf("line: %d", line);
    }

    if (bp->temporary())
      printf("  (TEMP)");

    filename = bp->filename();
    if (filename != nullptr) {
      printf("\tfile: %s", filename);
    }

    if (bp->name() != nullptr) {
      printf("\tfunc: %s", bp->name());
    }
    printf("\n");
  }
}

char *
Debugger::ParseBreakpointLine(char *input, const char **filename)
{
  // check if a filename precedes the breakpoint location
  char *sep;
  if ((sep = strchr(input, ':')) != nullptr) {
    // the user may have given a partial filename (e.g. without a path), so
    // walk through all files to find a match
    *sep = '\0';
    *filename = FindFileByPartialName(input);
    if (*filename == nullptr) {
      fputs("Invalid filename.\n", stdout);
      return nullptr;
    }

    // Skip colon and settle before line number
    input = sep + 1;
  }

  input = SkipWhitespace(input);

  return input;
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
  ke::AString symname;
  int dim;
  uint32_t idx[sDIMEN_MAX];
  const char *indexptr;
  char *behindname = nullptr;
  IDebugSymbol *sym;
  for (WatchTable::iterator iter = WatchTable::iterator(&watch_table_); !iter.empty(); iter.next()) {
    symname = *iter;

    indexptr = strchr(symname.chars(), '[');
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
    sym = FindDebugSymbol(symname.chars(), cip_, symbol_iterator);
    if (sym) {
      // Add the [ back again
      if (behindname != nullptr)
        *behindname = '[';

      printf("%d  %-12s ", num++, symname.chars());
      DisplayVariable(sym, idx, dim);
      printf("\n");
    }
    else {
      printf("%d  %-12s (not in scope)\n", num++, symname.chars());
    }
  }
  debuginfo->DestroySymbolIterator(symbol_iterator);
}

IDebugSymbol *
Debugger::FindDebugSymbol(const char* name, cell_t scopeaddr, IDebugSymbolIterator* symbol_iterator)
{
  cell_t codestart = 0, codeend = 0;
  IDebugSymbol* matching_symbol = nullptr;

  IPluginDebugInfo *debuginfo = selected_context_->GetRuntime()->GetDebugInfo();
  while (!symbol_iterator->Done()) {
    IDebugSymbol* sym = nullptr;
    while (!symbol_iterator->Done()) {
      sym = symbol_iterator->Next();
      // find (next) matching variable
      if (strcmp(sym->name(), name) == 0)
        break;
    }

    // check the range, keep a pointer to the symbol with the smallest range
    if (strcmp(sym->name(), name) == 0 &&
       (codestart == 0 && codeend == 0) ||
       (sym->codestart() >= codestart &&
        sym->codeend() <= codeend))
    {
      matching_symbol = sym;
      codestart = sym->codestart();
      codeend = sym->codeend();
    }
  }
  return matching_symbol;
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
Debugger::GetSymbolValue(IDebugSymbol *sym, uint32_t index, cell_t* value)
{
  // Tried to index a non-array.
  if (index > 0 && !sym->type().isArray())
    return false;

  // Index out of bounds.
  if (sym->type().dimcount() > 0 && index >= sym->type().dimcount())
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
Debugger::SetSymbolValue(IDebugSymbol *sym, uint32_t index, cell_t value)
{
  // Tried to index a non-array.
  if (index > 0 && !sym->type().isArray())
    return false;

  // Index out of bounds.
  if (sym->type().dimcount() > 0 && index >= sym->type().dimcount())
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
Debugger::GetSymbolString(IDebugSymbol *sym)
{
  // Make sure we're dealing with a one-dimensional array.
  if (!sym->type().isArray() || sym->type().dimcount() != 1)
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
Debugger::SetSymbolString(IDebugSymbol *sym, const char* value)
{
  // Make sure we're dealing with a one-dimensional array.
  if (!sym->type().isArray() || sym->type().dimcount() != 1)
    return nullptr;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(sym, &addr))
    return false;

  return selected_context_->StringToLocalUTF8(addr, sym->type().dimension(0), value, NULL) == SP_ERROR_NONE;
}

bool
Debugger::GetEffectiveSymbolAddress(IDebugSymbol *sym, cell_t *address)
{
  cell_t base = sym->address();
  // addresses of local vars are relative to the frame
  if (sym->scope() == Local || sym->scope() == Argument)
    base += frm_;

  // a reference
  cell_t *addr;
  if (sym->type().isReference()) {
    if (selected_context_->LocalToPhysAddr(base, &addr) != SP_ERROR_NONE)
      return false;

    assert(addr != nullptr);
    base = *addr;
  }

  *address = base;
  return true;
}

void
Debugger::PrintValue(ISymbolType &type, long value)
{
  if (type.isFloat()) {
    printf("%f", sp_ctof(value));
  }
  else if (type.isBoolean()) {
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
Debugger::DisplayVariable(IDebugSymbol *sym, uint32_t index[], uint32_t idxlevel)
{
  assert(index != nullptr);

  // first check whether the variable is visible at all
  if (cip_ < sym->codestart() || cip_ > sym->codeend()) {
    fputs("(not in scope)", stdout);
    return;
  }

  if (sym->type().isArray()) {
    // Check whether any of the indices are out of range
    uint32_t dim;
    for (dim = 0; dim < idxlevel && dim < sym->type().dimcount(); dim++) {
      if (sym->type().dimension(dim) > 0 && index[dim] >= sym->type().dimension(dim))
        break;
    }
    if (dim < idxlevel) {
      fputs("(index out of range)", stdout);
      return;
    }
  }

  cell_t value;
  // Print first dimension of array
  if (sym->type().isArray() && idxlevel == 0)
  {
    // Print string
    if (sym->type().isString()) {
      const char *str = GetSymbolString(sym);
      if (str != nullptr)
        printf("\"%s\"", str); // TODO: truncate to 40 chars
      else
        fputs("NULL_STRING", stdout);
    }
    // Print one-dimensional array
    else if (sym->type().dimcount() == 1) {
      uint32_t len = sym->type().dimension(0);
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
      if (len < sym->type().dimension(0) || sym->type().dimension(0) == 0)
        fputs(",...", stdout);
      fputs("}", stdout);
    }
    // Not supported..
    else {
      fputs("(multi-dimensional array)", stdout);
    }
  }
  else if (!sym->type().isArray() && idxlevel > 0) {
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
      sym->type().dimcount() == idxlevel)
      PrintValue(sym->type(), value);
    else if (sym->type().dimcount() != idxlevel)
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

const char *
Debugger::FindFileByPartialName(const char *partialname)
{
  // the user may have given a partial filename (e.g. without a path), so
  // walk through all files to find a match.
  IPluginDebugInfo *debuginfo = context_->GetRuntime()->GetDebugInfo();
  int len = strlen(partialname);
  int offs;
  const char *filename;
  for (uint32_t i = 0; i < debuginfo->NumFiles(); i++) {
    filename = debuginfo->GetFileName(i);
    offs = strlen(filename) - len;
    if (offs >= 0 &&
      !strncmp(filename + offs, partialname, len))
    {
      return filename;
    }
  }
  return nullptr;
}
