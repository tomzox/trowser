#!/usr/bin/env python3
# ------------------------------------------------------------------------ #
# Copyright (C) 2019 Th. Zoerner
# ------------------------------------------------------------------------ #
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# ------------------------------------------------------------------------ #
#
# DESCRIPTION:  This helper script was used to convert trowser.tcl from
# Tcl/Tk to Python/tk[inter]. The output of this script is far from a full
# translation, but at least removes most of the mechanical effort of syntax
# changes (e.g. intentionally excluding removal of "$"). Still, simple
# statements such as plain function calls, or widget creation, will not
# need any further work. But in particular non-trivial expressions, or all
# but the most basic build-in commands will require manual re-work.
#
# Note the script makes certain assumptions about coding style, in particular
# that functions names start with upper-case letter and variables start with
# lower-case letter. In output, Tcl/Tk-style widget names ".tree.sub.name"
# are converted to "wt.tree_sub_name" where "wt" is a class that is used to
# construct a global namespace. The latter will break when the original
# script does not use absolute path names, but instead relative ones such as
# ${wid}.sub.name; Such cases can be addressed by making ${wid} an object
# (see trowser.py for an example)
#
# ------------------------------------------------------------------------ #
import sys;
import re;

# =============================================================================
# convert sub-expressions
#
def conv_sub_expr(line):
  return line

def conv_expr(line):
  # string comparisons
  line = re.sub(r' eq +"', r' == "', line)
  line = re.sub(r' ne +"', r' != "', line)
  line = re.sub(r' eq +{}', r' == ""', line)
  line = re.sub(r' ne +{}', r' != ""', line)

  # boolean operators
  line = re.sub(r' \&\& +', r' and ', line)
  line = re.sub(r' \|\| +', r' or ', line)

  # complete expression is bareword
  if re.match(r"^[a-zA-Z_][a-zA-Z0-9_]*$", line):
    return '"' + line + '"'

  # empty list OR string
  if line == "{}": line = "None"

  # complete expression is "after" call (contains callback, to be treated specially)
  match = re.match(r"^ *\[after +(\d+|idle) +(.*)\] *$", line)
  if match:
    delay = match.group(1)
    cmd = match.group(2).strip()
    match2 = re.match(r"^\[list +([A-Z][a-zA-Z0-9_]*)( +.*|)\]$", cmd)
    if match2:
      funcn = match2.group(1)
      params = match2.group(2).strip()
      params = conv_parl(params)
      return "tk.after(%s, lambda: %s(%s))" % (delay, funcn, params)
    else:
      return "tk.after(" + delay + ", lambda: " + cmd + ")"

  # complete expression is function call
  match = re.match(r"^ *\[ *([a-zA-Z][a-zA-Z0-9_]*|(?:\.[a-z][a-zA-Z0-9_]*)+)( +.*|)\] *$", line)
  if match:
    funcn = match.group(1)
    params = match.group(2).strip()
    if not funcn in ("lindex", "lrange", "llength", "expr"): # handled below, but not in conv-parl
      if funcn.startswith("."):
        return conv_wid_call(funcn, params)
      else:
        params = conv_parl(params)
        return "%s(%s)" % (funcn, params)

  # complete expression is [expr ...]
  line = re.sub(r'^\[expr +\{(.*)\}\] *$', r'\1', line)

  # contains simple function call (i.e. without nested [...])
  def conv_fcall_match(match):
    funcn = match.group(1)
    params = match.group(2).strip()
    if funcn.startswith("."):
      return conv_wid_call(funcn, params)
    else:
      params = conv_parl(params)
      return "%s(%s)" % (funcn, params)

  line = re.sub(r"^\[([A-Z][a-zA-Z0-9_]*|(?:\.[a-z][a-zA-Z0-9_]*)+)( +[^\[\]]*|)\]", conv_fcall_match, line)

  # list lindex
  line = re.sub(r"\[lindex \$([a-zA-Z][a-zA-Z0-9_]*) +end\]", r"\1[-1]", line)
  line = re.sub(r"\[lindex \$([a-zA-Z][a-zA-Z0-9_]*) +", r"\1[", line)

  # list lrange
  line = re.sub(r"\[lrange \$([a-zA-Z][a-zA-Z0-9_]*) +([^ \[\]]+) +([^ \[\]]+)\]", r"\1[\2 : \3 + 1]", line)

  # list length
  line = re.sub(r"\[llength +\$([a-zA-Z][a-zA-Z0-9_]*)\]", r"len(\1)", line)

  # sub-expression building string for text index %d.0 (without nested func calls)
  line = re.sub(r'"\[expr \{([^\{\}\[\]]+)\}\](\.\d)"', r'"%d\2" % (\1)', line)
  line = re.sub(r'\[expr \{([^\{\}\[\]]+)\}\](\.\d)', r'%d\2 % (\1)', line)

  # sub-expression [expr ...] (without nested func calls)
  line = re.sub(r'\[expr +\{([^\{\[\]\}]*)\}\] *$', r'\1', line)

  return line

