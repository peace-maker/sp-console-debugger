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
#include <amtl/am-string.h>
#include <smx/smx-legacy-debuginfo.h>

using namespace SourcePawn;

const std::string
DebuggerCommand::GetMatch(const std::string& command) {
  // Check if any of the command names start with the given command input.
  std::string best_match;
  for (auto& name : names_) {
    if (match_start_only_) {
      if (command.find(name) == 0 && (best_match.empty() || best_match.size() > name.size()))
        best_match = name;
    }
    else {
      // Find the shortest alias that's matching the input.
      if (name.find(command) == 0 && (best_match.empty() || best_match.size() > name.size()))
        best_match = name;
    }
  }
  return best_match;
}

void
DebuggerCommand::ShortHelp() {
  // TODO: Align description at consistent offset.
  std::cout << "\t" << names_[0] << "\t" << description_ << std::endl;
}

CommandResult
BacktraceCommand::Accept(const std::string& command, const std::string& params) {
  std::cout << "Stack trace:\n";
  debugger_->DumpStack();
  return CR_StayCommandLoop;
}

CommandResult
BreakpointCommand::Accept(const std::string& command, const std::string& params) {
  if (params.empty()) {
    debugger_->breakpoints().ListBreakpoints();
    return CR_StayCommandLoop;
  }
  
  std::string filename = debugger_->currentfile();
  std::string breakpoint_location = debugger_->breakpoints().ParseBreakpointLine(params, &filename);
  if (breakpoint_location.empty())
    return CR_StayCommandLoop;

  bool isTemporary = !command.rfind("tb", 0);

  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  Breakpoint *bp = nullptr;
  // User specified a line number
  if (isdigit(breakpoint_location[0])) {
    bp = debugger_->breakpoints().AddBreakpoint(filename, strtol(breakpoint_location.c_str(), NULL, 10) - 1, isTemporary);
  }
  // User wants to add a breakpoint at the current location
  else if (breakpoint_location[0] == '.') {
    uint32_t bpline = 0;
    if (debuginfo->LookupLine(debugger_->cip(), &bpline) == SP_ERROR_NONE)
      bp = debugger_->breakpoints().AddBreakpoint(filename, debugger_->cip(), isTemporary);
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
  debuginfo->LookupLine(bp->addr(), &bpline);
  printf("Set breakpoint %zu in file %s on line %d", debugger_->breakpoints().GetBreakpointCount(), SkipPath(filename.c_str()), bpline);
  if (bp->name() != nullptr)
    printf(" in function %s", bp->name());
  fputs("\n", stdout);
  return CR_StayCommandLoop;
}

bool
BreakpointCommand::LongHelp(const std::string& command) {
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
ClearBreakpointCommand::Accept(const std::string& command, const std::string& params) {
  if (params.empty()) {
    std::cout << "\tInvalid syntax. Type \"? cbreak\" for help.\n";
    return CR_StayCommandLoop;
  }

  if (params == "*") {
    size_t num_bps = debugger_->breakpoints().GetBreakpointCount();
    debugger_->breakpoints().ClearAllBreakpoints();
    std::cout << "\tCleared all " << num_bps << " breakpoints.\n";
  }
  else {
    int number = debugger_->breakpoints().FindBreakpoint(params);
    if (number < 0 || !debugger_->breakpoints().ClearBreakpoint(number))
      std::cout << "\tUnknown breakpoint (or wrong syntax)\n";
    else
      std::cout << "\tCleared breakpoint " << number << ".\n";
  }
  return CR_StayCommandLoop;
}

bool
ClearBreakpointCommand::LongHelp(const std::string& command) {
  std::cout << "\tCBREAK may be abbreviated to CB\n\n"
    "\tCBREAK n\tremove breakpoint number \"n\"\n"
    "\tCBREAK *\tremove all breakpoints\n";
  return true;
}

CommandResult
ClearWatchVariableCommand::Accept(const std::string& command, const std::string& params) {
  if (params.empty()) {
    std::cout << "Missing variable name\n";
    return CR_StayCommandLoop;
  }

  SymbolManager& symbols = debugger_->symbols();
  // Asterix just removes all watched variables.
  if (params == "*") {
    symbols.ClearAllWatches();
  }
  else if (isdigit(params[0])) {
    // Delete watch by index
    if (!symbols.ClearWatch(atoi(params.c_str())))
      std::cout << "Bad watch number\n";
  }
  else {
    if (!symbols.ClearWatch(params))
      std::cout << "Variable not watched\n";
  }
  symbols.ListWatches();
  return CR_StayCommandLoop;
}

bool
ClearWatchVariableCommand::LongHelp(const std::string& command) {
  std::cout << "\tCWATCH may be abbreviated to CW\n\n"
      "\tCWATCH n\tremove watch number \"n\"\n"
      "\tCWATCH var\tremove watch from \"var\"\n"
    "\tCWATCH *\tremove all watches\n";
  return true;
}

CommandResult
ContinueCommand::Accept(const std::string& command, const std::string& params) {
  if (!params.empty()) {
    // "continue func" runs until the function returns.
    if (!stricmp(params.c_str(), "func")) {
      debugger_->SetRunmode(STEPOUT);
      return CR_LeaveCommandLoop;
    }

    // There is a parameter given -> run until that line!
    std::string filename = debugger_->currentfile();
    // ParseBreakpointLine prints an error.
    std::string breakpoint_location = debugger_->breakpoints().ParseBreakpointLine(params, &filename);
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
  // Break out of the debugger shell 
  // and continue execution of the plugin.
  return CR_LeaveCommandLoop;
}

bool
ContinueCommand::LongHelp(const std::string& command) {
  std::cout << "\tCONTINUE may be abbreviated to C\n\n"
    "\tCONTINUE\t\trun until the next breakpoint or program termination\n"
    "\tCONTINUE n\t\trun until line number \"n\"\n"
    "\tCONTINUE name:n\trun until line number \"n\" in file \"name\"\n"
    "\tCONTINUE func\t\trun until the current function returns (\"step out\")\n";
  return true;
}

// Mimic GDB's |x| command.
CommandResult
ExamineMemoryCommand::Accept(const std::string& command, const std::string& params) {
  // Just "x" is invalid.
  // TODO: Default options and remember previous selection.
  if (params.empty()) {
    fputs("Missing address.\n", stdout);
    return CR_StayCommandLoop;
  }

  // Format is x/[count][format][size] <address>
  // We require a slash.
  if (command[1] != '/') {
    fputs("Bad format specifier.\n", stdout);
    return CR_StayCommandLoop;
  }
  size_t fmt_idx = 2;

  // Skip count number
  while (isdigit(command[fmt_idx])) {
    fmt_idx++;
  }

  // Default count is 1.
  int count = 1;
  // Parse [count] number. The amount of stuff to display.
  if (fmt_idx != 2) {
    count = atoi(command.substr(2).c_str());
    if (count <= 0) {
      fputs("Invalid count.\n", stdout);
      return CR_StayCommandLoop;
    }
  }

  // Format letters are o(octal), x(hex), d(decimal), u(unsigned decimal)
  // t(binary), f(float), a(address), i(instruction), c(char) and s(string).
  char format = command[fmt_idx++];
  if (format != 'o' && format != 'x' && format != 'd' &&
    format != 'u' && format != 'f' && format != 'c' &&
    format != 's') {
    printf("Invalid format letter '%c'.\n", format);
    return CR_StayCommandLoop;
  }

  // Size letters are b(byte), h(halfword), w(word).
  char size_ltr = command[fmt_idx];

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
    return CR_StayCommandLoop;
  }

  // Skip the size letter.
  if (size_ltr != '\0')
    fmt_idx++;

  if (command[fmt_idx]) {
    fputs("Invalid output format string.\n", stdout);
    return CR_StayCommandLoop;
  }

  // Parse address.
  // We support 4 "magic" addresses:
  // $cip: instruction pointer
  // $sp: stack pointer
  // $hp: heap pointer
  // $frm: frame pointer
  cell_t address = 0;
  if (params[0] == '$') {
    if (!stricmp(params.c_str(), "$cip")) {
      address = debugger_->cip();
    }
    // TODO: adjust for selected frame like frm_.
    /*else if (!stricmp(params, "$sp")) {
      address = selected_context_->sp();
    }
    else if (!stricmp(params, "$hp")) {
      address = selected_context_->hp();
    }*/
    else if (!stricmp(params.c_str(), "$frm")) {
      address = debugger_->frm();
    }
    else {
      std::cout << "Unknown address" << params << ".\n";
      return CR_StayCommandLoop;
    }
  }
  // This is a raw address.
  else {
    address = (cell_t)strtol(params.c_str(), NULL, 0);
  }

  // Make sure we just read the plugin's memory.
  if (debugger_->ctx()->LocalToPhysAddr(address, nullptr) != SP_ERROR_NONE) {
    fputs("Address out of plugin's bounds.\n", stdout);
    return CR_StayCommandLoop;
  }

  // Print the memory
  // Create a format string for the desired output format.
  char fmt_string[16];
  switch (format) {
  case 'd':
  case 'u':
    ke::SafeSprintf(fmt_string, sizeof(fmt_string), "%%%d%c", size * 2, format);
    break;
  case 'o':
    ke::SafeSprintf(fmt_string, sizeof(fmt_string), "0%%0%d%c", size * 2, format);
    break;
  case 'x':
    ke::SafeSprintf(fmt_string, sizeof(fmt_string), "0x%%0%d%c", size * 2, format);
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
    assert(false);
    return CR_StayCommandLoop;
  }

  // Put |count| blocks of formated data on the console.
  cell_t *data;
  for (int i = 0; i < count; i++) {

    // Stop when reading out of bounds.
    // Get the data pointer we want to print.
    if (debugger_->ctx()->LocalToPhysAddr(address, &data) != SP_ERROR_NONE)
      break;

    // Put |line_break| blocks in one line.
    if (i % line_break == 0) {
      if (i > 0)
        fputs("\n", stdout);
      printf("0x%x: ", address);
    }

    // Print the data according to the specified format identifer.
    switch (format) {
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
  return CR_StayCommandLoop;
}

bool
ExamineMemoryCommand::LongHelp(const std::string& command) {
 std::cout << "\tX/FMT ADDRESS\texamine plugin memory at \"ADDRESS\"\n"
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
  ;
  return true;
}

CommandResult
FilesCommand::Accept(const std::string& command, const std::string& params) {
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

// Hack to access data in the VM currently not exposed to extensions.
// Highly depends on the VM version.
// TODO: Find a way to safely expose this "implementation detail".
namespace sp {
  class InlineFrameIterator
  {
  public:
    virtual ~InlineFrameIterator()
    {}

    // "done" should return true if, after the current frame, there are no more
    // frames to iterate.
    virtual bool done() const = 0;
    virtual void next() = 0;
    virtual int type() const = 0;
    virtual cell_t function_cip() const = 0;
    virtual cell_t cip() const = 0;
    virtual uint32_t native_index() const = 0;
  };

  class FrameIteratorHack
  {
  public:
    virtual void somefunc() = 0;
    void* ivk_;
    void* runtime_;
    intptr_t* next_exit_fp_;
    std::unique_ptr<InlineFrameIterator> frame_cursor_;
  };
}

CommandResult
FrameCommand::Accept(const std::string& command, const std::string& params) {
  if (params.empty() || !isdigit(params[0])) {
    std::cout << "Invalid syntax. Type \"? frame\" for help.\n";
    return CR_StayCommandLoop;
  }

  uint32_t frame = strtoul(params.c_str(), nullptr, 10);
  if (debugger_->framecount() <= frame) {
    std::cout << "Invalid frame. There are only " << debugger_->framecount() << " frames on the stack.\n";
    return CR_StayCommandLoop;
  }

  if (frame == debugger_->selectedframe()) {
    std::cout << "This frame is already selected.\n";
    return CR_StayCommandLoop;
  }

  IFrameIterator *frames = debugger_->basectx()->CreateFrameIterator();

  // Select this frame to operate on.
  uint32_t index = 0;
  for (; !frames->Done(); frames->Next(), index++) {
    // Iterator is at the chosen frame now.
    if (index == frame)
      break;
  }

  if (!frames->IsScriptedFrame()) {
    std::cout << frame << " is not a scripted frame.\n";
    debugger_->basectx()->DestroyFrameIterator(frames);
    return CR_StayCommandLoop;
  }

  // Get the plugin context of the target frame
  IPluginContext* ctx = frames->Context();
  // Reset the frame iterator again and count all above frames in the context.
  frames->Reset();
  index = 0;
  uint32_t num_scripted_frames = 0;
  for (; !frames->Done(); frames->Next(), index++) {
    if (frames->IsInternalFrame())
      continue;

    // Count the scripted frames in the context to find the right frm pointer.
    if (frames->IsScriptedFrame() && frames->Context() == ctx)
      num_scripted_frames++;
    // We've reached the chosen frame.
    if (index == frame)
      break;
  }

  // Update internal state for this frame.

  // FIXME: Properly expose this from the VM :D
  sp::FrameIteratorHack* frames_hack = reinterpret_cast<sp::FrameIteratorHack*>(frames);
  cell_t cip = frames_hack->frame_cursor_->cip();
  debugger_->basectx()->DestroyFrameIterator(frames);

  // TODO: Properly expose this from the VM :D
  cell_t frm = *(cell_t*)(uintptr_t(ctx) + sizeof(void*)*10 + sizeof(bool)*4 + sizeof(uint32_t)*2 + sizeof(cell_t)*3);
  // Find correct new frame pointer.
  cell_t* ptr;
  for (uint32_t i = 1; i < num_scripted_frames; i++) {
    if (ctx->LocalToPhysAddr(frm + 4, &ptr) != SP_ERROR_NONE) {
      std::cout << "Failed to find frame pointer of selected stack frame.\n";
      return CR_StayCommandLoop;
    }
    frm = *ptr;
  }

  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  uint32_t line = 0;
  debuginfo->LookupLine(cip, &line);
  debugger_->SetCurrentLine(line);

  const char *filename = nullptr;
  debuginfo->LookupFile(cip, &filename);
  debugger_->SetCurrentFile(filename);

  const char *function = nullptr;
  debuginfo->LookupFunction(cip, &function);
  debugger_->SetCurrentFunction(function);

  debugger_->UpdateSelectedContext(ctx, frame, cip, frm);
  std::cout << "Selected frame " << frame << ".\n";

  return CR_StayCommandLoop;
}

bool
FrameCommand::LongHelp(const std::string& command) {
  std::cout << "\tFRAME may be abbreviated to F\n\n"
    "\tFRAME n\tselect frame n and show/change local variables in that function\n";
  return true;
}

CommandResult
FunctionsCommand::Accept(const std::string& command, const std::string& params) {
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
NextCommand::Accept(const std::string& command, const std::string& params) {
  debugger_->SetRunmode(STEPOVER);
  return CR_LeaveCommandLoop;
}

CommandResult
PositionCommand::Accept(const std::string& command, const std::string& params) {
  debugger_->PrintCurrentPosition();
  return CR_StayCommandLoop;
}

CommandResult
PrintVariableCommand::Accept(const std::string& command, const std::string& params) {
  uint32_t idx[MAX_LEGACY_DIMENSIONS];
  memset(idx, 0, sizeof(idx));
  IPluginDebugInfo *debuginfo = debugger_->ctx()->GetRuntime()->GetDebugInfo();

  IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(debugger_->cip());
  if (params.empty() || params == "*") {
    // Display all variables that are in scope
    while (!symbol_iterator->Done()) {
      std::unique_ptr<SymbolWrapper> sym = std::make_unique<SymbolWrapper>(debugger_, symbol_iterator->Next());

      // Skip global variables in this list.
      if (params.empty() && sym->symbol()->scope() == Global)
        continue;

      // Print the name and address
      printf("%s\t<%#8x>\t%s\t", sym->ScopeToString(), (sym->symbol()->scope() == Local || sym->symbol()->scope() == Argument) ? debugger_->frm() + sym->symbol()->address() : sym->symbol()->address(), static_cast<std::string>(*sym).c_str());

      // Print the value.
      sym->DisplayVariable(idx, 0);
      fputs("\n", stdout);
    }
  }
  // Display a single variable with the given name.
  else {
    size_t index_offs = params.find('[');
    std::string name = params;

    if (index_offs != std::string::npos)
      name = params.substr(0, index_offs);

    // Parse all [x][y] dimensions
    int dim = 0;
    while (index_offs != std::string::npos && dim < MAX_LEGACY_DIMENSIONS) {
      idx[dim++] = atoi(params.substr(index_offs+1).c_str());
      index_offs = params.find('[', index_offs+1);
    }

    // find the symbol with the smallest scope
    std::unique_ptr<SymbolWrapper> sym = debugger_->symbols().FindDebugSymbol(name, debugger_->cip(), symbol_iterator);
    if (sym) {
      // Print variable address and name.
      // TODO: print type as well.
      printf("%s\t<%#8x>\t%s\t", sym->ScopeToString(), (sym->symbol()->scope() == Local || sym->symbol()->scope() == Argument) ? debugger_->frm() + sym->symbol()->address() : sym->symbol()->address(), params.c_str());
      // Print variable value.
      sym->DisplayVariable(idx, dim);
      fputs("\n", stdout);
    }
    else {
      fputs("\tSymbol not found, or not a variable\n", stdout);
    }
  }
  debuginfo->DestroySymbolIterator(symbol_iterator);
  return CR_StayCommandLoop;
}

bool
PrintVariableCommand::LongHelp(const std::string& command) {
  std::cout << "\tPRINT may be abbreviated to P\n\n"
    "\tPRINT\t\tdisplay all local variables that are currently in scope\n"
    "\tPRINT *\tdisplay all variables that are currently in scope including global variables\n"
    "\tPRINT var\tdisplay the value of variable \"var\"\n"
    "\tPRINT var[i]\tdisplay the value of array element \"var[i]\"\n";
  return true;
}

CommandResult
QuitCommand::Accept(const std::string& command, const std::string& params) {
  fputs("Clearing all breakpoints. Running normally.\n", stdout);
  debugger_->Deactivate();
  return CR_LeaveCommandLoop;
}

CommandResult
SetVariableCommand::Accept(const std::string& command, const std::string& params) {
  IPluginDebugInfo *debuginfo = debugger_->ctx()->GetRuntime()->GetDebugInfo();
  IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(debugger_->cip());

  // TODO: Allow float assignments.
  // See if this is an array assignment. Only supports single dimensional arrays.
  char varname[32], strvalue[1024];
  strvalue[0] = '\0';
  uint32_t index;
  cell_t value;
  if (sscanf(params.c_str(), " %31[^[ ][%d] = %d", varname, &index, &value) != 3) {
    index = 0;
    // Normal variable number assign
    if (sscanf(params.c_str(), " %31[^= ] = %d", varname, &value) != 2) {
      // String assign
      if (sscanf(params.c_str(), " %31[^= ] = \"%1023[^\"]\"", varname, strvalue) != 2) {
        varname[0] = '\0';
        strvalue[0] = '\0';
      }
    }
  }

  if (varname[0] != '\0') {
    // Find the symbol within the given range with the smallest scope.
    std::unique_ptr<SymbolWrapper> sym = debugger_->symbols().FindDebugSymbol(varname, debugger_->cip(), symbol_iterator);
    if (sym) {
      // User gave an integer as value
      if (strvalue[0] == '\0') {
        if (sym->SetSymbolValue(index, value)) {
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
        if (!sym->symbol()->type()->isArray()
          || sym->symbol()->type()->dimcount() != 1) {
          printf("%s is not a string.\n", varname);
        }
        else {
          if (sym->SetSymbolString(strvalue))
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
  return CR_StayCommandLoop;
}

bool
SetVariableCommand::LongHelp(const std::string& command) {
  std::cout << "\tSET var=value\t\tset variable \"var\" to the numeric value \"value\"\n"
    "\tSET var[i]=value\tset array item \"var\" to a numeric value\n"
    "\tSET var=\"value\"\t\tset string variable \"var\" to string \"value\"\n";
  return true;
}

CommandResult
StepCommand::Accept(const std::string& command, const std::string& params) {
  debugger_->SetRunmode(STEPPING);
  return CR_LeaveCommandLoop;
}

CommandResult
WatchVariableCommand::Accept(const std::string& command, const std::string& params) {
  if (params.empty()) {
    std::cout << "Missing variable name\n";
    return CR_StayCommandLoop;
  }

  SymbolManager& symbols = debugger_->symbols();
  // List watched variables right away after adding one.
  if (symbols.AddWatch(params))
    symbols.ListWatches();
  else
    std::cout << "Invalid watch\n";
  return CR_StayCommandLoop;
}

bool
WatchVariableCommand::LongHelp(const std::string& command) {
  std::cout << "\tWATCH may be abbreviated to W\n\n"
    "\tWATCH var\tset a new watch at variable \"var\"\n";
  return true;
}
