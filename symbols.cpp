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
#include "symbols.h"
#include "debugger.h"
#include <sstream>
#include <cstring>

bool
SymbolManager::Initialize() {
  return watch_table_.init();
}

std::unique_ptr<SymbolWrapper>
SymbolManager::FindDebugSymbol(const std::string& name, cell_t scopeaddr, SourcePawn::IDebugSymbolIterator* symbol_iterator)
{
  cell_t codestart = 0, codeend = 0;
  const SourcePawn::IDebugSymbol* matching_symbol = nullptr;

  SourcePawn::IPluginDebugInfo *debuginfo = debugger_->ctx()->GetRuntime()->GetDebugInfo();
  while (!symbol_iterator->Done()) {
    const SourcePawn::IDebugSymbol* sym = nullptr;
    while (!symbol_iterator->Done()) {
      sym = symbol_iterator->Next();
      // find (next) matching variable
      if (name == sym->name())
        break;
    }

    // check the range, keep a pointer to the symbol with the smallest range
    if (name == sym->name() &&
      ((codestart == 0 && codeend == 0) ||
      (sym->codestart() >= codestart &&
        sym->codeend() <= codeend)))
    {
      matching_symbol = sym;
      codestart = sym->codestart();
      codeend = sym->codeend();
    }
  }
  if (!matching_symbol)
    return nullptr;
  return std::make_unique<SymbolWrapper>(debugger_, matching_symbol);
}

bool
SymbolManager::AddWatch(const std::string& symname)
{
  WatchTable::Insert i = watch_table_.findForAdd(symname);
  if (i.found())
    return false;
  watch_table_.add(i, symname);
  return true;
}

bool
SymbolManager::ClearWatch(const std::string& symname)
{
  WatchTable::Result r = watch_table_.find(symname);
  if (!r.found())
    return false;
  watch_table_.remove(r);
  return true;
}

bool
SymbolManager::ClearWatch(uint32_t num)
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
SymbolManager::ClearAllWatches()
{
  watch_table_.clear();
}

void
SymbolManager::ListWatches()
{
  SourcePawn::IPluginDebugInfo *debuginfo = debugger_->ctx()->GetRuntime()->GetDebugInfo();
  SourcePawn::IDebugSymbolIterator* symbol_iterator = debuginfo->CreateSymbolIterator(debugger_->cip());

  uint32_t num = 1;
  for (WatchTable::iterator iter = WatchTable::iterator(&watch_table_); !iter.empty(); iter.next()) {
    std::string symname = *iter;

    size_t index_offs = symname.find('[');
    std::string name = symname;

    if (index_offs != std::string::npos)
      name = symname.substr(0, index_offs);

    // Parse all [x][y] dimensions
    int dim = 0;
    uint32_t idx[sDIMEN_MAX];
    while (index_offs != std::string::npos && dim < sDIMEN_MAX) {
      idx[dim++] = atoi(symname.substr(index_offs + 1).c_str());
      index_offs = symname.find('[', index_offs + 1);
    }

    // find the symbol with the smallest scope
    symbol_iterator->Reset();
    std::unique_ptr<SymbolWrapper> sym = FindDebugSymbol(name, debugger_->cip(), symbol_iterator);
    if (sym) {
      printf("%d  %-12s ", num++, symname.c_str());
      sym->DisplayVariable(idx, dim);
      printf("\n");
    }
    else {
      printf("%d  %-12s (not in scope)\n", num++, symname.c_str());
    }
  }
  debuginfo->DestroySymbolIterator(symbol_iterator);
}