# =============================================================================
# convert parameters for function call
#
def conv_parl(params):
  orig_params = params
  ok = True
  parl = []
  while ok:
    # string "..."
    match = re.match(r'^"((\\{2})*|(.*?[^\\](\\{2})*))"( |$)', params)
    if match:
      val = match.group(1)
      sep = match.group(5)
      if not " " in val:
        parl.append(val)
        params = params[len(match.group(0)):]
        continue

    # string {...}
    match = re.match(r"^\{((\\{1})*|(.*?[^\\](\\{1})*))\}( |$)", params)
    if match:
      val = match.group(1)
      sep = match.group(5)
      parl.append('"' + val + '"')
      params = params[len(match.group(0)):]
      continue

    # command substitution [...]
    match = re.match(r"^\[((\\{1})*|(.*?[^\\](\\{1})*))\]( |$)", params)
    if match:
      val = match.group(1)
      sep = match.group(5)
      parl.append(conv_expr("[" + val + "]"))
      params = params[len(match.group(0)):]
      continue

  # sub-expression building string for text index %d.0 (without nested func calls)
    match = re.match(r'^"\[expr \{([^\{\}\[\]]+)\}\](\.\d)"( |$)', params)
    if not match:
      match = re.match(r'^\[expr \{([^\{\}\[\]]+)\}\](\.\d)( |$)', params)
    if match:
      expr = match.group(1)
      lstr = match.group(2)
      sep = match.group(3)
      parl.append('"%%d%s" %% (%s)' % (lstr, expr))
      params = params[len(match.group(0)):]
      continue

    # variable $VAR
    match = re.match(r"^\$([a-zA-Z][a-zA-Z0-9_]*)( +|$)", params)
    if match:
      val = match.group(1)
      sep = match.group(2)
      parl.append(val)
      params = params[len(match.group(0)):]
      continue

    # number 1234 or 0xAFFE
    match = re.match(r"^([0-9]+|0x[a-fA-F0-9]+)( +|$)", params)
    if match:
      val = match.group(1)
      sep = match.group(2)
      parl.append(val)
      params = params[len(match.group(0)):]
      continue

    # widget name .dialog.frame.wid
    match = re.match(r"^((?:\.[a-z][a-zA-Z0-9_]*)+)( |$)", params)
    if match:
      val = match.group(1)
      sep = match.group(2)
      parl.append(conv_widn(val))
      params = params[len(match.group(0)):]
      continue

    # bareword
    match = re.match(r"^([a-zA-Z0-9_]+)( +|$)", params)
    if match:
      val = match.group(1)
      sep = match.group(2)
      parl.append('"' + val + '"')
      params = params[len(match.group(0)):]
      continue

    if params == "":
      break

    ok = False
  if ok:
    params = ", ".join(parl)
  else:
    params = orig_params
  return params

