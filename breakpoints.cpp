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

#include "breakpoints.h"
#include "debugger.h"
#include <iostream>

using namespace SourcePawn;

bool
BreakpointManager::Initialize()
{
  return breakpoint_map_.init();
}

size_t
BreakpointManager::GetBreakpointCount() const
{
  return breakpoint_map_.elements();
}

// Breakpoint handling
bool
BreakpointManager::CheckBreakpoint(cell_t cip)
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
BreakpointManager::AddBreakpoint(const std::string& file, cell_t addr, bool temporary)
{
  Breakpoint *bp;
  {
    // See if there's already a breakpoint in place here.
    BreakpointMap::Insert p = breakpoint_map_.findForAdd(addr);
    if (p.found())
      return p->value;

    const char *realname = nullptr;
    IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
    debuginfo->LookupFunction(addr, &realname);

    bp = new Breakpoint(debuginfo, addr, realname, temporary);
    breakpoint_map_.add(p, addr, bp);
  }

  return bp;
}

Breakpoint *
BreakpointManager::AddBreakpoint(const std::string& file, const std::string& function, bool temporary)
{
  std::string targetfile = debugger_->FindFileByPartialName(file);
  if (targetfile.empty())
    return nullptr;

  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  // Is there a function named like that in the file?
  uint32_t addr;
  if (debuginfo->LookupFunctionAddress(function.c_str(), targetfile.c_str(), &addr) != SP_ERROR_NONE)
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
BreakpointManager::ClearBreakpoint(int number)
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
BreakpointManager::ClearBreakpoint(Breakpoint * bp)
{
  BreakpointMap::Result res = breakpoint_map_.find(bp->addr());
  if (!res.found())
    return false;

  breakpoint_map_.remove(res);
  return true;
}

void
BreakpointManager::ClearAllBreakpoints()
{
  breakpoint_map_.clear();
}

int
BreakpointManager::FindBreakpoint(const std::string& input)
{
  std::string filename;
  // check if a filename precedes the breakpoint location
  std::string breakpoint = ParseBreakpointLine(input, &filename);

  // Shortcut for breakpoint indices.
  if (filename.empty() && isdigit(breakpoint[0]))
    return atoi(breakpoint.c_str());

  // Need a file for the line.
  if (filename.empty())
    return -1;

  Breakpoint *bp;
  const char *fname;
  uint32_t line;
  uint32_t number = 0;
  IPluginDebugInfo *debuginfo = debugger_->GetDebugInfo();
  for (BreakpointMap::iterator iter = breakpoint_map_.iter(); !iter.empty(); iter.next()) {
    bp = iter->value;
    if (debuginfo->LookupFile(bp->addr(), &fname) != SP_ERROR_NONE)
      fname = nullptr;
    number++;

    // Correct file?
    if (fname != nullptr && filename == fname) {
      // A function breakpoint
      if (bp->name() != nullptr && breakpoint == bp->name())
        return number;

      // Line breakpoint
      if (debuginfo->LookupLine(bp->addr(), &line) == SP_ERROR_NONE &&
        line == strtoul(breakpoint.c_str(), NULL, 10) - 1)
        return number;
    }
  }
  return -1;
}

void
BreakpointManager::ListBreakpoints()
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

const std::string
BreakpointManager::ParseBreakpointLine(const std::string& input, std::string* filename)
{
  // check if a filename precedes the breakpoint location
  size_t sep_offs = input.find(':');
  if (sep_offs != std::string::npos) {
    std::string partial_filename = input.substr(0, sep_offs);
    // the user may have given a partial filename (e.g. without a path), so
    // walk through all files to find a match
    const char* found_filename = debugger_->FindFileByPartialName(partial_filename);
    if (!found_filename) {
      std::cout << "Invalid filename.\n";
      return "";
    }
    *filename = found_filename;
    return input.substr(sep_offs + 1);
  }
  return input;
}