void
SymbolWrapper::DisplayVariable(uint32_t index[], uint32_t idxlevel)
{
  assert(index != nullptr);

  // first check whether the variable is visible at all
  if (debugger_->cip() < symbol_->codestart() || debugger_->cip() > symbol_->codeend()) {
    fputs("(not in scope)", stdout);
    return;
  }

  const SourcePawn::ISymbolType* type = symbol_->type();
  if (type->isArray()) {
    // Check whether any of the indices are out of range
    uint32_t dim;
    for (dim = 0; dim < idxlevel && dim < type->dimcount(); dim++) {
      if (type->dimension(dim) > 0 && index[dim] >= type->dimension(dim))
        break;
    }
    if (dim < idxlevel) {
      fputs("(index out of range)", stdout);
      return;
    }
  }

  cell_t value;
  if (type->isEnumStruct()) {
    uint32_t idx[sDIMEN_MAX];
    memset(idx, 0, sizeof(idx));
    fputs("{", stdout);
    for (uint32_t i = 0; i < type->esfieldcount(); i++) {
      if (i > 0)
        fputs(", ", stdout);

      const SourcePawn::IEnumStructField* field = type->esfield(i);
      printf("%s: ", field->name());
      if (field->type()->isArray())
        fputs("(array)", stdout);
      else {
        if (GetSymbolValue(field->offset(), &value))
          PrintValue(field->type(), value);
        else
          fputs("?", stdout);
      }
    }
    fputs("}", stdout);
  }
  // Print first dimension of array
  else if (type->isArray() && idxlevel == 0)
  {
    // Print string
    if (type->isString()) {
      const char *str = GetSymbolString();
      if (str != nullptr)
        printf("\"%s\"", str); // TODO: truncate to 40 chars
      else
        fputs("NULL_STRING", stdout);
    }
    // Print one-dimensional array
    else if (type->dimcount() == 1) {
      uint32_t len = type->dimension(0);
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
        if (GetSymbolValue(i, &value))
          PrintValue(type, value);
        else
          fputs("?", stdout);
      }
      if (len < type->dimension(0) || type->dimension(0) == 0)
        fputs(",...", stdout);
      fputs("}", stdout);
    }
    // Not supported..
    else {
      fputs("(multi-dimensional array)", stdout);
    }
  }
  else if (!type->isArray() && idxlevel > 0) {
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
        if (!GetSymbolValue(base, &value))
          break;
        base += value / sizeof(cell_t);
      }
    }

    if (GetSymbolValue(base + index[dim], &value) &&
      type->dimcount() == idxlevel)
      PrintValue(type, value);
    else if (type->dimcount() != idxlevel)
      fputs("(invalid number of dimensions)", stdout);
    else
      fputs("?", stdout);
  }
}