# =============================================================================
# Convert parameter which contains a command (i.e. eval'ed)
#
def conv_cmd_par(params, bind_par=""):
  match2 = re.match(r"^([a-zA-Z][a-zA-Z0-9_]*|(?:\.[a-z][a-zA-Z0-9_]*)+)( +.*|$)", params)
  if match2:
    cmd = match2.group(1)
    cmdpar = match2.group(2).strip()
    if cmd.startswith("."):
      cmd = conv_widn(cmd)
    if cmdpar == "":
      val = cmd
    else:
      val = "lambda" + bind_par + ": " + cmd + "(" + conv_parl(cmdpar) + ")"
  elif not " " in params:
    pass
  else:
    val = "lambda: " + params

  return val

# =============================================================================
# Convert widget option list to parameter list
#
WPARL_CONST = [
  'ACTIVE', 'ALL', 'ANCHOR', 'ARC', 'BASELINE', 'BEVEL', 'BOTH', 'BOTTOM',
  'BROWSE', 'BUTT', 'CASCADE', 'CENTER', 'CHAR', 'CHECKBUTTON', 'CHORD',
  'COMMAND', 'CURRENT', 'DISABLED', 'DOTBOX', 'E', 'END', 'EW', 'EXCEPTION',
  'EXTENDED', 'FALSE', 'FIRST', 'FLAT', 'GROOVE', 'HIDDEN', 'HORIZONTAL',
  'INSERT', 'INSIDE', 'LAST', 'LEFT', 'MITER', 'MOVETO', 'MULTIPLE', 'N', 'NE',
  'NO', 'NONE', 'NORMAL', 'NS', 'NSEW', 'NUMERIC', 'NW', 'OFF', 'ON', 'OUTSIDE',
  'PAGES', 'PIESLICE', 'PROJECTING', 'RADIOBUTTON', 'RAISED', 'READABLE',
  'RIDGE', 'RIGHT', 'ROUND', 'S', 'SCROLL', 'SE', 'SEL', 'SEL_FIRST',
  'SEL_LAST', 'SEPARATOR', 'SINGLE', 'SOLID', 'SUNKEN', 'SW', 'TOP', 'TRUE',
  'UNDERLINE', 'UNITS', 'VERTICAL', 'W', 'WORD', 'WRITABLE', 'X', 'Y', 'YES']

def conv_wparl(params, is_strict):
  orig_params = params
  ok = True
  parl = []
  while ok:
    # option followed by parameter string, number, bareword or $variable
    match = re.match("^\-([a-z]+) +(\"" r"((\\{3})*|(.*?[^\\](\\{3})*))" "\"|[^\-\"\{\[]\S*)( |$)", params)
    if match:
      opt = match.group(1)
      val = match.group(2)
      sep = match.group(7)
      val_const = val.upper()
      if val_const in WPARL_CONST:
        val = val_const
      elif val.startswith('"'):
        pass
      elif re.match(r"^\$([a-zA-Z][a-zA-Z0-9_]+)$", val):
        val = val[1:]
      elif re.match(r"^([0-9]+|0x[0-9a-fA-F]+)$", val):
        pass
      elif re.match(r"^[a-z]*command$", opt):
        # function call without parameters
        pass
      elif re.match(r"^[a-z]*variable$", opt) and re.match(r"^[a-zA-Z][a-zA-Z0-9_]*", val):
        # variable reference
        pass
      elif opt == "menu" and re.match(r"^((?:\.[a-z][a-zA-Z0-9_]*)+)$", val):
        val = conv_widn(val)
      else:
        val = '"' + val + '"'
      parl.append(opt + '=' + val)
      params = params[len(match.group(0)):]
      continue

    # option followed by {...}
    match = re.match(r"^\-([a-z]+) +\{(((\\{2})*|(.*?[^\\](\\{2})*)))\}( |$)", params)
    if match:
      opt = match.group(1)
      val = match.group(2)
      sep = match.group(7)
      if re.match(r"^[a-z]*command$", opt):
        val = conv_cmd_par(val)
      else:
        val = '"' + val + '"'
      parl.append(opt + '=' + val)
      params = params[len(match.group(0)):]
      continue

    # option followed by [...]
    match = re.match(r"^\-([a-z]+) +(\[((\\{2})*|(.*?[^\\](\\{2})*))\])( |$)", params)
    if match:
      opt = match.group(1)
      val = match.group(3)
      sep = match.group(7)

      val = conv_parl(val)
      parl.append(opt + '=' + val)
      params = params[len(match.group(0)):]
      continue

    # option without parameters (i.e. followed by another option)
    match = re.match("^\-([a-z]+) +\-", params)
    if match:
      opt = match.group(1)
      sep = match.group(2)
      parl.append(opt + '=1')
      params = params[len(match.group(0) - 1):]
      continue

    # option without parameters at end of list
    match = re.match("-([a-z]+)( |$)", params)
    if match:
      opt = match.group(1)
      sep = match.group(2)
      parl.append(opt + '=1')
      params = params[len(match.group(0)):]
      continue

    if params == "":
      break

    ok = False
  if ok:
    params = ", ".join(parl)
  elif not is_strict:
    # some widget commands take plain parameters (i.e. not in form of "-opt" switches)
    params = conv_parl(orig_params)
  else:
    params = orig_params
  return params

