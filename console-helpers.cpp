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
#include "console-helpers.h"

#if defined KE_POSIX
#include <unistd.h>
#include <termios.h>
#include <string.h>
#endif
#if defined KE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Convince debugging console to act more like an
// interactive input.
#if defined KE_POSIX
tcflag_t
GetTerminalLocalMode()
{
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  return term.c_lflag;
}

void
SetTerminalLocalMode(tcflag_t flag)
{
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag = flag;
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

unsigned int
EnableTerminalEcho()
{
  tcflag_t flags = GetTerminalLocalMode();
  tcflag_t old_flags = flags;
  flags |= (ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | IEXTEN);
  SetTerminalLocalMode(flags);
  return old_flags;
}

void
ResetTerminalEcho(unsigned int flag)
{
  SetTerminalLocalMode(flag);
}

unsigned int
DisableEngineWatchdog()
{
  return alarm(0);
}

void
ResetEngineWatchdog(unsigned int timeout)
{
  alarm(timeout);
}
#elif defined KE_WINDOWS
unsigned int
EnableTerminalEcho()
{
  DWORD mode, old_mode;
  HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hConsole, &mode);
  old_mode = mode;
  mode |= (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE);
  SetConsoleMode(hConsole, mode);
  return old_mode;
}

void
ResetTerminalEcho(unsigned int mode)
{
  HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
  SetConsoleMode(hConsole, mode);
}

unsigned int
DisableEngineWatchdog()
{
  return 0;
}

void
ResetEngineWatchdog(unsigned int timeout)
{
}
#endif

char *
TrimString(char *string)
{
  int pos;

  /* strip leading white space */
  while (*string != '\0' && *string <= ' ')
    memmove(string, string + 1, strlen(string));
  /* strip trailing white space */
  for (pos = strlen(string); pos>0 && string[pos - 1] <= ' '; pos--)
    string[pos - 1] = '\0';
  return string;
}

char *
SkipWhitespace(char *str)
{
  while (*str == ' ' || *str == '\t')
    str++;
  return (char*)str;
}

const char *
SkipPath(const char *str)
{
  if (str == nullptr)
    return nullptr;

  const char *p1 = strrchr(str, '\\');
  /* DOS/Windows pathnames */
  if (p1 != nullptr)
    p1++;
  else
    p1 = str;
  if (p1 == str && p1[1] == ':')
    p1 = str + 2;
  /* Unix pathnames */
  const char *p2 = strrchr(str, '/');
  if (p2 != nullptr)
    p2++;
  else
    p2 = str;
  return p1>p2 ? p1 : p2;
}
