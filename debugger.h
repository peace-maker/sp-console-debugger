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

#include "sp_vm_api.h"
#include "amtl/am-hashmap.h"
#include "console-helpers.h"

enum Runmode {
  STEPPING, /* step into functions */
  STEPOVER, /* step over functions */
  STEPOUT, /* run until the function returns */
  RUNNING, /* just run */
};

class Breakpoint;

class Debugger {
public:
  Debugger(SourcePawn::IPluginContext *context);
  bool Initialize();
  bool active();
  void Activate();
  void Deactivate();

  void HandleInput(cell_t cip, cell_t frm, bool isBp);
  void ListCommands(const char *command);

private:
  void HandleHelpCmd(const char *line);
  void HandleQuitCmd();
  bool HandleGoCmd(char *params);
  /*void HandleFunctionListCmd();
  void HandleFrameCmd(char *params);*/
  void HandleBreakpointCmd(char *command, char *params);
  void HandleClearBreakpointCmd(char *params);
  void HandleVariableDisplayCmd(char *params);
  void HandleSetVariableCmd(char *params);
  void HandleFilesListCmd();
 // void HandleDisplayFormatChangeCmd(char *params);
  void HandlePrintPositionCmd();
  /*void HandleWatchCmd(char *params);
  void HandleClearWatchCmd(char *params);
  void HandleDumpMemoryCmd(char *command, char *params);*/

public:
  Runmode runmode();
  void SetRunmode(Runmode runmode);
  cell_t lastframe();
  void SetLastFrame(cell_t lastfrm);
  uint32_t lastline();
  void SetLastLine(uint32_t line);
  uint32_t breakcount();
  void SetBreakCount(uint32_t breakcount);
  const char *currentfile();
  void SetCurrentFile(const char *file);

  Breakpoint *AddBreakpoint(const char *file, uint32_t line, bool temporary);
  Breakpoint *AddBreakpoint(const char *file, const char *function, bool temporary);
  bool ClearBreakpoint(int number);
  bool ClearBreakpoint(Breakpoint *);
  void ClearAllBreakpoints();
  bool CheckBreakpoint(cell_t cip);
  int FindBreakpoint(char *breakpoint);
  void ListBreakpoints();
  char *ParseBreakpointLine(char *input, const char **filename);
  size_t GetBreakpointCount();

private:
  void DumpStack();
  void PrintValue(SourcePawn::ISymbolType &type, long value);
  void DisplayVariable(SourcePawn::IDebugSymbol *sym, uint32_t index[], uint32_t idxlevel);
  SourcePawn::IDebugSymbol *FindDebugSymbol(const char* name, cell_t scopeaddr, SourcePawn::IDebugSymbolIterator* symbol_iterator);
  
  bool GetSymbolValue(SourcePawn::IDebugSymbol *sym, uint32_t index, cell_t* value);
  bool SetSymbolValue(SourcePawn::IDebugSymbol *sym, uint32_t index, cell_t value);
  const char* GetSymbolString(SourcePawn::IDebugSymbol *sym);
  bool SetSymbolString(SourcePawn::IDebugSymbol *sym, const char* value);
  bool GetEffectiveSymbolAddress(SourcePawn::IDebugSymbol *sym, cell_t *address);

private:
  const char *FindFileByPartialName(const char *partialname);
  const char *ScopeToString(SourcePawn::SymbolScope scope);

public:
  struct BreakpointMapPolicy {

    static inline uint32_t hash(ucell_t value) {
      return ke::HashInteger<4>(value);
    }

    static inline bool matches(ucell_t a, ucell_t b) {
      return a == b;
    }
  };
  typedef ke::HashMap<ucell_t, Breakpoint *, BreakpointMapPolicy> BreakpointMap;

  BreakpointMap breakpoint_map_;

private:
  SourcePawn::IPluginContext * context_;
  Runmode runmode_;
  cell_t lastfrm_;
  uint32_t lastline_;
  uint32_t breakcount_;
  const char *currentfile_;
  bool active_;

  // Temporary variables to use inside command loop
  cell_t cip_;
  cell_t frm_;
  uint32_t frame_count_;
  uint32_t selected_frame_;
  SourcePawn::IPluginContext *selected_context_;
};

class Breakpoint {
public:
  Breakpoint(SourcePawn::IPluginDebugInfo *debuginfo, ucell_t addr, const char *name, bool temporary = false)
    : debuginfo_(debuginfo),
    addr_(addr),
    name_(name),
    temporary_(temporary)
  {}

  ucell_t addr() {
    return addr_;
  }
  const char *name() {
    return name_;
  }
  bool temporary() {
    return temporary_;
  }
  const char *filename() {
    const char *filename;
    if (debuginfo_->LookupFile(addr_, &filename) == SP_ERROR_NONE)
      return SkipPath(filename);
    return "";
  }
  uint32_t line() {
    uint32_t line;
    if (debuginfo_->LookupLine(addr_, &line) == SP_ERROR_NONE)
      return line;
    return 0;
  }
private:
  SourcePawn::IPluginDebugInfo * debuginfo_; /* debug info of plugin the address is in */
  ucell_t addr_; /* address (in code or data segment) */
  const char *name_; /* name of the symbol (function) */
  bool temporary_; /* delete breakpoint when hit? */
};