# =============================================================================
# Convert widget name

def conv_widn(widn):
  return "wt." + re.sub(r"\.", "_", widn)[1:]

# =============================================================================
# Derive parent from widget name

def conv_wid_parent(widn):
  match = re.match(r"^(.*?)\.[^\.]+$", widn)
  if match:
    wpar = match.group(1)
    if wpar != "":
      return conv_widn(wpar)
    else:
      return "tk"
  else:
    return "wt.TODO"

# =============================================================================
# Convert widget call: name and parameters

def conv_wid_call(widn, params):
    widn = conv_widn(widn)
    # join sub-command names
    params = re.sub(r"^(tag (add|configure|lower|raise|nextrange|prevrange)|add (command|cascade|separator|checkbutton|radiobutton)|mark (set)|[xy]view moveto|selection (clear|from|to))",
                    lambda m: re.sub(r" ", r"_", m.group(0)), params)
    # make first param into widget class method
    match = re.match(r"(\S+)( +.*)?", params)
    if match:
      opn = match.group(1)
      if opn == "raise": opn = "lift"
      params = match.group(2).strip() if match.group(2) else ""
      params = conv_wparl(params, False)
      return "%s.%s(%s)" % (widn, opn, params)
    else:
      params = conv_wparl(params, False)
      return "%s %s" % (widn, params)