void
SymbolWrapper::PrintValue(const SourcePawn::ISymbolType* type, long value)
{
  if (type->isFloat32()) {
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
  else if (type->isString()) {
    if (value < 0x20 || value >= 0x7f)
      printf("'\\x%02lx'", value);
    else
      printf("'%c'", (char)value);
  }
  else {
    printf("%ld", value);
  }
  /*case DISP_HEX:
    printf("%lx", value);
    break;*/
}

const char *
SymbolWrapper::ScopeToString()
{
  SourcePawn::SymbolScope scope = symbol_->scope();
  static const char *scope_names[] = { "glb", "loc", "sta", "arg" };
  if (scope < 0 || static_cast<size_t>(scope) >= sizeof(scope_names) / sizeof(scope_names[0]))
    return "unk";
  return scope_names[scope];
}

bool
SymbolWrapper::GetSymbolValue(uint32_t index, cell_t* value)
{
  // Tried to index a non-array.
  if (index > 0 && !symbol_->type()->isArray())
    return false;

  // Index out of bounds.
  if (symbol_->type()->dimcount() > 0 && index >= symbol_->type()->dimension(0))
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  // Support indexing into string.
  uint32_t element_size = sizeof(cell_t);
  if (symbol_->type()->isString())
    element_size = sizeof(char);

  // Resolve index into array.
  cell_t *vptr;
  if (debugger_->ctx()->LocalToPhysAddr(addr + index * element_size, &vptr) != SP_ERROR_NONE)
    return false;

  if (vptr != nullptr) {
    *value = *vptr;
    // Only return the requested character.
    if (symbol_->type()->isString())
      *value &= 0xff;
  }

  return vptr != nullptr;
}

bool
SymbolWrapper::SetSymbolValue(uint32_t index, cell_t value)
{
  // Tried to index a non-array.
  if (index > 0 && !symbol_->type()->isArray())
    return false;

  // Index out of bounds.
  if (symbol_->type()->dimcount() > 0 && index >= symbol_->type()->dimension(0))
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  // Resolve index into array.
  cell_t *vptr;
  if (debugger_->ctx()->LocalToPhysAddr(addr + index * sizeof(cell_t), &vptr) != SP_ERROR_NONE)
    return false;

  if (vptr != nullptr)
    *vptr = value;

  return vptr != nullptr;
}

const char*
SymbolWrapper::GetSymbolString()
{
  // Make sure we're dealing with a one-dimensional array.
  if (!symbol_->type()->isArray() || symbol_->type()->dimcount() != 1)
    return nullptr;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return nullptr;

  char *str;
  if (debugger_->ctx()->LocalToStringNULL(addr, &str) != SP_ERROR_NONE)
    return nullptr;
  return str;
}

bool
SymbolWrapper::SetSymbolString(const char* value)
{
  // Make sure we're dealing with a one-dimensional array.
  if (!symbol_->type()->isArray() || symbol_->type()->dimcount() != 1)
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  return debugger_->ctx()->StringToLocalUTF8(addr, symbol_->type()->dimension(0), value, NULL) == SP_ERROR_NONE;
}

bool
SymbolWrapper::GetEffectiveSymbolAddress(cell_t *address)
{
  cell_t base = symbol_->address();
  // addresses of local vars are relative to the frame
  if (symbol_->scope() == SourcePawn::Local || symbol_->scope() == SourcePawn::Argument)
    base += debugger_->frm();

  // a reference. arrays are always passed by reference.
  cell_t *addr;
  if (symbol_->type()->isReference() || (symbol_->type()->isArray() && symbol_->scope() == SourcePawn::Argument)) {
    if (debugger_->ctx()->LocalToPhysAddr(base, &addr) != SP_ERROR_NONE)
      return false;

    assert(addr != nullptr);
    base = *addr;
  }

  *address = base;
  return true;
}

SymbolWrapper::operator std::string() const
{
  return renderType(symbol_->type(), symbol_->name() ? symbol_->name() : "");
}

std::string
SymbolWrapper::renderType(const SourcePawn::ISymbolType* type, const std::string& name) const
{
  std::stringstream output;
  if (type->isConstant())
    output << "const ";

  if (type->isReference())
    output << "&";

  if (type->isBoolean())
    output << "bool";
  else if (type->isInt32())
    output << "int";
  else if (type->isFloat32())
    output << "float";
  else if (type->isString())
    output << "char";
  else if (type->isAny())
    output << "any";
  else if (type->isInt32())
    output << "int";
  else if (type->isVoid())
    output << "void";
  //else if (type->isTopFunction())
  //  output << "Function";
  else if (type->isEnum() || type->isEnumStruct() || type->isStruct())
    output << type->name();
  else if (type->isObject())
    output << "<object>";
  else
    output << "<unknown>";

  if (type->isArray()) {
    // Fixed array dimensions are specified after the variable name
    // instead of in the type directly.
    bool hasPostDimensions = true;
    std::stringstream dimensions;
    for (uint32_t dim = 0; dim < type->dimcount(); dim++) {
      if (type->dimension(dim) > 0)
        dimensions << '[' << type->dimension(dim) << ']';
      else {
        dimensions << "[]";
        hasPostDimensions = false;
      }
    }

    if (!hasPostDimensions)
      output << dimensions.str();
    if (!name.empty())
      output << ' ' + name;
    if (hasPostDimensions)
      output << dimensions.str();
  }
  else if (!name.empty())
    output << ' ' << name;

  return output.str();
}

std::ostream&
operator<<(std::ostream& strm, const SymbolWrapper& sym)
{
  return strm << sym.renderType(sym.symbol()->type(), sym.symbol()->name() ? sym.symbol()->name() : "");
}