# =============================================================================
# Main
#
prev_close_block = 0
prev_line_cont = ""
line_idx = 0
for line in sys.stdin:
  line_idx += 1

  if line.startswith("#X#"):
    line = line[3:]

  if line[-2:] == "\\\n":
    if prev_line_cont != "":
      prev_line_cont += " " + line[:-2].strip()
    else:
      prev_line_cont = line[:-2].rstrip()
    continue

  if prev_line_cont != "":
    line = prev_line_cont + " " + line.strip()
    prev_line_cont = ""

  # function definition
  match = re.match(r"^( *)proc +(\S+) +\{*(.*)\} *\{ *$", line)
  if match:
    indent = match.group(1)
    name = match.group(2)
    params = match.group(3)
    params = re.sub(r'\{([a-zA-Z][a-zA-Z0-9_]+) (?:\{\}|"")\}', r'\1=""', params)
    params = re.sub(r"\{([a-zA-Z][a-zA-Z0-9_]+) (\d+|0x[a-fA-F0-9]+)\}", r'\1=\2', params)
    params = re.sub(r"\{([a-zA-Z][a-zA-Z0-9_]+) ([a-zA-Z][a-zA-Z0-9_]*)\}", r'\1="\2"', params)
    params = re.sub(r" +", r", ", params)
    print("%sdef %s(%s):" % (indent, name, params))
    continue

  # global definition
  match = re.match(r"^( *)global +(.*)$", line)
  if match:
    indent = match.group(1)
    list = match.group(2)
    list = re.sub(r" +", r", ", list)
    print("%sglobal %s" % (indent, list))
    continue

  # if/while
  match = re.match(r"^( *)(?:if|while) +\{(.*)\} *\{ *$", line)
  if match:
    indent = match.group(1)
    expr = conv_expr(match.group(2))
    print("%sif %s:" % (indent, expr))
    continue

  # elseif
  match = re.match(r"^( *)\} *elseif +\{(.*)\} *\{ *$", line)
  if match:
    indent = match.group(1)
    expr = conv_expr(match.group(2))
    print("%selif %s:" % (indent, expr))
    continue

  # else
  match = re.match(r"^( *)\} *else *\{ *$", line)
  if match:
    indent = match.group(1)
    print("%selse:" % (indent))
    continue

  # closing bracket
  match = re.match(r"^() *\} *$", line)
  if match:
    indent = match.group(1)
    if (prev_close_block + 1 != line_idx):
      print(indent)
    prev_close_block = line_idx
    continue

  # foreach
  match = re.match(r"^( *)foreach (\S+) +(\{(.*)\}|\[(.*)\]|\$[a-zA-Z][a-zA-Z0-9_]*(?:\([^\)]*\))?) *\{ *$", line)
  if match:
    indent = match.group(1)
    varn = match.group(2)
    expr = match.group(3)
    if expr.startswith("{"):
      expr = conv_expr(expr[1:-2])
    elif expr.startswith("["):
      expr = conv_expr(expr)
    else:
      expr = expr[1:]
    print("%sfor %s in %s:" % (indent, varn, expr))
    continue

  # return expr
  match = re.match(r"^( *)return +(.*)$", line)
  if match:
    indent = match.group(1)
    expr = conv_expr(match.group(2))
    print("%sreturn %s" % (indent, expr))
    continue

  # variable assignment
  match = re.match(r"^( *)set +([a-zA-Z][a-zA-Z0-9_]*) +(.*)$", line)
  if match:
    indent = match.group(1)
    varn = match.group(2)
    expr = conv_expr(match.group(3))
    print("%s%s = %s" % (indent, varn, expr))
    continue

  # array variable assignment
  match = re.match(r"^( *)set +([a-zA-Z][a-zA-Z0-9_]*)\(([^\)]*)\) +(.*)$", line)
  if match:
    indent = match.group(1)
    varn = match.group(2)
    idx = conv_expr(match.group(3))
    expr = conv_expr(match.group(4))
    if expr == "{}": expr = None  # might also be ""
    print("%s%s[%s] = %s" % (indent, varn, idx, expr))
    continue

  # variable unset
  match = re.match(r"^( *)unset +(?:\-nocomplain +)?(.*)$", line)
  if match:
    indent = match.group(1)
    varl = re.sub(r"  +", r" ", match.group(2).strip())
    for varn in varl.split(" "):
      print("%s%s = None" % (indent, varn))
    continue

  # incr
  match = re.match(r"^( *)incr ([a-zA-Z][a-zA-Z0-9_]*) +(\S.*)$", line)
  if match:
    indent = match.group(1)
    varn = match.group(2)
    val = match.group(3).strip()
    match = re.match(r"\-(\d+)", val)
    if match:
      print("%s%s -= %s" % (indent, varn, match.group(1)))
    else:
      print("%s%s += %s" % (indent, varn, val))
    continue

  # incr with step parameter
  match = re.match(r"^( *)incr ([a-zA-Z][a-zA-Z0-9_]*) *$", line)
  if match:
    indent = match.group(1)
    varn = match.group(2)
    print("%s%s += 1" % (indent, varn))
    continue

  # list append
  match = re.match(r"^( *)lappend ([a-zA-Z][a-zA-Z0-9_]*) +(\S.*)$", line)
  if match:
    indent = match.group(1)
    varn = match.group(2)
    expr = conv_expr(match.group(3).strip())
    print("%s%s.append(%s)" % (indent, varn, expr))
    continue

  # function call
  match = re.match(r"^( *)([A-Z][a-zA-Z0-9_]*|scan)( +.*|)$", line)
  if match:
    indent = match.group(1)
    funcn = match.group(2)
    params = match.group(3).strip()
    params = conv_parl(params)
    print("%s%s(%s)" % (indent, funcn, params))
    continue

  # widget function
  match = re.match(r"^( *)((?:\.[a-z][a-zA-Z0-9_]*)+) +(.*)$", line)
  if match:
    indent = match.group(1)
    widn = match.group(2)
    params = match.group(3).strip()

    print(indent + conv_wid_call(widn, params))
    continue

  # widget creation
  match = re.match(r"^( *)(label|frame|button|message|listbox|spinbox|radiobutton|checkbutton|canvas|toplevel|text|entry|menu|menubar|scrollbar) +((?:\.[a-z][a-zA-Z0-9_]*)+)( +.*|)$", line)
  if match:
    indent = match.group(1)
    cmd = match.group(2)
    widn = match.group(3)
    params = match.group(4).strip()

    cmd = cmd[0].upper() + cmd[1:]
    wid_parent = conv_wid_parent(widn)
    widn = conv_widn(widn)

    if params != "":
      params = conv_wparl(params, True)
      print("%s%s = %s(%s, %s)" % (indent, widn, cmd, wid_parent, params))
    else:
      print("%s%s = %s(%s)" % (indent, widn, cmd, wid_parent))
    continue

  # pack et.al.
  match = re.match(r"^( *)(pack|grid|destroy|focus|raise|wm [a-z_]+) +((?:\.[a-z][a-zA-Z0-9_]*)+)( .*|$)", line)
  if match:
    indent = match.group(1)
    cmdn = match.group(2)
    widl = [match.group(3)]
    params = match.group(4).strip()

    cmdn = re.sub(r" ", r"_", cmdn)
    if cmdn == "focus": cmdn = "focus_set"
    elif cmdn == "raise": cmdn = "lift"

    while True:
      match = re.match(r"^((?:\.[a-z][a-zA-Z0-9_]*)+)( +|$)", params)
      if match:
        widl.append(match.group(1))
        params = params[len(match.group(0)):]
      else:
        break

    params = conv_wparl(params, True)

    for widn in widl:
      widn = conv_widn(widn)
      print("%s%s.%s(%s)" % (indent, widn, cmdn, params))
    continue

  # bind
  match = re.match(r"^( *)bind +((?:\.[a-z][a-zA-Z0-9_]*)+|\$[a-zA-Z][a-zA-Z0-9_]+)( .*|$)", line)
  if match:
    indent = match.group(1)
    widn = match.group(2)
    params = match.group(3).strip()

    widn = conv_widn(widn)

    match2 = re.match(r"^(\<[a-zA-Z][a-zA-Z0-9\-]*\>) +", params)
    if match2:
      kpar = '"' + match2.group(1) + '", '
      params = params[len(match2.group(0)):]
    else:
      kpar = ""

    match2 = re.match(r"^\{(.*)\}$", params)
    if match2:
      params = match2.group(1)
    match2 = re.match(r'^\"(.*)\"$', params)
    if match2:
      params = match2.group(1)

    match2 = re.match(r"^(.*)\; *break$", params)
    if match2:
      params = "lambda e:BindCallAndBreak(" +  conv_cmd_par(match2.group(1)) + ")"
    else:
      params = conv_cmd_par(params, " e")

    print("%s%s.bind(%s%s)" % (indent, widn, kpar, params))
    continue

  print(line, end="")
