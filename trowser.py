#!/usr/bin/env python3

# ------------------------------------------------------------------------ #
# Copyright (C) 2007-2010,2019-2020,2023 T. Zoerner
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
# DESCRIPTION:  Browser for line-oriented text files, e.g. debug traces.
#
# ------------------------------------------------------------------------ #

import sys
import os
import errno
import bisect
import threading
from datetime import datetime
from datetime import timedelta
import time
import tempfile
import json
import re
import traceback
import tkinter
import tkinter.font as tkf
from tkinter import *
from tkinter import messagebox
from tkinter import filedialog
from tkinter import colorchooser

#
# This function is used during start-up to define fonts, colors and
# global event binding tags.
#
def InitResources():
  global font_normal, font_bold, font_hlink, img_marker

  # override default font for the Tk message box
  tk.eval("option add *Dialog.msg.font {helvetica 9 bold} userDefault")

  # place the "Help" menu button on the far right of the menu bar
  tk.eval("option add *Menu.useMotifHelp true")

  # fonts for text and label widgets
  font_normal = tkf.nametofont("TkDefaultFont")
  font_bold = DeriveFont(font_normal, 0, "bold")
  font_hlink = DeriveFont(font_normal, -2, "underline")

  # bindings to allow scrolling a text widget with the mouse wheel
  tk.bind_class("TextWheel", "<Button-4>", lambda e: e.widget.yview_scroll(-3, "units"))
  tk.bind_class("TextWheel", "<Button-5>", lambda e: e.widget.yview_scroll(3, "units"))
  tk.bind_class("TextWheel", "<MouseWheel>", lambda e: e.widget.yview_scroll(int(-(e.delta / 120) * 3), "units"))

  text_modifier_events = (
      "<<Clear>>", "<<Cut>>", "<<Paste>>", "<<PasteSelection>>",
      "<<Redo>>", "<<Undo>>", "<<TkAccentBackspace>>", "<Key-BackSpace>",
      "<Key>", "<Key-Delete>", "<Key-Insert>", "<Key-Return>",
      # Not modifiers, but events are overridden below
      "<Key-Up>", "<Key-Down>", "<Key-Left>", "<Key-Right>",
      "<Key-Tab>", "<Control-Key-c>")

  for event in set(tk.bind_class("Text")) - set(text_modifier_events):
      tk.bind_class("TextReadOnly", event, tk.bind_class("Text", event))

  # since Tk 8.6 there are no event handlers for <Key-Up> in tag Text anymore
  tk.bind_class("TextReadOnly", "<Key-Up>", lambda e: CursorMoveUpDown(wt.f1_t, -1))
  tk.bind_class("TextReadOnly", "<Key-Down>", lambda e: CursorMoveUpDown(wt.f1_t, 1))
  tk.bind_class("TextReadOnly", "<Key-Left>", lambda e: CursorMoveLeftRight(wt.f1_t, -1))
  tk.bind_class("TextReadOnly", "<Key-Right>", lambda e: CursorMoveLeftRight(wt.f1_t, 1))

  # bindings for a selection text widget (listbox emulation)
  # (uses non-standard cursor movement event bindings, hence not added here)
  for event in ( "<Button-2>", "<B2-Motion>", "<Key-Prior>", "<Key-Next>",
                 "<Shift-Key-Tab>", "<Control-Key-Tab>", "<Control-Shift-Key-Tab>" ):
    tk.bind_class("TextSel", event, tk.bind_class("Text", event))

  for event in ( "<Button-4>", "<Button-5>", "<MouseWheel>" ):
    tk.bind_class("TextReadOnly", event, tk.bind_class("TextWheel", event))
    tk.bind_class("TextSel", event, tk.bind_class("TextWheel", event))

  tk.bind_class("TextReadOnly", "<Control-Key-c>", tk.bind_class("Text", "<<Copy>>"))
  tk.bind_class("TextReadOnly", "<Key-Tab>", tk.bind_class("Text", "<Control-Key-Tab>"))
  tk.bind_class("TextSel", "<Key-Tab>", tk.bind_class("Text", "<Control-Key-Tab>"))

  # bookmark image which is inserted into the text widget
  img_marker = tk.eval("image create photo -data "
                       "R0lGODlhBwAHAMIAAAAAuPj8+Hh8+JiYmDAw+AAAAAAAAAAAACH"
                       "5BAEAAAEALAAAAAAHAAcAAAMUGDGsSwSMJ0RkpEIG4F2d5DBTkAAAOw==")


#
# This function creates the main window of the trace browser, including the
# menu at the top, the text area and a vertical scrollbar in the middle, and
# the search control dialog at the bottom.
#
def CreateMainWindow():
  global font_content, col_bg_content, col_fg_content, fmt_selection
  global win_geom, fmt_find, fmt_findinc
  global tlb_find, tlb_hall, tlb_case, tlb_regexp

  # menubar at the top of the main window
  wt.menubar = Menu(tk)

  wt.menubar_ctrl = Menu(wt.menubar, tearoff=0, postcommand=lambda:MenuPosted())
  wt.menubar_ctrl.add_command(label="Open file...", command=lambda:MenuCmd_OpenFile())
  wt.menubar_ctrl.add_command(label="Reload current file", state=DISABLED, command=lambda:MenuCmd_Reload())
  wt.menubar_ctrl.add_separator()
  wt.menubar_ctrl.add_command(label="Discard above cursor...", command=lambda:MenuCmd_Discard(False))
  wt.menubar_ctrl.add_command(label="Discard below cursor...", command=lambda:MenuCmd_Discard(True))
  wt.menubar_ctrl.add_separator()
  wt.menubar_ctrl.add_command(label="Font selection...", command=lambda:FontList_OpenDialog())
  wt.menubar_ctrl.add_separator()
  wt.menubar_ctrl.add_command(label="Quit", command=lambda:UserQuit())

  wt.menubar_search = Menu(wt.menubar, tearoff=0, postcommand=lambda:MenuPosted())
  wt.menubar_search.add_command(label="Search history...", command=lambda:SearchHistory_Open())
  wt.menubar_search.add_command(label="Edit highlight patterns...", command=lambda:TagList_OpenDialog())
  wt.menubar_search.add_separator()
  wt.menubar_search.add_command(label="List all search matches...", command=lambda:SearchAll(True, 0), accelerator="ALT-a")
  wt.menubar_search.add_command(label="List all matches above...", command=lambda:SearchAll(True, 1), accelerator="ALT-P")
  wt.menubar_search.add_command(label="List all matches below...", command=lambda:SearchAll(True, 1), accelerator="ALT-N")
  wt.menubar_search.add_separator()
  wt.menubar_search.add_command(label="Clear search highlight", command=lambda:SearchHighlightClear(), accelerator="&")
  wt.menubar_search.add_separator()
  wt.menubar_search.add_command(label="Goto line number...", command=lambda:KeyCmd_OpenDialog("goto"))

  wt.menubar_mark = Menu(wt.menubar, tearoff=0, postcommand=lambda:MenuPosted())
  wt.menubar_mark.add_command(label="Toggle bookmark", accelerator="m", command=Mark_ToggleAtInsert)
  wt.menubar_mark.add_command(label="List bookmarks", command=lambda:MarkList_OpenDialog())
  wt.menubar_mark.add_command(label="Delete all bookmarks", command=lambda:Mark_DeleteAll())
  wt.menubar_mark.add_separator()
  wt.menubar_mark.add_command(label="Jump to prev. bookmark", command=lambda:Mark_JumpNext(False), accelerator="'-")
  wt.menubar_mark.add_command(label="Jump to next bookmark", command=lambda:Mark_JumpNext(True), accelerator="'+")
  wt.menubar_mark.add_separator()
  wt.menubar_mark.add_command(label="Read bookmarks from file...", command=lambda:Mark_ReadFileFrom())
  wt.menubar_mark.add_command(label="Save bookmarks to file...", command=lambda:Mark_SaveFileAs())

  wt.menubar_help = Menu(wt.menubar, name="help", tearoff=0, postcommand=lambda:MenuPosted())
  dlg_help_add_menu_commands(wt.menubar_help)
  wt.menubar_help.add_separator()
  wt.menubar_help.add_command(label="About", command=lambda:OpenAboutDialog())

  wt.menubar.add_cascade(label="Control", menu=wt.menubar_ctrl, underline=0)
  wt.menubar.add_cascade(label="Search", menu=wt.menubar_search, underline=0)
  wt.menubar.add_cascade(label="Bookmarks", menu=wt.menubar_mark, underline=0)
  wt.menubar.add_cascade(label="Help", menu=wt.menubar_help)
  tk.config(menu=wt.menubar)

  # frame #1: text widget and scrollbar
  wt.f1 = Frame(tk)
  wt.f1_t = Text(wt.f1, width=1, height=1, wrap=NONE,
                 font=font_content, background=col_bg_content, foreground=col_fg_content,
                 cursor="top_left_arrow", relief=FLAT, exportselection=1)
  wt.f1_t.pack(side=LEFT, fill=BOTH, expand=1)
  wt.f1_sb = Scrollbar(wt.f1, orient=VERTICAL, command=wt.f1_t.yview, takefocus=0)
  wt.f1_sb.pack(side=LEFT, fill=Y)
  wt.f1_t.configure(yscrollcommand=wt.f1_sb.set)
  wt.f1.pack(side=TOP, fill=BOTH, expand=1)

  wt.f1_t.focus_set()
  # note: order is important: "find" must be lower than highlighting tags
  HighlightConfigure(wt.f1_t, "find", fmt_find)
  HighlightConfigure(wt.f1_t, "findinc", fmt_findinc)
  wt.f1_t.tag_config("margin", lmargin1=17)
  wt.f1_t.tag_config("bookmark", lmargin1=0)
  HighlightConfigure(wt.f1_t, "sel", fmt_selection)
  wt.f1_t.tag_lower("sel")

  wt.f1_t.bindtags([wt.f1_t, "TextReadOnly", tk, "all"])

  # commands to scroll the X/Y view
  KeyBinding_UpDown(wt.f1_t)
  KeyBinding_LeftRight(wt.f1_t)

  # commands to move the cursor
  # Note: values e.state MS-Windows: 1=SHIFT, 2=CAPS-Lock, 4=CTRL, 0x60000=ALT, 0x60004=Alt-GR, 0x40000=always set!
  wt.f1_t.bind("<Key-Home>", lambda e: BindCallAndBreak(lambda: KeyHomeEnd(wt.f1_t, "left") if ((e.state & 0xFF) == 0) else CursorGotoLine(wt.f1_t, "start")))
  wt.f1_t.bind("<Key-End>", lambda e: BindCallAndBreak(lambda: KeyHomeEnd(wt.f1_t, "right") if ((e.state & 0xFF) == 0) else CursorGotoLine(wt.f1_t, "end")))
  wt.f1_t.bind("<Key-space>", lambda e:BindCallKeyClrBreak(lambda: CursorMoveLeftRight(wt.f1_t, 1)))
  wt.f1_t.bind("<Key-BackSpace>", lambda e:BindCallKeyClrBreak(lambda: CursorMoveLeftRight(wt.f1_t, -1)))
  KeyCmdBind(wt.f1_t, "h", lambda: wt.f1_t.event_generate("<Left>"))
  KeyCmdBind(wt.f1_t, "l", lambda: wt.f1_t.event_generate("<Right>"))
  KeyCmdBind(wt.f1_t, "Return", lambda: CursorMoveLine(wt.f1_t, 1))
  KeyCmdBind(wt.f1_t, "w", lambda: CursorMoveWord(1, 0, 0))
  KeyCmdBind(wt.f1_t, "e", lambda: CursorMoveWord(1, 0, 1))
  KeyCmdBind(wt.f1_t, "b", lambda: CursorMoveWord(0, 0, 0))
  KeyCmdBind(wt.f1_t, "W", lambda: CursorMoveWord(1, 1, 0))
  KeyCmdBind(wt.f1_t, "E", lambda: CursorMoveWord(1, 1, 1))
  KeyCmdBind(wt.f1_t, "B", lambda: CursorMoveWord(0, 1, 0))
  KeyCmdBind(wt.f1_t, "ge", lambda: CursorMoveWord(0, 0, 1))
  KeyCmdBind(wt.f1_t, "gE", lambda: CursorMoveWord(0, 1, 1))
  KeyCmdBind(wt.f1_t, ";", lambda: SearchCharInLine("", 1))
  KeyCmdBind(wt.f1_t, ",", lambda: SearchCharInLine("", -1))
  # commands for searching & repeating
  KeyCmdBind(wt.f1_t, "/", lambda: SearchEnter(1))
  KeyCmdBind(wt.f1_t, "?", lambda: SearchEnter(0))
  KeyCmdBind(wt.f1_t, "n", lambda: SearchNext(1))
  KeyCmdBind(wt.f1_t, "N", lambda: SearchNext(0))
  KeyCmdBind(wt.f1_t, "*", lambda: SearchWord(1))
  KeyCmdBind(wt.f1_t, "#", lambda: SearchWord(0))
  KeyCmdBind(wt.f1_t, "&", lambda: SearchHighlightClear())
  wt.f1_t.bind("<Alt-Key-f>", lambda e: BindCallKeyClrBreak(lambda:wt.f2_e.focus_set()))
  wt.f1_t.bind("<Alt-Key-n>", lambda e: BindCallKeyClrBreak(lambda:SearchNext(1)))
  wt.f1_t.bind("<Alt-Key-p>", lambda e: BindCallKeyClrBreak(lambda:SearchNext(0)))
  wt.f1_t.bind("<Alt-Key-h>", lambda e: BindCallKeyClrBreak(lambda:SearchHighlightOnOff()))
  wt.f1_t.bind("<Alt-Key-a>", lambda e: BindCallKeyClrBreak(lambda:SearchAll(False, 0)))
  wt.f1_t.bind("<Alt-Key-N>", lambda e: BindCallKeyClrBreak(lambda:SearchAll(False, 1)))
  wt.f1_t.bind("<Alt-Key-P>", lambda e: BindCallKeyClrBreak(lambda:SearchAll(False, -1)))
  # misc
  KeyCmdBind(wt.f1_t, "i", lambda: SearchList_CopyCurrentLine())
  KeyCmdBind(wt.f1_t, "u", lambda: SearchList_Undo())
  wt.f1_t.bind("<Control-Key-r>", lambda e: SearchList_Redo())
  wt.f1_t.bind("<Control-Key-g>", lambda e: BindCallKeyClrBreak(DisplayLineNumber))
  wt.f1_t.bind("<Control-Key-o>", lambda e: BindCallKeyClrBreak(lambda:CursorJumpHistory(wt.f1_t, -1)))
  wt.f1_t.bind("<Control-Key-i>", lambda e: BindCallKeyClrBreak(lambda:CursorJumpHistory(wt.f1_t, 1)))
  wt.f1_t.bind("<Double-Button-1>", lambda e: BindCallKeyClrBreak(Mark_ToggleAtInsert) if (e.state == 0) else None)
  KeyCmdBind(wt.f1_t, "m", Mark_ToggleAtInsert)
  wt.f1_t.bind("<Alt-Key-w>", lambda e: BindCallAndBreak(ToggleLineWrap))
  wt.f1_t.bind("<Control-plus>", lambda e: BindCallKeyClr(lambda:ChangeFontSize(1)))
  wt.f1_t.bind("<Control-minus>", lambda e: BindCallKeyClr(lambda:ChangeFontSize(-1)))
  wt.f1_t.bind("<Control-Alt-Delete>", lambda e: DebugDumpAllState())
  # catch-all (processes "KeyCmdBind" from above)
  wt.f1_t.bind("<FocusIn>", lambda e: KeyClr())
  wt.f1_t.bind("<Return>", lambda e: "break" if KeyCmd(wt.f1_t, "Return") else None)
  wt.f1_t.bind("<KeyPress>", lambda e: "break" if KeyCmd(wt.f1_t, e.char) else None)

  # frame #2: search controls
  wt.f2 = Frame(borderwidth=2, relief=RAISED)
  wt.f2_l = Label(text="Find:", underline=0)
  wt.f2_e = Entry(width=20, textvariable=tlb_find, exportselection=0)
  wt.fs_mh = Menu(tearoff=0)
  wt.f2_bn = Button(text="Next", command=lambda:SearchNext(1), underline=0, pady=2)
  wt.f2_bp = Button(text="Prev.", command=lambda:SearchNext(0), underline=0, pady=2)
  wt.f2_bl = Button(text="All", command=lambda:SearchAll(True, 0), underline=0, pady=2)
  wt.f2_bh = Checkbutton(text="Highlight all", variable=tlb_hall, command=lambda:SearchHighlightSettingChange, underline=0)
  wt.f2_cb = Checkbutton(text="Match case", variable=tlb_case, command=lambda:SearchHighlightSettingChange, underline=6)
  wt.f2_re = Checkbutton(text="Reg.Exp.", variable=tlb_regexp, command=lambda:SearchHighlightSettingChange, underline=4)
  for wid in (wt.f2_l, wt.f2_e, wt.f2_bn, wt.f2_bp, wt.f2_bl, wt.f2_bh, wt.f2_cb, wt.f2_re):
    wid.pack(side=LEFT, anchor=W, padx=1)
  wt.f2_e.pack_configure(fill=X, expand=1)
  wt.f2.pack(side=TOP, fill=X)

  wt.f2_e.bind("<Escape>", lambda e: BindCallAndBreak(SearchAbort))
  wt.f2_e.bind("<Return>", lambda e: BindCallAndBreak(SearchReturn))
  wt.f2_e.bind("<FocusIn>", lambda e: SearchInit())
  wt.f2_e.bind("<FocusOut>", lambda e: SearchLeave())
  wt.f2_e.bind("<Control-n>", lambda e: BindCallAndBreak(lambda: SearchIncrement(1, False)))
  wt.f2_e.bind("<Control-N>", lambda e: BindCallAndBreak(lambda: SearchIncrement(0, False)))
  wt.f2_e.bind("<Key-Up>", lambda e: BindCallAndBreak(lambda: Search_BrowseHistory(True)))
  wt.f2_e.bind("<Key-Down>", lambda e: BindCallAndBreak(lambda: Search_BrowseHistory(False)))
  wt.f2_e.bind("<Control-d>", lambda e: BindCallAndBreak(Search_Complete))
  wt.f2_e.bind("<Control-D>", lambda e: BindCallAndBreak(Search_CompleteLeft))
  wt.f2_e.bind("<Control-x>", lambda e: BindCallAndBreak(Search_RemoveFromHistory))
  wt.f2_e.bind("<Control-c>", lambda e: BindCallAndBreak(SearchAbort))
  # disabled in v1.2 because of possible conflict with misconfigured backspace key
  #wt.f2_e.bind("<Control-h>", lambda e: BindCallAndBreak(lambda: TagList_AddSearch(tk)))
  #wt.f2_e.bind("<Control-H>", lambda e: BindCallAndBreak(SearchHistory_Open))
  wt.f2_e.bind("<Alt-Key-n>", lambda e: BindCallAndBreak(lambda: SearchNext(1)))
  wt.f2_e.bind("<Alt-Key-p>", lambda e: BindCallAndBreak(lambda: SearchNext(0)))
  wt.f2_e.bind("<Alt-Key-a>", lambda e: BindCallAndBreak(lambda: SearchAll(False, 0)))
  wt.f2_e.bind("<Alt-Key-N>", lambda e: BindCallAndBreak(lambda: SearchAll(False, 1)))
  wt.f2_e.bind("<Alt-Key-P>", lambda e: BindCallAndBreak(lambda: SearchAll(False, -1)))
  wt.f2_e.bind("<Alt-Key-c>", lambda e: BindCallAndBreak(lambda: SearchHighlightToggleVar(tlb_case)))
  wt.f2_e.bind("<Alt-Key-e>", lambda e: BindCallAndBreak(lambda: SearchHighlightToggleVar(tlb_regexp)))
  tlb_find.trace("w", SearchVarTrace) # add variable tlb_find write SearchVarTrace

  tk.protocol(name="WM_DELETE_WINDOW", func=UserQuit)
  tk.geometry(win_geom["main_win"])
  tk.positionfrom(who="user")
  wt.f1_t.bind("<Configure>", lambda e: ToplevelResized(e.widget, tk, wt.f1_t, "main_win"))

  # dummy widget for xselection handling
  wt.xselection = Label()
  wt.xselection.selection_handle(TextSel_XselectionHandler)


#
# This wrapper is used for event bindings to first call a function and then
# return "break" to the window manager, so that the event is not further
# processed.
#
def BindCallAndBreak(func):
  func()
  return "break"

def BindCallKeyClrBreak(func):
  func()
  KeyClr()
  return "break"

def BindCallKeyClr(func):
  func()
  KeyClr()

#
# This function creates the requested bitmaps if they don't exist yet
#
def CreateButtonBitmap(*args):
  for img in args:
    try:
      tk.eval("image height %s" % img)
    except:
      if img == "img_dropdown":
        # image for drop-down menu copied from combobox.tcl by Bryan Oakley
        tk.eval("image create bitmap img_dropdown -data \"" \
          "#define down_arrow_width 15\\n" \
          "#define down_arrow_height 15\\n" \
          "static char down_arrow_bits[] = {\\n" \
          "0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,\\n" \
          "0x00,0x80,0xf8,0x8f,0xf0,0x87,0xe0,0x83,\\n" \
          "0xc0,0x81,0x80,0x80,0x00,0x80,0x00,0x80,\\n" \
          "0x00,0x80,0x00,0x80,0x00,0x80};\"")

      if img == "img_down":
        tk.eval("image create bitmap img_down -data \"" \
          "#define ptr_down_width 16\\n" \
          "#define ptr_down_height 14\\n" \
          "static unsigned char ptr_down_bits[] = {\\n" \
          "0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,\\n" \
          "0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,\\n" \
          "0xc0,0x01,0xf8,0x0f,0xf0,0x07,0xe0,0x03,\\n" \
          "0xc0,0x01,0x80,0x00};\"")

      if img == "img_up":
        tk.eval("image create bitmap img_up -data \"" \
          "#define ptr_up_width 16\\n" \
          "#define ptr_up_height 14\\n" \
          "static unsigned char ptr_up_bits[] = {\\n" \
          "0x80,0x00,0xc0,0x01,0xe0,0x03,0xf0,0x07,\\n" \
          "0xf8,0x0f,0xc0,0x01,0xc0,0x01,0xc0,0x01,\\n" \
          "0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,\\n" \
          "0xc0,0x01,0xc0,0x01};\"")

# ----------------------------------------------------------------------------
#
# This function is called during start-up to create tags for all color
# highlights.
#
def HighlightCreateTags():
  global patlist

  for w in patlist:
    tagnam = w[4]

    HighlightConfigure(wt.f1_t, tagnam, w)
    wt.f1_t.tag_lower(tagnam, "find")


#
# This function is called after loading a new text to apply the color
# highlighting to the complete text. This means all matches on all
# highlight patterns has to be searched for. Since this can take some
# time, the operation done in the background to avoid blocking the user.
# The CPU is given up voluntarily after each pattern and after max. 100ms
#
def HighlightInit():
  global patlist, tid_high_init

  if wt_exists(wt.hipro):
    wt.hipro.destroy()
    wt.hipro = None

  if len(patlist) > 0:
    # create a progress bar as overlay to the main window
    wt.hipro = Frame(wt.f1_t, takefocus=0, relief=SUNKEN, borderwidth=2)
    wt.hipro_c = Canvas(wt.hipro, width=100, height=10, highlightthickness=0, takefocus=0)
    wt.hipro_c.pack()
    cid = wt.hipro_c.create_rectangle(0, 0, 0, 12, fill="#0b1ff7", outline="")
    wt.hipro.place(anchor=NW, x=0, y=0) #in=wt.f1_t

    wt.f1_t.tag_add("margin", "1.0", "end")

    wt.f1_t.configure(cursor="watch")

    # trigger highlighting for the 1st pattern in the background
    tid_high_init = tk.after(50, lambda: HighlightInitBg(0, cid, 0, 0))

    # apply highlighting on the text in the visible area (this is quick)
    # use the yview callback to redo highlighting in case the user scrolls
    Highlight_YviewRedirect()


#
# This function is a slave-function of HighlightInit. The function
# loops across all members in the global pattern list to apply color
# the respective highlighting. The loop is broken up by installing each
# new iteration as an idle event (and limiting each step to 100ms)
#
def HighlightInitBg(pat_idx, cid, line, loop_cnt):
  global block_bg_tasks, tid_search_inc, tid_search_hall, tid_search_list
  global patlist, tid_high_init

  if block_bg_tasks or (tid_search_inc is not None) or (tid_search_hall is not None) or (tid_search_list is not None):
    # background tasks are suspended - re-schedule with timer
    tid_high_init = tk.after(100, lambda:HighlightInitBg(pat_idx, cid, line, 0))
  elif loop_cnt > 10:
    # insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
    tid_high_init = tk.after(10, lambda:HighlightInitBg(pat_idx, cid, line, 0))

  elif pat_idx < len(patlist):
    w = patlist[pat_idx]
    tagnam = w[4]
    opt = Search_GetOptions(w[0], w[1], w[2])
    loop_cnt += 1

    # here we do the actual work:
    # apply the tag to all matching lines of text
    line = HighlightLines(w[0], tagnam, opt, line)

    if line >= 0:
      # not done yet - reschedule
      tid_high_init = tk.after_idle(lambda:HighlightInitBg(pat_idx, cid, line, loop_cnt))

    else:
      # trigger next tag
      pat_idx += 1
      tid_high_init = tk.after_idle(lambda:HighlightInitBg(pat_idx, cid, 1, loop_cnt))

      # update the progress bar
      wt.hipro_c.coords(cid, 0, 0, 100*pat_idx//len(patlist), 12)
  else:
    if wt_exists(wt.hipro):
      wt.hipro.destroy()
      wt.hipro = None
    wt.f1_t.configure(cursor="top_left_arrow")
    tid_high_init = None


#
# This function searches for all lines in the main text widget which match the
# given pattern and adds the given tag to them.  If the loop doesn't complete
# within 100ms, the search is paused and the function returns the number of the
# last searched line.  In this case the caller must invoke the funtion again
# (as an idle event, to allow user-interaction in-between.)
#
def HighlightLines(pat, tagnam, opt, line):
  max_line = int(wt.f1_t.index("end").split(".")[0])
  start_t = datetime.now()
  max_delta = timedelta(microseconds=100000)

  while line < max_line:
    pos = wt.f1_t.search(pat, "%d.0" % line, "end", **opt)
    if pos == "":
      break

    # match found, highlight this line
    line = int(pos.split(".")[0])
    wt.f1_t.tag_add(tagnam, "%d.0" % line, "%d.0" % (line + 1))
    # trigger the search result list dialog in case the line is included there too
    SearchList_HighlightLine(tagnam, line)
    line += 1

    # limit the runtime of the loop - return start line number for the next invocation
    if (datetime.now() - start_t > max_delta) and (line < max_line):
      return line

  # all done for this pattern
  return -1


#
# This helper function schedules the line highlight function until highlighting
# is complete for the given pattern.  This function is used to add highlighting
# for single tags (e.g. modified highlight patterns or colors; currently not used
# for search highlighting because a separate "cancel ID" is required.)
#
def HighlightAll(pat, tagnam, opt, line=1, loop_cnt=0):
  global tid_high_init, block_bg_tasks

  if block_bg_tasks:
    # background tasks are suspended - re-schedule with timer
    tid_high_init = tk.after(100, lambda: HighlightAll(pat, tagnam, opt, line, 0))
  elif loop_cnt > 10:
    # insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
    tid_high_init = tk.after(10, lambda: HighlightAll(pat, tagnam, opt, line, 0))
  else:
    line = HighlightLines(pat, tagnam, opt, line)
    if line >= 0:
      loop_cnt += 1
      tid_high_init = tk.after_idle(lambda: HighlightAll(pat, tagnam, opt, line, loop_cnt))
    else:
      wt.f1_t.configure(cursor="top_left_arrow")
      tid_high_init = None


#
# This function searches the currently visible text content for all lines
# which contain the given sub-string and marks these lines with the given tag.
#
def HighlightVisible(pat, tagnam, opt):
  start_pos = wt.f1_t.index("@1,1")
  end_pos = wt.f1_t.index("@%d,%d" % (wt.f1_t.winfo_width() - 1, wt.f1_t.winfo_height() - 1))
  line = int(start_pos.split(".")[0])
  max_line = int(end_pos.split(".")[0])
  #puts "visible $start_pos...$end_pos: $pat $opt"

  while line < max_line:
    pos = wt.f1_t.search(pat, "%d.0" % line, "end", **opt)
    if pos == "":
      break
    line = int(pos.split(".")[0])
    wt.f1_t.tag_add(tagnam, "%d.0" % line, "%d.0" % (line + 1))
    line += 1


#
# This callback is installed to the main text widget's yview. It is used
# to detect changes in the view to update highlighting if the highlighting
# task is not complete yet. The event is forwarded to the vertical scrollbar.
#
def Highlight_YviewCallback(frac1, frac2):
  global tid_high_init, tid_search_hall

  if tid_high_init is not None:
    global patlist
    for w in patlist:
      opt = Search_GetOptions(w[0], w[1], w[2])
      HighlightVisible(w[0], w[4], opt)

  if tid_search_hall is not None:
    global tlb_cur_hall_opt
    HighlightVisible(tlb_cur_hall_opt[0], "find", tlb_cur_hall_opt[1])

  # automatically remove the redirect if no longer needed
  if (tid_high_init is None) and (tid_search_hall is None):
    wt.f1_t.configure(yscrollcommand=wt.f1_sb.set)

  wt.f1_sb.set(frac1, frac2)


#
# This function redirect the yview callback from the scrollbar into the above
# function. This is used to install a redirection for the duration of the
# initial or search highlighting task.
#
def Highlight_YviewRedirect():
  wt.f1_t.configure(yscrollcommand=Highlight_YviewCallback)


#
# This function creates or updates a text widget tag with the options of
# a color highlight entry.  The function is called during start-up for all
# highlight patterns, and by the highlight edit dialog (also used for the
# sample text widget.)
#
def HighlightConfigure(wid, tagname, w):
  global font_content

  cfg = {}
  if w[8]:
    cfg["font"] = DeriveFont(font_content, 0, "bold")
  else:
    cfg["font"] = ""

  if w[9]:
    cfg["underline"] = w[9]
  else:
    cfg["underline"] = ""

  if w[10]:
    cfg["overstrike"] = w[10]
  else:
    cfg["overstrike"] = ""

  if w[13] != "":
    cfg["relief"] = w[13]
    cfg["borderwidth"] = w[14]
  else:
    cfg["relief"] = ""
    cfg["borderwidth"] = ""

  if w[15] > 0:
    cfg["spacing1"] = w[15]
    cfg["spacing3"] = w[15]
  else:
    cfg["spacing1"] = ""
    cfg["spacing3"] = ""

  cfg["background"] = w[6]
  cfg["foreground"] = w[7]

  cfg["bgstipple"] = w[11]
  cfg["fgstipple"] = w[12]

  wid.tag_config(tagname, **cfg)


#
# This function clears the current search color highlighting without
# resetting the search string. It's bound to the "&" key, but also used
# during regular search reset.
#
def SearchHighlightClear():
  global tlb_cur_hall_opt, tid_search_hall

  if tid_search_hall is not None: tk.after_cancel(tid_search_hall)
  tid_search_hall = None
  wt.f1_t.configure(cursor="top_left_arrow")

  wt.f1_t.tag_remove("find", "1.0", "end")
  wt.f1_t.tag_remove("findinc", "1.0", "end")
  tlb_cur_hall_opt = ["", []]

  SearchList_HighlightClear()


#
# This function triggers color highlighting of all lines of text which match
# the current search string.  The function is called when global highlighting
# is en-/disabled, when the search string is modified or when search options
# are changed.
#
def SearchHighlightUpdate(pat, opt):
  global tlb_hall, tlb_cur_hall_opt, tid_search_hall

  if pat != "":
    if tlb_hall.get():
      if opt.get("forwards"): del opt["forwards"]
      if opt.get("backwards"): del opt["backwards"]

      if tk.focus_get() != wt.f2_e:
        if ((tlb_cur_hall_opt[0] != pat) or
            (tlb_cur_hall_opt[1] != opt)):

          # display "busy" cursor until highlighting is finished
          wt.f1_t.configure(cursor="watch")

          # kill background highlight process for obsolete pattern
          if tid_search_hall is not None: tk.after_cancel(tid_search_hall)

          # start highlighting in the background
          tlb_cur_hall_opt = [pat, opt]
          tid_search_hall = tk.after(100, lambda: SearchHighlightAll(pat, "find", opt))

          # apply highlighting on the text in the visible area (this is quick)
          # (note this is required in addition to the redirect below)
          HighlightVisible(pat, "find", opt)

          # use the yview callback to redo highlighting in case the user scrolls
          Highlight_YviewRedirect()
      else:
        HighlightVisible(pat, "find", opt)
    else:
      SearchHighlightClear()


#
# This is a wrapper for the above function which works on the current
# pattern in the search entry field.
#
def SearchHighlightUpdateCurrent():
  global tlb_hall, tlb_find, tlb_regexp, tlb_case

  if tlb_hall.get():
    pat = tlb_find.get()
    if pat != "":
      if SearchExprCheck(pat, tlb_regexp.get(), True):
        opt = Search_GetOptions(pat, tlb_regexp.get(), tlb_case.get())
        SearchHighlightUpdate(pat, opt)


#
# This helper function calls the global search highlight function until
# highlighting is complete.
#
def SearchHighlightAll(pat, tagnam, opt, line=1, loop_cnt=0):
  global tid_search_hall, block_bg_tasks

  if block_bg_tasks:
    # background tasks are suspended - re-schedule with timer
    tid_search_hall = tk.after(100, lambda: SearchHighlightAll(pat, tagnam, opt, line, 0))
  elif loop_cnt > 10:
    # insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
    tid_search_hall = tk.after(10, lambda: SearchHighlightAll(pat, tagnam, opt, line, 0))
  else:
    line = HighlightLines(pat, tagnam, opt, line)
    if line >= 0:
      loop_cnt += 1
      tid_search_hall = tk.after_idle(lambda: SearchHighlightAll(pat, tagnam, opt, line, loop_cnt))
    else:
      tid_search_hall = None
      wt.f1_t.configure(cursor="top_left_arrow")


#
# This function is bound to the "Highlight all" checkbutton to en- or disable
# global highlighting.
#
def SearchHighlightOnOff():
  global tlb_hall

  tlb_hall.set(not tlb_hall.get())
  UpdateRcAfterIdle()

  SearchHighlightUpdateCurrent()


#
# This function is invoked after a change in search settings (i.e. case
# match, reg.exp. or global highlighting.)  The changed settings are
# stored in the RC file and a possible search highlighting is removed
# or updated (the latter only if global highlighting is enabled)
#
def SearchHighlightSettingChange():
  global tlb_hall

  UpdateRcAfterIdle()

  SearchHighlightClear()
  if tlb_hall.get():
    SearchHighlightUpdateCurrent()


def SearchHighlightToggleVar(obj):
  obj.set(obj.get() ^ 1)
  SearchHighlightSettingChange()

#
# This function is invoked when the user enters text in the "find" entry field.
# In contrary to the "atomic" search, this function only searches a small chunk
# of text, then re-schedules itself as an "idle" task.  The search can be aborted
# at any time by canceling the task.
#
def Search_Background(pat, is_fwd, opt, start, is_changed, callback):
  global block_bg_tasks, tid_search_inc, tid_search_list
  global tid_search_inc

  if block_bg_tasks:
    # background tasks are suspended - re-schedule with timer
    tid_search_inc = tk.after(100, lambda: Search_Background(pat, is_fwd, opt, start, is_changed, callback))
    return

  if is_fwd:
    end = wt.f1_t.index("end")
  else:
    end = "1.0"

  if start != end:
    if is_fwd:
      next = wt.f1_t.index(start + " + 5000 lines lineend")
    else:
      next = wt.f1_t.index(start + " - 5000 lines linestart")

    # invoke the actual search in the text widget content
    match_len = IntVar(tk, 0)
    pos = wt.f1_t.search(pat, start, next, count=match_len, **opt)

    if pos == "":
      tid_search_inc = tk.after_idle(lambda: Search_Background(pat, is_fwd, opt, next, is_changed, callback))
    else:
      tid_search_inc = None
      Search_HandleMatch(pos, match_len.get(), pat, opt, is_changed)
      callback(pos, pat, is_fwd, is_changed)

  else:
    tid_search_inc = None
    Search_HandleMatch("", 0, pat, opt, is_changed)
    callback("", pat, is_fwd, is_changed)


#
# This function searches the main text content for the expression in the
# search entry field, starting at the current cursor position. When a match
# is found, the cursor is moved there and the line is highlighed.
#
def Search_Atomic(pat, is_re, use_case, is_fwd, is_changed):
  global tlb_hall, tlb_last_dir

  pos = ""
  if (pat != "") and SearchExprCheck(pat, is_re, True):

    tlb_last_dir = is_fwd
    search_opt = Search_GetOptions(pat, is_re, use_case, tlb_last_dir)
    start_pos = Search_GetBase(is_fwd, False)
    CursorJumpPushPos(wt.f1_t)

    if is_fwd:
      search_range = [start_pos, wt.f1_t.index("end")]
    else:
      search_range = [start_pos, "1.0"]

    match_len = IntVar(tk, 0)

    if start_pos != search_range[1]:
      # invoke the actual search in the text widget content
      while True:
        pos = wt.f1_t.search(pat, search_range[0], search_range[1], count=match_len, **search_opt)

        # work-around for backwards search:
        # make sure the matching text is entirely to the left side of the cursor
        if (pos != "") and SearchOverlapCheck(is_fwd, start_pos, pos, match_len.get()):
          # match overlaps: search further backwards
          search_range[0] = pos
          continue

        break
    else:
      pos = ""

    # update cursor position and highlight
    Search_HandleMatch(pos, match_len.get(), pat, search_opt, is_changed)

  else:
    # empty or invalid expression: just remove old highlights
    SearchReset()

  return pos


#
# This helper function checks if the match returned for a backwards search
# overlaps the search start position (e.g. the match is 1 char left of the
# start pos, but 2 chars long)
#
def SearchOverlapCheck(is_fwd, start_pos, pos, match_len):
  if not is_fwd:
    # convert start position into numerical (e.g. may be "insert")
    try:
      (line1, char1) = map(int, start_pos.split("."))
    except:
      try:
        start_pos = wt.f1_t.index(start_pos)
        (line1, char1) = map(int, start_pos.split("."))
      except:
        return False

  try:
    (line2, char2) = map(int, wt.f1_t.index(pos).split("."))
    if (line1 == line2) and (char2 + match_len > char1):
      return True
  except:
    return False

  return False


#
# This function handles the result of a text search in the main window. If
# a match was found, the cursor is moved to the start of the match and the
# matching section and complete line are highlighted. Optionally, a background
# process to highlight all matches is started. If no match is found, any
# previously applied search highlighting is removed.
#
def Search_HandleMatch(pos, match_len, pat, opt, is_changed):
  global tlb_find_line, tlb_hall, tlb_cur_hall_opt

  if (pos != "") or is_changed:
    if not tlb_hall.get() or (tlb_cur_hall_opt[0] != pat):
      SearchHighlightClear()
    else:
      wt.f1_t.tag_remove("findinc", "1.0", "end")

  if pos != "":
    tlb_find_line = int(pos.split(".")[0])
    wt.f1_t.see(pos)
    wt.f1_t.mark_set("insert", pos)
    wt.f1_t.tag_add("find", "%d.0" % tlb_find_line, "%d.0" % (tlb_find_line + 1))
    if match_len > 0:
      wt.f1_t.tag_add("findinc", pos, "%s + %d chars" % (pos, match_len))

    SearchList_HighlightLine("find", tlb_find_line)
    SearchList_MatchView(tlb_find_line)

  if tlb_hall.get():
    SearchHighlightUpdate(pat, opt)


#
# This function displays a message if no match was found for a search
# pattern. This is split off from the search function so that some
# callers can override the message.
#
def Search_HandleNoMatch(pat, is_fwd):
  if pat != "":
    pat = ": " + pat

  if is_fwd:
    DisplayStatusLine("search", "warn", "No match until end of file" + pat)
  else:
    DisplayStatusLine("search", "warn", "No match until start of file" + pat)


#
# This function is bound to all changes of the search text in the
# "find" entry field. It's called when the user enters new text and
# triggers an incremental search.
#
def SearchVarTrace(name1, name2, op):
  global tid_search_inc
  global tlb_last_dir

  if tid_search_inc is not None: tk.after_cancel(tid_search_inc)
  tid_search_inc = tk.after(50, lambda: SearchIncrement(tlb_last_dir, True))


#
# This function performs a so-called "incremental" search after the user
# has modified the search text. This means searches are started already
# while the user is typing.
#
def SearchIncrement(is_fwd, is_changed):
  global tlb_find, tlb_regexp, tlb_case, tlb_last_dir, tlb_inc_base, tlb_inc_view
  global tid_search_inc

  tid_search_inc = None

  if tk.focus_get() == wt.f2_e:
    pat = tlb_find.get()
    if (pat != "") and SearchExprCheck(pat, tlb_regexp.get(), False):
      if tlb_inc_base is None:
        tlb_inc_base = Search_GetBase(is_fwd, True)
        tlb_inc_view = [wt.f1_t.xview()[0], wt.f1_t.yview()[0]]
        CursorJumpPushPos(wt.f1_t)

      if is_changed:
        wt.f1_t.tag_remove("findinc", "1.0", "end")
        wt.f1_t.tag_remove("find", "1.0", "end")
        start_pos = tlb_inc_base
        #wt.f1_t.xview_moveto(tlb_inc_view[0])
        #wt.f1_t.yview_moveto(tlb_inc_view[1])
        #wt.f1_t.mark_set("insert", tlb_inc_base)
        #wt.f1_t.see("insert")
      else:
        start_pos = Search_GetBase(is_fwd, False)

      opt = Search_GetOptions(pat, tlb_regexp.get(), tlb_case.get(), is_fwd)

      Search_Background(pat, is_fwd, opt, start_pos, is_changed, Search_IncMatch)

    else:
      SearchReset()

      if pat != "":
        DisplayStatusLine("search", "error", "Incomplete or invalid reg.exp.")
      else:
        ClearStatusLine("search")


#
# This function is invoked as callback after a background search for the
# incremental search in the entry field is completed.  (Before this call,
# cursor position and search highlights are already updated.)
#
def Search_IncMatch(pos, pat, is_fwd, is_changed):
  global tlb_inc_base, tlb_inc_view, tlb_history, tlb_hist_pos, tlb_hist_prefix

  if (pos == "") and (tlb_inc_base is not None):
    if is_changed:
      wt.f1_t.xview_moveto(tlb_inc_view[0])
      wt.f1_t.yview_moveto(tlb_inc_view[1])
      wt.f1_t.mark_set("insert", tlb_inc_base)
      wt.f1_t.see("insert")

    if is_fwd:
      DisplayStatusLine("search", "warn", "No match until end of file")
    else:
      DisplayStatusLine("search", "warn", "No match until start of file")
  else:
    ClearStatusLine("search")

  if tlb_hist_pos is not None:
    hl = tlb_history[tlb_hist_pos]
    if pat != hl[0]:
      tlb_hist_pos = None
      tlb_hist_prefix = None


#
# This function checks if the search pattern syntax is valid
#
def SearchExprCheck(pat, is_re, display):
  if is_re:
    try:
      # Text widget uses Tcl "re_syntax" which slightly differs from Python "re"
      #re.compile(pat)
      foo = Text(tk)
      foo.search(pat, 1.0, regexp=True)
      return True
    #except re.error as e:
    except Exception as e:
      if display:
        DisplayStatusLine("search", "error", "Syntax error in search expression: " + e.msg)
      return False
  else:
    return True


#
# This function returns the start position for a search.  The first search
# starts at the insertion cursor. If the cursor is not visible, the search
# starts at the top or bottom of the visible text. When a search is repeated,
# the search must start behind the previous match (for a forward search) to
# prevent finding the same word again, or finding an overlapping match. (For
# backwards searches overlaps cannot be handled via search arguments; such
# results are filtered out when a match is found.)
#
def Search_GetBase(is_fwd, is_init):
  if wt.f1_t.bbox("insert") is None:
    if is_fwd:
      wt.f1_t.mark_set("insert", "@1,1")
    else:
      wt.f1_t.mark_set("insert", "@%d,%d" % (wt.f1_t.winfo_width() - 1, wt.f1_t.winfo_height() - 1))

    start_pos = "insert"
  else:
    if is_init:
      start_pos = "insert"
    elif is_fwd:
      start_pos = "insert + 1 chars"
    else:
      start_pos = "insert"

  start_pos = wt.f1_t.index(start_pos)

  # move start position for forward search after the end of the previous match
  if is_fwd:
    # search for tag which marks the previous match (would have been cleared if the pattern changed)
    pos12 = wt.f1_t.tag_nextrange("findinc", start_pos + " linestart", start_pos + " lineend")
    if len(pos12) == 2:
      # check if the start position (i.e. the cursor) is still inside of the area of the match
      # (split lists contain text line index in [0] and char index in [1])
      start_lcp = start_pos.split(".")
      pos1_lcp = pos12[0].split(".")
      pos2_lcp = pos12[1].split(".")
      if (len(start_lcp) == 2) and (len(pos1_lcp) == 2) and (len(pos2_lcp) == 2):
        if (    (int(start_lcp[0]) == int(pos1_lcp[0]))  # same line
            and (int(start_lcp[1]) >= int(pos1_lcp[1]))  # char ranges overlap
            and (int(start_lcp[1])  < int(pos2_lcp[1])) ):
          start_pos = pos12[1]

  return start_pos


#
# This function translates user-options into search options for the text widget.
#
def Search_GetOptions(pat, is_re, use_case, is_fwd=-1):

  search_opt = {}
  if is_re:
    if re.search(r"[\.\\\*\+\?\(\[\{\^\$]", pat):
      search_opt["regexp"] = 1

  if use_case == 0:
    search_opt["nocase"] = 1

  if is_fwd != -1:
    if is_fwd:
      search_opt["forwards"] = 1
    else:
      search_opt["backwards"] = 1

  return search_opt


#
# This function is used by the various key bindings which repeat a
# previous search.
#
def SearchNext(is_fwd):
  global tlb_find, tlb_regexp, tlb_case, tlb_history

  ClearStatusLine("search")

  pat = tlb_find.get()
  if pat != "":
    found = Search_Atomic(pat, tlb_regexp.get(), tlb_case.get(), is_fwd, False)
    if found == "":
      Search_HandleNoMatch(pat, is_fwd)

  elif len(tlb_history) > 0:
    # empty expression: repeat last search
    hl = tlb_history[0]
    found = Search_Atomic(hl[0], hl[1], hl[2], is_fwd, False)
    if not found:
      Search_HandleNoMatch(hl[0], is_fwd)

  else:
    DisplayStatusLine("search", "error", "No pattern defined for search repetition")
    found = False

  return found


#
# This function is used by the "All" or "List all" buttons and assorted
# keyboard shortcuts to list all text lines matching the current search
# expression in a separate dialog window.
#
def SearchAll(raise_win, direction):
  global tlb_find, tlb_regexp, tlb_case, tlb_last_wid, tlb_find_focus

  pat = tlb_find.get()
  if pat:
    if SearchExprCheck(pat, tlb_regexp.get(), True):
      is_re = tlb_regexp.get()
      use_case = tlb_case.get()

      Search_AddHistory(pat, is_re, use_case)

      # make focus return and cursor jump back to original position
      if tlb_find_focus:
        SearchHighlightClear()
        SearchReset()

        # note more clean-up is triggered via the focus-out event
        wt.f1_t.focus_set()

        if tlb_last_wid is not None:
          tlb_last_wid.focus_set()
          # raise the caller's window above the main window
          top_wid = tlb_last_wid.winfo_toplevel()
          top_wid.lift()

      SearchList_Open(raise_win)
      SearchList_SearchMatches(True, pat, is_re, use_case, direction)

  else:
    DisplayStatusLine("search", "error", "No pattern defined for search repetition")


#
# This function resets the state of the search engine.  It is called when
# the search string is empty or a search is aborted with the Escape key.
#
def SearchReset():
  global tlb_cur_hall_opt, tlb_inc_base, tlb_inc_view

  wt.f1_t.tag_remove("find", "1.0", "end")
  wt.f1_t.tag_remove("findinc", "1.0", "end")
  tlb_cur_hall_opt = ["", []]

  if tlb_inc_base is not None:
    wt.f1_t.xview_moveto(tlb_inc_view[0])
    wt.f1_t.yview_moveto(tlb_inc_view[1])
    wt.f1_t.mark_set("insert", tlb_inc_base)
    wt.f1_t.see("insert")
    tlb_inc_base = None
    tlb_inc_view = None

  ClearStatusLine("search")


#
# This function is called when the "find" entry field receives keyboard focus
# to intialize the search state machine for a new search.
#
def SearchInit():
  global tlb_find_focus, tlb_hist_pos, tlb_hist_prefix

  if not tlb_find_focus:
    tlb_find_focus = True
    tlb_hist_pos = None
    tlb_hist_prefix = None

    ClearStatusLine("search")


#
# This function is called to move keyboard focus into the search entry field.
# The focus change will trigger the "init" function.  The caller can pass a
# widget to which focus is passed when leaving the search via the Return or
# Escape keys.
#
def SearchEnter(is_fwd, wid=None):
  global tlb_find, tlb_last_dir, tlb_last_wid

  tlb_last_dir = is_fwd
  tlb_find.set("")
  wt.f2_e.focus_set()

  # clear "highlight all" since search pattern is reset above
  SearchHighlightClear()

  tlb_last_wid = wid
  if tlb_last_wid is not None:
    # raise the search entry field above the caller's window
    top_wid = tlb_last_wid.winfo_toplevel()
    top_wid.lift()


#
# This function is bound to the FocusOut event in the search entry field.
# It resets the incremental search state.
#
def SearchLeave():
  global tlb_find, tlb_regexp, tlb_case, tlb_hall
  global tlb_inc_base, tlb_inc_view, tlb_find_focus, tlb_last_wid
  global tlb_hist_pos, tlb_hist_prefix
  global tid_search_inc

  if tid_search_inc is not None:
    tk.after_cancel(tid_search_inc)
    tid_search_inc = None

  # ignore if the keyboard focus is leaving towards another application
  try:
    focus_nam = tk.focus_get()
  except:
    # exception within tkinter occurs when switching to menu bar
    focus_nam = None

  if focus_nam is not None:
    pat = tlb_find.get()
    if SearchExprCheck(pat, tlb_regexp.get(), False):
      if tlb_hall.get():
        SearchHighlightUpdateCurrent()

      Search_AddHistory(pat, tlb_regexp.get(), tlb_case.get())

    tlb_inc_base = None
    tlb_inc_view = None
    tlb_hist_pos = None
    tlb_hist_prefix = None

    tlb_last_wid = None
    tlb_find_focus = 0


#
# This function is called when the search window is left via "Escape" key.
# The search highlighting is removed and the search text is deleted.
#
def SearchAbort():
  global tlb_find, tlb_regexp, tlb_case, tlb_last_wid

  pat = tlb_find.get()
  if SearchExprCheck(pat, tlb_regexp.get(), False):
    Search_AddHistory(pat, tlb_regexp.get(), tlb_case.get())

  tlb_find.set("")
  SearchReset()
  # note more clean-up is triggered via the focus-out event
  wt.f1_t.focus_set()

  if tlb_last_wid is not None:
    tk.focus_set(tlb_last_wid)

    top_wid = tlb_last_wid.winfo_toplevel()
    top_wid.lift()


#
# This function is bound to the Return key in the search entry field.
# If the search pattern is invalid (reg.exp. syntax) an error message is
# displayed and the focus stays in the entry field. Else, the keyboard
# focus is switched to the main window.
#
def SearchReturn():
  global tlb_find, tlb_regexp, tlb_history, tlb_last_dir, tlb_last_wid
  global tid_search_inc

  if tid_search_inc is not None:
    tk.after_cancel(tid_search_inc)
    tid_search_inc = None
    restart = True
  else:
    restart = False

  if tlb_find.get() == "":
    # empty expression: repeat last search
    if len(tlb_history) > 0:
      hl = tlb_history[0]
      tlb_find.set(hl[0])
      restart = True
    else:
      DisplayStatusLine("search", "error", "No pattern defined for search repetition")

  if SearchExprCheck(tlb_find.get(), tlb_regexp.get(), True):
    if restart:
      # incremental search not completed -> start regular search
      if SearchNext(tlb_last_dir) == "":
        global tlb_inc_view, tlb_inc_base
        if tlb_inc_base is not None:
          wt.f1_t.xview_moveto(tlb_inc_view[0])
          wt.f1_t.yview_moveto(tlb_inc_view[1])
          wt.f1_t.mark_set("insert", tlb_inc_base)
          wt.f1_t.see("insert")

    # note this implicitly triggers the leave event
    wt.f1_t.focus_set()
    if tlb_last_wid is not None:
      tlb_last_wid.focus_set()
      # raise the caller's window above the main window
      top_wid = tlb_last_wid.winfo_toplevel()
      top_wid.lift()


#
# This function add the given search string to the search history stack.
# If the string is already on the stack, it's moved to the top. Note: top
# of the stack is the front of the list.
#
def Search_AddHistory(txt, is_re, use_case):
  global tlb_history, tlb_hist_maxlen

  if txt != "":
    old_sel = SearchHistory_StoreSel()

    # search for the expression in the history (options not compared)
    idx = 0
    for hl in tlb_history:
      if hl[0] == txt: break
      idx += 1

    # remove the element if already in the list
    if idx < len(tlb_history):
      del tlb_history[idx]

    # insert the element at the top of the stack
    hl = [txt, is_re, use_case, int(time.time())]
    tlb_history.insert(0, hl)

    # maintain max. stack depth
    if len(tlb_history) > tlb_hist_maxlen:
      tlb_history = tlb_history[0 : tlb_hist_maxlen - 1]

    UpdateRcAfterIdle()

    SearchHistory_Fill()
    SearchHistory_RestoreSel(old_sel)


#
# This function is bound to the up/down cursor keys in the search entry
# field. The function is used to iterate through the search history stack.
# The "up" key starts with the most recently used pattern, the "down" key
# with the oldest. When the end of the history is reached, the original
# search string is displayed again.
#
def Search_BrowseHistory(is_up):
  global tlb_find, tlb_history, tlb_hist_pos, tlb_hist_prefix

  if len(tlb_history) > 0:
    if tlb_hist_pos is None:
      tlb_hist_prefix = tlb_find.get()
      if is_up:
        tlb_hist_pos = 0
      else:
        tlb_hist_pos = len(tlb_history) - 1

    elif is_up:
      if tlb_hist_pos + 1 < len(tlb_history):
        tlb_hist_pos += 1
      else:
        tlb_hist_pos = -1
    else:
      tlb_hist_pos -= 1

    if len(tlb_hist_prefix) > 0:
      tlb_hist_pos = Search_HistoryComplete(1 if is_up else -1)

    if tlb_hist_pos >= 0:
      hl = tlb_history[tlb_hist_pos]
      tlb_find.set(hl[0])
      wt.f2_e.icursor("end")
    else:
      # end of history reached -> reset
      tlb_find.set(tlb_hist_prefix)
      tlb_hist_pos = None
      tlb_hist_prefix = None
      wt.f2_e.icursor("end")


#
# This helper function searches the search history stack for a search
# string with a given prefix.
#
def Search_HistoryComplete(step):
  global tlb_find, tlb_hist_prefix, tlb_history, tlb_hist_pos, tlb_hist_prefix

  pat = tlb_find.get()
  pf_len = len(tlb_hist_prefix)
  idx = tlb_hist_pos

  while (idx >= 0) and (idx < len(tlb_history)):
    hl = tlb_history[idx]
    if hl[0][0:pf_len] == pat[0:pf_len]:
      return idx
    idx += step

  return -1


#
# This function is bound to "CTRL-d" in the "Find" entry field and
# performs auto-completion of a search text by adding any following
# characters in the word matched by the current expression.
#
def Search_Complete():
  global tlb_find, tlb_regexp, tlb_case

  pos = wt.f1_t.index("insert")
  if pos != "":
    dump = ExtractText(pos, pos + " lineend")
    off = 0

    if tlb_regexp.get():
      if tlb_case.get():
        opt = re.IGNORECASE
      else:
        opt = 0

      if SearchExprCheck(tlb_find.get(), tlb_regexp.get(), True):
        match = re.match("^" + tlb_find.get(), dump, opt)
        if match:
          off = len(match.group(0))
      else:
        return
    else:
      off = len(tlb_find.get())

    match = re.match(r"^(?:\W+|\w+)", dump[off :])
    if match:
      word = Search_EscapeSpecialChars(match.group(0), tlb_regexp.get())
      tlb_find.set(tlb_find.get() + word)
      wt.f2_e.selection_clear()
      wt.f2_e.selection_range(len(tlb_find.get()) - len(word), "end")
      wt.f2_e.icursor("end")
      wt.f2_e.xview_moveto(1)


#
# This function is bound to "CTRL-SHIFT-D" in the "Find" entry field and
# performs auto-completion to the left by adding any preceding characters
# before the current cursor position.
#
def Search_CompleteLeft():
  global tlb_find, tlb_regexp

  pos = wt.f1_t.index("insert")
  if pos != "":
    dump = ExtractText(pos + " linestart", pos)

    match = re.search(r"(?:\W+|\w+)$", dump)
    if match:
      word = Search_EscapeSpecialChars(match.group(0), tlb_regexp.get())
      tlb_find.set(word + tlb_find.get())
      wt.f2_e.selection_clear()
      wt.f2_e.selection_range(0, len(word))
      wt.f2_e.icursor(len(word))
      wt.f2_e.xview_moveto(0)


#
# This function if bound to "*" and "#" in the main text window (as in VIM)
# These keys allow to search for the word under the cursor in forward and
# backwards direction respectively.
#
def SearchWord(is_fwd):
  global tlb_find, tlb_regexp, tlb_case

  pos = wt.f1_t.index("insert")
  if pos != "":
    # extract word to the right starting at the cursor position
    dump = ExtractText(pos, pos + " lineend")
    match = re.match(r"^[\w\-]+", dump)
    if match:
      word = match.group(0)
      # complete word to the left
      dump = ExtractText(pos + " linestart", pos)
      match = re.search(r"[\w\-]+$", dump)
      if match:
        word = match.group(0) + word

      word = Search_EscapeSpecialChars(word, tlb_regexp.get())

      # add regexp to match on word boundaries
      if tlb_regexp.get():
        word = r"\m" + word + r"\M"

      tlb_find.set(word)
      Search_AddHistory(word, tlb_regexp.get(), tlb_case.get())

      ClearStatusLine("search")
      found = Search_Atomic(tlb_find.get(), tlb_regexp.get(), tlb_case.get(), is_fwd, True)

      if not found:
        Search_HandleNoMatch(tlb_find.get(), is_fwd)


#
# This function moves the cursor onto the next occurence of the given
# character in the current line.
#
def SearchCharInLine(char, dir):
  global last_inline_char, last_inline_dir

  ClearStatusLine("search_inline")
  if char != "":
    last_inline_char = char
    last_inline_dir = dir
  else:
    if last_inline_char is not None:
      char = last_inline_char
      dir = dir * last_inline_dir
    else:
      DisplayStatusLine("search_inline", "error", "No previous in-line character search")
      return

  pos = wt.f1_t.index("insert")
  if pos != "":
    if dir > 0:
      dump = ExtractText(pos, pos + " lineend")
      idx = dump.find(char)
      if idx != -1:
        wt.f1_t.mark_set("insert", "insert + %s chars" % idx)
        wt.f1_t.see("insert")
      else:
        DisplayStatusLine("search_inline", "warn", "Character \"%s\" not found until line end" % char)
    else:
      dump = ExtractText(pos + " linestart", pos)
      idx = dump.rfind(char)
      if idx != -1:
        wt.f1_t.mark_set("insert", "insert - %s chars" % (len(dump) - idx))
        wt.f1_t.see("insert")
      else:
        DisplayStatusLine("search_inline", "warn", "Character \"%s\" not found until line start" % char)


#
# This helper function escapes characters with special semantics in
# regular expressions in a given word. The function is used for adding
# arbitrary text to the search string.
#
def Search_EscapeSpecialChars(word, is_re):
  if is_re:
    #return re.escape(word) # safer, but also escapes space
    # Note target is Tcl's "re_syntax", not Python "re"
    return re.sub(r"([^\w\s_\-\:\=\%\"\!\'\;\,\#\/\<\>\@])", r"\\\1", word)
  else:
    return word


#
# This function is bound to "CTRL-x" in the "Find" entry field and
# removes the current entry from the search history.
#
def Search_RemoveFromHistory():
  global tlb_history, tlb_hist_pos, tlb_hist_prefix

  if (tlb_hist_pos is not None) and (tlb_hist_pos < len(tlb_history)):
    old_sel = SearchHistory_StoreSel()

    del tlb_history[tlb_hist_pos]
    UpdateRcAfterIdle()

    SearchHistory_Fill()
    SearchHistory_RestoreSel(old_sel)

    new_len = len(tlb_history)
    if new_len == 0:
      tlb_hist_pos = None
      tlb_hist_prefix = None
    elif tlb_hist_pos >= new_len:
      tlb_hist_pos = new_len - 1


# ----------------------------------------------------------------------------
#
# This function creates a small overlay which displays a temporary status
# message.
#
def DisplayStatusLine(topic, type, msg):
  global col_bg_content, tid_status_line, status_line_topic

  focus_nam = tk.focus_get()
  focus_wid = tk._nametowidget(focus_nam) if focus_nam is not None else None
  top_wid = focus_wid.winfo_toplevel() if focus_wid is not None else None

  if (top_wid is None):          wid = wt.f1_t
  elif (top_wid == wt.dlg_srch): wid = wt.dlg_srch_f1_l
  elif (top_wid == wt.dlg_hist): wid = wt.dlg_hist_f1_l
  elif (top_wid == wt.dlg_tags): wid = wt.dlg_tags_f1_l
  else:                          wid = wt.f1_t

  if   type == "error": col = "#ff6b6b"
  elif type == "warn":  col = "#ffcc5d"
  else:                 col = col_bg_content

  if not wt_exists(wt.stline):
    wt.stline = Frame(wid, background=col_bg_content, relief=RIDGE, borderwidth=2, takefocus=0)

    wt.stline_l = Label(wt.stline, text=msg, background=col, anchor=W)
    wt.stline_l.pack(side=LEFT, fill=BOTH, expand=1)

    wh = wid.winfo_height()
    fh = tk.call("font", "metrics", wt.stline_l.cget("font"), "-linespace")
    relh = (fh + 10) / (wh + 1)
    wt.stline.place(anchor=SW, bordermode=INSIDE, x=0, y=wh-fh, relheight=relh)
  else:
    wt.stline_l.configure(text=msg, background=col)

  if tid_status_line is not None: tk.after_cancel(tid_status_line)
  tid_status_line = tk.after(4000, lambda: wt.stline.destroy())
  status_line_topic = topic


#
# This function removes the status message display if it's currently visible
# and displays a message on the given topic.
#
def ClearStatusLine(topic):
  global tid_status_line, status_line_topic

  if (status_line_topic is not None) and (topic == status_line_topic):
    SafeDestroy(wt.stline)

    status_line_topic = None
    if tid_status_line is not None: tk.after_cancel(tid_status_line)
    tid_status_line = None


#
# This function is bound to CTRL-G in the main window. It displays the
# current line number and fraction of lines above the cursor in percent
# (i.e. same as VIM)
#
def DisplayLineNumber():
  global cur_filename

  if wt.f1_t.bbox("insert") is not None:
    pos = wt.f1_t.index("insert").split(".")
    if len(pos) == 2:
      (line, char) = map(int, pos)
      (end_line, char) = map(int, wt.f1_t.index("end").split("."))
      # if the last line is properly terminated with a newline char,
      # Tk inserts an empty line below - this should not be counted
      if char == 0:
        end_line -= 1

      if end_line > 0:
        val = int(100.0*line/end_line + 0.5)
        perc = " (%d%%)" % val
      else:
        perc = ""

      name = cur_filename
      if name == "": name = "STDIN"

      DisplayStatusLine("line_query", "msg", "%s: line %d of %d lines%s" %
                                                (name, line, end_line, perc))


#
# This function is bound to configure events on dialog windows, i.e. called
# when the window size or stacking changes. The function stores the new size
# and position so that the same geometry is automatically applied when the
# window is closed and re-opened.
#
# Note: this event is installed on the toplevel window, but also called for
# all its childs when they are resized (due to the bindtag mechanism.) This
# is the reason for passing widget and compare parameters.
#
def ToplevelResized(wid, top, cmp, var_id):
  global win_geom

  if wid == cmp:
    new_size = top.wm_geometry()

    if new_size != win_geom.get(var_id):
      win_geom[var_id] = new_size
      UpdateRcAfterIdle()


#
# Helper function to modify a font's size or appearance
#
def DeriveFont(afont, delta_size, style=None):
  afont = afont.copy()

  if (delta_size != 0):
    afont.configure(size=afont.cget("size")+delta_size)

  if style:
    if style == "bold":
      afont.configure(weight=tkf.BOLD)
    elif style == "underline":
      afont.configure(underline=1)

  return afont


#
# This function is bound to the Control +/- keys as a way to quickly
# adjust the content font size (as in web browsers)
#
def ChangeFontSize(delta):
  global font_content

  font_content.configure(size=font_content.cget("size") + delta)

  cerr = ApplyFont()
  if cerr is not None:
    DisplayStatusLine("font", "error", "Failed to apply the new font: " + cerr)
  else:
    ClearStatusLine("font")


#
# This function is called after a new fonct has been configured to apply
# the new font in the main window, text highlight tags and dialog texts.
# The function returns 1 on success and saves the font setting in the RC.
#
def ApplyFont():
  global font_content

  cerr = None
  try:
    wt.f1_t.configure(font=font_content)

    # save to rc
    UpdateRcAfterIdle()

    # apply font change to dialogs
    if wt_exists(wt.dlg_mark):
      wt.dlg_mark_l.configure(font=font_content)
    if wt_exists(wt.dlg_hist):
      wt.dlg_hist_f1_l.configure(font=font_content)
    if wt_exists(wt.dlg_srch):
      wt.dlg_srch_f1_l.configure(font=font_content)
    if wt_exists(wt.dlg_tags):
      wt.dlg_tags_f1_l.configure(font=font_content)

    # update font in highlight tags (in case some contain font modifiers)
    HighlightCreateTags()
    SearchList_CreateHighlightTags()
    MarkList_CreateHighlightTags()
  except Exception as e:
    cerr = "Failed to configure font:" + str(e)

  return cerr


#
# This function is bound to ALT-w and toggles wrapping of long lines
# in the main window.
#
def ToggleLineWrap():
  cur = wt.f1_t.cget("wrap")
  if cur == "none":
    wt.f1_t.configure(wrap=CHAR)
  else:
    wt.f1_t.configure(wrap=NONE)


#
# This function adjusts the view so that the line holding the cursor is
# placed at the top, center or bottom of the viewable area, if possible.
#
def YviewSet(wid, where, col):
  global font_content

  wid.see("insert")
  pos = wid.bbox("insert")
  if pos is not None:
    fh = font_content.metrics("linespace")
    wh = wid.winfo_height()
    bbox_y = pos[1]
    bbox_h = pos[3]

    if where == "top":
      delta = bbox_y // fh

    elif where == "center":
      delta = 0 - int((wh/2 - bbox_y + bbox_h/2) / fh)

    elif where == "bottom":
      delta = 0 - int((wh - bbox_y - bbox_h) / fh)

    else:
      delta = 0

    if delta > 0:
      wid.yview_scroll(delta, "units")
    elif delta < 0:
      wid.yview_scroll(delta, "units")

    if col == 0:
      wid.xview_moveto(0)
      wid.mark_set("insert", "insert linestart")
      CursorMoveLine(wid, 0)
    else:
      wid.see("insert")

  # synchronize the search result list (if open) with the main text
  (idx_l, idx_c) = map(int, wt.f1_t.index("insert").split("."))
  SearchList_MatchView(idx_l)


#
# This function scrolls the view vertically by the given number of lines.
# When the line holding the cursor is scrolled out of the window, the cursor
# is placed in the last visible line in scrolling direction.
#
def YviewScroll(wid, delta):
  global font_content

  wid.yview_scroll(delta, "units")

  fh = font_content.metrics("linespace")
  pos = wid.bbox("insert")

  # check if cursor is fully visible
  if (pos is None) or (pos[3] < fh):
    if delta < 0:
      wid.mark_set("insert", "@1,%d - 1 lines linestart" % wid.winfo_height())
    else:
      wid.mark_set("insert", "@1,1")


#
# This function scrolls the view vertically by half the screen height
# in the given direction.
#
def YviewScrollHalf(wid, dir):
  global font_content

  wh = wid.winfo_height()
  fh = font_content.metrics("linespace")
  if fh > 0:
    wh = int((wh + fh/2) / fh)
    YviewScroll(wid, int(wh/2 * dir))

#
# This function moves the cursor into a given line in the current view.
#
def CursorSetLine(wid, where, off):
  CursorJumpPushPos(wid)

  if where == "top":
    index = wid.index("@1,1 + %d lines" % off)
    if (off > 0) and not IsRowFullyVisible(wid, index):
      # offset out of range - set to bottom instead
      return CursorSetLine(wid, "bottom", 0)
    else:
      wid.mark_set("insert", index)

  elif where == "center":
    # note the offset parameter is not applicable in this case
    wid.mark_set("insert", "@1,%d" % (wid.winfo_height() // 2))

  elif where == "bottom":
    index = wid.index("@1,%d linestart - %d lines" % (wid.winfo_height(), off))
    if not IsRowFullyVisible(wid, index):
      if off == 0:
        # move cursor to the last fully visible line to avoid scrolling
        index = wid.index(index + " - 1 lines")
      else:
        # offset out of range - set to top instead
        return CursorSetLine(wid, "top", 0)

    wid.mark_set("insert", index)
  else:
    wid.mark_set("insert", "insert linestart")

  # place cursor on first non-blank character in the selected row
  CursorMoveLine(wid, 0)


#
# This function moves the cursor by the given number of lines up or down,
# scrolling if needed to keep the cursor position visible.
#
def CursorMoveUpDown(wid, delta):
  wid.mark_set("insert", "insert + %d lines" % delta)
  wid.see("insert")

#
# This function moves the cursor by the given number of characters left or
# right.  The cursor will wrap to the next line at the end of a line. The view
# is scrolling if needed to keep the cursor position visible.
#
def CursorMoveLeftRight(wid, delta):
  wid.mark_set("insert", "insert + %d chars" % delta)
  wid.see("insert")

#
# This function moves the cursor by the given number of lines and places
# the cursor on the first non-blank character in that line. The delta may
# be zero (e.g. to just place the cursor onto the first non-blank)
#
def CursorMoveLine(wid, delta):
  if delta > 0:
    wid.mark_set("insert", "insert linestart + %d lines" % delta)
  elif delta < 0:
    wid.mark_set("insert", "insert linestart %d lines" % delta)

  wid.xview_moveto(0)

  # forward to the first non-blank character
  dump = ExtractText("insert", "insert lineend")
  spc = len(dump) - len(dump.lstrip())
  if spc > 0:
    wid.mark_set("insert", "insert + %d chars" % spc)

  wid.see("insert")


#
# This function moves the cursor to the start or end of the main text.
# Additionally the cursor position prior to the jump is remembered in
# the jump stack.
#
def CursorGotoLine(wid, where):
  CursorJumpPushPos(wt.f1_t)
  if where == "start":
    wid.mark_set("insert", "1.0")
  elif where == "end":
    wid.mark_set("insert", "end -1 lines linestart")
  else:
    try:
      where_val = int(where)
      if where_val >= 0:
        wid.mark_set("insert", str(where) + ".0")
      else:
        wt.f1_t.mark_set("insert", "end - %d lines linestart" % (1 - where_val))
    except:
      pass

  # place the cursor on the first character in the line and make it visible
  CursorMoveLine(wid, 0)


#
# This function scrolls the view horizontally by the given number of characters.
# When the cursor is scrolled out of the window, it's placed in the last visible
# column in scrolling direction.
#
def XviewScroll(wid, how, delta, dir):
  pos_old = wid.bbox("insert")

  if how == "scroll":
    wid.xview_scroll(dir * delta, "units")
  else:
    wid.xview_moveto(delta)

  if pos_old is not None:
    # check if cursor is fully visible
    pos_new = wid.bbox("insert")
    if (pos_new is None) or (pos_new[2] == 0):
      ycoo = pos_old[1] + pos_old[3] // 2
      if dir < 0:
        wid.mark_set("insert", "@%d,%d" % (wid.winfo_width(), ycoo))
      else:
        wid.mark_set("insert", "@1,%d + 1 chars" % ycoo)


#
# This function scrolls the view horizontally by half the screen width
# in the given direction.
#
def XviewScrollHalf(wid, dir):
  xpos = wid.xview()
  w = wid.winfo_width()
  if w != 0:
    fract_visible = xpos[1] - xpos[0]
    off = xpos[0] + dir * (0.5 * fract_visible)
    if off > 1:  off = 1
    if off < 0:  off = 0

  XviewScroll(wid, "moveto", off, dir)


#
# This function adjusts the view so that the column holding the cursor is
# placed at the left or right of the viewable area, if possible.
#
def XviewSet(wid, where):
  xpos = wid.xview()
  coo = wid.bbox("insert")
  w = wid.winfo_width()
  if (coo is not None) and (w != 0):
    fract_visible = xpos[1] - xpos[0]
    fract_insert = (2 + coo[0] + coo[2]) / w

    if where == "left":
      off = xpos[0] + (fract_insert * fract_visible)
      if off > 1.0: off = 1.0
    else:
      off = xpos[0] - ((1 - fract_insert) * fract_visible)
      if off < 0.0: off = 0.0

    wid.xview_moveto(off)
    wid.see("insert")


#
# This function moves the cursor into a given column in the current view.
#
def CursorSetColumn(wid, where):
  if where == "left":
    wid.xview_moveto(0.0)
    wid.mark_set("insert", "insert linestart")

  elif where == "right":
    wid.mark_set("insert", "insert lineend")
    wid.see("insert")

  elif where == "left_line":
    wid.mark_set("insert", "insert linestart")
    CursorMoveLine(wid, 0)


#
# This function moves the cursor onto the next or previous word.
# (Same as "w", "b" et.al. in vim)
#
def CursorMoveWord(is_fwd, spc_only, to_end):
  pos = wt.f1_t.index("insert")
  if pos != "":
    if is_fwd:
      dump = ExtractText(pos, pos + " lineend")
      if spc_only:
        if to_end:
          match = re.match("^\s*\S*", dump)
        else:
          match = re.match("^\S*\s*", dump)
      else:
        if to_end:
          match = re.match("^\W*\w*", dump)
        else:
          match = re.match("^\w*\W*", dump)

      if match and ((len(match.group(0)) < len(dump)) or to_end):
        wt.f1_t.mark_set("insert", "insert + %d chars" % len(match.group(0)))
      else:
        wt.f1_t.mark_set("insert", "insert linestart + 1 lines")

    else:
      dump = ExtractText(pos + " linestart", pos)
      if spc_only:
        if to_end:
          match = re.search("\s(\s+)$", dump)
        else:
          match = re.search("(\S+\s*)$", dump)
      else:
        if to_end:
          match = re.search("\w(\W+\w*)$", dump)
        else:
          match = re.search("(\w+|\w+\W+)$", dump)

      if match:
        wt.f1_t.mark_set("insert", "insert - %d chars" % len(match.group(1)))
      else:
        wt.f1_t.mark_set("insert", "insert - 1 lines lineend")

    wt.f1_t.see("insert")


#
# Helper function which determines if the text at the given index in
# the given window is fully visible.
#
def IsRowFullyVisible(wid, index):
  global font_content

  fh = font_content.metrics("linespace")
  bbox = wid.bbox("insert")

  if (bbox is None) or (bbox[3] < fh):
    return 0
  else:
    return 1


#
# Helper function to extrace a range of characters from the content.
#
def ExtractText(pos1, pos2):
  dump = []
  for (key, val, idx) in wt.f1_t.dump(pos1, pos2, text=1):
    if key == "text":
      dump.append(val)

  return ''.join(dump)


#
# This function is called by all key bindings which make a large jump to
# push the current cusor position onto the jump stack. Both row and column
# are stored.  If the position is already on the stack, this entry is
# deleted (note for this comparison only the line number is considered.)
#
def CursorJumpPushPos(wid):
  global cur_jump_stack, cur_jump_idx

  if wid == wt.f1_t:
    cur_pos = wt.f1_t.index("insert")
    line = int(cur_pos.split(".")[0])
    # remove the line if already on the stack
    idx = 0
    for pos in cur_jump_stack:
      prev_line = int(pos.split(".")[0])
      if prev_line == line:
        del cur_jump_stack[idx]
        break
      idx += 1

    # append to the stack
    cur_jump_stack.append(cur_pos)
    cur_jump_idx = -1

    # limit size of the stack
    if len(cur_jump_stack) > 100:
      del cur_jump_stack[-1]


#
# This function is bound to command "''" (i.e. two apostrophes) in the main
# window. The command makes the cursor jump back to the origin of the last
# jump (NOT to the target of the last jump, which may be confusing.) The
# current position is pushed to the jump stack, if not already on the stack.
#
def CursorJumpToggle(wid):
  global cur_jump_stack, cur_jump_idx

  if len(cur_jump_stack) > 0:
    ClearStatusLine("keycmd")

    # push current position to the stack
    CursorJumpPushPos(wid)

    if len(cur_jump_stack) > 1:
      cur_jump_idx = len(cur_jump_stack) - 2
      pos = cur_jump_stack[cur_jump_idx]

      # FIXME this moves the cursor the the first char instead of the stored position
      try:
        wid.mark_set("insert", pos)
      except:
        pass
      CursorMoveLine(wid, 0)
      line = int(pos.split(".")[0])
      SearchList_MatchView(line)
    else:
      DisplayStatusLine("keycmd", "warn", "Already on the mark.")
  else:
    DisplayStatusLine("keycmd", "error", "Jump stack is empty.")


#
# This function is bound to the CTRL-O and CTRL-I commands in the main
# window. The function traverses backwards or forwards respectively
# through the jump stack. During the first call the current cursor
# position is pushed to the stack.
#
def CursorJumpHistory(wid, rel):
  global cur_jump_stack, cur_jump_idx

  ClearStatusLine("keycmd")
  if len(cur_jump_stack) > 0:
    if cur_jump_idx < 0:
      # push current position to the stack
      CursorJumpPushPos(wid)
      if (rel < 0) and (len(cur_jump_stack) >= 2):
        cur_jump_idx = len(cur_jump_stack) - 2
      else:
        cur_jump_idx = 0
    else:
      cur_jump_idx += rel
      if cur_jump_idx < 0:
        cur_jump_idx = len(cur_jump_stack) - 1
        DisplayStatusLine("keycmd", "warn", "Jump stack wrapped from oldest to newest.")
      elif cur_jump_idx >= len(cur_jump_stack):
        DisplayStatusLine("keycmd", "warn", "Jump stack wrapped from newest to oldest.")
        cur_jump_idx = 0

    pos = cur_jump_stack[cur_jump_idx]
    try:
      wid.mark_set("insert", pos)
    except:
      pass
    CursorMoveLine(wid, 0)

    line = int(pos.split(".")[0])
    SearchList_MatchView(line)

  else:
    DisplayStatusLine("keycmd", "error", "Jump stack is empty.")


#
# This helper function is used during start-up to store key bindings in a
# hash. These bindings are evaluated by function KeyCmd, which receives all
# "plain" key press events from the main window (i.e. non-control keys)
#
def KeyCmdBind(wid, char, cmd):
  reg = key_cmd_reg.get(wid)
  if reg is None:
    reg = {}
    key_cmd_reg[wid] = reg

  reg[char] = cmd


#
# This function is bound to key presses in the main window. It's called
# when none of the single-key bindings match. It's intended to handle
# complex key sequences, but also has to handle single key bindings for
# keys which can be part of sequences (e.g. "b" due to "zb")
#
def KeyCmd(wid, char):
  global last_key_char

  reg = key_cmd_reg.get(wid)
  if reg is None: return

  result = 0
  if char != "":
    if last_key_char == "'":
      # single quote char: jump to marker or bookmark
      ClearStatusLine("keycmd")
      if char == "'":
        CursorJumpToggle(wid)
      elif char == "^":
        # '^ and '$ are from less
        CursorGotoLine(wid, "start")
      elif char == "$":
        CursorGotoLine(wid, "end")
      elif char == "+":
        Mark_JumpNext(True)
      elif char == "-":
        Mark_JumpNext(False)
      else:
        DisplayStatusLine("keycmd", "error", "Undefined key sequence \"'%s\"" % char)

      last_key_char = ""
      result = 1

    elif (last_key_char == "z") or (last_key_char == "g"):
      ClearStatusLine("keycmd")
      char = last_key_char + char
      cb = reg.get(char)
      if cb is not None:
        cb()
      else:
        DisplayStatusLine("keycmd", "error", "Undefined key sequence \"%s\"" % char)

      last_key_char = ""
      result = 1

    elif last_key_char == "f":
      SearchCharInLine(char, 1)
      last_key_char = ""
      result = 1

    elif last_key_char == "F":
      SearchCharInLine(char, -1)
      last_key_char = ""
      result = 1

    else:
      last_key_char = ""

      cb = reg.get(char)
      if cb is not None:
        cb()

      elif "1" <= char <= "9":
        KeyCmd_OpenDialog("any", char)
        last_key_char = ""
        result = 1

      elif char in "z'fFg":
        last_key_char = char
        result = 1

  # return 1 if the key was consumed, else 0
  return result


#
# This function is called for all explicit key bindings to forget about
# any previously buffered partial multi-keypress commands.
#
def KeyClr():
  global last_key_char
  last_key_char = ""

def KeyHomeEnd(wid, dir):
  CursorSetColumn(wid, dir)
  KeyClr()

#
# This function adds key bindings for scrolling vertically
# to the given text widget.
#
def KeyBinding_UpDown(wid):
  wid.bind("<Control-Up>", lambda e: BindCallKeyClrBreak(lambda:YviewScroll(e.widget, -1)))
  wid.bind("<Control-Down>", lambda e: BindCallKeyClrBreak(lambda:YviewScroll(e.widget, 1)))
  wid.bind("<Control-f>", lambda e: BindCallKeyClrBreak(lambda:e.widget.event_generate("<Key-Next>")))
  wid.bind("<Control-b>", lambda e: BindCallKeyClrBreak(lambda:e.widget.event_generate("<Key-Prior>")))
  wid.bind("<Control-e>", lambda e: BindCallKeyClrBreak(lambda:YviewScroll(e.widget, 1)))
  wid.bind("<Control-y>", lambda e: BindCallKeyClrBreak(lambda:YviewScroll(e.widget, -1)))
  wid.bind("<Control-d>", lambda e: BindCallKeyClrBreak(lambda:YviewScrollHalf(e.widget, 1)))
  wid.bind("<Control-u>", lambda e: BindCallKeyClrBreak(lambda:YviewScrollHalf(e.widget, -1)))

  KeyCmdBind(wid, "z-", lambda: YviewSet(wid, "bottom", 0))
  KeyCmdBind(wid, "zb", lambda: YviewSet(wid, "bottom", 1))
  KeyCmdBind(wid, "z.", lambda: YviewSet(wid, "center", 0))
  KeyCmdBind(wid, "zz", lambda: YviewSet(wid, "center", 1))
  KeyCmdBind(wid, "zReturn", lambda: YviewSet(wid, "top", 0))
  KeyCmdBind(wid, "zt", lambda: YviewSet(wid, "top", 1))

  KeyCmdBind(wid, "+", lambda: CursorMoveLine(wid, 1))
  KeyCmdBind(wid, "-", lambda: CursorMoveLine(wid, -1))
  KeyCmdBind(wid, "k", lambda: wid.event_generate("<Up>"))
  KeyCmdBind(wid, "j", lambda: wid.event_generate("<Down>"))
  KeyCmdBind(wid, "H", lambda: CursorSetLine(wid, "top", 0))
  KeyCmdBind(wid, "M", lambda: CursorSetLine(wid, "center", 0))
  KeyCmdBind(wid, "L", lambda: CursorSetLine(wid, "bottom", 0))

  KeyCmdBind(wid, "G", lambda: CursorGotoLine(wid, "end"))
  KeyCmdBind(wid, "gg", lambda: CursorGotoLine(wid, "start"))


#
# This function adds key bindings for scrolling horizontally
# to the given text widget.
#
def KeyBinding_LeftRight(wid):
  wid.bind("<Control-Left>", lambda e: BindCallKeyClrBreak(lambda:XviewScroll(e.widget, "scroll", 1, -1)))
  wid.bind("<Control-Right>", lambda e: BindCallKeyClrBreak(lambda:XviewScroll(e.widget, "scroll", 1, 1)))

  KeyCmdBind(wid, "zl", lambda:XviewScroll(wid, "scroll", 1, 1))
  KeyCmdBind(wid, "zh", lambda:XviewScroll(wid, "scroll", 1, -1))
  KeyCmdBind(wid, "zL", lambda:XviewScrollHalf(wid, 1))
  KeyCmdBind(wid, "zH", lambda:XviewScrollHalf(wid, -1))
  KeyCmdBind(wid, "zs", lambda:XviewSet(wid, "left"))
  KeyCmdBind(wid, "ze", lambda:XviewSet(wid, "right"))

  KeyCmdBind(wid, "0", lambda:CursorSetColumn(wid, "left"))
  KeyCmdBind(wid, "^", lambda:CursorSetColumn(wid, "left_line"))
  KeyCmdBind(wid, "$", lambda:CursorSetColumn(wid, "right"))


# ----------------------------------------------------------------------------
#
# This function opens a tiny "overlay" dialog which allows to enter a line
# number.  The dialog is placed into the upper left corner of the text
# widget in the main window.
#
def KeyCmd_OpenDialog(type, txt=""):
  global keycmd_ent

  PreemptBgTasks()
  if not wt_exists(wt.dlg_key_e):
    wt.dlg_key = Frame(wt.f1_t, borderwidth=2, relief=RAISED)

    if type == "goto":
      cmd_text = "Goto line:"
    else:
      cmd_text = "Command:"

    keycmd_ent = StringVar(tk, txt)

    wt.dlg_key_l = Label(wt.dlg_key, text=cmd_text)
    wt.dlg_key_l.pack(side=LEFT, padx=5)
    wt.dlg_key_e = Entry(wt.dlg_key, width=12, textvariable=keycmd_ent, exportselection=0)
    wt.dlg_key_e.pack(side=LEFT, padx=5)

    if type == "goto":
      wt.dlg_key_e.bind("<Return>", lambda e: BindCallAndBreak(KeyCmd_ExecGoto))
    else:
      # line goto key binding
      wt.dlg_key_e.bind("<Key-g>", lambda e: BindCallAndBreak(KeyCmd_ExecGoto))
      wt.dlg_key_e.bind("<Shift-Key-G>", lambda e: BindCallAndBreak(KeyCmd_ExecGoto))
      # vertical cursor movement binding
      wt.dlg_key_e.bind("<Key-minus>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorUpDown(0)))
      wt.dlg_key_e.bind("<Key-plus>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorUpDown(1)))
      wt.dlg_key_e.bind("<Return>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorUpDown(1)))
      wt.dlg_key_e.bind("<Key-bar>", lambda e: KeyCmd_ExecAbsColumn)
      wt.dlg_key_e.bind("<Key-k>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorUpDown(0)))
      wt.dlg_key_e.bind("<Key-j>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorUpDown(1)))
      wt.dlg_key_e.bind("<Key-H>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorVertSet("top")))
      wt.dlg_key_e.bind("<Key-M>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorVertSet("center")))
      wt.dlg_key_e.bind("<Key-L>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorVertSet("bottom")))
      # horizontal/in-line cursor movement binding
      wt.dlg_key_e.bind("<Key-w>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-w>")))
      wt.dlg_key_e.bind("<Key-e>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-e>")))
      wt.dlg_key_e.bind("<Key-b>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-b>")))
      wt.dlg_key_e.bind("<Key-W>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-W>")))
      wt.dlg_key_e.bind("<Key-E>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-E>")))
      wt.dlg_key_e.bind("<Key-B>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-B>")))
      wt.dlg_key_e.bind("<Key-colon>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-colon>")))
      wt.dlg_key_e.bind("<Key-semicolon>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-semicolon>")))
      wt.dlg_key_e.bind("<Key-space>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-space>")))
      wt.dlg_key_e.bind("<Key-BackSpace>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-BackSpace>")))
      wt.dlg_key_e.bind("<Key-h>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-h>")))
      wt.dlg_key_e.bind("<Key-l>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecCursorMove("<Key-l>")))
      # search key binding
      wt.dlg_key_e.bind("<Key-n>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecSearch(1)))
      wt.dlg_key_e.bind("<Key-p>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecSearch(0)))
      wt.dlg_key_e.bind("<Shift-Key-N>", lambda e: BindCallAndBreak(lambda:KeyCmd_ExecSearch(0)))
      # catch-all
      wt.dlg_key_e.bind("<KeyPress>", lambda e: KeyCmd_KeyPress(e))

    wt.dlg_key_e.bind("<Escape>", lambda e: BindCallAndBreak(KeyCmd_Leave))
    wt.dlg_key_e.bind("<Leave>", lambda e: BindCallAndBreak(KeyCmd_Leave))
    wt.dlg_key_e.bind("<FocusOut>", lambda e: BindCallAndBreak(KeyCmd_Leave))
    wt.dlg_key.bind("<Leave>", lambda e: wt.dlg_key.destroy())
    wt.dlg_key_e.icursor("end")

    wt.dlg_key.place(anchor=NW, x=0, y=0) #in=wt.f1_t
    wt.dlg_key_e.focus_set()

  ResumeBgTasks()

def KeyCmd_KeyPress(ev):
  if ev.char == "|":
    # work-around: keysym <Key-bar> doesn't work on German keyboard
    KeyCmd_ExecAbsColumn()
    return "break"
  elif not ev.char.isdigit() and re.match(r'[\x21-\x7e]\s', ev.char): # [[:graph:]][[:space:]]
    return "break"
  else:
    return None


#
# This function is bound to all events which signal an exit of the goto
# dialog window. The window is destroyed.
#
def KeyCmd_Leave():
  global keycmd_ent

  wt.f1_t.focus_set()
  wt.dlg_key.destroy()
  del keycmd_ent


#
# This function scrolls the text in the main window to the line number
# entered in the "goto line" dialog window and closes the dialog. Line
# numbers start with 1. If the line numbers is negative, -1 refers to
# the last line.
#
def KeyCmd_ExecGoto():
  global keycmd_ent

  # check if the content is a valid line number
  val = 0
  try:
    val = int(keycmd_ent.get())
  except:
    DisplayStatusLine("keycmd", "error", "Input is not a valid line number: \"%s\"" % keycmd_ent.get())
    return

  ClearStatusLine("keycmd")
  CursorJumpPushPos(wt.f1_t)
  CursorGotoLine(wt.f1_t, val)
  KeyCmd_Leave()


#
# This function moves the cursor up or down by a given number of lines.
#
def KeyCmd_ExecCursorUpDown(is_fwd):
  global keycmd_ent

  # check if the content is a valid line number
  val = 0
  try:
    val = int(keycmd_ent.get())
  except:
    DisplayStatusLine("keycmd", "error", "Cursor movement commands require numeric input.")
    return

  ClearStatusLine("keycmd")
  if is_fwd:
    wt.f1_t.mark_set("insert", "insert linestart + %d lines" % val)
  else:
    wt.f1_t.mark_set("insert", "insert linestart - %d lines" % val)

  wt.f1_t.xview_moveto(0)
  wt.f1_t.see("insert")
  KeyCmd_Leave()


#
# This function sets the cursor into a given row, relative to top, or bottom.
# Placement into the middle is also supported, but without offset.
#
def KeyCmd_ExecCursorVertSet(where):
  global keycmd_ent

  # check if the content is a valid line number
  val = 0
  try:
    val = int(keycmd_ent.get())
  except:
    DisplayStatusLine("keycmd", "error", "Cursor movement commands require numeric input.")
    return

  ClearStatusLine("keycmd")
  CursorSetLine(wt.f1_t, where, val)
  KeyCmd_Leave()


#
# This function sets the cursor into a given column
#
def KeyCmd_ExecAbsColumn():
  global keycmd_ent

  # check if the content is a valid line number
  val = 0
  try:
    val = int(keycmd_ent.get())
  except:
    DisplayStatusLine("keycmd", "error", "Cursor column placement requires numeric input.")
    return

  ClearStatusLine("keycmd")
  # prevent running beyond the end of the line
  (max_line, max_col) = map(int, wt.f1_t.index("insert lineend").split("."))
  if val < max_col:
    wt.f1_t.mark_set("insert", "insert linestart + %d chars" % val)
  else:
    wt.f1_t.mark_set("insert", "insert lineend")

  wt.f1_t.see("insert")
  KeyCmd_Leave()


#
# This function starts a search from within the command popup window.
#
def KeyCmd_ExecSearch(is_fwd):
  global tlb_history, tlb_find, tlb_regexp, tlb_case
  global keycmd_ent

  # check if the content is a repeat count
  val = 0
  try:
    val = int(keycmd_ent.get())
  except:
    DisplayStatusLine("keycmd", "error", "Search repetition requires a numeric value as input.")
    return

  KeyCmd_Leave()
  ClearStatusLine("keycmd")
  ClearStatusLine("search")

  if tlb_find == "":
    # empty expression: repeat last search
    if len(tlb_history) > 0:
      hl = tlb_history[0]
      tlb_find.set(hl[0])

  if tlb_find.get() != "":
    count = 0
    for idx in range(val):
      found = Search_Atomic(tlb_find.get(), tlb_regexp.get(), tlb_case.get(), is_fwd, False)
      if found == "":
        limit = "end" if is_fwd else "start"
        if count == 0:
          DisplayStatusLine("search", "warn", "No match until %d of file: %s" % (limit, tlb_find.get()))
        else:
          DisplayStatusLine("search", "warn",
                            "Only %d of %d matches until %d of file" % (count, val, limit))
      count += 1
  else:
    DisplayStatusLine("search", "error", "No pattern defined for search repetition")


#
# This function moves the cursor as if the given key had been pressed
# the number of times specified in the number entry field.
#
def KeyCmd_ExecCursorMove(key):
  global keycmd_ent

  # check if the content is a repeat count
  val = 0
  try:
    val = int(keycmd_ent.get())
  except:
    DisplayStatusLine("keycmd", "error", "Cursor movement commands require numeric input.")
    return

  if val < 10000:
    ClearStatusLine("keycmd")
    KeyCmd_Leave()
    for idx in range(val):
      wt.f1_t.event_generate(key)
  else:
    DisplayStatusLine("keycmd", "error", "Repetition value too large: %d" % val)



# ----------------------------------------------------------------------------
#
# This function retrieves the "frame number" (timestamp) to which a given line
# of text belongs via pattern matching. Two methods for retrieving the number
# can be used, depending on which patterns are defined.
#
def ParseFrameTickNo(pos, fn_cache=None):
  global tick_pat_sep, tick_pat_num, tick_str_prefix

  # query the cache before parsing for the frame number
  if fn_cache is not None:
    if fn_cache.get(pos) is not None:
      # FN of this line is already known
      return fn_cache[pos]
    elif fn_cache.get(-1):
      prev_rslt = fn_cache[-1]
      line = int(wt.f1_t.index(pos).split(".")[0])
      if (line >= prev_rslt[0]) and (line < prev_rslt[1]):
        # line is within the range of the most recently parsed frame
        fn_cache[pos] = prev_rslt[2:4]
        return fn_cache[pos]

  # catch Reg-Exp exceptions because patterns are supplied by user
  try:
    if tick_pat_sep != "":
      # determine frame number by searching forwards and backwards for frame boundaries
      # marked by a frame separator pattern; then within these boundaries search for FN
      fn = ""
      tick_no = ""
      pos1 = wt.f1_t.search(tick_pat_sep, pos+" lineend", "1.0", regexp=1, backwards=1)
      if pos1 != "":
        dump = ExtractText(pos1, pos1 + " lineend")
        # FIXME Should not use Python "re" as syntax may be incompatible
        match = re.match(tick_pat_sep, dump)
        if match:
          tick_no = match.group(1)
      else:
        pos1 = "1.0"

      pos2 = wt.f1_t.search(tick_pat_sep, pos+" lineend", "end", regexp=1, forwards=1)
      if pos2 == "": pos2 = "end"

      match_len = IntVar(tk, 0)
      pos3 = wt.f1_t.search(tick_pat_num, pos1, pos2, regexp=1, count=match_len)
      if pos3 != "":
        dump = ExtractText(pos3, "%s + %d chars" % (pos3, match_len.get()))

        # FIXME Should not use Python "re" as syntax may be incompatible
        match = re.match(tick_pat_num, dump)
        if match:
          fn = match.group(1)

      prefix = [fn, tick_no]

      if fn_cache is not None:
        # add result to the cache
        fn_cache[pos] = prefix
        # add a special entry to the cache remembering the extent of the current frame
        line1 = int(wt.f1_t.index(pos1).split(".")[0])
        line2 = int(wt.f1_t.index(pos2).split(".")[0])
        fn_cache[-1] = [line1, line2, fn, tick_no]

    elif tick_pat_num != "":
      # determine frame number by searching backwards for the line holding the FN
      match_len = IntVar(tk, 0)
      prefix = []
      pos3 = wt.f1_t.search(tick_pat_num, pos + " lineend", "1.0", regexp=1, backwards=1, count=match_len)
      if pos3 != "":
        dump = ExtractText(pos3, "%s + %d chars" % (pos3, match_len.get()))
        match = re.match(tick_pat_num, dump)
        if match:
          prefix = [match.group(1), ""]

      if fn_cache is not None:
        fn_cache[pos] = prefix

    else:
      # FN parsing is disabled: omit the prefix
      prefix = []

  except re.error as e:
    print("Warning: tick pattern match error: " + e.msg, file=sys.stderr)
  except IndexError as e:
    print("Warning: tick pattern contains no capture", file=sys.stderr)

  return prefix


#
# This function adds or removes a bookmark at the given text line.
# The line is marked by inserting an image in the text and the bookmark
# is added to the bookmark list dialog, if it's currently open.
#
def Mark_Toggle(line, txt=""):
  global img_marker, mark_list, mark_list_modified
  global tick_str_prefix

  if mark_list.get(line) is None:
    if txt == "":
      fn_prefix = ParseFrameTickNo("insert")
      txt = ExtractText("%d.0" % line, "%d.0 lineend" % line).strip()
      mark_list[line] = tick_str_prefix + " ".join(str(x) for x in fn_prefix) + " " + txt
    else:
      mark_list[line] = txt

    wt.f1_t.image_create("%d.0" % line, image=img_marker, padx=5)
    wt.f1_t.tag_add("bookmark", "%d.0" % line)

    # extend highlighting tags to the inserted bookmark char
    for (key, val, idx) in wt.f1_t.dump("%d.1" % line, tag=1):
      if key == "tagon":
        wt.f1_t.tag_add(val, "%d.0" % line, idx)

    MarkList_Add(line)

  else:
    del mark_list[line]
    wt.f1_t.delete("%d.0" % line, "%d.1" % line)
    MarkList_Delete(line)

  SearchList_MarkLine(line)
  mark_list_modified = True


#
# This function adds or removes a bookmark at the current cursor position.
# The function is used to set bookmarks via key bindings.
#
def Mark_ToggleAtInsert():
  pos = wt.f1_t.index("insert")
  if pos != "":
    line = int(pos.split(".")[0])
    Mark_Toggle(line)


#
# This function moves the cursor into the given line and highlights the entire
# line. The "line" parameter is a text widget line number, starting at 1.
#
def Mark_Line(line):
  CursorJumpPushPos(wt.f1_t)

  # move the cursor into the specified line
  wt.f1_t.mark_set("insert", "%d.0" % line)
  wt.f1_t.see("insert")

  # remove a possible older highlight
  SearchHighlightClear()

  # highlight the specified line
  wt.f1_t.tag_add("find", "%d.0" % line, "%d.0" % (line + 1))


#
# This function moves the cursor onto the next bookmark in the given
# direction.
#
def Mark_JumpNext(is_fwd):
  global mark_list

  pos = wt.f1_t.index("insert")
  if pos != "":
    goto = None
    line = int(pos.split(".")[0])
    if is_fwd:
      for mark_line in sorted(mark_list.keys()):
        if mark_line > line:
          goto = mark_line
          break

    else:
      for mark_line in sorted(mark_list.keys(), reverse=True):
        if mark_line < line:
          goto = mark_line
          break

    if goto is not None:
      CursorJumpPushPos(wt.f1_t)
      wt.f1_t.mark_set("insert", "%d.0" % goto)
      wt.f1_t.see("insert")
      wt.f2_e.xview_moveto(0)

    else:
      if len(mark_list) == 0:
        DisplayStatusLine("bookmark", "error", "No bookmarks have been defined yet")
      elif is_fwd:
        DisplayStatusLine("bookmark", "warn", "No more bookmarks until end of file")
      else:
        DisplayStatusLine("bookmark", "warn", "No more bookmarks until start of file")


#
# This function deletes all bookmarks. It's called via the main menu.
# The function is intended esp. if a large number of bookmarks was imported
# previously from a file.
#
def Mark_DeleteAll():
  global mark_list, mark_list_modified

  count = len(mark_list)
  if count > 0:
    pls = "" if (count == 1) else "s"
    msg = "Really delete %d bookmark%s?" % (count, pls)
    answer = messagebox.askokcancel(parent=tk, message=msg)
    if answer:
      for line in list(mark_list.keys()): # copy keys before erasing from dict
        Mark_Toggle(line)
      mark_list_modified = False

  else:
    messagebox.showinfo(parent=tk, message="Your bookmark list is already empty.")


#
# This function reads a list of line numbers and tags from a file and
# adds them to the bookmark list. (Note already existing bookmarks are
# not discarded, hence there's no warning when bookmarks already exist.)
#
def Mark_ReadFile(filename):
  global mark_list, mark_list_modified

  bol = []
  line_num = 0
  try:
    with open(filename, "r") as f:
      for line in f.readlines():
        line_num += 1
        if re.match(r"^\s*(?:#|$)", line):
          continue

        match = re.match(r"^(\d+)(?:[ \t\:\.\,\;\=\'\/](.*))?$", line)
        if match:
          val = int(match.group(1))
          txt = ""
          if match.group(2) is not None:
            txt = match.group(2).strip()
          if txt == "":
            txt = "#%d" % val
          bol.append((val, txt))
        else:
          messagebox.showerror(parent=tk, message="Parse error in bookmark file, line #%d: \"%s\"." % (line_num, line[:40]))
          line_num = -1
          break

  except OSError as e:
    messagebox.showerror(parent=tk, message="Failed to read bookmarks file %s: %s" % (filename, e.strerror))

  if line_num >= 0:
    modif = mark_list_modified or (len(mark_list) != 0)

    pos = wt.f1_t.index("end")
    max_line = int(wt.f1_t.index("end").split(".")[0])
    warned = False

    for (line, txt) in bol:
      if (line < 0) or (line > max_line):
        if not warned:
          warned = True
          msg = "Invalid line number %d in bookmarks file \"%s\" (should be in range 0...%d)" % (line, filename, max_line)
          answer = messagebox.showwarning(parent=tk, type="okcancel", message=msg)
          if answer == "cancel":
            break
      else:
        if not mark_list.get(line):
          Mark_Toggle(line, txt)

    mark_list_modified = modif

    # update bookmark list dialog window, if opened
    MarkList_Fill()


#
# This function stores the bookmark list in a file.
#
def Mark_SaveFile(filename):
  global mark_list, mark_list_modified

  try:
    with open(filename, "w") as f:
      try:
        for line in sorted(mark_list.keys()):
          print("%d %s" % (line, mark_list[line]), file=f)

        mark_list_modified = 0

      except OSError as e:
        messagebox.showerror(parent=tk, message="Failed writing bookmarks into file %s: %s" % (filename, e.strerror))
  except OSError as e:
    messagebox.showerror(parent=tk, message="Failed to create file %s: %s" % (filename, e.strerror))


#
# This function is called by menu entry "Read bookmarks from file"
# The user is asked to select a file; if he does so it's content is read.
#
def Mark_ReadFileFrom():
  global cur_filename

  def_name = Mark_DefaultFile(cur_filename)
  if not os.path.isfile(def_name):
    def_name = ""

  filename = filedialog.askopenfilename(parent=tk, filetypes=(("Bookmarks", "*.bok"), ("all", "*")),
                                        title="Select bookmark file",
                                        initialfile=os.path.basename(def_name),
                                        initialdir=os.path.dirname(def_name))
  if len(filename) != 0:
    Mark_ReadFile(filename)


#
# This function automatically reads a previously stored bookmark list
# for a newly loaded file, if the bookmark file is named by the default
# naming convention, i.e. with ".bok" extension.
#
def Mark_ReadFileAuto():
  global cur_filename

  bok_name = Mark_DefaultFile(cur_filename)
  if bok_name != "":
    Mark_ReadFile(bok_name)


#
# This helper function determines the default filename for reading bookmarks.
# Default is the trace file name or base file name plus ".bok". The name is
# only returned if a file with this name actually exists and is not older
# than the trace file.
#
def Mark_DefaultFile(trace_name):
  bok_name = ""
  if trace_name != "":
    # must use catch around call to "mtime"
    try:
      st = os.stat(trace_name)
      cur_mtime = st.st_mtime
    except OSError:
      cur_mtime = 0

    if cur_mtime != 0:
      name = trace_name + ".bok"
      if not os.path.isfile(name):
        name2 = re.sub(r"\.[^\.]+$", ".bok", trace_name)
        if (name2 != trace_name) and os.path.isfile(name2):
          name = name2

      try:
        st = os.stat(name)
        if st.st_mtime >= cur_mtime:
          bok_name = name
        else:
          print(sys.argv[0] + ": warning: bookmark file " + name +
                " is older than content - not loaded", file=sys.stderr)
      except OSError:
        pass

  return bok_name


#
# This function is called by menu entry "Save bookmarks to file".
# The user is asked to select a file; if he does so the bookmarks are written to it.
#
def Mark_SaveFileAs():
  global mark_list, cur_filename

  if len(mark_list) > 0:
    if cur_filename != "":
      def_name = cur_filename + ".bok"
    else:
      def_name = ""

    filename = filedialog.asksaveasfilename(parent=tk, filetypes=(("Bookmarks", "*.bok"), ("all", "*")),
                                            title="Select bookmark file",
                                            initialfile=os.path.basename(def_name),
                                            initialdir=os.path.dirname(def_name))
    if filename:
      Mark_SaveFile(filename)

  else:
    messagebox.showinfo(parent=tk, message="Your bookmark list is empty.")


#
# This function offers to store the bookmark list into a file if the list was
# modified.  The function is called when the application is closed or a new file
# is loaded.
#
def Mark_OfferSave():
  global mark_list, mark_list_modified

  if mark_list_modified and (len(mark_list) > 0):
    answer = messagebox.askyesno(parent=tk, default="yes", title="Trace browser",
                                 message="Store changes in the bookmark list before quitting?")
    if answer:
      Mark_SaveFileAs()
      if mark_list_modified == 0:
        DisplayStatusLine("bookmark", "info", "Bookmarks have been saved")


# ----------------------------------------------------------------------------
#
# This function inserts a bookmark text into the listbox and copies
# color highlight tags from the main window so that the text displays
# in the same way.
#
def MarkList_Insert(idx, line):
  global patlist, mark_list

  tag_list = []
  for tag in wt.f1_t.tag_names("%d.1" % line):
    if re.match(r"^tag\d+$", tag):
      tag_list.append(tag)

  txt = mark_list[line] + "\n"

  # insert text (prepend space to improve visibility of selection)
  wt.dlg_mark_l.insert("%d.0" % (idx + 1), "  ", "margin", txt, tag_list)


#
# This function fills the bookmark list dialog window with all bookmarks.
#
def MarkList_Fill():
  global dlg_mark_shown, dlg_mark_list, mark_list

  if dlg_mark_shown:
    wt.dlg_mark_l.delete("1.0", "end")
    dlg_mark_list = []

    idx = 0
    for line in sorted(mark_list.keys()):
      dlg_mark_list.append(line)
      MarkList_Insert(idx, line)
      idx += 1


#
# This function is called after a bookmark was added to insert the text
# into the bookmark list dialog window.
#
def MarkList_Add(line):
  global dlg_mark_shown, mark_list, dlg_mark_list, dlg_mark_sel

  if dlg_mark_shown:
    idx = 0
    for l in dlg_mark_list:
      if l > line:
        break
      idx += 1

    dlg_mark_list.insert(idx, line)
    MarkList_Insert(idx, line)
    wt.dlg_mark_l.see("%d.0" % (idx + 1))
    dlg_mark_sel.TextSel_SetSelection([idx])


#
# This function is called after a bookmark was deleted to remove the text
# from the bookmark list dialog window.
#
def MarkList_Delete(line):
  global dlg_mark_shown, mark_list, dlg_mark_list, dlg_mark_sel

  if dlg_mark_shown:
    try:
      idx = dlg_mark_list.index(line)
      del dlg_mark_list[idx]
      wt.dlg_mark_l.delete("%d.0" % (idx + 1), "%d.0" % (idx + 2))
      dlg_mark_sel.TextSel_SetSelection([])
    except:
      pass


#
# This function is bound to the "delete" key and context menu entry to
# allow the user to remove a bookmark via the bookmark list dialog.
#
def MarkList_RemoveSelected():
  global dlg_mark_list, dlg_mark_sel

  sel = dlg_mark_sel.TextSel_GetSelection()

  # translate list indices to text lines, because indices change during removal
  line_list = [dlg_mark_list[idx] for idx in sel]

  # remove bookmarks on all selected lines
  for line in line_list:
    Mark_Toggle(line)


#
# This function is bound to the "insert" key and "rename" context menu
# entry to allow the user to edit the tag assigned to the selected bookmark.
# The function opens an "overlay" window with an entry field.
#
def MarkList_RenameSelected():
  global dlg_mark_sel

  sel = dlg_mark_sel.TextSel_GetSelection()
  if len(sel) == 1:
    MarkList_OpenRename(sel[0])


#
# This function is bound to changes of the selection in the bookmark list,
# i.e. it's called when the user uses the cursor keys or mouse button to
# select an entry.  The view in the main window is set to display the line
# which contains the bookmark. If more than one bookmark nothing is done.
#
def MarkList_SelectionChange(sel):
  global dlg_mark_list

  if len(sel) >= 1:
    line = dlg_mark_list[sel[0]]
    Mark_Line(line)
    SearchList_MatchView(line)

  if len(sel) > 1:
    for idx in sel[1:]:
      line = dlg_mark_list[idx]
      wt.f1_t.tag_add("find", "%d.0" % line, "%d.0" % (line + 1))


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
def MarkList_GetLen():
  global dlg_mark_list
  return len(dlg_mark_list)


#
# This function must be called when portions of the text in the main window
# have been deleted to update references to text lines. Parameter meaning:
# + top_l: this is the first line which is not deleted, or 1 if none
# + bottom_l: this line and all below have been removed, or 0 if none
#
def MarkList_AdjustLineNums(top_l, bottom_l):
  global mark_list

  new = {}
  for (line, title) in mark_list.items():
    if (line >= top_l) and ((line < bottom_l) or (bottom_l == 0)):
      new[line - top_l + 1] = title
  mark_list = new

  MarkList_Fill()


#
# This function assigns the given text to a bookmark with the given index
# in the bookmark list dialog.  The function is called when the user has
# closed the bookmark text entry dialog with "Return"
#
def MarkList_Rename(idx, txt):
  global dlg_mark_shown, dlg_mark_list, dlg_mark_sel, mark_list, mark_list_modified

  if dlg_mark_shown and (idx < len(dlg_mark_list)):
    line = dlg_mark_list[idx]
    if txt != "":
      mark_list[line] = txt
      mark_list_modified = True

      wt.dlg_mark_l.delete("%d.0" % (idx + 1), "%d.0" % (idx + 2))
      MarkList_Insert(idx, line)
      wt.dlg_mark_l.see("%d.0" % (idx + 1))
      dlg_mark_sel.TextSel_SetSelection([idx])


#
# This function pops up a context menu for the bookmark list dialog.
#
def MarkList_ContextMenu(xcoo, ycoo):
  global dlg_mark_list, dlg_mark_sel

  dlg_mark_sel.TextSel_ContextSelection(xcoo, ycoo)

  sel = dlg_mark_sel.TextSel_GetSelection()
  if len(sel) > 0:
    wt.dlg_mark_ctxmen.delete(0, "end")
    if len(sel) == 1:
      wt.dlg_mark_ctxmen.add_command(label="Rename marker", command=lambda sel=sel:MarkList_OpenRename(sel[0]))

    wt.dlg_mark_ctxmen.add_command(label="Remove marker", command=MarkList_RemoveSelected)

    rootx = wt.dlg_mark.winfo_rootx() + xcoo
    rooty = wt.dlg_mark.winfo_rooty() + ycoo
    tk.call("tk_popup", wt.dlg_mark_ctxmen, rootx, rooty, 0)


#
# This function creates or raises the bookmark list dialog. This dialog shows
# all currently defined bookmarks.
#
def MarkList_OpenDialog():
  global font_content, col_bg_content, col_fg_content
  global cur_filename, dlg_mark_shown, dlg_mark_sel, dlg_mark_geom

  PreemptBgTasks()
  if not dlg_mark_shown:
    wt.dlg_mark = Toplevel(tk)
    if cur_filename != "":
      wt.dlg_mark.wm_title("Bookmark list - " + cur_filename)
    else:
      wt.dlg_mark.wm_title("Bookmark list")

    wt.dlg_mark.wm_group(tk)

    char_w = font_content.measure(" ")
    wt.dlg_mark_l = Text(wt.dlg_mark, width=1, height=1, wrap=NONE, font=font_content, cursor="top_left_arrow",
                         foreground=col_fg_content, background=col_bg_content, exportselection=0,
                         insertofftime=0, insertwidth=2*char_w)
    wt.dlg_mark_l.pack(side=LEFT, fill=BOTH, expand=1)
    wt.dlg_mark_sb = Scrollbar(wt.dlg_mark, orient=VERTICAL, command=wt.dlg_mark_l.yview, takefocus=0)
    wt.dlg_mark_sb.pack(side=LEFT, fill=Y)
    wt.dlg_mark_l.configure(yscrollcommand=wt.dlg_mark_sb.set)

    wt.dlg_mark_l.bindtags([wt.dlg_mark_l, "TextSel", tk, "all"])
    dlg_mark_sel = TextSel(wt.dlg_mark_l, MarkList_SelectionChange, MarkList_GetLen, "browse")

    wt.dlg_mark_ctxmen = Menu(wt.dlg_mark, tearoff=0)

    wt.dlg_mark_l.bind("<Insert>", lambda e:BindCallAndBreak(MarkList_RenameSelected))
    wt.dlg_mark_l.bind("<Delete>", lambda e:BindCallAndBreak(MarkList_RemoveSelected))
    wt.dlg_mark_l.bind("<Escape>", lambda e:BindCallAndBreak(wt.dlg_mark.destroy))
    wt.dlg_mark_l.bind("<ButtonRelease-3>", lambda e: MarkList_ContextMenu(e.x, e.y))
    wt.dlg_mark_l.focus_set()

    dlg_mark_shown = True
    wt.dlg_mark_l.bind("<Destroy>", lambda e:MarkList_Quit(1), add="+")
    wt.dlg_mark.bind("<Configure>", lambda e:ToplevelResized(e.widget, wt.dlg_mark, wt.dlg_mark, "dlg_mark"))
    wt.dlg_mark.wm_protocol("WM_DELETE_WINDOW", lambda: MarkList_Quit(0))
    wt.dlg_mark.wm_geometry(win_geom["dlg_mark"])
    wt.dlg_mark.wm_positionfrom("user")

    MarkList_CreateHighlightTags()
    MarkList_Fill()

  else:
    wt.dlg_mark.wm_deiconify()
    wt.dlg_mark.lift()
    wt.dlg_mark_l.focus_set()

  ResumeBgTasks()


#
# This function creates the tags for selection and color highlighting.
# This is used for initialisation and after editing highlight tags.
#
def MarkList_CreateHighlightTags():
  global dlg_mark_shown, patlist, fmt_selection

  if dlg_mark_shown:
    for w in patlist:
      tagnam = w[4]
      HighlightConfigure(wt.dlg_mark_l, tagnam, w)

    HighlightConfigure(wt.dlg_mark_l, "sel", fmt_selection)

    wt.dlg_mark_l.tag_configure("margin", lmargin1=10)
    wt.dlg_mark_l.tag_lower("sel")


#
# This function is called after removal of tags in the Tag list dialog.
#
def MarkList_DeleteTag(tag):
  global dlg_mark_shown

  if dlg_mark_shown:
    wt.dlg_mark_l.tag_delete(tag)


#
# This function is bound to destroy
#
def MarkList_Quit(is_destroyed):
  global dlg_mark_shown

  dlg_mark_shown = False
  if not is_destroyed:
    wt.dlg_mark.destroy()

  SafeDestroy(wt.dlg_mark_mren)



# ----------------------------------------------------------------------------
#
# This function opens a tiny dialog window which allows to enter a new text
# tag for a selected bookmark. The dialog window is sized and placed so that
# it exactly covers the respective entry in the bookmark list dialog to make
# it appear as if the listbox entry could be edited directly.
#
def MarkList_OpenRename(idx):
  global dlg_mark_shown, dlg_mark_list, mark_list
  global mark_rename, mark_rename_idx
  global font_normal, font_bold, tick_str_prefix, tick_pat_num

  if dlg_mark_shown and (idx < len(dlg_mark_list)):
    wt.dlg_mark_l.see("%d.0" % (idx + 1))
    coo = wt.dlg_mark_l.dlineinfo("%d.0" % (idx + 1))
    if len(coo) == 5:
      SafeDestroy(wt.dlg_mark_mren)
      wt.dlg_mark_mren = Frame(wt.dlg_mark_l, takefocus=0, borderwidth=2, relief=RAISED)

      line = dlg_mark_list[idx]
      mark_rename = StringVar(tk, mark_list[line])
      mark_rename_idx = idx

      wt.dlg_mark_mren_e = Entry(wt.dlg_mark_mren, width=12, textvariable=mark_rename, exportselection=FALSE, font=font_normal)
      wt.dlg_mark_mren_e.pack(side=LEFT, fill=BOTH, expand=1)
      wt.dlg_mark_mren_e.bind("<Return>", lambda e:BindCallAndBreak(MarkList_Assign))
      wt.dlg_mark_mren_e.bind("<Escape>", lambda e:BindCallAndBreak(MarkList_LeaveRename))

      wt.dlg_mark_mren_b = Button(wt.dlg_mark_mren, text="X", padx=2, font=font_bold, command=MarkList_LeaveRename)
      wt.dlg_mark_mren_b.pack(side=LEFT)

      wt.dlg_mark_mren_e.selection_clear()
      off = 0
      if tick_pat_num != "":
        skip = len(tick_str_prefix)
        #if {([string compare -length $skip $tick_str_prefix $mark_rename.get()] == 0) &&
        #    ([regexp -start $skip {^[\d:,\.]+ *} $mark_rename.get() match])} {
        cur_txt = mark_rename.get()
        if (len(cur_txt) >= skip) and (cur_txt[: skip] == tick_str_prefix):
          match = re.match(r"^[\d:,\.]+ *", cur_txt[skip :])
          if match:
            off = len(match.group(0))

      wt.dlg_mark_mren_e.selection_from(off)
      wt.dlg_mark_mren_e.selection_to("end")
      wt.dlg_mark_mren_e.icursor(off)

      xcoo = coo[0] - 3
      ycoo = coo[1] - 3
      w = wt.dlg_mark_l.winfo_width()
      h = coo[3] + 6
      wt.dlg_mark_mren.place(anchor=NW, x=xcoo, y=ycoo, relwidth=1.0) # -in .dlg_mark.l

      wt.dlg_mark_mren_e.focus_set()
      wt.dlg_mark_mren.grab_set()


#
# This function is bound to the enter key-press in the renaming popup window.
# The function assigned the entered text to the bookmark and closes the popup.
#
def MarkList_Assign():
  global mark_rename, mark_rename_idx

  MarkList_Rename(mark_rename_idx, mark_rename.get())
  MarkList_LeaveRename()


#
# This function is bound to all events which signal an exit of the rename
# dialog window. The window is destroyed.
#
def MarkList_LeaveRename():
  global mark_rename, mark_rename_idx
  mark_rename = None
  mark_rename_idx = None
  SafeDestroy(wt.dlg_mark_mren)
  try:
    wt.dlg_mark_l.focus_set()
  except:
    pass


# ----------------------------------------------------------------------------
#
# This function creates or raises a dialog window which contains the
# search history, i.e. a list of previously used search expressions.
#
def SearchHistory_Open():
  global font_content, col_bg_content, col_fg_content, cur_filename
  global dlg_hist_shown, dlg_hist_sel, dlg_hist_geom

  PreemptBgTasks()
  if not dlg_hist_shown:
    wt.dlg_hist = Toplevel(tk)
    if cur_filename != "":
      wt.dlg_hist.wm_title("Search history - " + cur_filename)
    else:
      wt.dlg_hist.wm_title("Search history")

    wt.dlg_hist.wm_group(tk)

    wt.dlg_hist_f1 = Frame(wt.dlg_hist)
    wt.dlg_hist_f1_l = Text(wt.dlg_hist_f1, width=1, height=1, wrap=NONE, font=font_content, cursor="top_left_arrow", foreground=col_fg_content, background=col_bg_content, exportselection=0, insertofftime=0)
    wt.dlg_hist_f1_l.pack(side=LEFT, fill=BOTH, expand=1)
    wt.dlg_hist_f1_sb = Scrollbar(wt.dlg_hist_f1, orient=VERTICAL, command=wt.dlg_hist_f1_l.yview, takefocus=0)
    wt.dlg_hist_f1_sb.pack(side=LEFT, fill=Y)
    wt.dlg_hist_f1_l.configure(yscrollcommand=wt.dlg_hist_f1_sb.set)
    wt.dlg_hist_f1.pack(side=TOP, fill=BOTH, expand=1)

    wt.dlg_hist_f2 = Frame(wt.dlg_hist)
    wt.dlg_hist_f2_lab_one = Label(wt.dlg_hist_f2, text="Find:")
    wt.dlg_hist_f2_lab_one.grid(sticky=W, column=0, row=0, padx=5)
    wt.dlg_hist_f2_but_next = Button(wt.dlg_hist_f2, text="Next", command=lambda: SearchHistory_SearchNext(1), underline=0, state=DISABLED, pady=2)
    wt.dlg_hist_f2_but_next.grid(sticky="we", column=1, row=0)
    wt.dlg_hist_f2_but_prev = Button(wt.dlg_hist_f2, text="Prev.", command=lambda: SearchHistory_SearchNext(0), underline=0, state=DISABLED, pady=2)
    wt.dlg_hist_f2_but_prev.grid(sticky="we", column=2, row=0)
    wt.dlg_hist_f2_but_all = Button(wt.dlg_hist_f2, text="All", command=lambda: SearchHistory_SearchAll(0), underline=0, state=DISABLED, pady=2)
    wt.dlg_hist_f2_but_all.grid(sticky="we", column=3, row=0)
    wt.dlg_hist_f2_but_blw = Button(wt.dlg_hist_f2, text="All below", command=lambda: SearchHistory_SearchAll(1), state=DISABLED, pady=2)
    wt.dlg_hist_f2_but_blw.grid(sticky="we", column=4, row=0)
    wt.dlg_hist_f2_but_abve = Button(wt.dlg_hist_f2, text="All above", command=lambda: SearchHistory_SearchAll(-1), state=DISABLED, pady=2)
    wt.dlg_hist_f2_but_abve.grid(sticky="we", column=5, row=0)
    wt.dlg_hist_f2.pack(side=TOP, anchor=W, pady=2)

    dlg_hist_sel = TextSel(wt.dlg_hist_f1_l, SearchHistory_SelectionChange, SearchHistory_GetLen, "extended")

    wt.dlg_hist_f1_l.bindtags([wt.dlg_hist_f1_l, "TextSel", tk, "all"])
    wt.dlg_hist_f1_l.bind("<Double-Button-1>", lambda e:BindCallAndBreak(SearchHistory_Trigger))
    wt.dlg_hist_f1_l.bind("<ButtonRelease-3>", lambda e:BindCallAndBreak(lambda: SearchHistory_ContextMenu(e.x, e.y)))
    wt.dlg_hist_f1_l.bind("<Delete>", lambda e:BindCallAndBreak(SearchHistory_Remove))
    wt.dlg_hist_f1_l.bind("<Key-n>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchNext(1)))
    wt.dlg_hist_f1_l.bind("<Key-N>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchNext(0)))
    wt.dlg_hist_f1_l.bind("<Key-a>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchAll(0)))
    wt.dlg_hist_f1_l.bind("<Key-ampersand>", lambda e:BindCallAndBreak(SearchHighlightClear))
    wt.dlg_hist_f1_l.bind("<Alt-Key-n>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchNext(1)))
    wt.dlg_hist_f1_l.bind("<Alt-Key-p>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchNext(0)))
    wt.dlg_hist_f1_l.bind("<Alt-Key-a>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchAll(0)))
    wt.dlg_hist_f1_l.bind("<Alt-Key-P>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchAll(-1)))
    wt.dlg_hist_f1_l.bind("<Alt-Key-N>", lambda e:BindCallAndBreak(lambda: SearchHistory_SearchAll(1)))
    wt.dlg_hist_f1_l.focus_set()

    wt.dlg_hist_ctxmen = Menu(wt.dlg_hist, tearoff=0)

    dlg_hist_shown = True
    wt.dlg_hist_f1_l.bind("<Destroy>", lambda e:SearchHistory_Close(), add="+")
    wt.dlg_hist.bind("<Configure>", lambda e:ToplevelResized(e.widget, wt.dlg_hist, wt.dlg_hist, "dlg_hist"))
    wt.dlg_hist.wm_geometry(win_geom["dlg_hist"])
    wt.dlg_hist.wm_positionfrom("user")

    cw1 = font_content.measure("reg.exp.")
    cw2 = font_content.measure("ign.case")
    tab_pos = [cw1//2 + 5, "center", cw1 + cw2//2 + 5, "center", cw1+cw2 + 10, "left"]
    wt.dlg_hist_f1_l.configure(tabs=tab_pos)
    wt.dlg_hist_f1_l.tag_configure("small", font=DeriveFont(font_content, -2))

    SearchHistory_Fill()

  else:
    wt.dlg_hist.wm_deiconify()
    wt.dlg_hist.lift()
    wt.dlg_hist_f1_l.focus_set()

  ResumeBgTasks()


#
# This function is bound to destruction events on the search history dialog.
# The function releases all dialog resources.
#
def SearchHistory_Close():
  global dlg_hist_sel, dlg_hist_shown
  dlg_hist_sel = None
  dlg_hist_shown = False


#
# This function pops up a context menu for the search history dialog.
#
def SearchHistory_ContextMenu(xcoo, ycoo):
  global dlg_hist_sel

  dlg_hist_sel.TextSel_ContextSelection(xcoo, ycoo)
  sel = dlg_hist_sel.TextSel_GetSelection()

  wt.dlg_hist_ctxmen.delete(0, "end")

  c = 0
  if len(sel) > 0:
    if c > 0: wt.dlg_hist_ctxmen.add_separator()
    wt.dlg_hist_ctxmen.add_command(label="Remove selected expressions", command=SearchHistory_Remove)
    c += 1

  if len(sel) == 1:
    if c > 0: wt.dlg_hist_ctxmen.add_separator()
    wt.dlg_hist_ctxmen.add_command(label="Copy to the search entry field", command=SearchHistory_CopyToSearch)

  if c > 0:
    rootx = wt.dlg_hist.winfo_rootx() + xcoo
    rooty = wt.dlg_hist.winfo_rooty() + ycoo
    tk.call("tk_popup", wt.dlg_hist_ctxmen, rootx, rooty, 0)


#
# This function is bound to double-click on a history entry. The function
# starts a search with the selected expression.
#
def SearchHistory_Trigger():
  global tlb_last_dir
  SearchHistory_CopyToSearch()
  SearchHistory_SearchNext(tlb_last_dir)

#
# This function returns all currently selected patterns. This is used only
# to save the selection across history modifications.
#
def SearchHistory_StoreSel():
  global dlg_hist_shown, dlg_hist_sel, tlb_history

  copy = []
  if dlg_hist_shown:
    sel = dlg_hist_sel.TextSel_GetSelection()
    for idx in sel:
      copy.append(tlb_history[idx])

  return copy



#
# This function selects all patterns in the given list. This is used only
# to restore a previous selection after history modifications (e.g. sort
# order change due to use of a pattern for a search)
#
def SearchHistory_RestoreSel(copy):
  global dlg_hist_shown, dlg_hist_sel, tlb_history

  if dlg_hist_shown:
    sel = []
    for cphl in copy:
      idx = 0
      for hl in tlb_history:
        if ((hl[0] == cphl[0]) and
            (hl[1] == cphl[1]) and
            (hl[2] == cphl[2])):
          sel.append(idx)
          break

        idx += 1

    dlg_hist_sel.TextSel_SetSelection(sel)


#
# This function fills the search history dialog with all search expressions
# in the search history stack.  The last used expression is placed on top.
#
def SearchHistory_Fill():
  global dlg_hist_shown, tlb_history

  if dlg_hist_shown:
    wt.dlg_hist_f1_l.delete("1.0", "end")

    for hl in tlb_history:
      wt.dlg_hist_f1_l.insert("end", "\t", [],
                                     ("reg.exp." if hl[1] else "-"), ["small"], "\t", [],
                                     ("ign.case" if hl[2] else "-"), ["small"], "\t", [],
                                     hl[0], [], "\n", [])

    dlg_hist_sel.TextSel_SetSelection([])


#
# This function is bound to the "Remove selected lines" command in the
# search history list dialog's context menu.  All currently selected text
# lines are removed from the search list.
#
def SearchHistory_Remove():
  global dlg_hist_sel, tlb_history

  sel = dlg_hist_sel.TextSel_GetSelection()
  sel.sort(reverse=True)
  for idx in sel:
    line = "%d.0" % (idx + 1)
    del tlb_history[idx]
    wt.dlg_hist_f1_l.delete(line, line + " + 1 lines")

  dlg_hist_sel.TextSel_SetSelection([])
  UpdateRcAfterIdle()


#
# This function is invoked by the "Copy to search field" command in the
# search history list's context menu. (Note an almost identical menu entry
# exists in the tag list dialog.)
#
def SearchHistory_CopyToSearch():
  global tlb_history, tlb_find, tlb_regexp, tlb_case, tlb_find_focus
  global dlg_hist_sel

  sel = dlg_hist_sel.TextSel_GetSelection()
  if len(sel) == 1:
    # force focus into find entry field & suppress "Enter" event
    SearchInit()
    tlb_find_focus = 1
    wt.f2_e.focus_set()

    hl = tlb_history[sel[0]]

    SearchHighlightClear()
    tlb_find.set(hl[0])
    tlb_regexp.set(hl[1])
    tlb_case.set(hl[2])



#
# This function starts a search in the main text content for the selected
# expression, i.e. as if the word had been entered to the search text
# entry field. TODO: currently only one expression at a time can be searched
#
def SearchHistory_SearchNext(is_fwd):
  global dlg_hist_sel, tlb_history, tlb_find

  ClearStatusLine("search")

  sel = dlg_hist_sel.TextSel_GetSelection()
  if len(sel) == 1:
    hl = tlb_history[sel[0]]

    pat = hl[0]
    is_re = hl[1]
    use_case = hl[2]

    # move this expression to the top of the history
    Search_AddHistory(pat, is_re, use_case)

    # clear search entry field to avoid confusion (i.e. showing different expr.)
    tlb_find.set("")

    if SearchExprCheck(pat, is_re, True):
      found = Search_Atomic(pat, is_re, use_case, is_fwd, True)
      if found == "":
        Search_HandleNoMatch(pat, is_fwd)
        SearchHighlightClear()

  else:
    DisplayStatusLine("search", "error", "No expression selected")


#
# This function is bound to the "list all" button in the search history
# dialog. The function opens the search result list window and starts a
# search for all expressions which are currently selected in the history
# list (serializing multiple searches is handled by the search list dialog)
#
def SearchHistory_SearchAll(direction):
  global tlb_history
  global dlg_hist_sel

  ClearStatusLine("search")

  sel = dlg_hist_sel.TextSel_GetSelection()
  if len(sel) > 0:
    pat_list = []
    for idx in sel:
      hl = tlb_history[idx]
      pat_list.append(hl[0 : 2 + 1])

    for hl in pat_list:
      Search_AddHistory(hl[0], hl[1], hl[2])

    SearchList_Open(False)
    SearchList_StartSearchAll(pat_list, 1, direction)
  else:
    DisplayStatusLine("search", "error", "No expression selected")


#
# This function is a callback for selection changes in the search history dialog.
# This is used to enable/disable command buttons which require a certain number
# of selected items to work.
#
def SearchHistory_SelectionChange(sel):
  if len(sel) == 1:
    wt.dlg_hist_f2_but_next.configure(state=NORMAL)
    wt.dlg_hist_f2_but_prev.configure(state=NORMAL)
  else:
    wt.dlg_hist_f2_but_next.configure(state=DISABLED)
    wt.dlg_hist_f2_but_prev.configure(state=DISABLED)

  if len(sel) > 0:
    wt.dlg_hist_f2_but_all.configure(state=NORMAL)
    wt.dlg_hist_f2_but_abve.configure(state=NORMAL)
    wt.dlg_hist_f2_but_blw.configure(state=NORMAL)
  else:
    wt.dlg_hist_f2_but_all.configure(state=DISABLED)
    wt.dlg_hist_f2_but_abve.configure(state=DISABLED)
    wt.dlg_hist_f2_but_blw.configure(state=DISABLED)


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
def SearchHistory_GetLen():
  global tlb_history
  return len(tlb_history)



# ----------------------------------------------------------------------------
#
# This function creates or raises a dialog window which collects text lines
# matching one or more search expressions.  The user can also freely add or
# remove lines from the list.
#
def SearchList_Open(raise_win):
  global font_content, col_bg_content, col_fg_content, cur_filename
  global dlg_srch_shown, dlg_srch_geom, dlg_srch_sel, dlg_srch_lines, dlg_srch_fn_cache
  global dlg_srch_show_fn, dlg_srch_show_tick, dlg_srch_tick_delta, dlg_srch_tick_root
  global dlg_srch_highlight, dlg_srch_undo, dlg_srch_redo
  global tick_pat_sep, tick_pat_num

  PreemptBgTasks()
  if not dlg_srch_shown:
    # create/teser variables to default values
    dlg_srch_shown = True
    SearchList_Init()
    dlg_srch_show_fn = BooleanVar(tk, False)
    dlg_srch_show_tick = BooleanVar(tk, False)
    dlg_srch_tick_delta = BooleanVar(tk, False)
    dlg_srch_highlight = BooleanVar(tk, False)

    wt.dlg_srch = Toplevel(tk)
    if cur_filename != "":
      wt.dlg_srch.wm_title("Search matches - " + cur_filename)
    else:
      wt.dlg_srch.wm_title("Search matches")

    if tick_pat_sep == "":
      with_tick_num = DISABLED
    else:
      with_tick_num = NORMAL

    if tick_pat_num == "":
      with_fn = DISABLED
    else:
      with_fn = NORMAL

    wt.dlg_srch.wm_group(tk)

    wt.dlg_srch_menubar = Menu(wt.dlg_srch)
    wt.dlg_srch_menubar_ctrl = Menu(wt.dlg_srch_menubar, tearoff=0, postcommand=MenuPosted)
    wt.dlg_srch_menubar_ctrl.add_command(label="Load line numbers...", command=SearchList_LoadFrom)
    wt.dlg_srch_menubar_ctrl.add_command(label="Save text as...", command=lambda:SearchList_SaveFileAs(0))
    wt.dlg_srch_menubar_ctrl.add_command(label="Save line numbers...", command=lambda:SearchList_SaveFileAs(1))
    wt.dlg_srch_menubar_ctrl.add_separator()
    wt.dlg_srch_menubar_ctrl.add_command(label="Clear all", command=SearchList_Clear)
    wt.dlg_srch_menubar_ctrl.add_command(label="Close", command=wt.dlg_srch.destroy)
    wt.dlg_srch_menubar_edit = Menu(wt.dlg_srch_menubar, tearoff=0, postcommand=SearchList_MenuPosted)
    wt.dlg_srch_menubar_edit.add_command(label="Undo", command=SearchList_Undo, accelerator="u")
    wt.dlg_srch_menubar_edit.add_command(label="Redo", command=SearchList_Redo, accelerator="^r")
    wt.dlg_srch_menubar_edit.add_separator()
    wt.dlg_srch_menubar_edit.add_command(label="Import selected lines from main window", command=SearchList_CopyCurrentLine)
    wt.dlg_srch_menubar_edit.add_separator()
    wt.dlg_srch_menubar_edit.add_command(label="Remove selected lines", accelerator="Del", command=SearchList_RemoveSelection)
    wt.dlg_srch_menubar_search = Menu(wt.dlg_srch_menubar, tearoff=0, postcommand=MenuPosted)
    wt.dlg_srch_menubar_search.add_command(label="Search history...", command=SearchHistory_Open)
    wt.dlg_srch_menubar_search.add_command(label="Edit highlight patterns...", command=TagList_OpenDialog)
    wt.dlg_srch_menubar_search.add_separator()
    wt.dlg_srch_menubar_search.add_command(label="Insert all search matches...", command=lambda:SearchAll(1, 0), accelerator="ALT-a")
    wt.dlg_srch_menubar_search.add_command(label="Insert all matches above...", command=lambda:SearchAll(1 -1), accelerator="ALT-P")
    wt.dlg_srch_menubar_search.add_command(label="Insert all matches below...", command=lambda:SearchAll(1, 1), accelerator="ALT-N")
    wt.dlg_srch_menubar_edit.add_separator()
    wt.dlg_srch_menubar_edit.add_command(label="Add main window search matches", command=lambda:SearchList_AddMatches(0))
    wt.dlg_srch_menubar_edit.add_command(label="Remove main window search matches", command=lambda:SearchList_RemoveMatches(0))
    wt.dlg_srch_menubar_search.add_separator()
    wt.dlg_srch_menubar_search.add_command(label="Clear search highlight", command=SearchHighlightClear, accelerator="&")
    wt.dlg_srch_menubar_options = Menu(wt.dlg_srch_menubar, tearoff=0, postcommand=MenuPosted)
    wt.dlg_srch_menubar_options.add_checkbutton(label="Show frame number", command=SearchList_ToggleFrameNo, state=with_fn, variable=dlg_srch_show_fn, accelerator="ALT-f")
    wt.dlg_srch_menubar_options.add_checkbutton(label="Show tick number", command=SearchList_ToggleFrameNo, state=with_tick_num, variable=dlg_srch_show_tick, accelerator="ALT-t")
    wt.dlg_srch_menubar_options.add_checkbutton(label="Show tick number delta", command=SearchList_ToggleFrameNo, state=with_tick_num, variable=dlg_srch_tick_delta, accelerator="ALT-d")
    wt.dlg_srch_menubar_options.add_checkbutton(label="Highlight search", command=SearchList_ToggleHighlight, variable=dlg_srch_highlight, accelerator="ALT-h")
    wt.dlg_srch_menubar_options.add_separator()
    wt.dlg_srch_menubar_options.add_command(label="Select line as origin for tick delta", command=SearchList_SetFnRoot, state=with_tick_num, accelerator="ALT-0")

    wt.dlg_srch_menubar.add_cascade(label="Control", menu=wt.dlg_srch_menubar_ctrl, underline=0)
    wt.dlg_srch_menubar.add_cascade(label="Edit", menu=wt.dlg_srch_menubar_edit, underline=0)
    wt.dlg_srch_menubar.add_cascade(label="Search", menu=wt.dlg_srch_menubar_search, underline=0)
    wt.dlg_srch_menubar.add_cascade(label="Options", menu=wt.dlg_srch_menubar_options, underline=0)
    wt.dlg_srch.config(menu=wt.dlg_srch_menubar)

    char_w = font_content.measure(" ")
    wt.dlg_srch_f1 = Frame(wt.dlg_srch)
    wt.dlg_srch_f1_l = Text(wt.dlg_srch_f1, width=1, height=1, wrap=NONE, font=font_content,
                            cursor="top_left_arrow", foreground=col_fg_content,
                            background=col_bg_content, exportselection=0, insertofftime=0,
                            insertwidth=(2 * char_w))
    wt.dlg_srch_f1_l.pack(side=LEFT, fill=BOTH, expand=1)
    wt.dlg_srch_f1_sb = Scrollbar(wt.dlg_srch_f1, orient=VERTICAL, command=wt.dlg_srch_f1_l.yview, takefocus=0)
    wt.dlg_srch_f1_sb.pack(side=LEFT, fill=Y)
    wt.dlg_srch_f1_l.configure(yscrollcommand=wt.dlg_srch_f1_sb.set)
    wt.dlg_srch_f1.pack(side=TOP, fill=BOTH, expand=1)

    dlg_srch_sel = TextSel(wt.dlg_srch_f1_l, SearchList_SelectionChange, SearchList_GetLen, "browse")

    wt.dlg_srch_f1_l.bindtags([wt.dlg_srch_f1_l, "TextSel", tk, "all"])
    wt.dlg_srch_f1_l.bind("<ButtonRelease-3>", lambda e:BindCallAndBreak(lambda:SearchList_ContextMenu(e.x, e.y)))
    wt.dlg_srch_f1_l.bind("<Delete>", lambda e:BindCallAndBreak(SearchList_RemoveSelection))
    wt.dlg_srch_f1_l.bind("<Control-plus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(1)))
    wt.dlg_srch_f1_l.bind("<Control-minus>", lambda e:BindCallKeyClrBreak(lambda:ChangeFontSize(-1)))
    wt.dlg_srch_f1_l.bind("<Control-Key-g>", lambda e:BindCallKeyClrBreak(lambda:SearchList_DisplayStats()))
    KeyCmdBind(wt.dlg_srch_f1_l, "/", lambda:SearchEnter(1, wt.dlg_srch_f1_l))
    KeyCmdBind(wt.dlg_srch_f1_l, "?", lambda:SearchEnter(0, wt.dlg_srch_f1_l))
    KeyCmdBind(wt.dlg_srch_f1_l, "n", lambda:SearchList_SearchNext(1))
    KeyCmdBind(wt.dlg_srch_f1_l, "N", lambda:SearchList_SearchNext(0))
    KeyCmdBind(wt.dlg_srch_f1_l, "&", SearchHighlightClear)
    KeyCmdBind(wt.dlg_srch_f1_l, "m", SearchList_ToggleMark)
    KeyCmdBind(wt.dlg_srch_f1_l, "u", SearchList_Undo)
    wt.dlg_srch_f1_l.bind("<Control-Key-r>", lambda e: SearchList_Redo())
    wt.dlg_srch_f1_l.bind("<space>", lambda e:SearchList_SelectionChange(dlg_srch_sel.TextSel_GetSelection()))
    wt.dlg_srch_f1_l.bind("<Escape>", lambda e:SearchList_SearchAbort(False))
    wt.dlg_srch_f1_l.bind("<Alt-Key-h>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("highlight")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-f>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_fn")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-t>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("show_tick")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-d>", lambda e:BindCallAndBreak(lambda:SearchList_ToggleOpt("tick_delta")))
    wt.dlg_srch_f1_l.bind("<Alt-Key-0>", lambda e:BindCallAndBreak(SearchList_SetFnRoot))
    wt.dlg_srch_f1_l.bind("<Alt-Key-n>", lambda e:BindCallAndBreak(lambda:SearchNext(1)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-p>", lambda e:BindCallAndBreak(lambda:SearchNext(0)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-a>", lambda e:BindCallAndBreak(lambda:SearchAll(0, 0)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-N>", lambda e:BindCallAndBreak(lambda:SearchAll(0, 1)))
    wt.dlg_srch_f1_l.bind("<Alt-Key-P>", lambda e:BindCallAndBreak(lambda:SearchAll(0, -1)))
    wt.dlg_srch_f1_l.focus_set()

    wt.dlg_srch_ctxmen = Menu(wt.dlg_srch, tearoff=0)

    dlg_srch_shown = True
    wt.dlg_srch_f1_l.bind("<Destroy>", lambda e:SearchList_Close(), add="+")
    wt.dlg_srch.bind("<Configure>", lambda e:ToplevelResized(e.widget, wt.dlg_srch, wt.dlg_srch, "dlg_srch"))
    wt.dlg_srch.wm_geometry(win_geom["dlg_srch"])
    wt.dlg_srch.wm_positionfrom("user")

    SearchList_CreateHighlightTags()

  elif raise_win:
    wt.dlg_srch.wm_deiconify()
    wt.dlg_srch.lift()

  ResumeBgTasks()


#
# This function is bound to destruction events on the search list dialog window.
# The function stops background processes and releases all dialog resources.
#
def SearchList_Close():
  global dlg_srch_sel, dlg_srch_lines, dlg_srch_fn_cache, dlg_srch_shown

  SearchList_SearchAbort(False)

  dlg_srch_sel = None
  dlg_srch_lines = []
  dlg_srch_shown = False
  dlg_srch_undo = []
  dlg_srch_redo = []
  dlg_srch_fn_cache = {}


#
# This function removes all content in the search list.
#
def SearchList_Clear():
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel
  global dlg_srch_undo, dlg_srch_redo

  if dlg_srch_shown:
    SearchList_SearchAbort(False)

    if len(dlg_srch_lines) > 0:
      dlg_srch_undo.append([-1, [dlg_srch_lines[-1]]])
      dlg_srch_redo = []

    dlg_srch_lines = []
    wt.dlg_srch_f1_l.delete("1.0", "end")

    dlg_srch_sel.TextSel_SetSelection([])


#
# This function clears the content and resets the dialog state variables.
# The function is used when the window is newly opened or a new file is loaded.
#
def SearchList_Init():
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_fn_cache, dlg_srch_tick_root
  global dlg_srch_undo, dlg_srch_redo

  if dlg_srch_shown:
    dlg_srch_lines = []
    dlg_srch_undo = []
    dlg_srch_redo = []
    dlg_srch_tick_root = -1
    dlg_srch_fn_cache = {}


#
# This function is bound to several ALT key combinations in the search list to
# allow quick toggling of menu options.
#
def SearchList_ToggleOpt(opt):
  global dlg_srch_show_fn, dlg_srch_show_tick, dlg_srch_tick_delta, dlg_srch_highlight

  if opt == "highlight":
    dlg_srch_highlight.set(not dlg_srch_highlight.get())
    SearchList_ToggleHighlight()
  elif opt == "show_fn":
    dlg_srch_show_fn.set(not dlg_srch_show_fn.get())
    SearchList_ToggleFrameNo()
  elif opt == "show_tick":
    dlg_srch_show_tick.set(not dlg_srch_show_tick.get())
    SearchList_ToggleFrameNo()
  elif opt == "tick_delta":
    dlg_srch_tick_delta.set(not dlg_srch_tick_delta.get())
    SearchList_ToggleFrameNo()


#
# This function is called when the "edit" menu in the search list dialog is opened.
#
def SearchList_MenuPosted():
  global dlg_srch_undo, dlg_srch_redo

  if len(dlg_srch_undo) > 0:
    cmd = dlg_srch_undo[-1]
    op = "addition" if cmd[0] > 0 else "removal"
    wt.dlg_srch_menubar_edit.entryconfigure("Undo*", state=NORMAL,
                                            label="Undo (%s of %d lines)" % (op, len(cmd[1])))
  else:
    wt.dlg_srch_menubar_edit.entryconfigure("Undo*", state=DISABLED, label="Undo")

  if len(dlg_srch_redo) > 0:
    cmd = dlg_srch_redo[-1]
    op = "addition" if cmd[0] > 0 else "removal"
    wt.dlg_srch_menubar_edit.entryconfigure("Redo*", state=NORMAL,
                                            label="Redo (%s of %d lines)" % (op, len(cmd[1])))
  else:
    wt.dlg_srch_menubar_edit.entryconfigure("Redo*", state=DISABLED, label="Redo")

  MenuPosted()


#
# This function pops up a context menu for the search list dialog.
#
def SearchList_ContextMenu(xcoo, ycoo):
  global tlb_find
  global dlg_srch_sel, dlg_srch_lines
  global tick_pat_sep, tick_pat_num, tick_str_prefix, dlg_srch_fn_cache

  dlg_srch_sel.TextSel_ContextSelection(xcoo, ycoo)
  sel = dlg_srch_sel.TextSel_GetSelection()

  wt.dlg_srch_ctxmen.delete(0, "end")

  c = 0
  if tlb_find.get() != "":
    wt.dlg_srch_ctxmen.add_command(label="Import matches on current search", command=lambda:SearchList_AddMatches(0))
    c += 1

  if len(wt.f1_t.tag_nextrange("sel", "1.0")) == 2:
    wt.dlg_srch_ctxmen.add_command(label="Import main window selection", command=SearchList_AddMainSelection)
    c += 1

  if (len(sel) == 1) and ((tick_pat_sep != "") or (tick_pat_num != "")):
    line = dlg_srch_lines[sel[0]]
    fn = ParseFrameTickNo("%d.0" % line, dlg_srch_fn_cache)
    if fn:
      if c > 0: wt.dlg_srch_ctxmen.add_separator()
      wt.dlg_srch_ctxmen.add_command(label="Select line as origin for tick delta", command=SearchList_SetFnRoot)
      c = 1

  if len(sel) > 0:
    if c > 0: wt.dlg_srch_ctxmen.add_separator()
    wt.dlg_srch_ctxmen.add_command(label="Remove selected lines", command=SearchList_RemoveSelection)
    c = 1

  if c > 0:
    rootx = wt.dlg_srch.winfo_rootx() + xcoo
    rooty = wt.dlg_srch.winfo_rooty() + ycoo
    tk.call("tk_popup", wt.dlg_srch_ctxmen, rootx, rooty, 0)


#
# This function is bound to the "Remove selected lines" command in the
# search list dialog's context menu.  All currently selected text lines
# are removed from the search list.
#
def SearchList_RemoveSelection():
  global dlg_srch_sel, dlg_srch_lines
  global dlg_srch_undo, dlg_srch_redo

  if SearchList_SearchAbort():
    sel = dlg_srch_sel.TextSel_GetSelection()
    sel = sorted(sel, reverse=True)
    if len(sel) > 0:
      new_lines = dlg_srch_lines
      line_list = []
      for idx in sel:
        line_list.append(new_lines[idx])
        new_lines[idx] = -1

        line = "%d.0" % (idx + 1)
        wt.dlg_srch_f1_l.delete(line, line + " +1 lines")

      dlg_srch_lines = [x for x in new_lines if x != -1]

      dlg_srch_undo.append([-1, line_list])
      dlg_srch_redo = []

      dlg_srch_sel.TextSel_SetSelection([], False)


# This function is bound to "n", "N" in the search filter dialog. The function
# starts a regular search in the main window, but repeats until a matching
# line is found which is also listed in the filter dialog.
#
def SearchList_SearchNext(is_fwd):
  global dlg_srch_sel, dlg_srch_lines

  old_yview = wt.f1_t.yview()
  old_cpos = wt.f1_t.index("insert")

  if is_fwd:
    wt.f1_t.mark_set("insert", "insert lineend")
  else:
    wt.f1_t.mark_set("insert", "insert linestart")

  found_any = 0

  while True:
    found = SearchNext(is_fwd)
    if found:
      found_any = True
      # check if the found line is also listed in the search list
      line = int(found.split(".")[0])
      idx = SearchList_GetLineIdx(line)
      if (idx < len(dlg_srch_lines)) and (dlg_srch_lines[idx] == line):
        dlg_srch_sel.TextSel_SetSelection([idx], False)
        break
    else:
      break

  # if none found, set the cursor back to the original position
  if not found:
    wt.f1_t.mark_set("insert", old_cpos)
    wt.f1_t.yview_moveto(old_yview[0])

    if found_any:
      # match found in main window, but not in search result window
      DisplayStatusLine("search", "warn", "No match in search result list")


#
# This function is bound to the "Show frame number" checkbutton which toggles
# the display of frame numbers in front of each line on/off.
#
def SearchList_ToggleFrameNo():
  global dlg_srch_sel, dlg_srch_lines, dlg_srch_fn_cache, tick_pat_sep, tick_pat_num
  global dlg_srch_show_fn, dlg_srch_show_tick, dlg_srch_tick_delta, dlg_srch_tick_root

  if tick_pat_num == "":
    DisplayStatusLine("search", "error", "No patterns defined in the RC file for parsing frame numbers")
    dlg_srch_tick_delta.set(False)
    dlg_srch_show_tick.set(False)
    dlg_srch_show_fn.set(False)

  elif (tick_pat_sep == "") and (dlg_srch_show_tick.get() or dlg_srch_tick_delta.get()):
    DisplayStatusLine("search", "error", "No tick separator pattern defined in the RC file")
    dlg_srch_tick_delta.set(False)
    dlg_srch_show_tick.set(False)

  else:
    if SearchList_SearchAbort():
      if dlg_srch_tick_delta.get() and (dlg_srch_tick_root == -1):
        SearchList_SetFnRoot()
      else:
        SearchList_Refill()


#
# This function is bound to ALT-0 in the search result list and to the
# "Select root FN" context menu command. The function sets the currently
# selected line as origin for frame number delta calculations and enables
# frame number delta display, which requires a complete refresh of the list.
#
def SearchList_SetFnRoot():
  global dlg_srch_sel, dlg_srch_lines, dlg_srch_fn_cache, tick_pat_sep, tick_pat_num
  global dlg_srch_show_fn, dlg_srch_show_tick, dlg_srch_tick_delta, dlg_srch_tick_root

  if tick_pat_sep != "":
    if SearchList_SearchAbort():
      sel = dlg_srch_sel.TextSel_GetSelection()
      if len(sel) > 0:
        line = dlg_srch_lines[sel[0]]
        # extract the frame number from the text in the main window around the referenced line
        fn = ParseFrameTickNo("%d.0" % line, dlg_srch_fn_cache)
        if fn:
          try:
            dlg_srch_tick_delta.set(True)
            dlg_srch_tick_root = int(fn[1] if fn[1] else fn[0])
            DisplayStatusLine("search", "info", "Selected root number: " + str(dlg_srch_tick_root))
            SearchList_Refill()
          except:
            DisplayStatusLine("search", "warn", "Match returned by configured pattern is not a number")
        else:
          DisplayStatusLine("search", "error", "Select a line as origin for tick deltas")
      else:
        DisplayStatusLine("search", "error", "Select a line as origin for tick deltas")
  else:
    DisplayStatusLine("search", "error", "No tick separator pattern defined in the RC file")


#
# This function is bound to the "Highlight search" checkbutton which toggles
# highlighting of lines matching searches in the main window on/off.
#
def SearchList_ToggleHighlight():
  global dlg_srch_highlight, tlb_cur_hall_opt

  if dlg_srch_highlight.get():
    # search highlighting was enabled:
    # force update of global highlighting (in main and search result windows)
    tlb_cur_hall_opt = ["", []]
    SearchHighlightUpdateCurrent()
  else:
    # search highlighting was enabled: remove highlight tag in the search list dialog
    wt.dlg_srch_f1_l.tag_remove("find", "1.0", "end")


#
# This function is bound to the "Undo" menu command any keyboard shortcut.
# This reverts the last modification of the line list (i.e. last removal or
# addition, either via search or manually.)
#
def SearchList_Undo():
  global dlg_srch_shown, dlg_srch_undo, dlg_srch_redo
  global tid_search_list

  ClearStatusLine("search")
  if dlg_srch_shown:
    if len(dlg_srch_undo) > 0:
      if SearchList_SearchAbort():
        cmd = dlg_srch_undo[-1]
        del dlg_srch_undo[-1]

        tid_search_list = tk.after(10, lambda: SearchList_BgUndoRedoLoop(cmd[0], cmd[1], -1, 0))
    else:
      DisplayStatusLine("search", "error", "Already at oldest change in search list")


#
# This function is bound to the "Redo" menu command any keyboard shortcut.
# This reverts the last "undo", if any.
#
def SearchList_Redo():
  global dlg_srch_shown, dlg_srch_undo, dlg_srch_redo
  global tid_search_list

  ClearStatusLine("search")
  if dlg_srch_shown:
    if len(dlg_srch_redo) > 0:
      if SearchList_SearchAbort():
        cmd = dlg_srch_redo[-1]
        del dlg_srch_redo[-1]

        tid_search_list = tk.after(10, lambda: SearchList_BgUndoRedoLoop(cmd[0], cmd[1], 1, 0))
    else:
      DisplayStatusLine("search", "error", "Already at newest change in search list")


#
# This function acts as background process for undo and redo operations.
# Each iteration of this task works on at most 250-500 lines.
#
def SearchList_BgUndoRedoLoop(op, line_list, mode, off):
  global block_bg_tasks, tid_search_inc, tid_search_hall, tid_search_list
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel
  global dlg_srch_undo, dlg_srch_redo

  if block_bg_tasks or (tid_search_inc is not None) or (tid_search_hall is not None):
    # background tasks are suspended - re-schedule with timer
    tid_search_list = tk.after(100, lambda: SearchList_BgUndoRedoLoop(op, line_list, mode, off))

  elif dlg_srch_shown:
    anchor = SearchList_GetViewAnchor()
    line_frag = line_list[off : off + 400]
    off += 400

    SearchList_InvertCmd(op, line_frag, mode)

    do_add = (op > 0)
    if mode < 0:
      SearchList_BgSearch_AppendUndoList(dlg_srch_redo, do_add, line_frag)
    else:
      SearchList_BgSearch_AppendUndoList(dlg_srch_undo, do_add, line_frag)

    # select previously selected line again
    SearchList_SeeViewAnchor(anchor)

    if off <= len(line_list):
      # create or update the progress bar
      ratio = int(100.0 * off / len(line_list))
      SearchList_SearchProgress(ratio)

      tid_search_list = tk.after_idle(lambda:SearchList_BgUndoRedoLoop(op, line_list, mode, off))

    else:
      if mode < 0:
        SearchList_BgSearch_FinalizeUndoList(dlg_srch_redo)
      else:
        SearchList_BgSearch_FinalizeUndoList(dlg_srch_undo)

      tid_search_list = None
      SafeDestroy(wt.dlg_srch_slpro)
      SafeDestroy(wt.srch_abrt)
      wt.dlg_srch_f1_l.configure(cursor="top_left_arrow")

  else:
    tid_search_list = None


#
# This function performs a command for "undo" and "redo".
#
# ATTENTION: The lists this function works on may be large, so the function
#            needs to be tweaked for performance.
#
def SearchList_InvertCmd(op, line_list, mode):
  global dlg_srch_lines

  if op * mode < 0:
    # undo insertion, i.e. delete lines again
    new_lines = dlg_srch_lines[:]  # need to create copy
    for line in sorted(line_list, reverse=True):
      idx = SearchList_GetLineIdx(line)
      if (idx < len(dlg_srch_lines)) and (dlg_srch_lines[idx] == line):
        # remove line from display
        pos = "%d.0" % (idx + 1)
        wt.dlg_srch_f1_l.delete(pos, pos + " +1 lines")
        # mark entries to be deleted; filtered-out in bulk below
        new_lines[idx] = -1

    dlg_srch_lines = [x for x in new_lines if x != -1]

  elif op * mode > 0:
    # re-insert previously removed lines
    for line in sorted(line_list, reverse=True):
      idx = SearchList_GetLineIdx(line)
      if (idx >= len(dlg_srch_lines)) or (dlg_srch_lines[idx] != line):
        # insert line into display
        SearchList_InsertLine(line, "%d.0" % (idx + 1))
        # temporarily append to the end of the list; sorted in bulk later
        dlg_srch_lines.append(line)

    dlg_srch_lines.sort()


#
# Wrapper functions to simplify external interfaces
#
def SearchList_AddMatches(direction):
  global tlb_find, tlb_regexp, tlb_case
  SearchList_SearchMatches(1, tlb_find.get(), tlb_regexp.get(), tlb_case.get(), direction)


def SearchList_RemoveMatches(direction):
  global tlb_find, tlb_regexp, tlb_case
  SearchList_SearchMatches(0, tlb_find.get(), tlb_regexp.get(), tlb_case.get(), direction)


#
# This function is the external interface to the search list for adding
# or removing lines matching the given search pattern.  The search is
# performed in a background task, i.e. it's not completed when this
# function returns.
#
def SearchList_SearchMatches(do_add, pat, is_re, use_case, direction):
  global dlg_srch_sel

  if pat != "":
    if SearchExprCheck(pat, is_re, True):
      hl = [pat, is_re, use_case]
      pat_list = [hl]
      SearchList_StartSearchAll(pat_list, do_add, direction)


#
# Helper function which performs a binary search in the sorted line index
# list for the first value which is larger or equal to the given value.
# Returns the index of the element, or the length of the list if all
# values in the list are smaller.
#
def SearchList_GetLineIdx(ins_line):
  global dlg_srch_lines
  return bisect.bisect_left(dlg_srch_lines, ins_line)
#  end = len(dlg_srch_lines)
#  min = -1
#  max = end
#  if end > 0:
#    idx = end >> 1
#    end -= 1
#    while True:
#      el = dlg_srch_lines[idx]
#      if el < ins_line:
#        min = idx
#        idx = (idx + max) >> 1
#        if (idx >= max) or (idx <= min):
#          break
#
#      elif el > ins_line:
#        max = idx
#        idx = (min + idx) >> 1
#        if idx <= min:
#          break
#
#      else:
#        max = idx
#        break
#
#  return max


#
# This function starts the search in the main text content for all matches
# to a given pattern.  Matching lines are either inserted or removed from the
# search list. The search is performed in the background and NOT finished when
# this function returns.  Possibly still running older searches are aborted.
#
def SearchList_StartSearchAll(pat_list, do_add, direction):
  global dlg_srch_redo, tid_search_list

  if SearchList_SearchAbort():
    if direction == 0:
      line = 1
    else:
      line = int(wt.f1_t.index("insert").split(".")[0])

    # reset redo list
    dlg_srch_redo = []

    tid_search_list = tk.after(10, lambda: SearchList_BgSearchLoop(pat_list, do_add, direction, line, 0, 0))


#
# This function acts as background process to fill the search list window.
# The search loop continues for at most 100ms, then the function re-schedules
# itself as idle task.
#
def SearchList_BgSearchLoop(pat_list, do_add, direction, line, pat_idx, loop_cnt):
  global block_bg_tasks, tid_search_inc, tid_search_hall, tid_search_list
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel, dlg_srch_undo

  if block_bg_tasks or (tid_search_inc is not None) or (tid_search_hall is not None):
    # background tasks are suspended - re-schedule with timer
    tid_search_list = tk.after(100, lambda line=line: SearchList_BgSearchLoop(pat_list, do_add, direction, line, pat_idx, 0))

  elif loop_cnt > 10:
    tid_search_list = tk.after(10, lambda line=line: SearchList_BgSearchLoop(pat_list, do_add, direction, line, pat_idx, 0))

  elif dlg_srch_shown:
    max_line = int(wt.f1_t.index("end").split(".")[0])
    anchor = SearchList_GetViewAnchor()
    stop_t = datetime.now() + timedelta(microseconds=100000)
    hl = pat_list[pat_idx]
    pat = hl[0]
    opt = Search_GetOptions(pat, hl[1], hl[2], (0 if direction < 0 else 1))
    line_list = []
    off = 0
    if direction >= 0:
      last_line = "end"
    else:
      last_line = "1.0"

    while line < max_line:
      pos = wt.f1_t.search(pat, ("%d.0" % line), last_line, **opt)
      if pos == "":
        break

      line = int(pos.split(".")[0])
      idx = SearchList_GetLineIdx(line)
      if do_add:
        if (idx >= len(dlg_srch_lines)) or (dlg_srch_lines[idx] != line):
          SearchList_InsertLine(line, "%d.0" % (idx + off + 1))
          line_list.append(line)
          if direction >= 0:
            off += 1

      else:
        if (idx < len(dlg_srch_lines)) and (dlg_srch_lines[idx] == line):
          del dlg_srch_lines[idx]
          wt.dlg_srch_f1_l.delete("%d.0" % (idx + 1), "%d.0" % (idx + 2))
          line_list.append(line)
          break

      if direction >= 0:
        line += 1

      if datetime.now() >= stop_t:
        break

    if len(line_list) > 0:
      SearchList_BgSearch_AppendUndoList(dlg_srch_undo, do_add, line_list)
      if do_add:
        #set dlg_srch_lines [lsort -integer -increasing [concat $dlg_srch_lines $line_list]]
        dlg_srch_lines.extend(line_list)
        dlg_srch_lines.sort()

      # select previously selected line again
      SearchList_SeeViewAnchor(anchor)

    if (line < max_line) and (pos != ""):
      # create or update the progress bar
      if direction == 0:
        ratio = line / max_line
      elif direction < 0:
        thresh = int(wt.f1_t.index("insert").split(".")[0])
        ratio = 1 - (line / thresh)
      else:
        thresh = int(wt.f1_t.index("insert").split(".")[0])
        ratio = line / (max_line - thresh)

      ratio = int(100.0*(ratio + pat_idx)/len(pat_list))
      SearchList_SearchProgress(ratio)

      loop_cnt += 1
      tid_search_list = tk.after_idle(lambda line=line, loop_cnt=loop_cnt:
                                        SearchList_BgSearchLoop(pat_list, do_add, direction, line, pat_idx, loop_cnt))

    else:
      SearchList_BgSearch_FinalizeUndoList(dlg_srch_undo)
      pat_idx += 1
      if pat_idx < len(pat_list):
        loop_cnt += 1
        if direction == 0:
          line = 1
        else:
          line = int(wt.f1_t.index("insert").split(".")[0])

        tid_search_list = tk.after_idle(lambda line=line, loop_cnt=loop_cnt:
                                          SearchList_BgSearchLoop(pat_list, do_add, direction, line, pat_idx, loop_cnt))
      else:
        tid_search_list = None
        SafeDestroy(wt.dlg_srch_slpro)
        SafeDestroy(wt.srch_abrt)
        wt.dlg_srch_f1_l.configure(cursor="top_left_arrow")

  else:
    tid_search_list = None


#
# This function inserts all lines of text tagged with one of the given
# tags in the main window content into the search list.
#
def SearchList_StartSearchTags(tag_list, direction):
  global tid_search_list

  if SearchList_SearchAbort():
    if direction == 1:
      line = int(wt.f1_t.index("insert").split(".")[0])
    else:
      line = 1

    # reset redo list
    dlg_srch_redo = []

    tid_search_list = tk.after(10, lambda: SearchList_BgSearchTagsLoop(tag_list, 0, direction, line, 0))


#
# This function acts as background process to fill the search list window with
# matches on highlight tags.
#
def SearchList_BgSearchTagsLoop(tag_list, tag_idx, direction, line, loop_cnt):
  global block_bg_tasks, tid_search_inc, tid_search_hall, tid_search_list
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel, dlg_srch_undo

  if block_bg_tasks or (tid_search_inc is not None) or (tid_search_hall is not None):
    # background tasks are suspended - re-schedule with timer
    tid_search_list = tk.after(100, lambda: SearchList_BgSearchTagsLoop(tag_list, tag_idx, direction, line, 0))

  elif loop_cnt > 10:
    tid_search_list = tk.after(10, lambda: SearchList_BgSearchTagsLoop(tag_list, tag_idx, direction, line, 0))

  elif dlg_srch_shown:
    if direction < 0:
      last_line = wt.f1_t.index("insert")
    else:
      last_line = wt.f1_t.index("end")

    anchor = SearchList_GetViewAnchor()
    stop_t = datetime.now() + timedelta(microseconds=100000)
    tagnam = tag_list[tag_idx]
    line_list = []
    off = 0

    while True:
      pos12 = wt.f1_t.tag_nextrange(tagnam, "%d.0" %line, last_line)
      if len(pos12) == 0:
        break

      line = int(pos12[0].split(".")[0])
      idx = SearchList_GetLineIdx(line)
      if (idx >= len(dlg_srch_lines)) or (dlg_srch_lines[idx] != line):
        SearchList_InsertLine(line, "%d.0" % (idx + off + 1))
        line_list.append(line)
        off += 1

      line += 1

      if datetime.now() >= stop_t:
        break
    # -- end while --

    if len(line_list) > 0:
      SearchList_BgSearch_AppendUndoList(dlg_srch_undo, 1, line_list)
      dlg_srch_lines.extend(line_list)
      dlg_srch_lines.sort()

      # select previously selected line again
      SearchList_SeeViewAnchor(anchor)

    if len(pos12) == 2:
      # create or update the progress bar
      max_line = int(last_line.split(".")[0])
      thresh = int(wt.f1_t.index("insert").split(".")[0])
      if direction == 0:
        ratio = line / max_line
      elif direction < 0:
        ratio = line / thresh
      else:
        ratio = line / (max_line - thresh)

      ratio = int(100.0*(ratio + tag_idx)/len(tag_list))
      SearchList_SearchProgress(ratio)

      loop_cnt += 1
      tid_search_list = tk.after_idle(lambda: SearchList_BgSearchTagsLoop(tag_list, tag_idx, direction, line, loop_cnt))

    else:
      SearchList_BgSearch_FinalizeUndoList(dlg_srch_undo)
      tag_idx += 1
      if tag_idx < len(tag_list):
        loop_cnt += 1
        if direction == 1:
          line = int(wt.f1_t.index("insert").split(".")[0])
        else:
          line = 1

        tid_search_list = tk.after_idle(lambda: SearchList_BgSearchTagsLoop(tag_list, tag_idx, line, direction, loop_cnt))
      else:
        tid_search_list = None
        SafeDestroy(wt.dlg_srch_slpro)
        SafeDestroy(wt.srch_abrt)
        wt.dlg_srch_f1_l.configure(cursor="top_left_arrow")

  else:
    tid_search_list = None


#
# This function is called by the background search process to create and
# update the progress bar.
#
def SearchList_SearchProgress(ratio):
  if not wt_exists(wt.dlg_srch_slpro):
    wt.dlg_srch_slpro = Frame(wt.dlg_srch, takefocus=0, relief=SUNKEN, borderwidth=2)

    wt.dlg_srch_slpro_c = Canvas(wt.dlg_srch_slpro, width=100, height=10, highlightthickness=0, takefocus=0)
    wt.dlg_srch_slpro_c.create_rectangle(0, 0, ratio, 12, fill="#0b1ff7", outline="")
    wt.dlg_srch_slpro_c.pack()
    wt.dlg_srch_slpro.place(anchor=NW, x=0, y=0)  # -in .dlg_srch.f1.l

    wt.dlg_srch_f1_l.configure(cursor="watch")

  else:
    wt.dlg_srch_slpro_c.coords("all", 0, 0, ratio, 12)


#
# This function stops a possibly ongoing background search in the search
# list dialog. Optionally the user is asked it he really wants to abort.
# The function returns 0 and does not abort the background action if the
# user selects "Cancel", else it returns 1.  The caller MUST check the
# return value if parameter "do_warn" is TRUE.
#
def SearchList_SearchAbort(do_warn=True):
  global tid_search_list, dlg_srch_undo, dlg_srch_redo
  global vwait_search_complete

  cancel_new = False

  if tid_search_list is not None:
    if do_warn:
      vwait_search_complete = StringVar(tk, "wait")

      PreemptBgTasks()
      wt.srch_abrt = Toplevel(tk)
      wt.srch_abrt.wm_transient(tk)
      wt.srch_abrt.wm_geometry("+%d+%d" % (wt.dlg_srch.winfo_rootx() + 100,
                                           wt.dlg_srch.winfo_rooty() + 100))
      wt.srch_abrt.wm_title("Confirm abort of search")

      wt.srch_abrt_f1 = Frame(wt.srch_abrt)
      wt.srch_abrt_f1_icon = Button(wt.srch_abrt_f1, bitmap="question", relief=FLAT, takefocus=0)
      wt.srch_abrt_f1_icon.pack(side=LEFT, padx=10, pady=20)
      wt.srch_abrt_f1_msg = Label(wt.srch_abrt_f1, justify=LEFT, text="This command will abort the ongoing search operation.\nPlease confirm, or wait until this message disappears.")
      wt.srch_abrt_f1_msg.pack(side=LEFT, padx=10, pady=20)
      wt.srch_abrt_f1.pack(side=TOP)

      wt.srch_abrt_f3 = Frame(wt.srch_abrt)
      wt.srch_abrt_f3_cancel = Button(wt.srch_abrt_f3, text="Cancel",
                                      command=lambda: SearchList_AbortVwaitSet("cancel_new"))
      wt.srch_abrt_f3_ok = Button(wt.srch_abrt_f3, text="Ok", default=ACTIVE,
                                  command=lambda: SearchList_AbortVwaitSet("abort_cur"))
      wt.srch_abrt_f3_cancel.pack(side=LEFT, padx=10, pady=5)
      wt.srch_abrt_f3_ok.pack(side=LEFT, padx=10, pady=5)
      wt.srch_abrt_f3.pack(side=TOP)

      wt.srch_abrt_f1_icon.bindtags([wt.srch_abrt, "all"])
      wt.srch_abrt.bind("<Escape>", lambda e: SearchList_AbortVwaitSet("cancel_new"))
      wt.srch_abrt.bind("<Return>", lambda e: SearchList_AbortVwaitSet("abort_cur"))
      # closing the message popup while the bg task is still busy is equivalent to "cancel"
      # (note the variable must be modified in any case so that vwait returns)
      wt.srch_abrt_f1.bind("<Destroy>", lambda e: SearchList_DestroyCb())
      # clicks into other windows (routed here due to grab) raise the popup window
      wt.srch_abrt.bind("<Button-1>", lambda e: wt.srch_abrt.lift())
      wt.srch_abrt.bind("<Button-3>", lambda e: wt.srch_abrt.lift())
      wt.srch_abrt_f3_ok.focus_set()
      wt.srch_abrt.grab_set()

      ResumeBgTasks()

      # block here until the user responds or the background task finishes
      tk.wait_variable(vwait_search_complete)
      SafeDestroy(wt.srch_abrt)

      cancel_new = (vwait_search_complete.get() == "cancel_new")
      vwait_search_complete.set("")

    else:
      SafeDestroy(wt.srch_abrt)
  else:
    SafeDestroy(wt.srch_abrt)

  if not cancel_new and (tid_search_list is not None):
    SearchList_BgSearch_FinalizeUndoList(dlg_srch_undo)
    SearchList_BgSearch_FinalizeUndoList(dlg_srch_redo)

    # stop the background process
    if tid_search_list is not None: tk.after_cancel(tid_search_list)
    tid_search_list = None

    # remove the progress bar
    SafeDestroy(wt.dlg_srch_slpro)
    try:
      wt.dlg_srch_f1_l.configure(cursor="top_left_arrow")
    except:
      pass

    DisplayStatusLine("search", "warn", "Search list operation was aborted")

  return not cancel_new


def SearchList_DestroyCb():
  if vwait_search_complete.get() == "wait":
    if tid_search_list is not None:
      vwait_search_complete.set("cancel_new")
    else:
      vwait_search_complete.set("obsolete")

def SearchList_AbortVwaitSet(val):
  global vwait_search_complete
  vwait_search_complete.set(val)

#
# This helper function is called before modifications of the search result
# list by the various background tasks to determine a line which can serve
# as "anchor" for the view, i.e. which will be made visible again after the
# insertions or removals (which may lead to scrolling.)
#
def SearchList_GetViewAnchor():
  global dlg_srch_sel, dlg_srch_lines

  sel = dlg_srch_sel.TextSel_GetSelection()
  if len(sel) > 0:
    # keep selection visible
    return [1, dlg_srch_lines[sel[0]]]
  else:
    # no selection - check if line near cursor in main win is visible
    line = int(wt.f1_t.index("insert").split(".")[0])
    idx = SearchList_GetLineIdx(line)
    if ((idx < len(dlg_srch_lines)) and
        (wt.dlg_srch_f1_l.bbox("%d.0" % idx) is not None)):
      return [0, dlg_srch_lines[idx]]

  return [0, -1]


#
# This helper function is called after modifications of the search result
# list by the various background tasks to make the previously determined
# "anchor" line visible and to adjust the selection.
#
def SearchList_SeeViewAnchor(info):
  global dlg_srch_sel, dlg_srch_lines

  anchor = info[1]
  if anchor >= 0:
    #set idx [lsearch -exact -integer -sorted -increasing $dlg_srch_lines $anchor]
    idx = SearchList_GetLineIdx(anchor)
    if (idx < len(dlg_srch_lines)) and (dlg_srch_lines[idx] == anchor):
      wt.dlg_srch_f1_l.see("%d.0" % (idx + 1))
      if info[0]:
        dlg_srch_sel.TextSel_SetSelection([idx], False)
      return

  dlg_srch_sel.TextSel_SetSelection([], False)


#
# This function is during background tasks which fill the search match dialog
# after adding new matches. The function adds the respective line numbers to
# the undo list. If there's already an undo item for the current search, the
# numbers are merged into it.
#
def SearchList_BgSearch_AppendUndoList(undo_list, do_add, line_list):
  opcode = 2 if do_add else -2
  if len(undo_list) > 0:
    prev_undo = undo_list[-1]
    if prev_undo[0] == opcode:
      prev_undo[1].extend(line_list)
    else:
      undo_list.append([opcode, line_list])
  else:
    undo_list.append([opcode, line_list])


#
# This function is invoked at the end of background tasks which fill the
# search list window to mark the entry on the undo list as closed (so that
# future search matches go into a new undo element.)
#
def SearchList_BgSearch_FinalizeUndoList(undo_list):
  if len(undo_list) > 0:
    prev_op = undo_list[-1][0]
    if (prev_op == 2) or (prev_op == -2):
      prev_op = -1 if (prev_op == -2) else 1
      undo_list[-1][0] = prev_op


#
# This function inserts all selected lines in the main window into the list.
#
def SearchList_AddMainSelection():
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel
  global dlg_srch_undo, dlg_srch_redo

  if dlg_srch_shown and SearchList_SearchAbort():
    pos12 = wt.f1_t.tag_nextrange("sel", "1.0")
    if len(pos12) == 2:
      line = int(pos12[0].split(".")[0])
      (line_2, char) = map(int, pos12[1].split("."))
      if char == 0: line_2 -= 1

      line_list = []
      idx_list = []
      while line <= line_2:
        idx = SearchList_GetLineIdx(line)
        if (idx >= len(dlg_srch_lines)) or (dlg_srch_lines[idx] != line):
          idx += len(line_list)
          SearchList_InsertLine(line, "%d.0" % (idx + 1))
          line_list.append(line)
          idx_list.append(idx)
        line += 1

      if len(line_list) > 0:
        dlg_srch_lines.extend(line_list)
        dlg_srch_lines.sort()
        dlg_srch_undo.append([1, line_list])
        dlg_srch_redo = []

        dlg_srch_sel.TextSel_SetSelection(idx_list, False)
        wt.dlg_srch_f1_l.see("%d.0" % idx_list[0])


#
# This function inserts either the line in the main window holding the cursor
# or all selected lines into the search result list.  It's bound to the "i" key
# press event in the main window.
#
def SearchList_CopyCurrentLine():
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel
  global dlg_srch_undo, dlg_srch_redo

  if not dlg_srch_shown:
    SearchList_Open(False)

  if dlg_srch_shown and SearchList_SearchAbort():
    pos12 = wt.f1_t.tag_nextrange("sel", "1.0")
    # ignore selection if not visible (because it can be irritating when "i"
    # inserts some random line instead of the one holding the cursor)
    if (len(pos12) == 2) and (wt.f1_t.bbox(pos12[0]) is not None):
      # selection exists: add all selected lines
      SearchList_AddMainSelection()
    else:
      # get line number of the cursor position
      line = int(wt.f1_t.index("insert").split(".")[0])
      idx = SearchList_GetLineIdx(line)
      if (idx >= len(dlg_srch_lines)) or (dlg_srch_lines[idx] != line):
        dlg_srch_lines.insert(idx, line)
        pos = "%d.0" % (idx + 1)
        SearchList_InsertLine(line, pos)
        wt.dlg_srch_f1_l.see(pos)
        dlg_srch_sel.TextSel_SetSelection([idx], False)

        dlg_srch_undo.append([1, [line]])
        dlg_srch_redo = []

      else:
        # line is already included - make it visible & select it
        SearchList_MatchView(line)


#
# This function creates the tags for selection and color highlighting.
# This is used for initialisation and after editing highlight tags.
#
def SearchList_CreateHighlightTags():
  global patlist, fmt_find, fmt_selection
  global dlg_srch_sel, dlg_srch_shown

  if dlg_srch_shown:
    # create highlight tags
    for w in patlist:
      HighlightConfigure(wt.dlg_srch_f1_l, w[4], w)

    # create text tag for search highlights
    HighlightConfigure(wt.dlg_srch_f1_l, "find", fmt_find)
    wt.dlg_srch_f1_l.tag_raise("find")

    # raise sel above highlight tag, but below find
    HighlightConfigure(wt.dlg_srch_f1_l, "sel", fmt_selection)
    wt.dlg_srch_f1_l.tag_lower("sel")

    # create tag to invisibly mark prefixes (used to exclude the text from search and mark-up)
    # padding is added to align bookmarked lines (note padding is smaller than
    # in the main window because there's extra space in front anyways)
    wt.dlg_srch_f1_l.tag_configure("prefix", lmargin1=11)
    wt.dlg_srch_f1_l.tag_configure("bookmark", lmargin1=0)


#
# This function is called after removal of tags in the Tag list dialog.
#
def SearchList_DeleteTag(tag):
  global dlg_srch_shown

  if dlg_srch_shown:
    wt.dlg_srch_f1_l.tag_delete(tag)


#
# This function is called out of the main window's highlight loop for every line
# to which a highlight is applied.
#
def SearchList_HighlightLine(tag, line):
  global dlg_srch_shown, dlg_srch_highlight, dlg_srch_lines
  global tid_high_init

  if dlg_srch_shown:
    if dlg_srch_highlight.get() or (tid_high_init is not None):
      idx = SearchList_GetLineIdx(line)
      if (idx < len(dlg_srch_lines)) and (dlg_srch_lines[idx] == line):
        try:
          wt.dlg_srch_f1_l.tag_add(tag, "%d.0" % (idx + 1), "%d.0" % (idx + 2))
        except:
          pass


#
# This function is bound to the "Toggle highlight" checkbutton in the
# search list dialog's menu.  The function enables or disables search highlight.
#
def SearchList_HighlightClear():
  global dlg_srch_shown

  if dlg_srch_shown:
    wt.dlg_srch_f1_l.tag_remove("find", "1.0", "end")


#
# This function adjusts the view in the search result list so that the given
# main window's text line becomes visible.
#
def SearchList_MatchView(line):
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel

  if dlg_srch_shown:
    idx = SearchList_GetLineIdx(line)
    if idx < len(dlg_srch_lines):
      wt.dlg_srch_f1_l.see("%d.0" % idx)
      wt.dlg_srch_f1_l.see("%d.0" % (idx + 1))
      wt.dlg_srch_f1_l.mark_set("insert", "%d.0" % (idx + 1))

      # move selection onto the line; clear selection if line is not in the list
      if dlg_srch_lines[idx] == line:
        dlg_srch_sel.TextSel_SetSelection([idx], False)
      else:
        dlg_srch_sel.TextSel_SetSelection([], False)

    else:
      wt.dlg_srch_f1_l.see("end")
      wt.dlg_srch_f1_l.mark_set("insert", "end")


#
# This function is called when a bookmark is added or removed in the main
# window.  The function displays the bookmark in the respective line in
# the search filter dialog, if the line is currently visible.  (Note this
# function is not used to mark lines which are newly inserted into the
# dialog and which already have a bookmark; see the insert function below)
#
def SearchList_MarkLine(line):
  global dlg_srch_shown, dlg_srch_lines
  global img_marker, mark_list

  if dlg_srch_shown:
    idx = SearchList_GetLineIdx(line)
    if (idx < len(dlg_srch_lines)) and (dlg_srch_lines[idx] == line):
      pos = "%d.0" % (idx + 1)
      if mark_list.get(line):
        wt.dlg_srch_f1_l.image_create(pos, image=img_marker, padx=2)
        wt.dlg_srch_f1_l.tag_add("bookmark", pos)
        wt.dlg_srch_f1_l.see(pos)

        (lil, lic) = map(int, wt.dlg_srch_f1_l.index("insert").split("."))
        if lic > 0:
          wt.dlg_srch_f1_l.mark_set("insert", "%d.0" % lil)

      else:
        wt.dlg_srch_f1_l.delete(pos)


#
# This function is bound to the "m" key in the search filter dialog.
# The function adds or removes a bookmark on the currently selected
# line (but only if exactly one line is selected.)
#
def SearchList_ToggleMark():
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_sel

  sel = dlg_srch_sel.TextSel_GetSelection()
  if len(sel) == 1:
    line = dlg_srch_lines[sel[0]]
    Mark_Toggle(line)


#
# This function copies a line of text (including highlighting tags and
# bookmark marker) from the main window into the the search filter dialog.
#
def SearchList_InsertLine(line_idx, ins_pos):
  global dlg_srch_show_fn, dlg_srch_show_tick, dlg_srch_tick_delta, dlg_srch_tick_root
  global dlg_srch_fn_cache
  global tick_pat_sep, tick_str_prefix, img_marker, mark_list

  # copy text content and tags out of the main window
  pos = "%d.0" % line_idx
  dump = ExtractText(pos + " linestart", pos + " lineend")
  tag_list = [x for x in wt.f1_t.tag_names(pos) if x.startswith("tag")]

  if dlg_srch_tick_delta.get() or dlg_srch_show_fn.get() or dlg_srch_show_tick.get():
    fn = ParseFrameTickNo(pos, dlg_srch_fn_cache)
    if fn:
        tick_no = fn[1]
        fn = fn[0]
    else:
        tick_no = "?"
        fn = "?"

    prefix = ""
    if dlg_srch_tick_delta.get():
      try:
        prefix = str(int(tick_no) - dlg_srch_tick_root)
      except:
        prefix = "?"

    if dlg_srch_show_tick.get():
      if prefix != "":
        prefix += ":"
      prefix += tick_no

    if dlg_srch_show_fn.get():
      if prefix != "":
        prefix += ":"
      prefix += tick_str_prefix + fn

    prefix = "   " + prefix + " "
  else:
    prefix = "   "

  # display the text
  wt.dlg_srch_f1_l.insert(ins_pos, prefix, [prefix], dump + "\n", tag_list)

  # add bookmark, if this line is marked
  if mark_list.get(line_idx) is not None:
    wt.dlg_srch_f1_l.image_create(ins_pos, image=img_marker, padx=2)
    wt.dlg_srch_f1_l.tag_add("bookmark", ins_pos)


#
# This function fills the search list dialog with all text lines indicated in
# the dialog's line number list (note the first line has number 1)
#
def SearchList_Refill():
  # WARNING: caller must invoke SearchList_SearchAbort
  tid_search_list = tk.after(10, lambda: SearchList_BgRefillLoop(0))


#
# This function acts as background process to refill the search list window
# with previous content, but in a different format (e.g. with added frame nums)
#
def SearchList_BgRefillLoop(off):
  global block_bg_tasks, tid_search_inc, tid_search_hall, tid_search_list
  global dlg_srch_shown, dlg_srch_lines

  if block_bg_tasks or (tid_search_inc is not None) or (tid_search_hall is not None):
    # background tasks are suspended - re-schedule with timer
    tid_search_list = tk.after(100, lambda: SearchList_BgRefillLoop(off))

  elif dlg_srch_shown:
    end_off = off + 400               # end_off is actually last+1
    if end_off > len(dlg_srch_lines): # not >=
      end_off = len(dlg_srch_lines)

    # replace each line separately with the new format
    while off < end_off:
      txt_line = dlg_srch_lines[off]
      pos = "%d.0" % (off + 1)

      wt.dlg_srch_f1_l.delete(pos, pos + " +1 line")
      SearchList_InsertLine(txt_line, pos)
      off += 1

    # refresh the selection
    dlg_srch_sel.TextSel_ShowSelection()

    if off < len(dlg_srch_lines):
      # create or update the progress bar
      ratio = 100.0*off // len(dlg_srch_lines)
      SearchList_SearchProgress(ratio)

      tid_search_list = tk.after_idle(lambda: SearchList_BgRefillLoop(off))

    else:
      tid_search_list = None
      SafeDestroy(wt.dlg_srch_slpro)
      SafeDestroy(wt.srch_abrt)
      wt.dlg_srch_f1_l.configure(cursor="top_left_arrow")

  else:
    tid_search_list = None


#
# This function is a callback for selection changes in the search list dialog.
# If a single line is selected, the view in the main window is changed to
# display the respective line.
#
def SearchList_SelectionChange(sel):
  global dlg_srch_sel, dlg_srch_lines

  if len(sel) == 1:
    idx = sel[0]
    if idx < len(dlg_srch_lines):
      Mark_Line(dlg_srch_lines[idx])


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
def SearchList_GetLen():
  global dlg_srch_sel, dlg_srch_lines
  return len(dlg_srch_lines)


#
# This function must be called when portions of the text in the main window
# have been deleted to update references to text lines. Parameter meaning:
# - top_l: this is the first line which is not deleted, or 1 if none
# - bottom_l: this line and all below have been removed, or 0 if none
#
def SearchList_AdjustLineNums(top_l, bottom_l):
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_undo, dlg_srch_redo, dlg_srch_fn_cache

  if dlg_srch_shown:
    if bottom_l == 0:
      # delete from 1 ... topl
      idx = SearchList_GetLineIdx(top_l)
      if idx > 0:
        wt.dlg_srch_f1_l.delete("1.0", "%d.0" % (idx + 1))
      tmpl = [line - top_l + 1 for line in dlg_srch_lines[idx:]]
      dlg_srch_lines = tmpl

    else:
      # delete from bottom_l ... end
      idx = SearchList_GetLineIdx(bottom_l)
      if idx < len(dlg_srch_lines):
        wt.dlg_srch_f1_l.delete("%d.0" % (idx + 1), "end")
        del dlg_srch_lines[idx:]

    tmp2 = []
    for cmd in dlg_srch_undo:
      tmpl = []
      for line in cmd[1]:
        if (line >= top_l) and ((line < bottom_l) or (bottom_l == 0)):
          tmpl.append(line - top_l + 1)

      if len(tmpl) > 0:
        tmp2.append([cmd[0], tmpl])

    dlg_srch_undo = tmp2
    dlg_srch_redo = []
    dlg_srch_fn_cache = {}


#
# This function stores all text lines in the search result window into a file.
#
def SearchList_SaveFile(filename, lnum_only):
  global dlg_srch_lines

  try:
    with open(filename, "w") as f:
      if lnum_only:
        # save line numbers only (i.e. line numbers in main window)
        for line in dlg_srch_lines:
          print(line, file=f)
      else:
        # save text content
        for line in dlg_srch_lines:
          print(ExtractText("%d.0" % line, "%d.0 lineend" % line), file=f)
  except OSError as e:
    messagebox.showerror(parent=wt.dlg_srch, message="Error writing file \"%s\": %s" % (filename, e.strerror))


#
# This function is called by menu entry "Save as..." in the search dialog.
# The user is asked to select an output file; if he does so the list
# content is written into it, in the format selected by the user.
#
def SearchList_SaveFileAs(lnum_only):
  global dlg_srch_lines

  PreemptBgTasks()
  if len(dlg_srch_lines) > 0:
    def_name = ""
    filename = filedialog.asksaveasfilename(parent=wt.dlg_srch, filetypes=(("all", "*"), ("Text", "*.txt")),
                                            title="Select output file",
                                            initialfile=os.path.basename(def_name),
                                            initialdir=os.path.dirname(def_name))
    if filename:
      SearchList_SaveFile(filename, lnum_only)

  else:
    messagebox.showinfo(parent=wt.dlg_srch, message="The search result list is empty.")
  ResumeBgTasks()


#
# This function is called by menu entry "Load line numbers..." in the search
# result dialog. The user is asked to select an input file; if he does so a
# list of line numbers is extracted from it and lines with these numbers is
# copied from the main window.
#
def SearchList_LoadFrom():
  if dlg_srch_shown and SearchList_SearchAbort():
    PreemptBgTasks()
    def_name = ""
    filename = filedialog.askopenfilename(parent=wt.dlg_srch, filetypes=(("all", "*"), ("Text", "*.txt")),
                                          title="Select file for reading",
                                          initialfile=os.path.basename(def_name),
                                          initialdir=os.path.dirname(def_name))
    ResumeBgTasks()

    if len(filename) != 0:
      try:
        with open(filename, "r") as f:
          max_line = int(wt.f1_t.index("end").split(".")[0])
          answer = ""
          skipped = 0
          synerr = 0
          line_list = []

          for line_str in f.readlines():
            match = re.match(r"^(\d+)", line_str)
            if match:
              line = int(match.group(0))
              if (line > 0) and (line < max_line):
                line_list.append(line)
              else:
                skipped += 1
            elif not re.match(r"^\\s*(?:#.*)?$", line_str):
              synerr += 1

          if (skipped > 0) or (synerr > 0):
            msg = "Ignored "
            if skipped > 0:
              msg = msg + "%d lines with a value outside of the allowed range 1 .. %d" % (skipped, max_line)
            if synerr > 0:
              if msg != "": msg = msg + " and"
              msg = msg + (" %d non-empty lines without a number value" % synerr)

            answer = messagebox.showwarning(parent=wt.dlg_srch, type="okcancel", message=msg)

          if answer != "cancel":
            if len(line_list) > 0:
              # sort & remove duplicate line numbers
              line_list = sorted(set(line_list))
              tid_search_list = tk.after(10, lambda: SearchList_BgLoadLoop(line_list, 0))
            else:
              messagebox.showerror(parent=wt.dlg_srch, message="No valid line numbers found in the file")

      except OSError as e:
        messagebox.showerror(parent=wt.dlg_srch, message="Error reading file: " + e.strerror)


#
# This function acts as background process to fill the search list window
# with a given list of line indices.
#
def SearchList_BgLoadLoop(line_list, off):
  global block_bg_tasks, tid_search_inc, tid_search_hall, tid_search_list
  global dlg_srch_shown, dlg_srch_lines, dlg_srch_undo, dlg_srch_redo

  if block_bg_tasks or (tid_search_inc is not None) or (tid_search_hall is not None):
    # background tasks are suspended - re-schedule with timer
    tid_search_list = tk.after(100, lambda: SearchList_BgLoadLoop(line_list, off))

  elif dlg_srch_shown:
    anchor = SearchList_GetViewAnchor()
    start_off = off
    end_off = off + 400          # end_off is actually last+1
    if end_off > len(line_list): # not >=
      end_off = len(line_list)

    # insert the lines into the text widget
    ins_off = 0
    while off < end_off:
      line = line_list[off]
      idx = SearchList_GetLineIdx(line)
      if (idx >= len(dlg_srch_lines)) or (dlg_srch_lines[idx] != line):
        SearchList_InsertLine(line, "%d.0" % (idx + ins_off + 1))
        ins_off += 1
      off += 1

    # add the lines to the list (in bulk, for performance)
    dlg_srch_lines.extend(line_list[start_off : end_off])
    dlg_srch_lines.sort()
    dlg_srch_redo = []
    SearchList_BgSearch_AppendUndoList(dlg_srch_undo, 1, line_list[start_off : end_off])

    SearchList_SeeViewAnchor(anchor)

    if off < len(line_list):
      # create or update the progress bar
      ratio = 100.0*off // len(line_list)
      SearchList_SearchProgress(ratio)

      tid_search_list = tk.after_idle(lambda: SearchList_BgLoadLoop(line_list, off))
    else:
      SearchList_BgSearch_FinalizeUndoList(dlg_srch_undo)
      tid_search_list = None
      SafeDestroy(wt.dlg_srch_slpro)
      SafeDestroy(wt.srch_abrt)
      wt.dlg_srch_f1_l.configure(cursor="top_left_arrow")

  else:
    tid_search_list = None


#
# This function is bound to CTRL-g in the search list and displays stats
# about the content of the search result list.
#
def SearchList_DisplayStats():
  global dlg_srch_sel, dlg_srch_lines

  sel = dlg_srch_sel.TextSel_GetSelection()
  if len(sel) == 1:
    line_idx = sel[0] + 1
    msg = "line %d of %d in the search list" % (line_idx, len(dlg_srch_lines))
  else:
    msg = "%d lines in the search list" % len(dlg_srch_lines)

  DisplayStatusLine("line_query", "msg", msg)


# ----------------------------------------------------------------------------
#
# This function creates or raises the color highlighting tags list dialog.
# This dialog shows all currently defined tag assignments.
#
def TagList_OpenDialog():
  global font_content, col_bg_content, col_fg_content
  global dlg_tags_shown, dlg_tags_geom, dlg_tags_sel

  PreemptBgTasks()
  if not dlg_tags_shown:
    wt.dlg_tags = Toplevel(tk)
    wt.dlg_tags.wm_title("Color highlights list")
    wt.dlg_tags.wm_group(tk)

    CreateButtonBitmap("img_up", "img_down")

    wt.dlg_tags_menubar = Menu(wt.dlg_tags)
    wt.dlg_tags_menubar_edit = Menu(wt.dlg_tags_menubar, tearoff=0, postcommand=lambda: TagList_ContextPopulate(wt.dlg_tags_menubar_edit, 1))
    wt.dlg_tags_menubar.add_cascade(label="Edit", menu=wt.dlg_tags_menubar_edit, underline=0)
    wt.dlg_tags_menubar.add_command(label="Move Up", command=TagList_ShiftUp, state=DISABLED)
    wt.dlg_tags_menubar.add_command(label="Move Down", command=TagList_ShiftDown, state=DISABLED)
    wt.dlg_tags.config(menu=wt.dlg_tags_menubar)

    char_w = font_content.measure(" ")
    wt.dlg_tags_f1 = Frame(wt.dlg_tags)
    wt.dlg_tags_f1_l = Text(wt.dlg_tags_f1, width=1, height=1, wrap=NONE, font=font_content,
                            cursor="top_left_arrow", foreground=col_fg_content, background=col_bg_content,
                            exportselection=0, insertofftime=0, insertwidth=2*char_w)
    wt.dlg_tags_f1_l.pack(side=LEFT, fill=BOTH, expand=1)
    wt.dlg_tags_f1_sb = Scrollbar(wt.dlg_tags_f1, orient=VERTICAL, command=wt.dlg_tags_f1_l.yview, takefocus=0)
    wt.dlg_tags_f1_sb.pack(side=LEFT, fill=Y)
    wt.dlg_tags_f1_sb.pack(side=LEFT, fill=Y)
    wt.dlg_tags_f1_l.configure(yscrollcommand=wt.dlg_tags_f1_sb.set)
    wt.dlg_tags_f1.pack(side=TOP, fill=BOTH, expand=1)

    dlg_tags_sel = TextSel(wt.dlg_tags_f1_l, TagList_SelectionChange, TagList_GetLen, "extended")
    wt.dlg_tags_f1_l.bindtags([wt.dlg_tags_f1_l, "TextSel", tk, "all"])

    wt.dlg_tags_f2 = Frame(wt.dlg_tags)
    wt.dlg_tags_f2_l = Label(wt.dlg_tags_f2, text="Find:")
    wt.dlg_tags_f2_l.grid(sticky="we", column=0, row=0)
    wt.dlg_tags_f2_but_next = Button(wt.dlg_tags_f2, text="Next", command=lambda: TagList_Search(1), underline=0, state=DISABLED)
    wt.dlg_tags_f2_but_next.grid(sticky="we", column=1, row=0)
    wt.dlg_tags_f2_but_prev = Button(wt.dlg_tags_f2, text="Prev.", command=lambda: TagList_Search(0), underline=0, state=DISABLED)
    wt.dlg_tags_f2_but_prev.grid(sticky="we", column=2, row=0)
    wt.dlg_tags_f2_but_all = Button(wt.dlg_tags_f2, text="All", command=lambda: TagList_SearchList(0), underline=0, state=DISABLED)
    wt.dlg_tags_f2_but_all.grid(sticky="we", column=3, row=0)
    wt.dlg_tags_f2_but_blw = Button(wt.dlg_tags_f2, text="All below", command=lambda: TagList_SearchList(1), state=DISABLED)
    wt.dlg_tags_f2_but_blw.grid(sticky="we", column=4, row=0)
    wt.dlg_tags_f2_but_abve = Button(wt.dlg_tags_f2, text="All above", command=lambda: TagList_SearchList(-1), state=DISABLED)
    wt.dlg_tags_f2_but_abve.grid(sticky="we", column=5, row=0)
    wt.dlg_tags_f2.pack(side=TOP, anchor=W, pady=2)

    wt.dlg_tags_ctxmen = Menu(wt.dlg_tags, tearoff=0)

    wt.dlg_tags_f1_l.bind("<Double-Button-1>", lambda e:BindCallAndBreak(lambda: TagList_DoubleClick(e.x, e.y)))
    wt.dlg_tags_f1_l.bind("<ButtonRelease-3>", lambda e:BindCallAndBreak(lambda: TagList_ContextMenu(e.x, e.y)))
    wt.dlg_tags_f1_l.bind("<Delete>", lambda e:BindCallAndBreak(TagList_RemoveSelection))
    wt.dlg_tags_f1_l.bind("<Key-slash>", lambda e:BindCallAndBreak(lambda: SearchEnter(1, wt.dlg_tags_f1_l)))
    wt.dlg_tags_f1_l.bind("<Key-question>", lambda e:BindCallAndBreak(lambda: SearchEnter(0, wt.dlg_tags_f1_l)))
    wt.dlg_tags_f1_l.bind("<Key-ampersand>", lambda e:BindCallAndBreak(SearchHighlightClear))
    wt.dlg_tags_f1_l.bind("<Key-n>", lambda e:BindCallAndBreak(lambda: TagList_Search(1)))
    wt.dlg_tags_f1_l.bind("<Key-N>", lambda e:BindCallAndBreak(lambda: TagList_Search(0)))
    wt.dlg_tags_f1_l.bind("<Alt-Key-n>", lambda e:BindCallAndBreak(lambda: TagList_Search(1)))
    wt.dlg_tags_f1_l.bind("<Alt-Key-p>", lambda e:BindCallAndBreak(lambda: TagList_Search(0)))
    wt.dlg_tags_f1_l.bind("<Alt-Key-a>", lambda e:BindCallAndBreak(lambda: TagList_SearchList(0)))
    wt.dlg_tags_f1_l.bind("<Alt-Key-N>", lambda e:BindCallAndBreak(lambda: TagList_SearchList(1)))
    wt.dlg_tags_f1_l.bind("<Alt-Key-P>", lambda e:BindCallAndBreak(lambda: TagList_SearchList(-1)))
    wt.dlg_tags_f1_l.focus_set()

    dlg_tags_shown = True
    wt.dlg_tags_f1_l.bind("<Destroy>", lambda e: TagList_DlgDestroyCb(), add="+")
    wt.dlg_tags.bind("<Configure>", lambda e:ToplevelResized(e.widget, wt.dlg_tags, wt.dlg_tags, "dlg_tags"))
    wt.dlg_tags.wm_geometry(win_geom["dlg_tags"])
    wt.dlg_tags.wm_positionfrom("user")

    TagList_Fill()

  else:
    wt.dlg_tags.wm_deiconify()
    wt.dlg_tags.lift()

  ResumeBgTasks()


#
# This function is bound to destruction events on the tag list dialog window.
# The function marks the dialog as closed.
#
def TagList_DlgDestroyCb():
    global dlg_tags_shown
    dlg_tags_shown = False


#
# This function is bound to right mouse clicks in the highlight tag list and pops
# up a context menu. If the mouse click occurred outside of the current selection
# the selection is updated. Then the menu is populated and shown at the mouse
# coordinates.
#
def TagList_ContextMenu(xcoo, ycoo):
  global dlg_tags_sel

  dlg_tags_sel.TextSel_ContextSelection(xcoo, ycoo)

  TagList_ContextPopulate(wt.dlg_tags_ctxmen, 0)

  rootx = wt.dlg_tags.winfo_rootx() + xcoo
  rooty = wt.dlg_tags.winfo_rooty() + ycoo
  tk.call("tk_popup", wt.dlg_tags_ctxmen, rootx, rooty, 0)


#
# This function is used both to populate the "Edit" menu and the context menu.
# The contents depend on the number of selected items.
#
def TagList_ContextPopulate(wid, show_all):
  global tlb_find, dlg_tags_sel

  sel = dlg_tags_sel.TextSel_GetSelection()
  sel_cnt = len(sel)
  sel_el = sel[0] if (sel_cnt > 0) else None

  state_find = DISABLED if (tlb_find.get() == "") else NORMAL
  state_find_sel_1 = DISABLED if ((tlb_find.get() == "") or (sel_cnt != 1)) else NORMAL
  state_sel_1 = DISABLED if (sel_cnt != 1) else NORMAL
  state_sel_n0 = DISABLED if (sel_cnt == 0) else NORMAL

  wid.delete(0, "end")
  if (state_sel_1 == NORMAL) or show_all:
    wid.add_command(label="Change background color", command=lambda:TagList_PopupColorPalette(sel_el, 0), state=state_sel_1)
    wid.add_command(label="Edit markup...", command=lambda:Markup_OpenDialog(sel_el), state=state_sel_1)
    wid.add_separator()
    wid.add_command(label="Copy to search field", command=lambda:TagList_CopyToSearch(sel_el), state=state_sel_1)
    wid.add_command(label="Update from search field", command=lambda:TagList_CopyFromSearch(sel_el), state=state_find_sel_1)

  wid.add_command(label="Add current search", command=lambda:TagList_AddSearch(wt.dlg_tags), state=state_find)
  if (state_sel_n0 == NORMAL) or show_all:
    wid.add_separator()
    wid.add_command(label="Remove selected entries", command=lambda:TagList_Remove(sel), state=state_sel_n0)


#
# This function is bound to double mouse button clicks onto an entry in
# the highlight list. The function opens the markup editor dialog.
#
def TagList_DoubleClick(xcoo, ycoo):
  global patlist, dlg_tags_sel

  sel = dlg_tags_sel.TextSel_GetSelection()
  if len(sel) == 1:
    Markup_OpenDialog(sel[0])


#
# This function is bound to the "up" button next to the color highlight list.
# Each selected item (selection may be non-consecutive) is shifted up by one line.
#
def TagList_ShiftUp():
  global patlist, dlg_tags_sel

  sel = dlg_tags_sel.TextSel_GetSelection()
  sel = sorted(sel) # must not sort inline
  if (len(sel) > 0) and (sel[0] > 0):
    new_sel = []
    for idx in sel:
      # remove the item in the listbox widget above the shifted one
      TagList_DisplayDelete(idx - 1)
      # re-insert the just removed item below the shifted one
      TagList_DisplayInsert(idx, idx - 1)

      # perform the same swap in the associated list
      (patlist[idx-1], patlist[idx]) = (patlist[idx], patlist[idx-1])

      new_sel.append(idx - 1)

    # redraw selection
    dlg_tags_sel.TextSel_SetSelection(new_sel)


#
# This function is bound to the "down" button next to the color highlight
# list.  Each selected item is shifted down by one line.
#
def TagList_ShiftDown():
  global patlist, dlg_tags_sel

  sel = dlg_tags_sel.TextSel_GetSelection()
  sel = sorted(sel, reverse=True) # must not sort inline
  if (len(sel) > 0) and (sel[0] < len(patlist) - 1):
    new_sel = []
    for idx in sel:
      TagList_DisplayDelete(idx + 1)
      TagList_DisplayInsert(idx, idx + 1)

      # perform the same swap in the associated list
      (patlist[idx], patlist[idx+1]) = (patlist[idx+1], patlist[idx])

      new_sel.append(idx + 1)

    dlg_tags_sel.TextSel_SetSelection(new_sel)


#
# This function is bound to the next/prev buttons below the highlight tags
# list. The function searches for the next line which is tagged with one of
# the selected highlighting tags (i.e. no text search is performed!) When
# multiple tags are searched for, the closest match is used.
#
def TagList_Search(is_fwd):
  global patlist, dlg_tags_sel

  min_line = -1
  sel = dlg_tags_sel.TextSel_GetSelection()

  for pat_idx in sel:
    w = patlist[pat_idx]

    Search_AddHistory(w[0], w[1], w[2])

    tagnam = w[4]
    start_pos = Search_GetBase(is_fwd, False)

    if is_fwd:
      pos12 = wt.f1_t.tag_nextrange(tagnam, start_pos)
    else:
      pos12 = wt.f1_t.tag_prevrange(tagnam, start_pos)

    if len(pos12) == 2:
      line = int(pos12[0].split(".")[0])
      if ((min_line == -1) or
          ((line < min_line) if is_fwd else (line > min_line))):
        min_line = line

  if min_line > 0:
    ClearStatusLine("search")
    Mark_Line(min_line)
    SearchList_HighlightLine("find", min_line)
    SearchList_MatchView(min_line)
  else:
    if len(sel) == 0:
      DisplayStatusLine("search", "error", "No pattern is selected in the list")
    else:
      Search_HandleNoMatch("", is_fwd)


#
# This function is bound to the "List all" button in the color tags dialog.
# The function opens the search result window and adds all lines matching
# the pattern for the currently selected color tags.
#
def TagList_SearchList(direction):
  global patlist, dlg_tags_sel

  min_line = -1
  sel = dlg_tags_sel.TextSel_GetSelection()
  if len(sel) > 0:
    tag_list = []
    for pat_idx in sel:
      w = patlist[pat_idx]
      tag_list.append(w[4])

    ClearStatusLine("search")
    SearchList_Open(False)
    SearchList_StartSearchTags(tag_list, direction)

  else:
    DisplayStatusLine("search", "error", "No pattern is selected in the list")


#
# This function is bound to changes of the selection in the color tags list.
#
def TagList_SelectionChange(sel):
  global dlg_tags_sel

  state = DISABLED if (len(sel) == 0) else NORMAL

  wt.dlg_tags_f2_but_next.configure(state=state)
  wt.dlg_tags_f2_but_prev.configure(state=state)
  wt.dlg_tags_f2_but_all.configure(state=state)
  wt.dlg_tags_f2_but_blw.configure(state=state)
  wt.dlg_tags_f2_but_abve.configure(state=state)

  wt.dlg_tags_menubar.entryconfigure(2, state=state)
  wt.dlg_tags_menubar.entryconfigure(3, state=state)


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
def TagList_GetLen():
  global patlist
  return len(patlist)


#
# This function updates a color tag text in the listbox.
#
def TagList_Update(pat_idx):
  global dlg_tags_shown, patlist

  if dlg_tags_shown:
    if pat_idx < len(patlist):
      w = patlist[pat_idx]

      TagList_Fill()
      dlg_tags_sel.TextSel_SetSelection([])

      wt.dlg_tags_f1_l.see("%d.0" % (pat_idx + 1))


#
# This function removes an entry from the listbox.
#
def TagList_DisplayDelete(pat_idx):
  wt.dlg_tags_f1_l.delete("%d.0" % (pat_idx + 1), "%d.0" % (pat_idx + 2))


#
# This function inserts a color tag text into the listbox and applies its
# highlight format options.
#
def TagList_DisplayInsert(pos, pat_idx):
  global patlist

  w = patlist[pat_idx]
  txt = w[0] + "\n"

  # insert text (prepend space to improve visibility of selection)
  wt.dlg_tags_f1_l.insert("%d.0" % (pos + 1), "  ", "margin", txt, w[4])


#
# This function fills the highlight pattern list dialog window with all
# list entries.
#
def TagList_Fill():
  global dlg_tags_shown, patlist, fmt_selection

  if dlg_tags_shown:
    wt.dlg_tags_f1_l.delete("1.0", "end")

    idx = 0
    for w in patlist:
      HighlightConfigure(wt.dlg_tags_f1_l, w[4], w)

      TagList_DisplayInsert(idx, idx)
      idx += 1

    # configure appearance of selected rows
    wt.dlg_tags_f1_l.tag_configure("margin", lmargin1=6)
    HighlightConfigure(wt.dlg_tags_f1_l, "sel", fmt_selection)
    wt.dlg_tags_f1_l.tag_lower("sel")


#
# This function allows to edit a color assigned to a tags entry.
#
def TagList_PopupColorPalette(pat_idx, is_fg):
  global patlist

  if pat_idx < len(patlist):
    wt.dlg_tags_f1_l.see("%d.0" % (pat_idx + 1))

    cool = wt.dlg_tags_f1_l.dlineinfo("%d.0" % (pat_idx + 1))
    rootx = wt.dlg_tags.winfo_rootx() + cool[0]
    rooty = wt.dlg_tags.winfo_rooty() + cool[1]

    w = patlist[pat_idx]
    col_idx = 7 if is_fg else 6
    def_col = w[col_idx]

    PaletteMenu_Popup(wt.dlg_tags, rootx, rooty,
                      lambda col:TagList_UpdateColor(col, pat_idx, is_fg),
                      w[col_idx])


#
# This function is invoked after a direct color change via the popup color palette.
# The new color is saved in the highlight list and applied to the main window
# and the highlight dialog's list. NOTE: the color value my be an empty string
# (color "none" refers to the default fore- and background colors)
#
def TagList_UpdateColor(col, pat_idx, is_fg):
  global patlist, dlg_tags_sel

  if pat_idx < len(patlist):
    w = patlist[pat_idx]
    col_idx = 7 if is_fg else 6

    w[col_idx] = col
    try:
      wt.dlg_tags_f1_l.tag_configure(w[4], background=w[6], foreground=w[7])
      # clear selection so that the color becomes visible
      dlg_tags_sel.TextSel_SetSelection([])

      patlist[pat_idx] = w
      UpdateRcAfterIdle()

      wt.f1_t.tag_configure(w[4], background=w[6], foreground=w[7])

      SearchList_CreateHighlightTags()
      MarkList_CreateHighlightTags()
    except Exception as e:
      messagebox.showerror(parent=wt.dlg_srch, message="Failed to update color")


#
# This function is invoked by the "Add current search" entry in the highlight
# list's context menu.
#
def TagList_AddSearch(parent):
  global tlb_find, tlb_regexp, tlb_case
  global dlg_tags_shown, dlg_tags_sel, patlist, fmt_find

  if tlb_find.get() != "":
    # search a free tag index
    dup_idx = -1
    nam_idx = 0
    idx = 0
    for w in patlist:
      tag_idx = int(w[4][3:]) # parse tag name: "tag%d"
      if tag_idx >= nam_idx:
        nam_idx = tag_idx + 1

      if w[0] == tlb_find.get():
        dup_idx = idx

      idx += 1

    answer = "no"
    if dup_idx >= 0:
      answer = messagebox.showwarning(parent=parent, type="yesnocancel",
                 message="The same search expression is already used - overwrite this entry?")
      if answer == "cancel":
        return

    if answer == "no":
      # append new entry
      pat_idx = len(patlist)

      w = [tlb_find.get(), int(tlb_regexp.get()), int(tlb_case.get()), "default", "tag%d" % nam_idx, ""]
      w.extend(fmt_find[6:])
      patlist.append(w)

    else:
      # replace pattern and search options in existing entry
      pat_idx = dup_idx
      w = patlist[pat_idx]
      w[0] = tlb_find.get()
      w[1] = int(tlb_regexp.get())
      w[2] = int(tlb_case.get())

    # add the tag to the main window text widget
    HighlightCreateTags()

    # tag matching lines in the main window
    wt.f1_t.configure(cursor="watch")
    opt = Search_GetOptions(w[0], w[1], w[2])
    HighlightAll(w[0], w[4], opt)
    SearchHighlightClear()

    if dlg_tags_shown:
      # insert the entry into the listbox
      TagList_Fill()
      dlg_tags_sel.TextSel_SetSelection([pat_idx])
      wt.dlg_tags_f1_l.see("%d.0" % (pat_idx + 1))
    else:
      TagList_OpenDialog()

  else:
    messagebox.showerror(parent=parent, message="First enter a search text or regular expression in the main window's \"Find\" field.")


#
# This function is invoked by the "Copy to search field" command in the
# highlight list's context menu.
#
def TagList_CopyToSearch(pat_idx):
  global patlist
  global tlb_find_focus, tlb_find, tlb_regexp, tlb_case

  if pat_idx < len(patlist):
    w = patlist[pat_idx]

    # force focus into find entry field & suppress "Enter" event
    SearchInit()
    tlb_find_focus = 1
    wt.f2_e.focus_set()

    SearchHighlightClear()
    tlb_find.set(w[0])
    tlb_regexp.set(w[1])
    tlb_case.set(w[2])


#
# This function is invoked by the "Update from search field" command in the
# highlight list's context menu.
#
def TagList_CopyFromSearch(pat_idx):
  global tlb_find_focus, tlb_find, tlb_regexp, tlb_case
  global patlist

  if pat_idx < len(patlist):
    answer = messagebox.askokcancel(parent=wt.dlg_tags, message="Please confirm overwriting the search pattern for this entry? This cannot be undone")
    if answer:
      w = patlist[pat_idx]
      w[0] = tlb_find.get()
      w[1] = int(tlb_regexp.get())
      w[2] = int(tlb_case.get())
      UpdateRcAfterIdle()

      # apply the tag to the text content
      wt.f1_t.tag_remove(w[4], "1.0", "end")
      opt = Search_GetOptions(w[0], w[1], w[2])
      HighlightAll(w[0], w[4], opt)

      TagList_DisplayDelete(pat_idx)
      TagList_DisplayInsert(pat_idx, pat_idx)
      dlg_tags_sel.TextSel_SetSelection([pat_idx])
      wt.dlg_tags_f1_l.see("%d.0" % (pat_idx + 1))


#
# This function is invoked by the "Remove entry" command in the highlight
# list's context menu.
#
def TagList_Remove(pat_sel):
  global patlist

  cnt = len(pat_sel)
  if cnt > 0:
    if cnt == 1:
      msg = "Really remove this entry? This cannot be undone"
    else:
      msg = "Really remove all %d selected entries? This cannot be undone" % cnt

    answer = messagebox.askyesno(parent=wt.dlg_tags, message=msg)
    if answer:
      for idx in sorted(set(pat_sel), reverse=True):
        if idx < len(patlist):
          tagname = patlist[idx][4]
          del patlist[idx]

          # remove the highlight in the main window
          wt.f1_t.tag_delete(tagname)

          # remove the highlight in other dialogs, if currently open
          SearchList_DeleteTag(tagname)
          MarkList_DeleteTag(tagname)

      UpdateRcAfterIdle()

      # remove the entry in the listbox
      TagList_Fill()
      dlg_tags_sel.TextSel_SetSelection([])


#
# This function is bound to the "Delete" key in the highlight list.
#
def TagList_RemoveSelection():
  global dlg_tags_sel

  sel = dlg_tags_sel.TextSel_GetSelection()
  if len(sel) > 0:
    TagList_Remove(sel)


# ----------------------------------------------------------------------------
#
# This function creates or raises the font selection dialog.
#
def FontList_OpenDialog():
  global font_normal, font_content, dlg_font_shown
  global dlg_font_fams, dlg_font_size, dlg_font_bold

  PreemptBgTasks()
  if not dlg_font_shown:
    wt.dlg_font = Toplevel(tk)
    wt.dlg_font.wm_title("Font selection")
    wt.dlg_font.wm_group(tk)

    dlg_font_bold = BooleanVar(tk, False)
    dlg_font_size = IntVar(tk, 10)

    # frame #1: listbox with all available fonts
    wt.dlg_font_f1 = Frame(wt.dlg_font)
    wt.dlg_font_f1_fams = Listbox(wt.dlg_font_f1, width=40, height=10, font=font_normal,
                                  exportselection=FALSE, cursor="top_left_arrow", selectmode=BROWSE)
    wt.dlg_font_f1_fams.pack(side=LEFT, fill=BOTH, expand=1)
    wt.dlg_font_f1_sb = Scrollbar(wt.dlg_font_f1, orient=VERTICAL, command=wt.dlg_font_f1_fams.yview, takefocus=0)
    wt.dlg_font_f1_sb.pack(side=LEFT, fill=Y)
    wt.dlg_font_f1_fams.configure(yscrollcommand=wt.dlg_font_f1_sb.set)
    wt.dlg_font_f1.pack(side=TOP, fill=BOTH, expand=1, padx=5, pady=5)
    wt.dlg_font_f1_fams.bind("<<ListboxSelect>>", lambda e: BindCallAndBreak(FontList_Selection))

    # frame #2: size and weight controls
    wt.dlg_font_f2 = Frame(wt.dlg_font)
    wt.dlg_font_f2_lab_sz = Label(wt.dlg_font_f2, text="Font size:")
    wt.dlg_font_f2_lab_sz.pack(side=LEFT)
    wt.dlg_font_f2_sz = Spinbox(wt.dlg_font_f2, from_=1, to=99, width=3, textvariable=dlg_font_size, command=FontList_Selection)
    wt.dlg_font_f2_sz.pack(side=LEFT)
    wt.dlg_font_f2_bold = Checkbutton(wt.dlg_font_f2, text="bold", variable=dlg_font_bold, command=FontList_Selection)
    wt.dlg_font_f2_bold.pack(side=LEFT, padx=15)
    wt.dlg_font_f2.pack(side=TOP, fill=X, padx=5, pady=5)

    # frame #3: demo text
    wt.dlg_font_demo = Text(wt.dlg_font, width=20, height=4, wrap=NONE, exportselection=FALSE, relief=RIDGE, takefocus=0)
    wt.dlg_font_demo.pack(side=TOP, fill=X, padx=15, pady=10)
    wt.dlg_font_demo.bindtags([wt.dlg_font_demo, "TextReadOnly", tk, "all"])

    wt.dlg_font_demo.insert("end", "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n")
    wt.dlg_font_demo.insert("end", "abcdefghijklmnopqrstuvwxyz\n")
    wt.dlg_font_demo.insert("end", "0123456789\n")
    wt.dlg_font_demo.insert("end", "AAA ,,,...---;;;:::___+++=== AAA\n")

    # frame #4: ok/abort buttons
    wt.dlg_font_f3 = Frame(wt.dlg_font)
    wt.dlg_font_f3_abort = Button(wt.dlg_font_f3, text="Abort", command=lambda: FontList_Quit(False))
    wt.dlg_font_f3_ok = Button(wt.dlg_font_f3, text="Ok", default=ACTIVE, command=lambda: FontList_Quit(True))
    wt.dlg_font_f3_abort.pack(side=LEFT, padx=10, pady=5)
    wt.dlg_font_f3_ok.pack(side=LEFT, padx=10, pady=5)
    wt.dlg_font_f3.pack(side=TOP)

    wt.dlg_font_f1.bind("<Destroy>", lambda e: FontList_ClosedDialog())
    wt.dlg_font_f3_ok.bind("<Return>", lambda e: wt.dlg_font_f3_ok.event_generate("<space>"))
    wt.dlg_font.bind("<Escape>", lambda e: wt.dlg_font_f3_abort.event_generate("<space>"))
    wt.dlg_font_f1_fams.focus_set()
    dlg_font_shown = True

    # fill font list and select current font
    FontList_Fill()
    dlg_font_bold.set(font_content.cget("weight") != "normal")
    dlg_font_size.set(font_content.cget("size"))
    cur_fam = font_content.cget("family")
    try:
      idx = dlg_font_fams.index(cur_fam)  # raises ValueError if not found
      wt.dlg_font_f1_fams.activate(idx)
      wt.dlg_font_f1_fams.selection_set(idx)
      wt.dlg_font_f1_fams.see(idx)
    except ValueError:
      pass
    # finally update demo box with the currently selected font
    FontList_Selection()

  else:
    wt.dlg_font.wm_deiconify()
    wt.dlg_font.lift()
    wt.dlg_font_f1_fams.focus_set()

  ResumeBgTasks()


#
# This function is bound to destruction events on the dialog.
# The function marks the dialog as closed.
#
def FontList_ClosedDialog():
    global dlg_font_shown
    dlg_font_shown = False


#
# This function fills the font selection listbox with  list of all
# font families which are available on the system.
#
def FontList_Fill():
  global dlg_font_fams, dlg_font_size, dlg_font_bold

  # remove duplicates, then sort alphabetically
  dlg_font_fams = sorted(set(tkf.families(displayof=tk)))

  for f in dlg_font_fams:
    wt.dlg_font_f1_fams.insert("end", f)


#
# This function is bound to changes of the selection in the font list
# or changes in the size and weight controls.  The function applies
# the selection to the demo text.
#
def FontList_Selection():
  global dlg_font_fams, dlg_font_size, dlg_font_bold

  sel = wt.dlg_font_f1_fams.curselection()
  if (len(sel) == 1) and (sel[0] < len(dlg_font_fams)):
    name = "{%s} %d" % (dlg_font_fams[sel[0]], dlg_font_size.get())
    if dlg_font_bold.get():
      name = name + " bold"

    # note this succeeds even for unknown fonts, so no try/except needed
    wt.dlg_font_demo.configure(font=name)


#
# This function is bound to the "Ok" and "Abort" command buttons.
# In case of OK the function checks and stores the selection.
# In case of abort, the function just closes the dialog.
#
def FontList_Quit(do_store):
  global dlg_font_fams, dlg_font_size, dlg_font_bold
  global font_content

  if do_store:
    sel = wt.dlg_font_f1_fams.curselection()
    if (len(sel) == 1) and (sel[0] < len(dlg_font_fams)):
      font_content.configure(family=dlg_font_fams[sel[0]],
                             size=dlg_font_size.get(),
                             weight=(tkf.BOLD if dlg_font_bold.get() else tkf.NORMAL))

      cerr = ApplyFont()
      if cerr is not None:
        messagebox.showerror(parent=wt.dlg_font, message="Selected font is unavailable: " + cerr)
        return
    else:
      messagebox.showerror(parent=wt.dlg_font,
                           message="No font is selected - Use \"Abort\" to leave without changes.")
      return

  dlg_font_fams = None
  dlg_font_size = None
  dlg_font_bold = None
  wt.dlg_font.destroy()


# ----------------------------------------------------------------------------
#
# This function creates or raises the color highlight edit dialog.
#
def Markup_OpenDialog(pat_idx):
  global font_normal, font_bold, font_content, font_hlink
  global col_bg_content, col_fg_content
  global fmt_selection, dlg_fmt_shown, dlg_fmt
  global patlist, col_palette

  # fail-safety
  if pat_idx >= len(patlist): return
  Markup_InitConfig(pat_idx)

  if not dlg_fmt_shown:
    wt.dlg_fmt = Toplevel(tk)
    wt.dlg_fmt.wm_title("Markup editor")
    wt.dlg_fmt.wm_group(wt.dlg_tags)

    wt.dlg_fmt_epat = Entry(wt.dlg_fmt, width=12, textvariable=dlg_fmt["pat"], exportselection=FALSE,
                            justify=CENTER, font=font_bold, relief=GROOVE, borderwidth=2)
    wt.dlg_fmt_epat.pack(side=TOP, fill=X, expand=1, padx=20, pady=4, anchor="c")
    wt.dlg_fmt_fop = Frame(wt.dlg_fmt)
    wt.dlg_fmt_fop_mcase = Checkbutton(wt.dlg_fmt_fop, text="Match case", variable=dlg_fmt["mcase"], font=font_normal)
    wt.dlg_fmt_fop_regexpt = Checkbutton(wt.dlg_fmt_fop, text="Reg.Exp.", variable=dlg_fmt["regexp"], font=font_normal)
    wt.dlg_fmt_fop_mcase.pack(side=LEFT, padx=2)
    wt.dlg_fmt_fop_regexpt.pack(side=LEFT, padx=2)
    wt.dlg_fmt_fop.pack(side=TOP)

    char_w = font_content.measure(" ")
    wt.dlg_fmt_sample = Text(wt.dlg_fmt, height=5, width=35, font=font_content, wrap=NONE,
                             foreground=col_fg_content, background=col_bg_content, relief=SUNKEN,
                             borderwidth=1, takefocus=0, highlightthickness=0, exportselection=0,
                             insertofftime=0, insertwidth=2*char_w)
    wt.dlg_fmt_sample.pack(side=TOP, padx=5, pady=6)
    wt.dlg_fmt_sample.bindtags([wt.dlg_fmt_sample, "TextReadOnly", wt.dlg_fmt, "all"])

    lh = font_content.metrics("linespace")
    wt.dlg_fmt_sample.tag_configure("spacing", spacing1=lh)
    wt.dlg_fmt_sample.tag_configure("margin", lmargin1=17)
    HighlightConfigure(wt.dlg_fmt_sample, "sel", fmt_selection)
    wt.dlg_fmt_sample.tag_lower("sel")
    wt.dlg_fmt_sample.tag_configure("sample")
    wt.dlg_fmt_sample.insert("1.0", "Text line above\n", ["margin", "spacing"],
                                    "Text sample ... sample text\n", ["margin", "sample"],
                                    "Text line below\n", ["margin"])

    row = 0
    wt.dlg_fmt_mb = Frame(wt.dlg_fmt)
    wt.dlg_fmt_mb_fnt_lab = Label(wt.dlg_fmt_mb, text="Font:")
    wt.dlg_fmt_mb_fnt_lab.grid(sticky=W, column=0, row=row)
    wt.dlg_fmt_mb_fnt_f = Frame(wt.dlg_fmt_mb)
    wt.dlg_fmt_mb_fnt_f_chk_bold = Checkbutton(wt.dlg_fmt_mb_fnt_f, text="bold", variable=dlg_fmt["bold"],
                                               command=Markup_UpdateFormat, font=font_normal)
    wt.dlg_fmt_mb_fnt_f_chk_underline = Checkbutton(wt.dlg_fmt_mb_fnt_f, text="underline", font=font_normal,
                                                    variable=dlg_fmt["underline"], command=Markup_UpdateFormat)
    wt.dlg_fmt_mb_fnt_f_chk_overstrike = Checkbutton(wt.dlg_fmt_mb_fnt_f, text="overstrike", font=font_normal,
                                                    variable=dlg_fmt["overstrike"], command=Markup_UpdateFormat)
    wt.dlg_fmt_mb_fnt_f_chk_bold.pack(side=LEFT, padx=5)
    wt.dlg_fmt_mb_fnt_f_chk_underline.pack(side=LEFT, padx=5)
    wt.dlg_fmt_mb_fnt_f_chk_overstrike.pack(side=LEFT, padx=5)
    wt.dlg_fmt_mb_fnt_f.grid(sticky=W, column=1, row=row, columnspan=4, pady=2)
    row += 1

    wt.dlg_fmt_mb_bg_lab = Label(wt.dlg_fmt_mb, text="Background:")
    wt.dlg_fmt_mb_bg_lab.grid(sticky=W, column=0, row=row)
    wt.dlg_fmt_mb_bgcol_lab = Label(wt.dlg_fmt_mb, text="Color:", font=font_normal)
    wt.dlg_fmt_mb_bgcol_lab.grid(sticky=W, column=1, row=row)
    wt.dlg_fmt_mb_bgcol_mb = Markup_ImageButton(wt.dlg_fmt_mb, "bgcol")
    wt.dlg_fmt_mb_bgcol_mb.wid.grid(sticky=W, column=2, row=row, padx=10, pady=2)
    wt.dlg_fmt_mb_bgpat_lab = Label(wt.dlg_fmt_mb, text="Pattern:", font=font_normal)
    wt.dlg_fmt_mb_bgpat_lab.grid(sticky=W, column=3, row=row)
    wt.dlg_fmt_mb_bgpat_mb = Markup_ImageButton(wt.dlg_fmt_mb, "bgpat")
    wt.dlg_fmt_mb_bgpat_mb.wid.grid(sticky=W, column=4, row=row, padx=10, pady=2)
    row += 1

    wt.dlg_fmt_mb_fgc_lab = Label(wt.dlg_fmt_mb, text="Text:")
    wt.dlg_fmt_mb_fgc_lab.grid(sticky=W, column=0, row=row)
    wt.dlg_fmt_mb_fgcol_lab = Label(wt.dlg_fmt_mb, text="Color:", font=font_normal)
    wt.dlg_fmt_mb_fgcol_lab.grid(sticky=W, column=1, row=row)
    wt.dlg_fmt_mb_fgc_mb = Markup_ImageButton(wt.dlg_fmt_mb, "fgcol")
    wt.dlg_fmt_mb_fgc_mb.wid.grid(sticky=W, column=2, row=row, padx=10, pady=2)
    wt.dlg_fmt_mb_fgpat_lab = Label(wt.dlg_fmt_mb, text="Pattern:", font=font_normal)
    wt.dlg_fmt_mb_fgpat_lab.grid(sticky=W, column=3, row=row)
    wt.dlg_fmt_mb_fgpat_mb = Markup_ImageButton(wt.dlg_fmt_mb, "fgpat")
    wt.dlg_fmt_mb_fgpat_mb.wid.grid(sticky=W, column=4, row=row, padx=10, pady=2)
    row += 1

    wt.dlg_fmt_mb_bgpat_mb.wid_men.add_radiobutton(label="none - 100% filled", value="none",
                                                   variable=dlg_fmt["bgpat"], command=Markup_UpdateFormat)
    for cmd in (75, 50, 25, 12):
      wt.dlg_fmt_mb_bgpat_mb.wid_men.add_radiobutton(compound=LEFT, label="  %d%% filled" % cmd, value="gray%d" % cmd,
                                                     variable=dlg_fmt["bgpat"], bitmap="gray%d" % cmd,
                                                     command=Markup_UpdateFormat)

    wt.dlg_fmt_mb_fgpat_mb.wid_men.add_radiobutton(label="none - 100% filled", value="none",
                                                   variable=dlg_fmt["fgpat"], command=Markup_UpdateFormat)
    for cmd in (75, 50, 25, 12):
      wt.dlg_fmt_mb_fgpat_mb.wid_men.add_radiobutton(compound=LEFT, label="  %d%% filled" % cmd, value="gray%d" % cmd,
                                                     variable=dlg_fmt["fgpat"], bitmap="gray%d" % cmd,
                                                     command=Markup_UpdateFormat)

    wt.dlg_fmt_mb_bd_lab = Label(wt.dlg_fmt_mb, text="Border:")
    wt.dlg_fmt_mb_bd_lab.grid(sticky=W, column=0, row=row)
    wt.dlg_fmt_mb_ref_lab = Label(wt.dlg_fmt_mb, text="Relief:", font=font_normal)
    wt.dlg_fmt_mb_ref_lab.grid(sticky=W, column=1, row=row)
    wt.dlg_fmt_mb_ref_mb = Markup_ImageButton(wt.dlg_fmt_mb, "relief")
    wt.dlg_fmt_mb_ref_mb.wid.grid(sticky=W, column=2, row=row, padx=10, pady=2)

    for cmd in ("flat", "raised", "sunken", "ridge", "groove", "solid"):
      wt.dlg_fmt_mb_ref_mb.wid_men.add_radiobutton(label=cmd, variable=dlg_fmt["relief"], value=cmd,
                                                   command=Markup_UpdateFormat)

    wt.dlg_fmt_mb_bwd_lab = Label(wt.dlg_fmt_mb, text="Width:", font=font_normal)
    wt.dlg_fmt_mb_bwd_lab.grid(sticky=W, column=3, row=row)
    wt.dlg_fmt_mb_bdw_sb = Spinbox(wt.dlg_fmt_mb, from_=1, to=9, width=3, borderwidth=1,
                                   textvariable=dlg_fmt["border"], command=Markup_UpdateFormat)
    wt.dlg_fmt_mb_bdw_sb.bind("<Return>", Markup_UpdateFormat)
    wt.dlg_fmt_mb_bdw_sb.grid(sticky=W, column=4, row=row, padx=10, pady=2)
    row += 1

    wt.dlg_fmt_mb_lsp_lab = Label(wt.dlg_fmt_mb, text="Line spacing:")
    wt.dlg_fmt_mb_lsp_lab.grid(sticky=W, column=0, row=row)
    wt.dlg_fmt_mb_lsp2_lab = Label(wt.dlg_fmt_mb, text="Distance:", font=font_normal)
    wt.dlg_fmt_mb_lsp2_lab.grid(sticky=W, column=3, row=row)
    wt.dlg_fmt_mb_lsp_sb = Spinbox(wt.dlg_fmt_mb, from_=0, to=999, width=3, borderwidth=1,
                                   textvariable=dlg_fmt["spacing"], command=Markup_UpdateFormat)
    wt.dlg_fmt_mb_lsp_sb.bind("<Return>", Markup_UpdateFormat)
    wt.dlg_fmt_mb_lsp_sb.grid(sticky=W, column=4, row=row, padx=10, pady=2)
    row += 1
    wt.dlg_fmt_mb.pack(side=TOP, padx=5, pady=3, anchor=NW)

    wt.dlg_fmt_cop = Button(wt.dlg_fmt, text="Edit color palette...", command=Palette_OpenDialog,
                            borderwidth=0, relief=FLAT, font=font_hlink,
                            foreground="#0000ff", activeforeground="#0000ff", padx=0, pady=0)
    wt.dlg_fmt_cop.pack(side=TOP, anchor=W, padx=5, pady=4)

    wt.dlg_fmt_f2 = Frame(wt.dlg_fmt)
    wt.dlg_fmt_f2_abort = Button(wt.dlg_fmt_f2, text="Abort", command=lambda: Markup_Save(0, 1))
    wt.dlg_fmt_f2_apply = Button(wt.dlg_fmt_f2, text="Apply", command=lambda: Markup_Save(1, 0))
    wt.dlg_fmt_f2_ok = Button(wt.dlg_fmt_f2, text="Ok", default=ACTIVE, command=lambda: Markup_Save(1, 1))
    wt.dlg_fmt_f2_abort.pack(side=LEFT, padx=10, pady=4)
    wt.dlg_fmt_f2_apply.pack(side=LEFT, padx=10, pady=4)
    wt.dlg_fmt_f2_ok.pack(side=LEFT, padx=10, pady=4)
    wt.dlg_fmt_f2.pack(side=TOP)

    dlg_fmt_shown = True
    wt.dlg_fmt_mb.bind("<Destroy>", lambda e: Markup_DestroyCb(), add="+")

  else:
    wt.dlg_fmt.wm_deiconify()
    wt.dlg_fmt.lift()

  Markup_UpdateFormat()
  wt.dlg_fmt_epat.selection_clear()
  wt.dlg_fmt_epat.icursor("end")


#
# This function is bound to destruction events on the dialog.
# The function marks the dialog as closed.
#
def Markup_DestroyCb():
  global dlg_fmt_shown
  dlg_fmt_shown = False

#
# This function is called when the mark-up dialog is opened to copy the
# format parameters from the global patlist into the dialog's hash array.
#
def Markup_InitConfig(pat_idx):
  global patlist, dlg_fmt

  dlg_fmt = {}

  w = patlist[pat_idx]
  dlg_fmt["pat"] = StringVar(tk, w[0])
  dlg_fmt["regexp"] = BooleanVar(tk, w[1])
  dlg_fmt["mcase"] = BooleanVar(tk, w[2])
  dlg_fmt["tagnam"] = w[4] # not passed to Tk
  dlg_fmt["bgcol"] = StringVar(tk, w[6])
  dlg_fmt["fgcol"] = StringVar(tk, w[7])
  dlg_fmt["bold"] = BooleanVar(tk, w[8])
  dlg_fmt["underline"] = BooleanVar(tk, w[9])
  dlg_fmt["overstrike"] = BooleanVar(tk, w[10])
  dlg_fmt["bgpat"] = StringVar(tk, w[11] if w[11] else "none")
  dlg_fmt["fgpat"] = StringVar(tk, w[12] if w[12] else "none")
  dlg_fmt["relief"] = StringVar(tk, w[13] if w[13] else "flat")
  dlg_fmt["border"] = IntVar(tk, w[14])
  dlg_fmt["spacing"] = IntVar(tk, w[15])


#
# This function is called when the mark-up dialog is closed to build a
# parameter list from the dialog's temporary hash array.
#
def Markup_GetConfig(pat_idx):
  global dlg_fmt, patlist

  if pat_idx >= 0:
    w = patlist[pat_idx]
  else:
    w = [""] * 16

  bgpat = dlg_fmt["bgpat"].get()
  fgpat = dlg_fmt["fgpat"].get()
  relief = dlg_fmt["relief"].get()
  border = dlg_fmt["border"].get()
  spacing = dlg_fmt["spacing"].get()
  if bgpat == "none": bgpat = ""
  if fgpat == "none": fgpat = ""
  if relief == "flat": relief = ""
  #if not re.match(r"^\d+$", border): border = 1
  #if not re.match(r"^\d+$", spacing): spacing = 0

  w[0]  = dlg_fmt["pat"].get()
  w[1]  = dlg_fmt["regexp"].get()
  w[2]  = dlg_fmt["mcase"].get()

  w[6]  = dlg_fmt["bgcol"].get()
  w[7]  = dlg_fmt["fgcol"].get()
  w[8]  = dlg_fmt["bold"].get()
  w[9]  = dlg_fmt["underline"].get()
  w[10] = dlg_fmt["overstrike"].get()
  w[11] = bgpat
  w[12] = fgpat
  w[13] = relief
  w[14] = border
  w[15] = spacing

  return w


#
# This function is bound to the "Ok" and "Abort" buttons in the mark-up dialog.
#
def Markup_Save(do_save, do_quit):
  global dlg_fmt, patlist

  if do_save:
    # determine the edited pattern's index in the list (use the unique tag
    # which doesn't change even if the list is reordered)
    pat_idx = -1
    tagnam = dlg_fmt["tagnam"]
    idx = 0
    for w in patlist:
      if w[4] == tagnam:
        pat_idx = idx
        break
      idx += 1

    if pat_idx >= 0:
      old_w = patlist[pat_idx]
      w = Markup_GetConfig(pat_idx)
      patlist[pat_idx] = w
      UpdateRcAfterIdle()

      # update hightlight color listbox
      TagList_Update(pat_idx)

      # update tag in the search result dialog
      SearchList_CreateHighlightTags()
      MarkList_CreateHighlightTags()

      # update tag in the main window
      HighlightConfigure(wt.f1_t, tagnam, w)

      # check if the search pattern changed
      if ((old_w[0] != w[0]) or
          (old_w[1] != w[1]) or
          (old_w[2] != w[2])):

        # remove the tag and re-apply to the text
        wt.f1_t.tag_remove(tagnam, "1.0", "end")

        opt = Search_GetOptions(w[0], w[1], w[2])
        HighlightAll(w[0], tagnam, opt)

    else:
      messagebox.showerror(parent=wt.dlg_fmt, message="This element has already been deleted.")
      return

  if do_quit:
    dlg_fmt = None
    wt.dlg_fmt.destroy()


#
# This function is called whenever a format parameter is changed to update
# the sample text and the control widgets.
#
def Markup_UpdateFormat():
  global dlg_fmt, font_content, col_bg_content, col_fg_content

  HighlightConfigure(wt.dlg_fmt_sample, "sample", Markup_GetConfig(-1))

  if dlg_fmt["relief"].get() != "none":
    wt.dlg_fmt_mb_bdw_sb.configure(state=NORMAL)
  else:
    wt.dlg_fmt_mb_bdw_sb.configure(state=DISABLED)

  # adjust spacing above first line to center the content vertically
  lh = font_content.metrics("linespace")
  spc = lh - dlg_fmt["spacing"].get()
  if spc < 0: spc = 0
  wt.dlg_fmt_sample.tag_configure("spacing", spacing1=spc)

  # update the entry widgets
  if dlg_fmt["bgcol"].get() != "":
    wt.dlg_fmt_mb_bgcol_mb.wid_c.configure(background=dlg_fmt["bgcol"].get())
  else:
    wt.dlg_fmt_mb_bgcol_mb.wid_c.configure(background=col_bg_content)

  if dlg_fmt["fgcol"].get() != "":
    wt.dlg_fmt_mb_fgc_mb.wid_c.configure(background=dlg_fmt["fgcol"].get())
  else:
    wt.dlg_fmt_mb_fgc_mb.wid_c.configure(background=col_fg_content)

  if dlg_fmt["bgpat"].get() != "none":
    wt.dlg_fmt_mb_bgpat_mb.wid_c.itemconfigure("all", bitmap=dlg_fmt["bgpat"].get())
  else:
    wt.dlg_fmt_mb_bgpat_mb.wid_c.itemconfigure("all", bitmap="")

  if dlg_fmt["fgpat"].get() != "none":
    wt.dlg_fmt_mb_fgpat_mb.wid_c.itemconfigure("all", bitmap=dlg_fmt["fgpat"].get())
  else:
    wt.dlg_fmt_mb_fgpat_mb.wid_c.itemconfigure("all", bitmap="")

  wt.dlg_fmt_mb_ref_mb.wid_c_w.configure(relief=dlg_fmt["relief"].get())


#
# This function is used during creation of the markup editor dialog to
# create the widgets for color, pattern and relief selection. The widget
# consists of a rectangle which displays the current choice and a button
# which triggers a popup menu when pressed.
#
class Markup_ImageButton(object):
  def __init__(self, parent, type):
    global dlg_fmt

    CreateButtonBitmap("img_dropdown")
    self.wid = Frame(parent, relief=SUNKEN, borderwidth=1)
    iw = tk.call("image", "width", "img_dropdown") + 4
    ih = tk.call("image", "height", "img_dropdown")
    self.wid_c = Canvas(self.wid, width=iw, height=ih, highlightthickness=0, takefocus=0, borderwidth=0)
    self.wid_c.pack(fill=BOTH, expand=1, side=LEFT)
    self.wid_b = Button(self.wid, image="img_dropdown", highlightthickness=1, borderwidth=1, relief=RAISED)
    self.wid_b.pack(side=LEFT)
    self.wid_men = Menu(self.wid, tearoff=0)

    if type.endswith("col"):
      self.wid_b.configure(command=lambda:Markup_PopupColorPalette(self.wid, type))
    elif type.endswith("pat"):
      self.wid_c.create_bitmap(2, 2, anchor="nw")
      self.wid_b.configure(command=lambda:Markup_PopupPatternMenu(self.wid, self.wid_men))
    elif type == "relief":
      self.wid_c_w = Frame(self.wid_c, width=10, height=10, borderwidth=2, relief=FLAT)
      self.wid_c.create_window(3, 3, anchor="nw", window=self.wid_c_w, width=12, height=12)
      self.wid_b.configure(command=lambda:Markup_PopupPatternMenu(self.wid, self.wid_men))


#
# This helper function is invoked when the "drop down" button is pressed
# on a color selction widget: it opens the color palette menu directly
# below the widget.
#
def Markup_PopupColorPalette(wid, type):
  global dlg_fmt

  rootx = wid.winfo_rootx()
  rooty = wid.winfo_rooty() + wid.winfo_height()
  PaletteMenu_Popup(wt.dlg_fmt, rootx, rooty,
                    lambda col:Markup_UpdateColor(col, type, 0),
                    dlg_fmt[type].get())


#
# This helper function is invoked when the "drop down" button is pressed
# on a pattern selction widget: it opens the associated menu directly
# below the widget.
#
def Markup_PopupPatternMenu(wid, wid_men):
  rootx = wid.winfo_rootx()
  rooty = wid.winfo_rooty() + wid.winfo_height()
  tk.call("tk_popup", wid_men, rootx, rooty, 0)


#
# This helper function is invoked as callback after a color was selected
# in the palette popup menu.
#
def Markup_UpdateColor(col, type, is_fg):
  dlg_fmt[type].set(col)
  Markup_UpdateFormat()


# ----------------------------------------------------------------------------
#
# This function creates or raises the color palette dialog which allows to
# add, delete, modify or reorder colors used for highlighting.
#
def Palette_OpenDialog():
  global font_normal, dlg_cols_shown, dlg_cols_palette, dlg_cols_cid
  global col_palette

  if not dlg_cols_shown:
    wt.dlg_cols = Toplevel(tk)
    wt.dlg_cols.wm_title("Color palette")
    wt.dlg_cols.wm_group(tk)

    msg = "Pre-define a color palette for quick selection\n" \
          "when changing colors. Use the context menu\n" \
          "or drag-and-drop to modify the palette:"
    wt.dlg_cols_lab_hd = Label(wt.dlg_cols, text=msg, font=font_normal, justify=LEFT)
    wt.dlg_cols_lab_hd.pack(side=TOP, anchor=W, pady=5, padx=5)

    wt.dlg_cols_c = Canvas(wt.dlg_cols, background=wt.dlg_cols.cget("background"), cursor="top_left_arrow")
    wt.dlg_cols_c.pack(side=TOP, padx=10, pady=10, anchor=W)

    wt.dlg_cols_c.bind("<ButtonRelease-3>", lambda e: Palette_ContextMenu(e.x, e.y))
    wt.dlg_cols_c.bind("<Destroy>", lambda e: Palette_ClosedDialog(), add="+")
    dlg_cols_shown = True

    wt.dlg_cols_f2 = Frame(wt.dlg_cols)
    wt.dlg_cols_f2_abort = Button(wt.dlg_cols_f2, text="Abort", command=lambda:Palette_Save(0))
    wt.dlg_cols_f2_ok = Button(wt.dlg_cols_f2, text="Ok", default="active", command=lambda:Palette_Save(1))
    wt.dlg_cols_f2_abort.pack(side=LEFT, padx=10, pady=5)
    wt.dlg_cols_f2_ok.pack(side=LEFT, padx=10, pady=5)
    wt.dlg_cols_f2.pack(side=TOP)

    wt.dlg_cols_ctxmen = Menu(wt.dlg_cols, tearoff=0)

    dlg_cols_palette = col_palette
    Palette_Fill(wt.dlg_cols_c, dlg_cols_palette)

  else:
    wt.dlg_cols.wm_deiconify()
    wt.dlg_cols.lift()


#
# This function is bound to destruction events on the dialog.
# The function marks the dialog as closed.
#
def Palette_ClosedDialog():
  global dlg_cols_shown
  dlg_cols_shown = False

#
# This function fills the color palette canvas with rectangles which
# each display one of the currently defined colors. Each rectangle gets
# mouse bindings for a context menu and changing the order of colors.
#
def Palette_Fill(wid, pal, sz=20, sel_cmd=None):
  global dlg_cols_cid

  wid.delete("all")
  dlg_cols_cid = []

  x = 2
  y = 2
  col_idx = 0
  idx = 0
  for col in pal:
    cid = wid.create_rectangle(x, y, x + sz, y + sz,
                               outline="black", fill=col,
                               activeoutline="black", activewidth=2)
    dlg_cols_cid.append(cid)

    if sel_cmd is None:
      wid.tag_bind(cid, "<Double-Button-1>", lambda e, idx=idx, cid=cid: Palette_EditColor(idx, cid))
      wid.tag_bind(cid, "<B1-Motion>", lambda e, idx=idx, cid=cid: Palette_MoveColor(idx, cid, e.x, e.y))
      wid.tag_bind(cid, "<ButtonRelease-1>", lambda e, idx=idx, cid=cid: Palette_MoveColorEnd(idx, cid, e.x, e.y))
    else:
      wid.tag_bind(cid, "<Button-1>", lambda e, col=col: sel_cmd(col))

    x += sz
    col_idx += 1
    if col_idx >= 10:
      y += sz
      x = 2
      col_idx = 0

    idx += 1

  wid.configure(width=(10 * sz + 3+3),
                height=(int((len(pal) + 10-1) / 10) * sz + 2+2))


#
# This function is bound to right mouse clicks on color items.
#
def Palette_ContextMenu(xcoo, ycoo):
  global dlg_cols_palette, dlg_cols_cid

  cid = wt.dlg_cols_c.find("closest", xcoo, ycoo)
  if len(cid) == 1:
    cid = cid[0]
    for idx in range(len(dlg_cols_cid)):
      if dlg_cols_cid[idx] == cid:
        break
    else:
      return

    wt.dlg_cols_ctxmen.delete(0, "end")
    wt.dlg_cols_ctxmen.add_command(label="", background=dlg_cols_palette[idx], state=DISABLED)
    wt.dlg_cols_ctxmen.add_separator()
    wt.dlg_cols_ctxmen.add_command(label="Change this color...", command=lambda:Palette_EditColor(idx, cid))
    wt.dlg_cols_ctxmen.add_command(label="Duplicate this color", command=lambda:Palette_DuplicateColor(idx, cid))
    wt.dlg_cols_ctxmen.add_command(label="Insert new color (white)", command=lambda:Palette_InsertColor(idx, cid))
    wt.dlg_cols_ctxmen.add_separator()
    wt.dlg_cols_ctxmen.add_command(label="Remove this color", command=lambda: Palette_RemoveColor(idx))

    rootx = wt.dlg_cols.winfo_rootx() + xcoo
    rooty = wt.dlg_cols.winfo_rooty() + ycoo
    tk.call("tk_popup", wt.dlg_cols_ctxmen, rootx, rooty, 0)


#
# This function is bound to the "remove this color" menu item in the
# color palette context menu.
#
def Palette_RemoveColor(idx):
  global dlg_cols_palette

  if idx < len(dlg_cols_palette):
    del dlg_cols_palette[idx]
    Palette_Fill(wt.dlg_cols_c, dlg_cols_palette)


#
# This function is bound to the "insert new color" menu item in the
# color palette entries. It inserts an white color entry at the mouse
# pointer position.
#
def Palette_InsertColor(idx, cid):
  global dlg_cols_palette

  dlg_cols_palette.insert(idx, "#ffffff")
  Palette_Fill(wt.dlg_cols_c, dlg_cols_palette)


def Palette_DuplicateColor(idx, cid):
  global dlg_cols_palette

  if idx < len(dlg_cols_palette):
    col = dlg_cols_palette[idx]
    dlg_cols_palette.insert(idx, col)
    Palette_Fill(wt.dlg_cols_c, dlg_cols_palette)


#
# This function is bound to the "edit this color" menu item in the
# color palette context menu.
#
def Palette_EditColor(idx, cid):
  global dlg_cols_palette

  col = dlg_cols_palette[idx]
  col = colorchooser.askcolor(initialcolor=col, parent=wt.dlg_cols, title="Select color")
  if col is not None:
    col = col[1]
    dlg_cols_palette[idx] = col
    wt.dlg_cols_c.itemconfigure(cid, fill=col)


#
# This function is bound to motion events on color palette entries while
# the left mouse button is helt down.
#
def Palette_MoveColor(idx, cid, xcoo, ycoo):
  sz = 20
  sz_2 = 0 - (sz /2)
  xcoo += sz_2
  ycoo += sz_2
  wt.dlg_cols_c.tag_raise(cid)
  wt.dlg_cols_c.coords(xcoo + sz, ycoo + sz)


#
# This function is bound to the mouse button release event on color palette
# entries. It's used to change the order of colors by drag-and-drop.
#
def Palette_MoveColorEnd(idx, cid, xcoo, ycoo):
  global dlg_cols_palette

  sz = 20
  xcoo -= 2
  ycoo -= 2
  col_idx = 0 if xcoo < 0 else xcoo // sz
  row_idx = 0 if ycoo < 0 else ycoo // sz

  new_idx = (row_idx * 10) + col_idx
  col = dlg_cols_palette[idx]
  del dlg_cols_palette[idx]
  if new_idx < len(dlg_cols_palette):
    dlg_cols_palette.insert(new_idx, col)
  else:
    dlg_cols_palette.append(col)

  Palette_Fill(wt.dlg_cols_c, dlg_cols_palette)


#
# This function is bound to the "ok" and "abort" buttons. Ths function
# closes the color palette dialog. In case of "ok" the edited palette
# is stored.
#
def Palette_Save(do_save):
  global col_palette, dlg_cols_palette, dlg_cols_cid

  if do_save:
    col_palette = dlg_cols_palette
    UpdateRcAfterIdle()

  dlg_cols_palette = None
  dlg_cols_cid = None
  wt.dlg_cols.destroy()


#
# This function creates a menu with all the colors. It's usually used as
# sub-menu (i.e. cascade) in other menus.
#
def PaletteMenu_Popup(parent, rootx, rooty, cmd, col_def):
  global col_palette, font_hlink

  wt.colsel = Toplevel(tk, highlightthickness=0)
  wt.colsel.wm_title("Color selection menu")
  wt.colsel.wm_transient(parent)
  wt.colsel.wm_geometry("+%d+%d" % (rootx, rooty))
  wt.colsel.wm_resizable(0, 0)

  wt.colsel_c = Canvas(wt.colsel, background=wt.colsel.cget("background"),
                       cursor="top_left_arrow", highlightthickness=0)
  wt.colsel_c.pack(side=TOP)

  wt.colsel_f1 = Frame(wt.colsel)
  wt.colsel_f1_b_other = Button(wt.colsel_f1, text="Other...",
                         command=lambda:PaletteMenu_OtherColor(parent, cmd, col_def),
                         borderwidth=0, relief=FLAT, font=font_hlink,
                         foreground="#0000ff", activeforeground="#0000ff", padx=0, pady=0)
  wt.colsel_f1_b_other.pack(side=LEFT, expand=1, anchor=W)
  wt.colsel_f1_b_none = Button(wt.colsel_f1, text="None", command=lambda:cmd(""),
                               borderwidth=0, relief=FLAT, font=font_hlink,
                               foreground="#0000ff", activeforeground="#0000ff", padx=0, pady=0)
  wt.colsel_f1_b_none.pack(side=LEFT, expand=1, anchor=E)
  wt.colsel_f1.pack(side=TOP, fill=X, expand=1)

  Palette_Fill(wt.colsel_c, col_palette, 15, cmd)

  wt.colsel.bind("<ButtonRelease-1>", lambda e: wt.colsel.destroy())
  wt.colsel_c.focus_set()
  wt.colsel.grab_set()


#
# This helper function is bound to "Other..." in the palette popup menu.
# This function opens the color editor and returns the selected color to
# the owner of the palette popup, if any.
#
def PaletteMenu_OtherColor(parent_wid, cmd, col_def):
  wt.colsel.destroy()
  if col_def == "":
    col_def = "#000000"

  # result: ((r, g, b), color_object)
  col = colorchooser.askcolor(initialcolor=col_def, parent=parent_wid, title="Select color")
  if col[1] is not None:
    cmd(col[1])


# ----------------------------------------------------------------------------
#
# This function creates the "About" dialog with copyleft info
#
def OpenAboutDialog():
  global font_normal, font_bold

  PreemptBgTasks()
  if not wt_exists(wt.about):
    wt.about = Toplevel(tk)
    wt.about.wm_title("About")
    wt.about.wm_group(tk)
    wt.about.wm_transient(tk)
    wt.about.wm_resizable(1, 1)

    wt.about_name = Label(wt.about, text="Trace Browser", font=font_bold)
    wt.about_name.pack(side=TOP, padx=5, pady=5)

    wt.about_copyr1 = Label(wt.about, text="Version 2.1\n"
                                           "Copyright (C) 2007-2010,2019-2020,2023 T. Zoerner")
    wt.about_copyr1.pack(side=TOP, padx=5)

    url = "https://github.com/tomzox/trowser"
    wt.about_url = Label(wt.about, text=url, fg="blue", cursor="top_left_arrow")
    wt.about_url.pack(side=TOP, padx=5, pady=5)

    msg ="""
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.  

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""
    wt.about_m = Message(wt.about, font=font_normal, text=msg)
    wt.about_m.pack(side=TOP)
    wt.about_dismiss = Button(wt.about, text="Close", command=wt.about.destroy)
    wt.about_dismiss.pack(padx=5, pady=10)

    wt.about_dismiss.bind("<Return>", lambda e: wt.about_dismiss.event_generate("<space>"))
    wt.about_dismiss.bind("<Escape>", lambda e: wt.about_dismiss.event_generate("<space>"))
    wt.about_url.bind("<ButtonRelease-1>", lambda e: TextSel_XselectionExport(True, url))
    wt.about_dismiss.focus_set()

  else:
    wt.about.wm_deiconify()
    wt.about_dismiss.focus_set()
    wt.about.lift()

  ResumeBgTasks()


# ----------------------------------------------------------------------------
#
# The following class allows using a text widget in the way of a listbox, i.e.
# allowing to select one or more lines. The mouse bindings are similar to the
# listbox "extended" mode. The cursor key bindings differ from the listbox, as
# there is no "active" element (i.e. there's no separate cursor from the
# selection.)
#
# Member variables:
# - text widget whose selection is managed by the class instance
# - callback to invoke after selection changes
# - callback which provides the content list length
# - ID of "after" event handler while scrolling via mouse, or None
# - scrolling speed
# - anchor element index OR last selection cursor pos
# - list of indices of selected lines (starting at zero)
#
class TextSel(object):
  #
  # This constructor is called after a text widget is created for initializing
  # all member variables and for adding key and mouse event bindings for
  # handling the selection.
  #
  def __init__(self, wid, cb_proc, len_proc, mode):
    self.wid = wid
    self.cb_proc = cb_proc
    self.len_proc = len_proc
    self.scroll_tid = None
    self.scroll_speed = 0
    self.anchor_idx = -1
    self.sel = []

    self.wid.bind("<Control-ButtonPress-1>", lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_Pick(e.x, e.y)))
    self.wid.bind("<Shift-ButtonPress-1>", lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_Resize(e.x, e.y)))
    self.wid.bind("<ButtonPress-1>",   lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_Button(e.x, e.y)))
    self.wid.bind("<ButtonRelease-1>", lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_MotionEnd()))
    if mode == "browse":
      self.wid.bind("<B1-Motion>",    lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_Button(e.x, e.y)))
    else:
      self.wid.bind("<B1-Motion>",    lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_Motion(e.x, e.y)))

    self.wid.bind("<Shift-Key-Up>",   lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyResize(-1)))
    self.wid.bind("<Shift-Key-Down>", lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyResize(1)))
    self.wid.bind("<Key-Up>",         lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyUpDown(-1)))
    self.wid.bind("<Key-Down>",       lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyUpDown(1)))
    self.wid.bind("<Shift-Key-Home>", lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyHomeEnd(0, 1)))
    self.wid.bind("<Shift-Key-End>",  lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyHomeEnd(1, 1)))
    self.wid.bind("<Key-Home>",       lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyHomeEnd(0, 0)))
    self.wid.bind("<Key-End>",        lambda e, self=self: BindCallAndBreak(lambda: self.TextSel_KeyHomeEnd(1, 0)))

    #KeyBinding_UpDown(self.wid)
    #KeyBinding_LeftRight(self.wid)
    self.wid.bind("<FocusIn>", lambda e, self=self: KeyClr())
    #bind $wid <Return> "if {\[KeyCmd $wid Return\]} break"
    self.wid.bind("<KeyPress>", lambda e, self=self: "break" if KeyCmd(self.wid, e.char) else None)
    self.wid.bind("<Control-Key-a>", lambda e: BindCallAndBreak(lambda: self.TextSel_SelectAll()))
    self.wid.bind("<Control-Key-c>", lambda e: BindCallAndBreak(lambda: self.TextSel_CopyClipboard(1)))

    self.wid.bind("<Key-Prior>", lambda e, self=self: self.TextSel_SetSelection([]))
    self.wid.bind("<Key-Next>",  lambda e, self=self: self.TextSel_SetSelection([]))


  #
  # This is an interface function which allows outside users to retrieve a
  # list of selected elements (i.e. a list of indices)
  #
  def TextSel_GetSelection(self):
    return self.sel


  #
  # This is an interface function which allows to modify the selection
  # externally.
  #
  def TextSel_SetSelection(self, sel, do_callback=True):
    if len(sel) > 0:
      self.anchor_idx = sel[0]
    self.sel = sel

    self.TextSel_ShowSelection()

    if do_callback:
      self.cb_proc(self.sel)


  #
  # This is an interface function which is used by context menus to check
  # if the item under the mouse pointer is included in the selection.
  # If not, the selection is set to consist only of the pointed item.
  #
  def TextSel_ContextSelection(self, xcoo, ycoo):
    line = self.TextSel_Coo2Line(xcoo, ycoo)
    if line != -1:
      if len(self.sel) != 0:
        if not line in self.sel:
          # click was outside the current selection -> replace selection
          self.TextSel_SetSelection([line])
      else:
        # nothing selected yet -> select element under the mouse pointer
        self.TextSel_SetSelection([line])
    else:
      self.TextSel_SetSelection([])


  #
  # This function is bound to button-press events in the text widget while
  # neither Control nor Shift keys are pressed.  A previous selection is
  # is cleared and the entry below the mouse (if any) is selected.
  #
  def TextSel_Button(self, xcoo, ycoo):
    line = self.TextSel_Coo2Line(xcoo, ycoo)
    old_sel = self.sel
    if (line != -1) and (line < self.len_proc()):
      # select the entry under the mouse pointer
      self.anchor_idx = line
      self.sel = [line]
      notify = 1
    else:
      # mouse pointer is not above a list entry -> clear selection
      self.sel = []
      notify = 0

    # update display if the selection changed
    if old_sel != self.sel:
      self.TextSel_ShowSelection()
      notify = 1

    # invoke notification callback if an element was selected or de-selected
    if notify:
      self.cb_proc(self.sel)

    self.wid.focus_set()


  #
  # This function is bound to mouse pointer motion events in the text widget
  # while the mouse button is pressed down. This allows changing the extent
  # of the selection. The originally selected item ("anchor") always remains
  # selected.  If the pointer is moved above or below the widget borders,
  # the text is scrolled.
  #
  def TextSel_Motion(self, xcoo, ycoo):
    # the anchor element is the one above which the mouse button was pressed
    # (the check here is for fail-safety only, should always be fulfilled)
    if self.anchor_idx >= 0:
      wh = self.wid.winfo_height()
      # check if the mouse is still inside of the widget area
      if (ycoo >= 0) and (ycoo < wh):
        # identify the item under the mouse pointer
        line = self.TextSel_Coo2Line(xcoo, ycoo)
        if line != -1:
          # build list of all consecutive indices between the anchor and the mouse position
          sel = TextSel.IdxRange(self.anchor_idx, line)
          # update display and invoke notification callback if the selection changed
          if sel != self.sel:
            self.sel = sel
            self.TextSel_ShowSelection()
            self.cb_proc(self.sel)

        # cancel scrolling timer, as the mouse is now back inside the widget
        if self.scroll_tid is not None:
          tk.after_cancel(self.scroll_tid)
          self.scroll_tid = None

      else:
        # mouse is outside of the text widget - start scrolling
        # scrolling speed is determined by how far the mouse is outside
        fh = tk.call("font", "metrics", self.wid.cget("font"), "-linespace")
        if ycoo < 0:
          delta = 0 - ycoo
        else:
          delta = ycoo - wh

        delay = 500 - delta * 100 // fh
        if (delay > 500): delay = 500
        if (delay <  50): delay =  50
        if self.scroll_tid is None:
          # start timer and remember it's ID to be able to cancel it later
          delta = -1 if (ycoo < 0) else 1
          self.scroll_tid = tk.after(delay, lambda: self.TextSel_MotionScroll(delta))
          self.scroll_delay = delay
        else:
          # timer already active - just update the delay
          self.scroll_delay = delay


  #
  # This timer event handler is activated when the mouse is moved outside of
  # the text widget while the mouse button is pressed. The handler re-installs
  # itself and is only stopped when the button is released or the mouse is
  # moved back inside the widget area.  The function invariably scrolls the
  # text by one line. Scrolling speed is varied by means of the delay time.
  #
  def TextSel_MotionScroll(self, delta):
    # scroll up or down by one line
    self.wid.yview_scroll(delta, "units")

    # extend the selection to the end of the viewable area
    if delta < 0:
      self.TextSel_Motion(0, 0)
    else:
      self.TextSel_Motion(0, self.wid.winfo_height() - 1)

    # install the timer again (possibly with a changed delay if the mouse was moved)
    self.scroll_tid = tk.after(self.scroll_speed, lambda: self.TextSel_MotionScroll(delta))


  #
  # This function is boud to mouse button release events and stops a
  # possible on-going scrolling timer.
  #
  def TextSel_MotionEnd(self):
    if self.scroll_tid is not None:
      tk.after_cancel(self.scroll_tid)
      self.scroll_tid = None


  #
  # This function is bound to mouse button events while the Control key is
  # pressed. The item below the mouse pointer is toggled in the selection.
  # Otherwise the selection is left unchanged.  Note this operation always
  # clears the "anchor" element, i.e. the selection cannot be modified
  # using "Shift-Click" afterwards.
  #
  def TextSel_Pick(self, xcoo, ycoo):
    line = self.TextSel_Coo2Line(xcoo, ycoo)
    if line != -1:
      # check if the item is already selected
      try:
        pick_idx = self.sel.index(line)
        # already selected -> remove from selection
        del self.sel[pick_idx]
      except:
        pick_idx = -1
        self.sel.append(line)

      if len(self.sel) <= 1:
        self.anchor_idx = line

      self.TextSel_ShowSelection()
      self.cb_proc(self.sel)


  #
  # This function is bound to mouse button events while the Shift key is
  # pressed. The selection is changed to cover all items starting at the
  # anchor item and the item under the mouse pointer.  If no anchor is
  # defined, the selection is reset and only the item under the mouse is
  # selected.
  #
  def TextSel_Resize(self, xcoo, ycoo):
    line = self.TextSel_Coo2Line(xcoo, ycoo)
    if line != -1:
      if self.anchor_idx != -1:
        self.sel = TextSel.IdxRange(self.anchor_idx, line)
        self.TextSel_ShowSelection()
        self.cb_proc(self.sel)
      else:
        self.TextSel_Button(xcoo, ycoo)


  #
  # This function is bound to the up/down cursor keys. If no selection
  # exists, the viewable first item in cursor direction is selected.
  # If a selection exists, it's cleared and the item next to the
  # previous selection in cursor direction is selected.
  #
  def TextSel_KeyUpDown(self, delta):
    content_len = self.len_proc()
    if content_len > 0:
      sel = sorted(self.sel)
      if len(sel) != 0:
        # selection already exists -> determine item below or above
        if delta < 0:
          line = sel[0]
        else:
          line = sel[-1]

        # determine the newly selected item
        line += delta

        if (line >= 0) and (line < content_len):
          # set selection on the new line
          self.anchor_idx = line
          self.sel = [line]

          self.TextSel_ShowSelection()
          self.wid.see("%d.0" % (line + 1))
          self.cb_proc(self.sel)

        elif len(sel) > 1:
          # selection already includes last line - restrict selection to this single line
          if delta < 0:
            line = 0
          else:
            line = content_len - 1

          self.anchor_idx = line
          self.sel = [line]

          self.TextSel_ShowSelection()
          self.wid.see("%d.0" % (line + 1))
          self.cb_proc(self.sel)

      else:
        # no selection exists yet -> use last anchor, or top/bottom visible line
        if ((self.anchor_idx >= 0) and
            (self.wid.bbox("%d.0" % (self.anchor_idx + 1)) is not None)):
          idx = "%d.0" % (self.anchor_idx + 1)
        else:
          if delta > 0:
            idx = "{@1,1}"
          else:
            idx = "@1,%d" % (self.wid.winfo_height() - 1)

        pos = self.wid.index(idx)
        if pos != "":
          line = int(pos.split(".")[0])
          if line > 0:
            line -= 1
            if line >= content_len: line = content_len - 1
            self.anchor_idx = line
            self.sel = [line]

            self.TextSel_ShowSelection()
            self.wid.see("%d.0" % (line + 1))
            self.cb_proc(self.sel)


  #
  # This function is bound to the up/down cursor keys while the Shift key
  # is pressed. The selection is changed to cover all items starting at the
  # anchor item and the next item above or below the current selection.
  #
  def TextSel_KeyResize(self, delta):
    content_len = self.len_proc()
    if len(self.sel) > 0:
      sel = sorted(self.sel)
      # decide if we manipulate the upper or lower end of the selection:
      # use the opposite side of the anchor element
      if self.anchor_idx == sel[-1]:
        line = sel[0]
      else:
        line = sel[-1]

      line += delta
      if (line >= 0) and (line < content_len):
        self.sel = TextSel.IdxRange(self.anchor_idx, line)

        self.TextSel_ShowSelection()
        self.wid.see("%d.0" % (line + 1))
        self.cb_proc(self.sel)

    else:
      self.TextSel_KeyUpDown(delta)


  #
  # This function is bound to the "Home" and "End" keys.  While the Shift
  # key is not pressed, the first or last element in the list are selected.
  # If the Shift key is pressed, the selection is extended to include all
  # items between the anchor and the first or last item.
  #
  def TextSel_KeyHomeEnd(self, is_end, is_resize):
    content_len = self.len_proc()
    if content_len > 0:
      if is_end:
        line = content_len - 1
      else:
        line = 0

      if is_resize == 0:
        self.anchor_idx = line
        self.sel = [line]
      else:
        if self.anchor_idx >= 0:
          self.sel = TextSel.IdxRange(self.anchor_idx, line)

      self.TextSel_ShowSelection()
      self.wid.see("%d.0" % (line + 1))
      self.cb_proc(self.sel)


  #
  # This function is bound to the "CTRL-A" key to select all entries in
  # the list.
  #
  def TextSel_SelectAll(self):
    content_len = self.len_proc()
    if content_len > 0:
      self.sel = TextSel.IdxRange(0, content_len - 1)

      self.TextSel_ShowSelection()
      self.cb_proc(self.sel)


  #
  # This helper function is used to build a list of all indices between
  # (and including) two given values in increasing order.
  #
  def IdxRange(start, end):
    if start > end:
      return list(range(end, start + 1))
    else:
      return list(range(start, end + 1))


  #
  # This function displays a selection in the text widget by adding the
  # "sel" tag to all selected lines.  (Note the view is not affected, i.e.
  # the selection may be outside of the viewable area.)
  #
  def TextSel_ShowSelection(self):
    # first remove any existing highlight
    self.wid.tag_remove("sel", "1.0", "end")

    # select each selected line (may be non-consecutive)
    for line in self.sel:
      self.wid.tag_add("sel", "%d.0" % (line + 1), "%d.0" % (line + 2))

    if (len(self.sel) == 0) or (self.anchor_idx == -1):
      self.wid.mark_set("insert", "end")
    else:
      self.wid.mark_set("insert", "%d.0" % (self.anchor_idx + 1))


  #
  # This function determines the line under the mouse pointer.
  # If the pointer is not above a content line, -1 is returned.
  #
  def TextSel_Coo2Line(self, xcoo, ycoo):
    pos = self.wid.index("@%d,%d" % (xcoo,ycoo))
    if pos != "":
      line = int(pos.split(".")[0]) - 1
      if (line >= 0) and (line < self.len_proc()):
        return line
    return -1


  #
  # This function has to be called when an item has been inserted into the
  # list to adapt the selection: Indices following the insertion are
  # incremented.  The new element is not included in the selection.
  #
  def TextSel_AdjustInsert(self, line):
    self.sel = [(x if x < line else x+1) for x in self.sel]

    if self.anchor_idx >= line:
      self.anchor_idx += 1


  #
  # This function has to be called when an item has been deleted from the
  # list (asynchronously, i.e. not via a command related to the selection)
  # to adapt the list of selected lines: The deleted line is removed from
  # the selection (if included) and following indices are decremented.
  #
  def TextSel_AdjustDeletion(self, line):
    self.sel = [(x if x < line else x-1) for x in self.sel if x != line]

    if self.anchor_idx > line:
      self.anchor_idx -= 1

  #
  # This handler is bound to CTRL-C in the selection and performs <<Copy>>
  # (i.e. copies the content of all selected lines to the clipboard.)
  #
  def TextSel_CopyClipboard(self, to_clipboard):
    msg = "".join([self.wid.get("%d.0" % (line + 1), "%d.0" % (line + 2)) for line in self.sel])

    TextSel_XselectionExport(to_clipboard, msg)


#
# This helper function is installed as "selection handler" on a dummy widget in
# the main window. The function simply returns a text that was previously
# stored for export via the selection. After storing a new text the selection
# must be set to be "owned" by the dummy widget, so that it gets querues by the
# X window system.
#
def TextSel_XselectionHandler(off, xlen):
  global main_selection_txt
  try:
      off = int(off)
      xlen = int(xlen)
      return main_selection_txt[off : (off + xlen)]
  except:
      return ""


#
# This function can be called to copy the given text to the clipboard (from
# where it can by retrieved by other applications, usually upon "paste"
# commands by the user) and X selection mechanism (from where the user can
# paste it via click with the middle mouse button).
#
def TextSel_XselectionExport(to_clipboard, str):
  global main_selection_txt

  # update X selection
  main_selection_txt = str
  wt.xselection.selection_own()

  if to_clipboard:
    tk.clipboard_clear()
    tk.clipboard_append(str)


# ----------------------------------------------------------------------------

class LoadPipe(object):
  def __init__(self):
    global load_file_mode

    self._opt_file_close = IntVar(tk, 0)             # option configurable via dlg.
    self._opt_file_mode = IntVar(tk, load_file_mode) # copy of cfg. opt. for display
    self._dlg_read_total = IntVar(tk, 0)     # copy of _read_total for display
    self._dlg_read_buffered = IntVar(tk, 0)  # copy of _read_buffered for display
    self._dlg_file_limit = IntVar(tk, (load_buf_size + (1024*1024-1)) // (1024*1024))

    self._read_total = 0        # number of bytes read from file; may grow beyond 32 bit!
    self._read_buffered = 0     # number of bytes read & still in buffer
    self._file_data = []        # buffer for data read from file
    self._file_complete = ""    # buffer for reporting error messages from background thread
    self.is_eof = False         # True once file.read() returned EOF

    self._thr_ctrl = 0          # thread request: 0=load; 1=suspend; 2=exit
    self._thr_inst = None       # threading object of background thread
    self._thr_lock = threading.Lock()
    self._thr_cv = threading.Condition(self._thr_lock)
    self._tid_bg_poll = None    # tk.after ID for polling bg thread status changes
    self._stat_upd_ind = 0
    self._stat_upd_cnf = 0
    self._ctrl_upd_ind = 0
    self._ctrl_upd_cnf = 0


  #
  # This function opens the "Loading from STDIN" status dialog.
  #
  def LoadPipe_OpenDialog(self):
    global font_normal, dlg_load_shown

    if not dlg_load_shown:
      wt.dlg_load = Toplevel(tk)
      wt.dlg_load.wm_title("Loading from STDIN...")
      wt.dlg_load.wm_group(tk)
      wt.dlg_load.wm_transient(tk)
      xcoo = wt.f1_t.winfo_rootx() + 50


      ycoo = wt.f1_t.winfo_rooty() + 50
      wt.dlg_load.wm_geometry("+%d+%d" % (xcoo, ycoo))

      wt.dlg_load_f1 = Frame(wt.dlg_load)
      row = 0
      wt.dlg_load_f1_lab_total = Label(wt.dlg_load_f1, text="Loaded data:")
      wt.dlg_load_f1_lab_total.grid(sticky=W, column=0, row=row)
      wt.dlg_load_f1_val_total = Label(wt.dlg_load_f1, textvariable=self._dlg_read_total, font=font_normal)
      wt.dlg_load_f1_val_total.grid(sticky=W, column=1, row=row, columnspan=2)
      row += 1
      wt.dlg_load_f1_lab_bufil = Label(wt.dlg_load_f1, text="Buffered data:")
      wt.dlg_load_f1_lab_bufil.grid(sticky=W, column=0, row=row)
      wt.dlg_load_f1_val_bufil = Label(wt.dlg_load_f1, textvariable=self._dlg_read_buffered, font=font_normal)
      wt.dlg_load_f1_val_bufil.grid(sticky=W, column=1, row=row, columnspan=2)
      row += 1

      wt.dlg_load_f1_lab_bufsz = Label(wt.dlg_load_f1, text="Buffer size:")
      wt.dlg_load_f1_lab_bufsz.grid(sticky=W, column=0, row=row)
      wt.dlg_load_f1_f11 = Frame(wt.dlg_load_f1)
      wt.dlg_load_f1_f11_val_bufsz = Spinbox(wt.dlg_load_f1_f11, from_=1, to=999, width=4, textvariable=self._dlg_file_limit)
      wt.dlg_load_f1_f11_val_bufsz.pack(side=LEFT)
      wt.dlg_load_f1_f11_lab_bufmb = Label(wt.dlg_load_f1_f11, text="MByte", font=font_normal)
      wt.dlg_load_f1_f11_lab_bufmb.pack(side=LEFT, pady=5)
      wt.dlg_load_f1_f11.grid(sticky=W, column=1, row=row, columnspan=2)
      row += 1

      wt.dlg_load_f1_lab_mode = Label(wt.dlg_load_f1, text="Mode:")
      wt.dlg_load_f1_lab_mode.grid(sticky=W, column=0, row=row)
      wt.dlg_load_f1_ohead = Radiobutton(wt.dlg_load_f1, text="head", variable=self._opt_file_mode, value=0)
      wt.dlg_load_f1_ohead.grid(sticky=W, column=1, row=row)
      wt.dlg_load_f1_otail = Radiobutton(wt.dlg_load_f1, text="tail", variable=self._opt_file_mode, value=1)
      wt.dlg_load_f1_otail.grid(sticky=W, column=2, row=row)
      wt.dlg_load_f1.pack(side=TOP, padx=5, pady=5)
      row += 1

      #wt.dlg_load_f1_lab_close = Label(wt.dlg_load_f1, text="Close file:")
      #wt.dlg_load_f1_lab_close.grid(sticky=W, column=0, row=row)
      #wt.dlg_load_f1_val_close = Checkbutton(wt.dlg_load_f1, variable=self._opt_file_close, text="close after read")
      #wt.dlg_load_f1_val_close.grid(sticky=W, column=1, row=row, columnspan=2)
      #row += 1

      wt.dlg_load_cmd = Frame(wt.dlg_load)
      # Note button texts are modified to Abort/Ok while reading from file is stopped
      wt.dlg_load_cmd_stop = Button(wt.dlg_load_cmd, text="Stop", state=NORMAL)
      wt.dlg_load_cmd_stop.pack(side=LEFT, padx=10)
      wt.dlg_load_cmd_ok = Button(wt.dlg_load_cmd, text="Ok", state=DISABLED)
      wt.dlg_load_cmd_ok.pack(side=LEFT, padx=10)
      wt.dlg_load_cmd.pack(side=TOP, pady=5)

      dlg_load_shown = True
      wt.dlg_load_cmd.bind("<Destroy>", lambda e:self.LoadPipe_DialogCloseCb(), add="+")
      #wt.dlg_load.wm_protocol("WM_DELETE_WINDOW", None)


  def LoadPipe_DialogCloseCb(self):
    global dlg_load_shown
    dlg_load_shown = False

  #
  # This function updates the command buttons after starting or stopping input
  #
  def LoadPipe_DialogConfigure(self, is_read_stopped):
    if is_read_stopped:
      wt.dlg_load_cmd_stop.configure(text="Abort", command=lambda:self.LoadPipe_CmdAbort())
      wt.dlg_load_cmd_ok.configure(state=NORMAL, command=lambda: self.LoadPipe_CmdContinue())

      # widen grab from "Stop" button to complete dialog for allowing the user to modify settings
      wt.dlg_load.grab_set()
    else:
      wt.dlg_load_cmd_stop.configure(text="Stop", command=lambda:self.LoadPipe_CmdStop())
      wt.dlg_load_cmd_ok.configure(state=DISABLED)

      # prohibit modifications of settings; allow the "Stop" button only
      wt.dlg_load_cmd_stop.grab_set()


  #
  # This function is bound to the "Abort" button in the "Load from pipe" dialog
  # (note this button is shown only while loading is stopped; else it is
  # replaced by "Stop".) The function closes the dialog. Note: data that
  # already has been loaded is kept and displayed.
  #
  def LoadPipe_CmdAbort(self):
    self.LoadPipe_Insert(False)


  #
  # This function is bound to the "Stop" button in the "Load from pipe"
  # dialog (note this button replaces "Abort" while loading is ongoing.)
  # The function temporarily suspends loading to allow the user to change
  # settings or abort loading.
  #
  def LoadPipe_CmdStop(self):
    # switch buttons in the dialog
    self.LoadPipe_DialogConfigure(True)
    with self._thr_lock:
      self._thr_ctrl = 1
      self._thr_cv.notify()


  #
  # This function is bound to the "Ok" button in the "Load from pipe"
  #
  def LoadPipe_CmdContinue(self):
    global load_buf_size, load_file_mode

    # apply possible change of buffer mode and limit by the user
    load_file_mode = self._opt_file_mode.get()
    try:
      val = 1024*1024 * int(self._dlg_file_limit.get())
    except:
      tk.after_idle(lambda:messagebox.showerror(parent=tk, message="Buffer size is not a number: " + self._dlg_file_limit.get()))
      return

    if abs(val - load_buf_size) >= 1024*1024:
      load_buf_size = val
      UpdateRcAfterIdle()

    if (self._opt_file_mode.get() == 0) and (self._read_buffered >= load_buf_size):
      # "head" mode confirmed by user and buffer is full -> close the dialog
      self.LoadPipe_Insert(False)
    else:
      self.LoadPipe_DialogConfigure(False)

      # create the reader thread again to resume loading data
      with self._thr_lock:
        self._thr_ctrl = 0
        self._thr_cv.notify()


  #
  # This function discards data in the load buffer queue if the length
  # limit is exceeded.  The buffer queue is an array of character strings
  # (each string the result of a "read" command.)  The function is called
  # after each read in tail mode, so it must be efficient (i.e. esp. avoid
  # copying large buffers.)
  #
  def LoadPipe_LimitData(self, exact):
    global load_buf_size

    # tail mode: delete oldest data / head mode: delete newest data
    if load_file_mode == 0:
      lidx = -1
    else:
      lidx = 0

    # calculate how much data must be discarded
    rest = self._read_buffered - load_buf_size

    # unhook complete data buffers from the queue
    while ((rest > 0) and
           (len(self._file_data) > 0) and
           (len(self._file_data[lidx]) <= rest)):

      buflen = len(self._file_data[lidx])
      rest = rest - buflen
      self._read_buffered -= buflen
      del self._file_data[lidx]

    # truncate the last data buffer in the queue (only if exact limit is requested)
    if exact and (rest > 0) and (len(self._file_data) > 0):
      buflen = len(self._file_data[lidx])
      data = self._file_data[lidx]
      if load_file_mode == 0:
        data = data[buflen - rest :]
      else:
        data = data[:rest]

      self._file_data[lidx] = data
      self._read_buffered -= rest


  #
  # This function is installed as handler for asynchronous read events
  # when reading text data from STDIN, i.e. via a pipe.
  #
  def LoadPipe_BgLoop(self):
    global load_buf_size

    try:
      while True:
        # limit read length to buffer size ("head" mode only)
        with self._thr_lock:
          while self._thr_ctrl == 1:
            self._thr_cv.wait()
          if self._thr_ctrl == 2:
            break;

          size = 64000
          if (load_file_mode == 0) and (self._read_buffered + size > load_buf_size):
            size = load_buf_size - self._read_buffered

        if size > 0:
          data = sys.stdin.read(size)
          buflen = len(data)
          if buflen > 0:
            with self._thr_lock:
              self._read_total += buflen
              self._read_buffered += buflen
              # data chunk is added to an array (i.e. not a single char string) for efficiency
              self._file_data.append(data)

              # discard oldest data when buffer size limit is exceeded ("tail" mode only)
              if (load_file_mode != 0) and (self._read_buffered > load_buf_size):
                self.LoadPipe_LimitData(0)

              self._stat_upd_ind += 1

            continue

          else:
            # end-of-file reached -> stop loading
            with self._thr_lock:
              self.is_eof = True
              self._ctrl_upd_ind += 1
            break
        else:
          with self._thr_lock:
            if self._thr_ctrl == 0:
              self._ctrl_upd_ind += 1
              self._thr_cv.wait()

    except IOError as e:
      with self._thr_lock:
        self._file_complete = e.strerror
        self.is_eof = True
        self._ctrl_upd_ind += 1


  #
  # This function is run periodically to poll for status changes in the
  # background thread. This is unfortunately needed as there's no way for
  # triggering an event in Tk from another thread.
  #
  def LoadPipe_BgPolling(self):
    if self._stat_upd_ind != self._stat_upd_cnf:
      with self._thr_lock:
        if (self._read_total >= 1000000) or (self._read_total > 4 * load_buf_size):
          self._dlg_read_total.set("%2.1f MByte" % (self._read_total / 0x100000))
        else:
          self._dlg_read_total.set(self._read_total)

        self._dlg_read_buffered.set(self._read_buffered)
        self._stat_upd_cnf = self._stat_upd_ind

    if self._ctrl_upd_ind != self._ctrl_upd_cnf:
      if self._file_complete == "":
        # success (no read error, although EOF may have been reached)
        if (self._opt_file_mode.get() != 0) or self.is_eof:
          self.LoadPipe_Insert(True)
        else:
          # keep dialog open to allow user switching file mode or other parameters
          self.LoadPipe_DialogConfigure(True)

      else:
        messagebox.showerror(parent=tk, message="Read error on STDIN: " + self._file_complete)
        try:
          sys.stdin.close()
        except OSError:
          pass
        self.LoadPipe_Insert(True)

      self._ctrl_upd_cnf = self._ctrl_upd_ind

    # finally reschedule the event
    self._tid_bg_poll = tk.after(100, lambda: load_pipe.LoadPipe_BgPolling())


  #
  # This function loads a text file from STDIN. A status dialog is opened
  # if loading takes longer than a few seconds or if the current buffer
  # size is exceeded.
  #
  def LoadPipe_Start(self):
    # re-initialize in case loading is continued
    #self._file_data = []

    #if self._opt_file_mode.get() == 0:
    #  del self._file_data[:]
    #  self._read_buffered = 0

    wt.f1_t.configure(cursor="watch")
    self.LoadPipe_OpenDialog()
    self.LoadPipe_DialogConfigure(False)

    # install an event handler to read the data asynchronously
    #fcntl.fcntl(sys.stdin.fileno(), fcntl.F_SETFL, os.O_NONBLOCK)
    with self._thr_lock:
      self._thr_ctrl = 0
      self._thr_cv.notify()

    if self._thr_inst is None:
      self._thr_inst = threading.Thread(target=lambda:self.LoadPipe_BgLoop(), daemon=True)
      self._thr_inst.start()

    self._tid_bg_poll = tk.after(100, lambda: load_pipe.LoadPipe_BgPolling())


  #
  # This function closes the pipe-loading dialog window and inserts the loaded
  # text (if any) into the main window.
  #
  def LoadPipe_Insert(self, from_thread):
    global dlg_load_shown

    if from_thread and (self._opt_file_close or self.is_eof):
      try:
        sys.stdin.close()
      except OSError:
        pass

      with self._thr_lock:
        self._thr_ctrl = 2
        self._thr_cv.notify()

      # joining is not possible as thread may block in read()
      #self._thr_inst.join()
      #self._thr_inst = None

    if self._tid_bg_poll is not None:
      tk.after_cancel(self._tid_bg_poll)
      self._tid_bg_poll = None

    if dlg_load_shown:
      dlg_load_shown = False
      wt.dlg_load.destroy()

    # thread may still be active, so
    with self._thr_lock:
      # limit content length to the exact maximum (e.g. in case the user changed sizes)
      self.LoadPipe_LimitData(1)

      l_file_data = self._file_data
      self._file_data = []
      self._read_buffered = 0

    # insert the data into the text widget
    for data in l_file_data:
      wt.f1_t.insert("end", data)

    # finally initiate color highlighting etc.
    InitContent()
    wt.f1_t.configure(cursor="top_left_arrow")


# ----------------------------------------------------------------------------
#
# This function loads a text file (or parts of it) into the text widget.
#
def LoadFile(filename):
  global cur_filename, load_pipe, load_buf_size, load_file_mode

  cur_filename = filename
  load_pipe = None

  try:
    file = open(filename, "rb")

    # apply file length limit
    stat = os.fstat(file.fileno())
    if load_file_mode and (stat.st_size > load_buf_size):
      file.seek(0 - load_buf_size, 2)

    # insert the data into the text widget
    data = file.read(load_buf_size)
    file.close()

  except OSError as e:
    messagebox.showerror(message="Failed to load file %s: %s" % (filename, e.strerror))
    data = b""

  wt.f1_t.insert("end", data)

  # add missing newline at end of file
  if data and (data[-1] != b"\n"[0]):
    wt.f1_t.insert("end", "\n")

  InitContent()


#
# This function initializes the text widget and control state for a
# newly loaded text.
#
def InitContent():
  global tid_search_inc, tid_search_hall, tid_high_init
  global cur_filename, load_pipe, dlg_mark_shown

  if tid_high_init is not None: tk.after_cancel(tid_high_init)
  if tid_search_inc is not None: tk.after_cancel(tid_search_inc)
  if tid_search_hall is not None: tk.after_cancel(tid_search_hall)
  tid_high_init = None
  tid_search_inc = None
  tid_search_hall = None

  # window title and main menu
  if cur_filename != "":
    tk.title(cur_filename + " - Trace browser")
    if wt_exists(wt.dlg_mark):
      wt.dlg_mark.title("Bookmark list [" + cur_filename + "]")

    wt.menubar_ctrl.entryconfigure(1, state=NORMAL, label="Reload current file")
  else:
    wt.menubar_ctrl.entryconfigure(1, label="Continue loading STDIN...")
    if not load_pipe.is_eof:
      wt.menubar_ctrl.entryconfigure(1, state=NORMAL)
    else:
      wt.menubar_ctrl.entryconfigure(1, state=DISABLED)

    tk.title("Trace browser")

  # switch from "watch" to default cursor
  wt.f1_t.configure(cursor="top_left_arrow")
  # set cursor to the end of file
  wt.f1_t.mark_set("insert", "end")
  CursorMoveLine(wt.f1_t, 0)

  global cur_jump_stack
  cur_jump_stack = []
  cur_jump_idx = -1
  # read bookmarks from the default file
  Mark_ReadFileAuto()
  # start color highlighting in the background
  HighlightInit()


#
# This procedure discards all text content and aborts all ongoing
# activity and timers. The function is called before new data is
# loaded.
#
def DiscardContent():
  global patlist, mark_list, mark_list_modified

  # the following is a work-around for a performance issue in the text widget:
  # deleting text with large numbers of tags is extremely slow, so we clear
  # the tags first (needed for Tcl/Tk 8.4.7)
  for w in patlist:
    wt.f1_t.tag_remove(w[4], "1.0", "end")

  # discard the current trace content
  wt.f1_t.delete("1.0", "end")

  SearchReset()

  mark_list = {}
  mark_list_modified = False
  MarkList_Fill()

  SearchList_Clear()
  SearchList_Init()


#
# This function is bound to the "Discard content" menu commands.
# The parameter specifies if content above or below the cursor is discarded.
#
def MenuCmd_Discard(is_fwd):
  global cur_filename

  PreemptBgTasks()
  if is_fwd:
    # delete everything below the line holding the cursor
    (first_l, first_c) = map(int, wt.f1_t.index("insert +1 lines linestart").split("."))
    (last_l, last_c) = map(int, wt.f1_t.index("end").split("."))
    count = last_l - first_l
    if (last_c == 0) and (count > 0):
      count -= 1

    if count == 0:
      messagebox.showerror(message="Already at the bottom")
      return

  else:
    # delete everything above the line holding the cursor
    first_l = 1
    first_c = 0
    (last_l, last_c) = map(int, wt.f1_t.index("insert linestart").split("."))
    count = last_l - first_l
    if count == 0:
      messagebox.showerror(message="Already at the top")
      return

  ResumeBgTasks()

  # ask for confirmation, as this cannot be undone
  if count > 0:
    pl = "" if (count == 1) else "s"
    (end_l, end_c) = map(int, wt.f1_t.index("end").split("."))
    if end_l > 1:
      if end_c == 0: end_l -= 1
      tmp_val = int(100.0*count/(end_l-1) + 0.5)
      perc = " (%d%%)" % tmp_val
    else:
      perc = ""

    msg = "Please confirm removing %d line%s%s" % (count, pl, perc)
    if cur_filename != "":
      msg = msg + "\n(The file will not be modified)"
    answer = messagebox.askokcancel(parent=tk, message=msg)
    if answer:
      if SearchList_SearchAbort():
        SearchHighlightClear()

        # the following is a work-around for a performance issue in the text widget:
        # deleting text with large numbers of tags is extremely slow, so we clear
        # the tags first (needed as of Tcl/Tk 8.4.7 to .13)
        global patlist
        for w in patlist:
          wt.f1_t.tag_remove(w[4], "%d.%d" % (first_l, first_c), "%d.%d" % (last_l, last_c))

        # perform the removal
        wt.f1_t.delete("%d.%d" % (first_l, first_c), "%d.%d" % (last_l, last_c))

        # re-start initial highlighting, if not complete yet
        global tid_high_init
        if tid_high_init is not None:
          tk.after_cancel(tid_high_init)
          tid_high_init = None
          HighlightInit()

        SearchReset()
        global cur_jump_stack, cur_jump_idx
        cur_jump_stack = []
        cur_jump_idx = -1

        MarkList_AdjustLineNums(1 if is_fwd else last_l, first_l if is_fwd else 0)
        SearchList_AdjustLineNums(1 if is_fwd else last_l, first_l if is_fwd else 0)


#
# This function is bound to the "Reload current file" menu command.
#
def MenuCmd_Reload():
  global load_pipe, cur_filename

  if load_pipe is not None:
    if not load_pipe.is_eof:
      DiscardContent()
      tk.after_idle(lambda: load_pipe.LoadPipe_Start())
  else:
    DiscardContent()
    tk.after_idle(lambda: LoadFile(cur_filename))


#
# This function is bound to the "Load file" menu command.  The function
# allows to specify a file from which a new trace is read. The current browser
# contents are discarded and all bookmarks are cleared.
#
def MenuCmd_OpenFile():
  # offer to save old bookmarks before discarding them below
  Mark_OfferSave()

  filename = filedialog.askopenfilename(parent=tk, filetypes=(("trace", "out.*"), ("all", "*")))
  if filename:
    DiscardContent()
    tk.after_idle(lambda: LoadFile(filename))


#
# This function is installed as callback for destroy requests on the
# main window to store the search history and bookmarks.
#
def UserQuit():
  UpdateRcFile()
  Mark_OfferSave()
  tk.destroy()
  sys.exit(0)


#
# This function sets a global flag which makes background tasks sleep
# for a short time so that an interactive task can be completed. It's
# essential that ResumeBgTasks is called afterwards and that the caller
# doesn't block.
#
def PreemptBgTasks():
  global block_bg_tasks, block_bg_caller, tid_resume_bg

  block_bg_caller.append(["LOCK", traceback.format_tb(sys.exc_info()[2], limit=-2)])
  if tid_resume_bg is not None:
    # no incr in this case b/c resume was called, but decr delayed
    block_bg_tasks = 1
    tk.after_cancel(tid_resume_bg)
    tid_resume_bg = None
  else:
    block_bg_tasks += 1


#
# This function allows background tasks to resume after all pending events
# have been processed.  Note the extra delay via idle and additional timer
# is required to make sure all X events (e.g. from opening a new dialog
# window) have been processed.
#
def ResumeBgTasks():
  global block_bg_tasks, block_bg_caller, tid_resume_bg

  if block_bg_tasks > 1:
    block_bg_caller.append(["DEC #"+str(block_bg_tasks), traceback.format_tb(sys.exc_info()[2], limit=-2)])
    block_bg_tasks -= 1
  else:
    block_bg_caller.append(["UNLOCK", traceback.format_tb(sys.exc_info()[2], limit=-2)])
    if tid_resume_bg is not None: tk.after_cancel(tid_resume_bg)
    tid_resume_bg = tk.after_idle(lambda: ClearBgTasks(1))


#
# This function is installed as idle and timer event to finally allow
# background tasks to resume.  The handler once re-installs itself via
# a timer to make extra-sure all pending activity is done.  Note the
# whole procedure is similar to calling "update" (which is avoided
# though because it inflicts race conditions.)
#
def ClearBgTasks(flag):
  global block_bg_tasks, tid_resume_bg

  if flag:
    if block_bg_tasks == 0:
      print("Warning: nested call of ResumeBgTasks(?) - internal error", file=sys.stderr)
    else:
      block_bg_tasks -= 1
      block_bg_caller = []

    tid_resume_bg = None
  else:
    tid_resume_bg = tk.after(250, lambda: ClearBgTasks(0))


#
# This helper function is installed as post command for all menu popups
# so that idle-event driven background tasks are shortly suspended while
# a menu popup is displayed. Without this the GUI may freeze until the
# background task has finished.
#
def MenuPosted():
  PreemptBgTasks()
  ResumeBgTasks()


#
# Helper function for destroying a widget that might no longer exist
#
def SafeDestroy(wid):
  try:
    wid.destroy()
  except:
    pass

#
# Debug only: This function dumps all global variables and their content
# to STDERR. Additionally info about all running tasks is printed.
#
def DebugDumpAllState():
  print("#--- debug dump of scalars and lists ---#", file=sys.stderr)
  for (var,val) in globals().items():
    if not var.startswith("__"):
      print(var, "=", val, file=sys.stderr)

  print("#--- debug dump of tasks ---#", file=sys.stderr)
  for id in tk.call("after", "info"):
    try:
      print(id, "=", tk.call("after", "info", id))
    except:
      pass


# ----------------------------------------------------------------------------
# The following data is automatically generated - do not edit
# Generated by ./tools/pod2help.py from doc/trowser.pod

helpIndex = {}
helpIndex['Description'] = 0
helpIndex['Key bindings'] = 1
helpIndex['Options'] = 2
helpIndex['Environment'] = 3
helpIndex['Files'] = 4
helpIndex['Caveats'] = 5

helpSections = {}
helpSections[(1,1)] = '''Key Bindings in the Main Window'''
helpSections[(1,2)] = '''Key Bindings in the Search Entry Field'''
helpSections[(1,3)] = '''Key Bindings in the Search Result Window'''
helpSections[(1,4)] = '''Key Bindings in Dialogs'''

helpTexts = {}
helpTexts[0] = (('''Description''', 'title1'), ('''
''', ''), ('''Trowser''', 'underlined'), (''' is a graphical browser for large line-oriented text files with color highlighting and a highly flexible search and cherry-picking window. Trowser was developed as an alternative to UNIX-tool "less" when analyzing debug log files (aka traces - hence the name).
''', ''), ('''Trowser has a graphical interface, but is designed to allow browsing via the keyboard at least to the same extent as less. Key bindings and the cursor positioning concept are derived from vim.
''', ''), ('''Note in this context "line-oriented" denotes that each line of text is considered a data unit.  Color highlighting (including search matches) will always apply the highlight to a complete line of text.
''', ''), ('''When you start trowser for the first time, you'll have to create highlight patterns for your type of file.  To do this, first enter a search pattern and verify that it matches the intended lines. Then open the ''', ''), ('''Edit highlight patterns''', 'underlined'), (''' dialog in the ''', ''), ('''Search''', 'underlined'), (''' menu, press the right mouse button to open the context menu and select ''', ''), ('''Add current search''', 'underlined'), ('''. You can change the highlight color or select a different kind of mark-up by double-clicking on the new entry in the dialog, or by selecting ''', ''), ('''Edit markup''', 'underlined'), (''' in the context menu.  To define new colors, click on ''', ''), ('''Edit color palette''', 'underlined'), (''' at the bottom of the markup editor dialog.
''', ''), ('''There are several ways to quickly navigate in the file to lines matching search patterns: Firstly, you can search forwards or backwards to any sub-string or pattern you enter in the ''', ''), ('''Find:''', 'underlined'), (''' field. Secondly, you can repeat previous searches by opening the search history dialog and double-clicking on an entry, or by clicking ''', ''), ('''Next''', 'underlined'), (''' or ''', ''), ('''Previous''', 'underlined'), ('''. Third, you can assign bookmarks to selected text lines and jump in-between those lines by clicking on them in the bookmark list dialog or via ''', ''), (''''+''', 'fixed'), (''' and ''', ''), (''''-''', 'fixed'), (''' key bindings (not in vim.) Fourth, you can search for patterns defined in the color highlight list by selecting a pattern in the list and then clicking on ''', ''), ('''Next''', 'underlined'), (''' or ''', ''), ('''Previous''', 'underlined'), (''' in the pattern list dialog. Fifth, you can open the ''', ''), ('''Search result list''', 'underlined'), (''' (via the ''', ''), ('''Search''', 'underlined'), (''' menu or by clicking on ''', ''), ('''List all''', 'underlined'), (''' in any dialog or by entering ''', ''), ('''ALT-a''', 'fixed'), (''') to display all text lines which match a set of patterns and click on an entry in this list to jump to the respective line in the main window. Sixth, you can manually copy arbitrary lines from the main text window into the search result window via the ''', ''), ('''\ 'i'\ ''', 'fixed'), (''' key (not in vim.)
''', ''), ('''The search filter list is one of the main features of the trace browser, as it allows to consecutively build an arbitrary sub-set of text lines in the main window. You can not only use one or more search patterns to add text, but also add selected text lines from the main text window via the ''', ''), ('''i''', 'fixed'), (''' key binding and remove individual lines again, either manually or by use of a search pattern.  Additionally you can use bookmarks in the search result window. When searching in the main window, the search result list will scroll to show the same region of text. Thus you effectively can navigate the text on three levels: Bookmarks > Search list > Main text.
''', ''), ('''Both the bookmark and search result lists support prefixing all entries with a "frame number". This is useful when your input file does not have time-stamp prefixes on each line. In this case trowser can search for a preceding time-stamp and automatically prefix bookmarked lines with this number.  Additionally trowser allows to fetch a "frame number" which is not printed in the same line as the frame interval start line. In this case trowser searches the next frame start lines in forward and backward direction and then inside of that range for a line containing the frame number value.  Note for the search result list this feature is disabled by default for performance reasons. It must be enabled in the dialog's ''', ''), ('''Options''', 'underlined'), (''' menu. The search patterns used to locate time-stamps currently have to be inserted into the RC file manually.
''', ''), ('''For performance reasons most search-related commands are executed as background processes, so that the GUI remains responsive during search. For example, this applies to the initial color highlighting, global search highlighting, incremental search while editing search patterns and filling the search result list.  Such background activity is indicated by display of a progress bar and switching the mouse cursor to a watch or hourglass image.  You still can use trowser as usual during this time though.  The background activity is automatically aborted or restarted when a conflicting command is entered (e.g. when the search pattern is modified during global search highlighting.)
''', ''), )

helpTexts[1] = (('''Key bindings''', 'title1'), ('''
''', ''), ('''Generally, keyboard focus can be moved between control elements (e.g. buttons, check-boxes and text containers) using the ''', ''), ('''TAB''', 'underlined'), (''' or ''', ''), ('''Shift-TAB''', 'underlined'), ('''.  The widget with the keyboard focus is marked by a black border.  After start-up, keyboard focus is in the main text window.  Functions which are bound to mouse clicks on buttons etc. can be activated via the keyboard using the ''', ''), ('''Space''', 'underlined'), (''' bar. Many functions can also be activated via shortcuts: Press the ''', ''), ('''ALT''', 'underlined'), (''' key plus the character which is underlines in the button description (e.g. Press ''', ''), ('''ALT-c''', 'fixed'), (''' to open the ''', ''), ('''Control''', 'underlined'), (''' menu, or ''', ''), ('''ALT-a''', 'fixed'), (''' to simulate a mouse-click on the ''', ''), ('''All''', 'underlined'), (''' button at the bottom of the main window.)
''', ''), ('''In the following descriptions, ''', ''), ('''^X''', 'fixed'), (''' means ''', ''), ('''Control-X''', 'underlined'), (''', i.e. holding the Control key pressed while pressing the ''', ''), ('''X''', 'fixed'), (''' key. ''', ''), ('''ALT''', 'underlined'), (''' refers to the key to the left of the ''', ''), ('''Space''', 'underlined'), (''' bar.  Enclosing quotes must be typed.
''', ''), ('''Key Bindings in the Main Window''', 'title2'), ('''
''', ''), ('''The commands in this section can be used when the keyboard focus is in the main window.
''', ''), ('''Commands to move the cursor or change the view:
''', ''), ('''Up''', 'underlined'), (''', ''', ''), ('''Down''', 'underlined'), (''', ''', ''), ('''Left''', 'underlined'), (''', ''', ''), ('''Right''', 'underlined'), ('''
''', ''), ('''Move the cursor in the respective direction. When the cursor hits the end of the visible area (i.e. the view), the text is scrolled vertically by a line or horizontally by a character (same as in vim, except for the smoother horizontal scrolling)
''', 'indent'), ('''Control-Up''', 'underlined'), (''', ''', ''), ('''Control-Down''', 'underlined'), (''', ''', ''), ('''Control-Left''', 'underlined'), (''', ''', ''), ('''Control-Right''', 'underlined'), ('''
''', ''), ('''Scroll the view by a line or character in the respective direction (not in vim - unless you have added "map <C-Down> ^E" etc. in ''', 'indent'), ('''.vimrc''', ('underlined', 'indent')), (''')
''', 'indent'), ('''h''', 'fixed'), (''', ''', ''), ('''l''', 'fixed'), (''', ''', ''), ('''k''', 'fixed'), (''', ''', ''), ('''j''', 'fixed'), ('''
''', ''), ('''Move the cursor left, right, up or down (same as in vim)
''', 'indent'), ('''Return''', 'underlined'), (''', ''', ''), ('''+''', 'fixed'), (''', ''', ''), ('''-''', 'fixed'), ('''
''', ''), ('''Move the cursor to the start of the following or preceding line (to be exact: the first non-blank character) (same as in vim)
''', 'indent'), ('''Space''', 'underlined'), (''', ''', ''), ('''BackSpace''', 'underlined'), ('''
''', ''), ('''Move the cursor to the next or preceding character (same as in vim)
''', 'indent'), ('''Home''', 'underlined'), (''', ''', ''), ('''End''', 'underlined'), (''', ''', ''), ('''0''', 'fixed'), (''', ''', ''), ('''$''', 'fixed'), (''', ''', ''), ('''^''', 'fixed'), ('''
''', ''), ('''Move the cursor to the first or last character of the current line (same as in vim)
''', 'indent'), ('''Control-Home''', 'underlined'), (''', ''', ''), ('''Control-End''', 'underlined'), (''', ''', ''), ('''G''', 'fixed'), (''', ''', ''), ('''gg''', 'fixed'), ('''
''', ''), ('''Move the cursor to the start or end of the file (same as in vim) Note an equivalent alternative to ''', 'indent'), ('''gg''', ('fixed', 'indent')), (''' is ''', 'indent'), ('''1g''', ('fixed', 'indent')), ('''.
''', 'indent'), ('''H''', 'fixed'), (''', ''', ''), ('''M''', 'fixed'), (''', ''', ''), ('''L''', 'fixed'), ('''
''', ''), ('''Move the cursor to the start of the line at the top, middle or bottom of the view (same as in vim)
''', 'indent'), ('''w''', 'fixed'), (''', ''', ''), ('''e''', 'fixed'), (''', ''', ''), ('''b''', 'fixed'), (''', ''', ''), ('''W''', 'fixed'), (''', ''', ''), ('''E''', 'fixed'), (''', ''', ''), ('''B''', 'fixed'), (''', ''', ''), ('''ge''', 'fixed'), (''', ''', ''), ('''gE''', 'fixed'), ('''
''', ''), ('''Move the cursor to the start or end of the next or preceding word (same as in vim)
''', 'indent'), ('''^e''', 'fixed'), (''', ''', ''), ('''^y''', 'fixed'), (''', ''', ''), ('''^d''', 'fixed'), (''', ''', ''), ('''^u''', 'fixed'), (''', ''', ''), ('''^f''', 'fixed'), (''', ''', ''), ('''^b''', 'fixed'), ('''
''', ''), ('''Scroll the screen by a single line, half a screen or a full screen forwards or backwards (same as in vim)
''', 'indent'), ('''z''', 'fixed'), ('''Return''', 'underlined'), (''', ''', ''), ('''z.''', 'fixed'), (''', ''', ''), ('''z-''', 'fixed'), ('''
''', ''), ('''Adjusts the view vertically so that the current line is at the top, middle or bottom of the screen and places the cursor on the first non-blank character (same as in vim)  The horizontal view is set to start at the left border.
''', 'indent'), ('''zt''', 'fixed'), (''', ''', ''), ('''zz''', 'fixed'), (''', ''', ''), ('''zb''', 'fixed'), ('''
''', ''), ('''Adjusts the view so that the current line is at the top, middle or bottom of the screen; the cursor position is unchanged (same as in vim)
''', 'indent'), ('''zl''', 'fixed'), (''', ''', ''), ('''zh''', 'fixed'), (''', ''', ''), ('''zL''', 'fixed'), (''', ''', ''), ('''zH''', 'fixed'), ('''
''', ''), ('''Move the view horizontally to the left or right (same as in vim)
''', 'indent'), ('''zs''', 'fixed'), (''', ''', ''), ('''ze''', 'fixed'), ('''
''', ''), ('''Scroll the view horizontally so that the current cursor column is placed at the left or the right side of the screen (as far as possible); in any case the cursor position remains unchanged (same as in vim)
''', 'indent'), ('''f''', 'fixed'), (''', ''', ''), ('''F''', 'fixed'), ('''
''', ''), ('''Search for the following character in the same line to the right or left respectively (same as in vim)
''', 'indent'), (''';''', 'fixed'), (''', ''', ''), (''',''', 'fixed'), (''' (semicolon, comma)
''', ''), ('''Repeat a previous in-line search (''', 'indent'), ('''f''', ('fixed', 'indent')), (''' or ''', 'indent'), ('''F''', ('fixed', 'indent')), (''') in the same or opposite direction respectively (same as in vim)
''', 'indent'), ('''''''', 'fixed'), (''' (two apostrophes)
''', ''), ('''Moves the cursor to the position before the latest jump (same as in vim and less.)  A "jump" is one of the following commands: ''', 'indent'), ('''\ '\ ''', ('fixed', 'indent')), (''', ''', 'indent'), ('''G''', ('fixed', 'indent')), (''', ''', 'indent'), ('''/''', ('fixed', 'indent')), (''', ''', 'indent'), ('''?''', ('fixed', 'indent')), (''', ''', 'indent'), ('''n''', ('fixed', 'indent')), (''', ''', 'indent'), ('''N''', ('fixed', 'indent')), (''', ''', 'indent'), ('''L''', ('fixed', 'indent')), (''', ''', 'indent'), ('''M''', ('fixed', 'indent')), (''' and ''', 'indent'), ('''H''', ('fixed', 'indent')), (''' (same as in vim.)  Note movements controlled via the GUI, such as the bookmark list or search result list, do not modify the jump list.
''', 'indent'), (''''+''', 'fixed'), (''', ''', ''), (''''-''', 'fixed'), ('''
''', ''), ('''Moves the cursor to the next or previous bookmark (not in vim)
''', 'indent'), (''''^''', 'fixed'), (''', ''', ''), (''''$''', 'fixed'), ('''
''', ''), ('''Moves the cursor to the start or end of file (same as in less; not in vim)
''', 'indent'), ('''^o''', 'fixed'), (''', ''', ''), ('''^i''', 'fixed'), ('''
''', ''), ('''Moves the cursor to the next older (or newer respectively) position in the jump list (same as in vim; note ''', 'indent'), ('''TAB''', ('fixed', 'indent')), (''' which is identical to ''', 'indent'), ('''^i''', ('fixed', 'indent')), (''' in vim has a different meaning here.) See ''', 'indent'), ('''''''', ('fixed', 'indent')), (''' for a list of commands which are considered jumps and add pre-jump cursor positions to the list.
''', 'indent'), ('''1''', 'fixed'), (''', ''', ''), ('''2''', 'fixed'), (''', ... ''', ''), ('''9''', 'fixed'), ('''
''', ''), ('''A number without leading zeroes can be used to repeat the subsequent key command or place the cursor on a given line or column (same as in vim)
''', 'indent'), ('''For example: ''', 'indent'), ('''1G''', ('fixed', 'indent')), (''' places the cursor in the first line of the file; ''', 'indent'), ('''10|''', ('fixed', 'indent')), (''' places the cursor in the tenth column of the current line (line and column numbering starts at 1.)  Note the number cannot start with zero, as ''', 'indent'), ('''0''', ('fixed', 'indent')), (''' is treated specially (immediately moves the cursor into the first column, same as in vim.)
''', 'indent'), ('''Searching and repeating:
''', ''), ('''/''', 'fixed'), (''', ''', ''), ('''?''', 'fixed'), ('''
''', ''), ('''Search for the following pattern (same as in vim.) Similar to vim, the keyboard focus is moved from the main text into a small text entry field (command line in vim) Note the previous search pattern is always cleared when re-entering the entry field, but all previously used patterns are still available in the history which can be accessed with the cursor up/down keys like in vim. Note in addition, you can use ''', 'indent'), ('''^d''', ('fixed', 'indent')), (''' in the search field to copy the text under the cursor in the main window into the search field, word by word.
''', 'indent'), ('''As soon as a search expression is typed into the field, an incremental search is started and matching lines are highlighted. The cursor in the main text isn't actually moved there until the search is completed by pressing ''', 'indent'), ('''Return''', ('fixed', 'indent')), ('''.  The search can be aborted by ''', 'indent'), ('''^C''', ('fixed', 'indent')), (''' or ''', 'indent'), ('''Escape''', ('fixed', 'indent')), ('''. For more details see ''', 'indent'), ('''Key bindings: Key Bindings in the Search Entry Field''', ('href', 'indent')), ('''.
''', 'indent'), ('''n''', 'fixed'), (''', ''', ''), ('''N''', 'fixed'), ('''
''', ''), ('''Repeats the previous search in forward or backwards direction respectively (similar to vim - however in contrary to vim ''', 'indent'), ('''n''', ('fixed', 'indent')), (''' always searches forward and ''', 'indent'), ('''N''', ('fixed', 'indent')), (''' always backwards because the standard vim behavior of remembering and reversing the search direction with ''', 'indent'), ('''N''', ('fixed', 'indent')), (''' is very confusing.)
''', 'indent'), ('''*''', 'fixed'), (''', ''', ''), ('''#''', 'fixed'), ('''
''', ''), ('''Searches for the word under the cursor in forward or backwards direction respectively (same as in vim)  Note when regular expression search mode is not enabled, this command performs a plain sub-string text search. Else, word boundary matches are placed around the search text, as done by vim.
''', 'indent'), ('''&''', 'fixed'), ('''
''', ''), ('''Remove the highlighting of previous search matches (not in vim as such, but can be added via ''', 'indent'), ('''map & :nohlsearch^M''', ('fixed', 'indent')), (''' in ''', 'indent'), ('''.vimrc''', ('underlined', 'indent')), (''')  Note this does not disable highlighting in subsequent searches.
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''f''', 'fixed'), ('''
''', ''), ('''Moves the focus in the search search entry field.  This is equivalent to ''', 'indent'), ('''/''', ('fixed', 'indent')), (''' or ''', 'indent'), ('''?''', ('fixed', 'indent')), (''' but without changing the search direction (not in vim) This is equivalent to clicking into the "Find:" entry field with the mouse button.
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''n''', 'fixed'), (''', ''', ''), ('''ALT-''', 'underlined'), (''' ''', ''), ('''p''', 'fixed'), ('''
''', ''), ('''Repeat a previous search, equivalent to ''', 'indent'), ('''n''', ('fixed', 'indent')), (''' and ''', 'indent'), ('''N''', ('fixed', 'indent')), (''' (not in vim)
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''h''', 'fixed'), ('''
''', ''), ('''Enable the "Highlight all" option, i.e. highlight all lines in the text where the current search pattern matches (not in vim)
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''a''', 'fixed'), ('''
''', ''), ('''Open the search result window and fill it with all text lines which match the current search pattern (not in vim)
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''N''', 'fixed'), (''', ''', ''), ('''ALT-''', 'underlined'), (''' ''', ''), ('''P''', 'fixed'), ('''
''', ''), ('''Open the search result window and fill it with all text lines below or above the current cursor position respectively which match the current search pattern (not in vim)
''', 'indent'), ('''The following commands can be used to change the selection.
''', ''), ('''Note that selected text is automatically exported and can be pasted into other applications.
''', ''), ('''Shift-Left''', 'underlined'), (''', ''', ''), ('''Shift-Right''', 'underlined'), (''', ''', ''), ('''Shift-Up''', 'underlined'), (''', ''', ''), ('''Shift-Down''', 'underlined'), ('''
''', ''), ('''Starts or extends the selection in the respective direction (not in vim) Note that trowser only supports the character-wise selection mode (like ''', 'indent'), ('''v''', ('fixed', 'indent')), (''' in vim)
''', 'indent'), ('''Shift-Home''', 'underlined'), (''', ''', ''), ('''Shift-End''', 'underlined'), ('''
''', ''), ('''Starts or extends the selection from the current cursor position to the start or end of the current line (not in vim)
''', 'indent'), ('''Control-Shift-Home''', 'underlined'), (''', ''', ''), ('''Control-Shift-End''', 'underlined'), ('''
''', ''), ('''Starts or extends the selection from the current cursor position to the start or end of the file (not in vim)
''', 'indent'), ('''^c''', 'fixed'), ('''
''', ''), ('''Copies the currently selected text to the clipboard.  (Note that this command is actually superfluous as the text is copied as soon as some text is selected.)
''', 'indent'), ('''Misc. commands (none of these are in vim):
''', ''), ('''m''', 'fixed'), ('''
''', ''), ('''This key, or double-clicking into a text line, toggles a bookmark in the respective line (different from vim; note setting named bookmarks is not supported.)  Additionally the view of the search result list, if open, will be centered around the line (even if the marked line is not included in the search results.)
''', 'indent'), ('''i''', 'fixed'), ('''
''', ''), ('''Insert (i.e. copy) the text line holding the cursor into the search result window. If a selection exists and is currently visible, the selected lines are copied instead. (Note the restriction to visibility of the selection exists to avoid confusion about ''', 'indent'), ('''i''', ('fixed', 'indent')), (''' not working on the current text line.)
''', 'indent'), ('''u''', 'fixed'), (''', ''', ''), ('''^r''', 'fixed'), ('''
''', ''), ('''Undo or redo respectively the last addition or removal of text lines in the search list done by ''', 'indent'), ('''i''', ('fixed', 'indent')), (''' or "Search All" (different from vim.)
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''+''', 'fixed'), (''', ''', ''), ('''ALT-''', 'underlined'), (''' ''', ''), ('''-''', 'fixed'), ('''
''', ''), ('''Increases or decreases the font size for the text content. Note the behavior when reaching the maximum or minimum font size is undefined.
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''w''', 'fixed'), ('''
''', ''), ('''Toggle line-wrap for text in the main window (i.e. text lines which are longer than the window width will wrap into the next line.)
''', 'indent'), ('''Key Bindings in the Search Entry Field''', 'title2'), ('''
''', ''), ('''The following commands can be used when the keyboard focus is in the ''', ''), ('''search entry field''', 'bold'), (''' at the bottom of the main window:
''', ''), ('''Return''', 'underlined'), ('''
''', ''), ('''Store the current pattern in the search history and return focus to the main window with the cursor on the next match (same as vim)   Note the cursor is already moved via incremental search when entering the text (including the highlighting of adjacent matches) so the search and cursor movement need not be done again here.  This command is equivalent to leaving the search field by clicking with the mouse outside or switching keyboard focus via ''', 'indent'), ('''TAB''', ('underlined', 'indent')), (''' or ''', 'indent'), ('''Shift-TAB''', ('underlined', 'indent')), ('''.
''', 'indent'), ('''Escape''', 'underlined'), (''', ''', ''), ('''^c''', 'fixed'), ('''
''', ''), ('''Abort the current search, i.e. return focus to the main window and place the cursor on the previous position. The search pattern in the entry field is still pushed onto the history (same as in vim.)
''', 'indent'), ('''^a''', 'fixed'), (''', ''', ''), ('''^e''', 'fixed'), ('''
''', ''), ('''Move the insertion cursor to the start or end of the search text entry field (''', 'indent'), ('''^e''', ('fixed', 'indent')), (''' is same as in vim; ''', 'indent'), ('''^a''', ('fixed', 'indent')), (''' is not in vim.)  Note: movement and selection via cursor keys works in the same way as described for the main text.
''', 'indent'), ('''^n''', 'fixed'), (''', ''', ''), ('''^N''', 'fixed'), ('''
''', ''), ('''Jump to the next or previous match respectively for the current pattern using incremental search.  Note these commands do not affect the fall-back cursor position, i.e. when the search is aborted or the pattern is changed, the cursor returns to the original start position (not in vim)
''', 'indent'), ('''Up''', 'underlined'), (''', ''', ''), ('''Down''', 'underlined'), ('''
''', ''), ('''Copies the previous or next pattern in the search history into the entry field. If the entry field already contains some text, the search is restricted to patterns with the same prefix.
''', 'indent'), ('''^d''', 'fixed'), (''', ''', ''), ('''^D''', 'fixed'), ('''
''', ''), ('''Complete the search text with the text to the right or left of the current match in the main text (i.e. right or left of the text marked with green background color.)
''', 'indent'), ('''^x''', 'fixed'), ('''
''', ''), ('''Remove the currently used pattern in the search history, if the current pattern was copied by use of ''', 'indent'), ('''Up''', ('underlined', 'indent')), (''' or ''', 'indent'), ('''Down''', ('underlined', 'indent')), (''' (not in vim)
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''n''', 'fixed'), (''', ''', ''), ('''ALT-''', 'underlined'), (''' ''', ''), ('''p''', 'fixed'), ('''
''', ''), ('''Same as pressing the ''', 'indent'), ('''Next''', ('underlined', 'indent')), (''' or ''', 'indent'), ('''Previous''', ('underlined', 'indent')), (''' buttons respectively, i.e. search for the current pattern in forward or backwards direction and add the pattern to the search history. Keyboard focus remains in the search entry field.
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''a''', 'fixed'), ('''
''', ''), ('''Open the search result window and fill it with all text lines which match the current search pattern (not in vim)  Additionally, keyboard focus is moved back into the main window.
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''N''', 'fixed'), (''', ''', ''), ('''ALT-''', 'underlined'), (''' ''', ''), ('''P''', 'fixed'), ('''
''', ''), ('''Open the search result window and fill it with all text lines below or above the current cursor position respectively which match the current search pattern (not in vim)  Additionally, the keyboard focus is moved back into the main window.
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''c''', 'fixed'), ('''
''', ''), ('''Toggle the "match case" option, i.e. equivalent to clicking on ''', 'indent'), ('''Match case''', ('underlined', 'indent')), (''' (not in vim)
''', 'indent'), ('''ALT-''', 'underlined'), (''' ''', ''), ('''e''', 'fixed'), ('''
''', ''), ('''Toggle the regular expression search option, i.e. equivalent to clicking on button ''', 'indent'), ('''Reg.Exp.''', ('underlined', 'indent')), (''' (not in vim.)  When this option is enabled, special characters are parsed according to ''', 'indent'), ('''re_syntax''', ('underlined', 'indent')), (''' Tcl manual page; the syntax is almost identical to Perl with few exceptions (notably ''', 'indent'), ('''\m''', ('fixed', 'indent')), (''' and ''', 'indent'), ('''\M''', ('fixed', 'indent')), (''' to match beginning and end of words)  When the option is not enabled, no characters have a special meaning (i.e. even "''', 'indent'), ('''*''', ('fixed', 'indent')), ('''") and a simple sub-string search is started.
''', 'indent'), ('''Note: for performance reasons it's recommended to use case-sensitive sub-string searches for color highlighting, especially if you have many patterns. This is usually faster than combining multiple patterns with ''', 'indent'), ('''|''', ('fixed', 'indent')), (''' in a regular expression.
''', 'indent'), ('''Key Bindings in the Search Result Window''', 'title2'), ('''
''', ''), ('''The following commands can be used in the search result window (i.e. the list filled by "Search All" and lines copied from the main window via the ''', ''), ('''i''', 'fixed'), (''' key binding.)
''', ''), ('''For users who prefer controls via the mouse it should be noted that there's a context menu which opens via a click with the right mouse button into a line, which has equivalent commands to the ones listed below.
''', ''), ('''m''', 'fixed'), ('''
''', ''), ('''Bookmark the currently selected line.  The line will be marked both in the search result window and the main window.
''', 'indent'), ('''Delete''', 'underlined'), ('''
''', ''), ('''Remove the selected lines from the search result list.
''', 'indent'), ('''u''', 'fixed'), ('''
''', ''), ('''Undo the last addition or removal.
''', 'indent'), ('''^r''', 'fixed'), ('''
''', ''), ('''Redo the last addition or removal (if previously undone.)
''', 'indent'), ('''/''', 'fixed'), (''', ''', ''), ('''?''', 'fixed'), ('''
''', ''), ('''Moves the keyboard focus in the search entry field in the main window for entering a search expression. The behavior of the search is the same as in the main window. When leaving the search entry field via ''', 'indent'), ('''Return''', ('fixed', 'indent')), (''' or ''', 'indent'), ('''Escape''', ('fixed', 'indent')), (''', the keyboard focus returns to the search list.
''', 'indent'), ('''n''', 'fixed'), (''', ''', ''), ('''N''', 'fixed'), ('''
''', ''), ('''Repeat the last search in downwards or upwards direction respectively. The search is restricted to lines in the search result window.
''', 'indent'), ('''Escape''', 'underlined'), ('''
''', ''), ('''Abort an ongoing search. Lines which were already found and added to the search result window will remain. (You can still remove these lines using "undo".)
''', 'indent'), ('''&''', 'fixed'), ('''
''', ''), ('''Same as in the main window: Remove the highlighting of previous search matches (same as ''', 'indent'), (''':nohlsearch''', ('fixed', 'indent')), (''' in vim) and of lines highlighted in the main window by positioning via selections in the search result list.
''', 'indent'), ('''In addition to the above, the general selection dialog key bindings in the next section also work in the search result window.
''', ''), ('''Key Bindings in Dialogs''', 'title2'), ('''
''', ''), ('''The following commands can be used to manipulate the selection cursor in all dialogs which display lists (i.e. search result list, search history, bookmarks, highlight pattern editor)  Note there's no distinction between selection and cursor in these dialogs. This means you cannot move the selection cursor from line A to D using the keyboard without temporarily selecting lines B and C in-between.
''', ''), ('''Of course you can also manipulate the selection via the mouse in the usual ways, i.e. clicking on single entries, or dragging the mouse to select multiple elements, or pressing the mouse while holding Control or Shift keys pressed to add or remove single elements or extend the selection respectively.
''', ''), ('''Up''', 'underlined'), (''', ''', ''), ('''Down''', 'underlined'), ('''
''', ''), ('''Move the selection cursor one line up or down respectively, scrolling the view if necessary.  If no line is selected yet, the cursor is placed on the first or last line; if the previously selected line is still in the visible area, the cursor is placed there instead.
''', 'indent'), ('''Home''', 'underlined'), (''', ''', ''), ('''End''', 'underlined'), ('''
''', ''), ('''Move the selection cursor on the first or last item in the list.
''', 'indent'), ('''Shift-Up''', 'underlined'), (''', ''', ''), ('''Shift-Down''', 'underlined'), (''', ''', ''), ('''Shift-Home''', 'underlined'), (''', ''', ''), ('''Shift-End''', 'underlined'), ('''
''', ''), ('''Extend or reduce the selection in the given direction, or to the start or end of the list.
''', 'indent'), ('''Page-Up''', 'underlined'), (''', ''', ''), ('''Page-Down''', 'underlined'), ('''
''', ''), ('''Scroll the view up or down by a page. These commands remove the selection cursor.
''', 'indent'), )

helpTexts[2] = (('''Options''', 'title1'), ('''
''', ''), ('''The following command line options are available:
''', ''), ('''-h''', 'bold'), (''' ''', ''), ('''limit''', 'underlined'), (''', ''', ''), ('''--head=limit''', 'bold'), ('''
''', ''), ('''This option specifies the maximum number of bytes read from the start of the input file or stream, i.e. any following text is silently ignored.
''', 'indent'), ('''The limit value is remembered in the configuration file and used in the next invocation unless overridden.  When neither ''', 'indent'), ('''-h''', ('bold', 'indent')), (''' or ''', 'indent'), ('''-t''', ('bold', 'indent')), (''' are specified and data is loaded from a stream via STDIN, a small dialog window pops up when the buffer limit is exceeded. This allows the user to select between head and tail modes manually.
''', 'indent'), ('''-t''', 'bold'), (''' ''', ''), ('''limit''', 'underlined'), (''', ''', ''), ('''--tail=limit''', 'bold'), ('''
''', ''), ('''This option specifies the maximum number of bytes to be read into the display buffer.  If the input is a file which is larger then the given buffer limit, text at the beginning of the file is skipped. If the input is a stream, all data is read into a temporary queue until the end-of-stream is reached; then the last ''', 'indent'), ('''limit''', ('underlined', 'indent')), (''' number of bytes which were read from the stream are loaded into the display buffer.
''', 'indent'), ('''The limit value is remembered in the configuration file and used in the next invocation unless overridden.
''', 'indent'), ('''-r''', 'bold'), (''' ''', ''), ('''path''', 'underlined'), (''', ''', ''), ('''--rcfile=path''', 'bold'), ('''
''', ''), ('''This option can be used to specify an alternate configuration file. When this option is not present, the configuration file is stored in the home directory, see section FILES.
''', 'indent'), )

helpTexts[3] = (('''Environment''', 'title1'), ('''
''', ''), ('''trowser''', 'bold'), (''' only evaluates the standard variables ''', ''), ('''DISPLAY''', 'bold'), (''' (X11 display address) and ''', ''), ('''HOME''', 'bold'), (''' (home directory, for storing the configuration file.)
''', ''), )

helpTexts[4] = (('''Files''', 'title1'), ('''
''', ''), ('''$HOME/.config/trowser/trowser.py.rc''', 'bold'), ('''
''', ''), ('''UNIX''', ('underlined', 'indent')), (''': Configuration file where all personal settings and the search history are stored. Per default this file is created in your home directory, but a different path and file name can be specified with the ''', 'indent'), ('''--rcfile''', ('bold', 'indent')), (''' option (see ''', 'indent'), ('''Options''', ('href', 'indent')), (''').
''', 'indent'), ('''During updates to this file, trowser temporarily creates a file called ''', 'indent'), ('''.trowserc.XXXXX.tmp''', ('fixed', 'indent')), (''' in the home directory, where "XXXXX" is a random number. The old file is then replaced with this new file. This procedure will obviously fail if your home directory is not writable.
''', 'indent'), )

helpTexts[5] = (('''Caveats''', 'title1'), ('''
''', ''), ('''Currently only one pattern list for color highlighting is supported. Hence different highlighting for different file types can only be done by choosing different configuration files when starting trowser (see the ''', ''), ('''--rcfile''', 'underlined'), (''' option.)
''', ''), ('''Vim compatibility: Not all vim navigation commands are implemented; Command repetition is supported only for a small sub-set of commands; Some commands behave slightly differently from vim (most notably the bookmark related commands.) vim's range and selection commands are not supported at all.
''', ''), ('''Search repetition by pressing "Next" or "Previous" or the search history dialog is currently not interruptable and may take quite a while if the next match is several MB away. (This can be avoided by repeating the search via the entry field's internal search history, i.e. ''', ''), ('''/''', 'fixed'), (''' and ''', ''), ('''Up''', 'underlined'), (''')
''', ''), ('''Searching with regular expressions is very slow in large files. This is unfortunately a property of the "text" Tk widget. Thus use of regular expressions for highlighting is not recommended. (As a work-around, trowser automatically falls back to plain string search if there are no control characters in the search expression.)
''', ''), ('''Some GUI activity (e.g. selecting a range on text with the mouse) will render active background tasks uninteruptable, i.e. the GUI will become unresponsive until the background task has completed.
''', ''), ('''File store and load dialogs do not maintain a history of previously used files or directories. (This is so because it's expected that these features will not be used very often.)
''', ''), ('''The pipe load and search result list dialogs are not designed very well yet (i.e. even more so than the other dialogs). Suggestions for improvements are welcome.
''', ''), ('''Some configuration options cannot be modified via the GUI and require manually editing the configuration file.
''', ''), )

# ----------------------------------------------------------------------------
#
# This class implements the help dialog.
#

dlg_help = None

help_titles = []
help_fg = "black"
help_bg = "#FFFFA0"
help_font_normal = None
help_font_fixed = None
help_font_bold = None
help_font_title1 = None
help_font_title2 = None


def dlg_help_create_dialog(index, subheading="", subrange=""):
    global dlg_help

    if not help_font_normal:
        dlg_help_define_fonts()

    if dlg_help and wt_exists(dlg_help.wid_top):
        dlg_help.raise_window(index, subheading, subrange)
    else:
        dlg_help = Help_dialog(index, subheading, subrange)


def dlg_help_define_fonts():
    global help_font_normal, help_font_fixed, help_font_title1, help_font_title2, help_font_bold

    help_font_fixed = "TkFixedFont"
    help_font_normal = tkf.Font(font="TkTextFont")

    opt = help_font_normal.configure()
    opt["weight"] = tkf.BOLD
    help_font_bold = tkf.Font(**opt)
    opt["size"] += 2
    help_font_title2 = tkf.Font(**opt)
    opt["size"] += 2
    help_font_title1 = tkf.Font(**opt)


def dlg_help_add_menu_commands(wid_men):
    global help_titles

    for title, idx in helpIndex.items():
        help_titles.append(title)

    Help_dialog.fill_menu(wid_men)


class Help_dialog(object):
    def __init__(self, index, subheading, subrange):
        global win_geom
        self.chapter_idx = -1
        self.help_stack = []

        self.wid_top = Toplevel(tk)
        self.wid_top.wm_title("GtestGui: Manual")
        self.wid_top.wm_group(tk)

        self.__create_buttons()
        self.__create_text_widget()

        self.wid_top.bind("<Configure>", lambda e:ToplevelResized(e.widget, self.wid_top, self.wid_top, "dlg_help"))
        self.wid_top.wm_geometry(win_geom["dlg_help"])
        self.wid_top.wm_positionfrom("user")

        self.wid_txt.focus_set()

        self.__fill_help_text(index, subheading, subrange)


    def __create_buttons(self):
        wid_frm = Frame(self.wid_top)
        but_cmd_chpt = Menubutton(wid_frm, text="Chapters", relief=FLAT, underline=0)
        but_cmd_prev = Button(wid_frm, text="Previous", width=7, relief=FLAT, underline=0)
        but_cmd_next = Button(wid_frm, text="Next", width=7, relief=FLAT, underline=0)
        but_cmd_dismiss = Button(wid_frm, text="Dismiss", relief=FLAT,
                                    command=self.__destroy_window)
        but_cmd_chpt.grid(row=1, column=1, padx=5)
        but_cmd_prev.grid(row=1, column=3)
        but_cmd_next.grid(row=1, column=4)
        but_cmd_dismiss.grid(row=1, column=6, padx=5)
        wid_frm.columnconfigure(2, weight=1)
        wid_frm.columnconfigure(5, weight=1)
        wid_frm.pack(side=TOP, fill=X)
        wid_frm.bind("<Destroy>", lambda e: self.__destroy_window())

        men_chpt = Menu(but_cmd_chpt, tearoff=0)
        but_cmd_chpt.configure(menu=men_chpt)
        Help_dialog.fill_menu(men_chpt)

        self.but_cmd_prev = but_cmd_prev
        self.but_cmd_next = but_cmd_next

    @staticmethod
    def fill_menu(wid_men):
        for idx in range(len(help_titles)):
            wid_men.add_command(label=help_titles[idx],
                                command=lambda idx=idx: dlg_help_create_dialog(idx))
            for foo, sub in sorted([x for x in helpSections.keys() if x[0] == idx]):
                title = helpSections[(idx, sub)]
                wid_men.add_command(label="- " + title,
                            command=lambda idx=idx, title=title: dlg_help_create_dialog(idx, title))


    def __create_text_widget(self):
        wid_frm = Frame(self.wid_top)
        wid_txt = Text(wid_frm, width=80, wrap=WORD,
                          foreground=help_fg, background=help_bg, font=help_font_normal,
                          spacing3=6, cursor="circle", takefocus=1)
        wid_txt.pack(side=LEFT, fill=BOTH, expand=1)
        wid_sb = Scrollbar(wid_frm, orient=VERTICAL, command=wid_txt.yview, takefocus=0)
        wid_txt.configure(yscrollcommand=wid_sb.set)
        wid_sb.pack(fill=Y, anchor=E, side=LEFT)
        wid_frm.pack(side=TOP, fill=BOTH, expand=1)

        # define tags for various nroff text formats
        wid_txt.tag_configure("title1", font=help_font_title1, spacing3=10)
        wid_txt.tag_configure("title2", font=help_font_title2, spacing1=20, spacing3=10)
        wid_txt.tag_configure("indent", lmargin1=30, lmargin2=30)
        wid_txt.tag_configure("bold", font=help_font_bold)
        wid_txt.tag_configure("underlined", underline=1)
        wid_txt.tag_configure("fixed", font=help_font_fixed)
        wid_txt.tag_configure("pfixed", font=help_font_fixed, spacing1=0, spacing2=0, spacing3=0)
        wid_txt.tag_configure("href", underline=1, foreground="blue")
        wid_txt.tag_bind("href", "<ButtonRelease-1>", lambda e: self.__follow_help_hyperlink())
        wid_txt.tag_bind("href", "<Enter>", lambda e: self.wid_txt.configure(cursor="top_left_arrow"))
        wid_txt.tag_bind("href", "<Leave>", lambda e: self.wid_txt.configure(cursor="circle"))

        # allow to scroll the text with the cursor keys
        wid_txt.bindtags([wid_txt, "TextReadOnly", self.wid_top, "all"])
        wid_txt.bind("<Up>", lambda e, self=self: BindCallAndBreak(
                                lambda: wid_txt.yview(SCROLL, -1, "unit")))
        wid_txt.bind("<Down>", lambda e, self=self: BindCallAndBreak(
                                lambda: wid_txt.yview(SCROLL, 1, "unit")))
        wid_txt.bind("<Prior>", lambda e, self=self: BindCallAndBreak(
                                lambda: wid_txt.yview(SCROLL, -1, "pages")))
        wid_txt.bind("<Next>", lambda e, self=self: BindCallAndBreak(
                                lambda: wid_txt.yview(SCROLL, 1, "pages")))
        wid_txt.bind("<Home>", lambda e, self=self: BindCallAndBreak(
                                lambda: wid_txt.yview(MOVETO, 0.0)))
        wid_txt.bind("<End>", lambda e, self=self: BindCallAndBreak(
                                lambda: wid_txt.yview(MOVETO, 1.0)))
        wid_txt.bind("<Enter>", lambda e, self=self: BindCallAndBreak(
                                lambda: e.widget.focus_set()))
        wid_txt.bind("<Escape>", lambda e: self.__destroy_window())
        wid_txt.bind("<Alt-Key-n>", lambda e: self.but_cmd_next.invoke())
        wid_txt.bind("<Alt-Key-p>", lambda e: self.but_cmd_prev.invoke())

        self.wid_txt = wid_txt


    def raise_window(self, index, subheading="", subrange=""):
        self.wid_top.lift()
        self.__fill_help_text(index, subheading, subrange)


    def __fill_help_text(self, index, subheading, subrange):
        self.wid_txt.configure(state=NORMAL)
        self.wid_txt.delete("1.0", "end")
        self.wid_txt.yview(MOVETO, 0.0)

        # fill the widget with the formatted text
        for htext, tlabel in helpTexts[index]:
            self.wid_txt.insert("end", htext, tlabel)

        self.wid_txt.configure(state=DISABLED)

        # bring the given text section into view
        if (len(subrange) == 2) and subrange[0]:
            self.wid_txt.see(subrange[1])
            self.wid_txt.see(subrange[0])
        elif subheading:
            # search for the string at the beginning of the line only (prevents matches on hyperlinks)
            pattern = "^" + str(subheading)
            pos = self.wid_txt.search(pattern, regexp=True, index="1.0")
            if pos:
                self.wid_txt.see(pos)
                # make sure the header is at the top of the page
                bbox = self.wid_txt.bbox(pos)
                if bbox:
                    bbox_y = bbox[1]
                    bbox_h = bbox[3]
                    self.wid_txt.yview(SCROLL, bbox_y // bbox_h, "units")
                    self.wid_txt.see(pos)

        # define/update bindings for left/right command buttons
        if helpTexts.get(index - 1, None):
            self.but_cmd_prev.configure(command=lambda: self.raise_window(index - 1), state=NORMAL)
        else:
            self.but_cmd_prev.configure(command=lambda: None, state=DISABLED)

        if helpTexts.get(index + 1, None):
            self.but_cmd_next.configure(command=lambda: self.raise_window(index + 1), state=NORMAL)
        else:
            self.but_cmd_next.configure(command=lambda: None, state=DISABLED)

        self.chapter_idx = index


    def __destroy_window(self):
        global dlg_help
        SafeDestroy(self.wid_top)
        dlg_help = None


    def __follow_help_hyperlink(self):
        global helpIndex

        # the text under the mouse carries the mark 'current'
        curidx = self.wid_txt.index("current + 1 char")

        # determine the range of the 'href' tag under the mouse
        range = self.wid_txt.tag_prevrange("href", curidx)

        # cut out the text in that range
        hlink = self.wid_txt.get(*range)

        # check if the text contains a sub-section specification
        match = re.match(r"(.*): *(.*)", hlink)
        if match:
            hlink = match.group(1)
            subsect = match.group(2)
        else:
            subsect = ""

        if helpIndex.get(hlink, None):
            self.raise_window(helpIndex[hlink], subsect)


# ----------------------------------------------------------------------------
#
# This function reads configuration variables from the rc file.
# The function is called once during start-up.
#
def LoadRcFile():
  global tlb_history, tlb_hist_maxlen, tlb_case, tlb_regexp, tlb_hall
  global patlist, col_palette, tick_pat_sep, tick_pat_num, tick_str_prefix
  global font_content, col_bg_content, col_fg_content, fmt_find, fmt_findinc
  global win_geom, fmt_selection
  global load_buf_size
  global rcfile_version, myrcfile

  error = False
  ver_check = False
  rc_compat_version = None
  line_no = 0
  font_content_opt = ""

  try:
    with open(myrcfile, "r") as rcfile:
      for line in rcfile:
        line_no += 1
        if line == "___END___":
          break
        if re.match(r"^\s*(?:#.*)?$", line):
          continue

        match = re.match(r"^([a-z][a-z0-9_\:]*)=(.+)$", line)
        if match:
          var = match.group(1)
          try:
            val = json.loads(match.group(2))

            if (var == "tlb_history"):         tlb_history = val
            elif (var == "tlb_hist_maxlen"):   tlb_hist_maxlen = val
            elif (var == "tlb_case"):          tlb_case.set(val)
            elif (var == "tlb_regexp"):        tlb_regexp.set(val)
            elif (var == "tlb_hall"):          tlb_hall.set(val)
            elif (var == "patlist"):           patlist = val
            elif (var == "col_palette"):       col_palette = val
            elif (var == "tick_pat_sep"):      tick_pat_sep = val
            elif (var == "tick_pat_num"):      tick_pat_num = val
            elif (var == "tick_str_prefix"):   tick_str_prefix = val
            elif (var == "font_content"):      font_content_opt = val
            elif (var == "fmt_selection"):     fmt_selection = val
            elif (var == "col_bg_content"):    col_bg_content = val
            elif (var == "col_fg_content"):    col_fg_content = val
            elif (var == "fmt_find"):          fmt_find = val
            elif (var == "fmt_findinc"):       fmt_findinc = val
            elif (var == "load_buf_size"):     load_buf_size = val
            elif (var == "rcfile_version"):    rcfile_version = val
            elif (var == "rc_compat_version"): rc_compat_version = val
            elif (var == "rc_timestamp"):      pass
            elif (var.startswith("win_geom:")): win_geom[var[9:]] = val
            else:
              print("Warning: ignoring unknown keyword in rcfile line %d:" % line_no, var, file=sys.stderr)

          except json.decoder.JSONDecodeError:
            messagebox.showerror(message="Syntax error decoding rcfile line %d: %s" % (line_no, line[:40]), title="Trace browser")
            error = True

        elif not error:
          messagebox.showerror(message="Syntax error in rc file, line #%d: %s" % (line_no, line[:40]), title="Trace browser")
          error = True

        elif not ver_check:
          # check if the given rc file is from a newer version
          if rc_compat_version is not None:
            if rc_compat_version > rcfile_version:
              messagebox.showerror(message="rc file '%s' is from an incompatible, "
                                   "newer browser version (%s) and cannot be loaded."
                                   % (myrcfile, rcfile_version), title="Trace browser")

              # change name of rc file so that the newer one isn't overwritten
              myrcfile = myrcfile + "." + rcfile_version
              # abort loading further data (would overwrite valid defaults)
              return

            ver_check = True

    try:
      if font_content_opt:
        font_content = tkf.Font(**font_content_opt)
    except Exception as e:
      print("Error configuring content font:", str(e), file=sys.stderr)

    # override config var with command line options
    try:
      global load_buf_size_opt
      if load_buf_size_opt != 0:
        load_buf_size = load_buf_size_opt
    except:
      pass

  except OSError as e:
    if e.errno != errno.ENOENT:
      print("Failed to load config file:", str(e), file=sys.stderr)


#
# This function writes persistent configuration variables into the RC file
#
def UpdateRcFile():
  global myrcfile, rcfile_compat, rcfile_version
  global tid_update_rc_sec, tid_update_rc_min, rc_file_error
  global tlb_history, tlb_hist_maxlen, tlb_case, tlb_regexp, tlb_hall
  global dlg_mark_geom, dlg_hist_geom, dlg_srch_geom, dlg_tags_geom, main_win_geom
  global patlist, col_palette, tick_pat_sep, tick_pat_num, tick_str_prefix
  global font_content, col_bg_content, col_fg_content, fmt_find, fmt_findinc
  global fmt_selection, load_buf_size

  if tid_update_rc_sec: tk.after_cancel(tid_update_rc_sec)
  if tid_update_rc_min: tk.after_cancel(tid_update_rc_min)
  tid_update_rc_min = None

  try:
    with tempfile.NamedTemporaryFile(mode="w", delete=False, dir=os.path.dirname(myrcfile),
                                     prefix=myrcfile, suffix=".tmp") as rcfile:
      timestamp = str(datetime.now())
      print("#\n"
            "# trowser configuration file\n"
            "#\n"
            "# This file is automatically generated - do not edit\n"
            "# Written at: %s\n"
            "#\n" % timestamp, file=rcfile, end="")

      # dump software version
      print("rcfile_version=", json.dumps(rcfile_version), file=rcfile)
      print("rc_compat_version=", json.dumps(rcfile_compat), file=rcfile)
      print("rc_timestamp=", json.dumps(timestamp), file=rcfile)

      # dump highlighting patterns
      print("patlist=", json.dumps(patlist), file=rcfile)

      # dump color palette
      print("col_palette=", json.dumps(col_palette), file=rcfile)

      # frame number parser patterns
      print("tick_pat_sep=", json.dumps(tick_pat_sep), file=rcfile)
      print("tick_pat_num=", json.dumps(tick_pat_num), file=rcfile)
      print("tick_str_prefix=", json.dumps(tick_str_prefix), file=rcfile)

      # dump search history
      # (renamed from "tlb_hist" in v1.3 due to format change)
      print("tlb_history=", json.dumps(tlb_history), file=rcfile)

      # dump search settings
      print("tlb_case=", json.dumps(tlb_case.get()), file=rcfile)
      print("tlb_regexp=", json.dumps(tlb_regexp.get()), file=rcfile)
      print("tlb_hall=", json.dumps(tlb_hall.get()), file=rcfile)
      print("tlb_hist_maxlen=", json.dumps(tlb_hist_maxlen), file=rcfile)

      # dialog sizes
      print("win_geom:dlg_mark=", json.dumps(win_geom["dlg_mark"]), file=rcfile)
      print("win_geom:dlg_hist=", json.dumps(win_geom["dlg_hist"]), file=rcfile)
      print("win_geom:dlg_srch=", json.dumps(win_geom["dlg_srch"]), file=rcfile)
      print("win_geom:dlg_tags=", json.dumps(win_geom["dlg_tags"]), file=rcfile)
      print("win_geom:dlg_help=", json.dumps(win_geom["dlg_help"]), file=rcfile)
      print("win_geom:main_win=", json.dumps(win_geom["main_win"]), file=rcfile)

      # font and color settings
      print("font_content=", json.dumps(font_content.configure()), file=rcfile)
      print("col_bg_content=", json.dumps(col_bg_content), file=rcfile)
      print("col_fg_content=", json.dumps(col_fg_content), file=rcfile)
      print("fmt_find=", json.dumps(fmt_find), file=rcfile)
      print("fmt_findinc=", json.dumps(fmt_findinc), file=rcfile)
      print("fmt_selection=", json.dumps(fmt_selection), file=rcfile)

      # misc (note the head/tail mode is omitted intentionally)
      print("load_buf_size=", json.dumps(load_buf_size), file=rcfile)

    # copy attributes on the new file
    try:
      st = os.stat(myrcfile)
      try:
        os.chmod(rcfile.name, st.st_mode & 0o777)
        if (os.name == "posix"):
          os.chown(rcfile.name, st.st_uid, st.st_gid)
      except OSError as e:
        print("Warning: Failed to update mode/permissions on %s: %s" % (myrcfile, e.strerror), file=sys.stderr)
    except OSError as e:
      pass

    # move the new file over the old one
    try:
      # MS-Windows does not allow renaming when the target file already exists,
      # so we need to remove the target first. DISADVANTAGE: operation is not atomic
      if (os.name != "posix"):
        try:
          os.remove(myrcfile)
        except OSError:
          pass
      os.rename(rcfile.name, myrcfile)
      rc_file_error = False
    except OSError as e:
      if not rc_file_error:
        messagebox.showerror(message="Could not replace rc file %s: %s" % (myrcfile, e.strerror), title="Trace browser")
      os.remove(rcfile.name)
      rc_file_error = True

  except OSError as e:
    # write error - remove the file fragment, report to user
    if not rc_file_error:
      messagebox.showerror(message="Failed to write file %s: %s" % (myrcfile, e.strerror), title="Trace browser")
      rc_file_error = True
    os.remove(rcfile.name)


#
# This function is used to trigger writing the RC file after changes.
# The write is delayed by a few seconds to avoid writing the file multiple
# times when multiple values are changed. This timer is restarted when
# another change occurs during the delay, however only up to a limit.
#
def UpdateRcAfterIdle():
  global tid_update_rc_sec, tid_update_rc_min

  if tid_update_rc_sec: tk.after_cancel(tid_update_rc_sec)
  tid_update_rc_sec = tk.after(3000, UpdateRcFile)

  if not tid_update_rc_min:
    tid_update_rc_min = tk.after(60000, UpdateRcFile)


def GetRcFilePath():
    if (os.name == "posix"):
        xdg_config_home = os.environ.get("XDG_CONFIG_HOME")
        home = os.path.expanduser("~")

        if xdg_config_home is not None and os.path.exists(xdg_config_home):
            rc_file = os.path.join(xdg_config_home, "trowser", "trowser.py.rc")

        elif home is not None and os.path.exists(home):
            config_dir = os.path.join(home, ".config")
            if os.path.exists(config_dir) and os.path.isdir(config_dir):
                rc_file = os.path.join(home, ".config", "trowser", "trowser.py.rc")
            else:
                rc_file = os.path.join(home, ".trowser.py.rc")

        else:
            rc_file = ".trowser.py.rc"

        os.makedirs(os.path.dirname(rc_file), exist_ok=True)

    else: # TODO win32
        rc_file = "trowser.ini"

    return rc_file


# ----------------------------------------------------------------------------
#
# This function is called when the program is started with -help to list all
# possible command line options.
#
def PrintUsage(argvn="", reason=""):
  if argvn != "":
    print("%s: %s: %s" % (sys.argv[0], reason, argvn), file=sys.stderr)

  print("Usage: %s [options] {file|-}" % sys.argv[0], file=sys.stderr)

  if argvn != "":
    print("Use -h or --help for a list of options", file=sys.stderr)
  else:
    print("The following options are available:", file=sys.stderr)
    print("  --head=size\t\tLoad <size> bytes from the start of the file", file=sys.stderr)
    print("  --tail=size\t\tLoad <size> bytes from the end of the file", file=sys.stderr)
    print("  --rcfile=<path>\tUse alternate config file (default: ~/.trowserc)", file=sys.stderr)

  sys.exit(1)


#
# This helper function checks if a command line flag which requires an
# argument is followed by at least another word on the command line.
#
def ParseArgvLenCheck(arg_idx):
  if arg_idx + 1 >= len(sys.argv):
    PrintUsage(sys.argv[arg_idx], "this option requires an argument")


#
# This helper function reads an integer value from a command line parameter
#
def ParseArgInt(opt, val):
  try:
    return int(val)
  except:
    PrintUsage(opt, "\"%s\" is not a numerical value" % val)

#
# This function parses and evaluates the command line arguments.
#
def ParseArgv():
  global load_file_mode, load_buf_size_opt

  file_seen = False
  arg_idx = 1
  while arg_idx < len(sys.argv):
    arg = sys.argv[arg_idx]

    if arg.startswith("-") and (arg != "-"):
      if arg == "-t":
        ParseArgvLenCheck(arg_idx)
        arg_idx += 1
        load_buf_size_opt = ParseArgInt(arg, sys.argv[arg_idx])
        load_file_mode = 1

      elif arg.startswith("--tail"):
        match = re.match("^--tail=(.+)$", arg)
        if match:
          load_buf_size_opt = ParseArgInt(arg, match.group(1))
          load_file_mode = 1
        else:
          PrintUsage(arg, "requires a numerical argument (e.g. --tail=10000000)")

      elif arg == "-h":
        ParseArgvLenCheck(arg_idx)
        arg_idx += 1
        load_buf_size_opt = ParseArgInt(arg, sys.argv[arg_idx])
        load_file_mode = 0

      elif arg.startswith("--head"):
        match = re.match("^--head=(.+)$", arg)
        if match:
          load_buf_size_opt = ParseArgInt(arg, match.group(1))
          load_file_mode = 0
        else:
          PrintUsage(arg, "requires a numerical argument (e.g. --head=10000000)")

      elif arg == "-r":
        if arg_idx + 1 < len(sys.argv):
          arg_idx += 1
          myrcfile = sys.argv[arg_idx]
        else:
          PrintUsage(arg, "this option requires an argument")

      elif arg.startswith("--rcfile"):
        match = re.match("^--rcfile=(.+)$", arg)
        if match:
          myrcfile = match.group(0)
        else:
          PrintUsage(arg, "requires a path argument (e.g. --rcfile=foo/bar)")

      elif arg == "-?" or arg == "--help":
        PrintUsage()

      else:
        PrintUsage(arg, "unknown option")

    else:
      if arg_idx + 1 >= len(sys.argv):
        file_seen = True
      else:
        arg_idx += 1
        PrintUsage(sys.argv[arg_idx], "only one file name expected")

    arg_idx += 1

  if not file_seen:
    print("File name missing (use \"-\" for stdin)", file=sys.stderr)
    PrintUsage()


# ----------------------------------------------------------------------------
# This section defines a "dummy" class which is used to emulate a global
# namespace for widgets equivalently to Tcl/Tk. Every created widget is
# assigned to a variable within this static namespace (i.e. the class is
# never instantiated), so that all other functions have access. Note this
# design is chosen only for backwards-compatibility.
#
class wt:
  # Instantiate all top-level widgets as "None" to allow checking if the
  # respective dialog is open via static method wt_exists(WID) below.
  about = None
  hipro = None
  stline = None
  srch_abrt = None
  dlg_load = None
  dlg_srch_slpro = None
  dlg_srch = None
  dlg_hist = None
  dlg_key_e = None
  dlg_mark = None
  dlg_mark_mren = None
  dlg_font = None
  dlg_tags = None

#
# This method checks if the dialog using the given top-level widget is
# currently open.
#
def wt_exists(obj):
  if obj is not None:
    try:
      obj.configure()
      return True
    except Exception as e:
      return False
    return False

# ----------------------------------------------------------------------------
#
# Global variables
#
# IMPORTANT NOTE: A lot of the variables definitions below are copied into
# the personal configuration files. Hence the assignments below are only
# defaults for the very first use of this program. Later on the values are
# overridden by the rc file contents.

# This variable contains the search string in the "find" entry field, i.e.
# it's automatically updated when the user modifies the text in the widget.
# A variable trace is used to trigger incremental searches after each change.
tlb_find = ""

# This variable is used to remember search pattern and options while a
# background search highlighting is active and afterwards to avoid
# unnecessarily repeating the search while the pattern is unchanged.
# The pattern is set to an empty string when no highlights are shown.
tlb_cur_hall_opt = ["", []]

# This variable contains the stack of previously used search expressions
# The top of the stack, aka the most recently used expression, is at the
# front of the list. Each element is a list with the following elements:
# 0: sub-string or regular expression
# 1: reg.exp. yes/no:=1/0
# 2: match case yes/no:=1/0
# 3: timestamp of last use
tlb_history = []

# This variable defines the maximum length of the search history list.
# This configuration option currently can only be set here.
tlb_hist_maxlen = 50

# These variables contain search options which can be set by checkbuttons.
tlb_case = False
tlb_regexp = False
tlb_hall = False

# This variable stores the search direction: 0:=backwards, 1:=forwards
tlb_last_dir = 1

# This variable indicates if the search entry field has keyboard focus.
tlb_find_focus = 0

# This variable contains the name of the widget which had input focus before
# the focus was moved into the search entry field. Focus will return there
# after Return or Escape
tlb_last_wid = None

# These variables hold the cursor position and Y-view from before the start of an
# incremental search (they are used to move the cursor back to the start position)
tlb_inc_base = None
tlb_inc_view = None

# These variables are used when cycling through the search history via the up/down
# keys in the search text entry field. They hold the current index in the history
# stack and the prefix string (i.e. the text in the entry field from before
# opening the history.)
tlb_hist_pos = None
tlb_hist_prefix = None

# This variable is used to parse multi-key command sequences.
last_key_char = ""

# This list remembers cursor positions preceding "large jumps" (i.e. searches,
# or positioning commands "G", "H", "L", "M" etc.) Used to allow jumping back.
# The second variable is used when jumping back and forward inside the list.
cur_jump_stack = []
cur_jump_idx = -1

# This hash array stores the bookmark list. Array keys are text line numbers,
# the values are the bookmark text (i.e. initially a copy of the bookmarked
# line of text)
mark_list = {}

# This variable tracks if the marker list was changed since the last save.
# This is used to offer automatic save upon quit.
mark_list_modified = False

# These variables are used by the bookmark list dialog.
dlg_mark_list = []

# These variables hold IDs of timers and background tasks (i.e. scripts delayed
# by "after")  They are used to cancel the scripts when necessary.
tid_search_inc = None
tid_search_list = None
tid_search_hall = None
tid_high_init = None
tid_update_rc_sec = None
tid_update_rc_min = None
tid_status_line = None
tid_resume_bg = None

# This variable is incremented for temporarily suspending background tasks
# while an interactive operation is performed (e.g. a dialog window is opened.)
# The variable is decremented at the end of that task. The last decrement to
# zero is done from within an "after idle" to make sure display updates are
# completed. The second variable is for debugging an imbalance of increments
# and decrements.
block_bg_tasks = 0
block_bg_caller = []

# This array contains key bindings for different dialog windows.
key_cmd_reg = {}
last_inline_char = None
last_inline_dir = None

# This variable holds the status of the status message popup: When None, the
# popup is not shown. Else it refers to a string indicating the originator,
# which is used to allow clearing the message before the regular expiry time.
status_line_topic = None

# These variable are set to True while the respective dialog is open. The
# variables are reset to False via widget destruction callback. The variables
# are checked in various dialog handler functions and hooks to skip processing
# in case the dialog is no longer open.
dlg_cols_shown = False
dlg_fmt_shown = False
dlg_font_shown = False
dlg_hist_shown = False
dlg_load_shown = False
dlg_mark_shown = False
dlg_srch_shown = False
dlg_tags_shown = False

# These variables hold the font and color definitions for the main text content.
font_content_default = "TkFixedFont"
col_bg_content = "#e2e2e8"
col_fg_content = "#000000"

# These variables hold the markup definitions for search match highlighting
# and selected text in the main window and dialogs. (Note the selection mark-up
# has lower precedence than color highlighting for mark-up types which are
# used in both. Search highlighting in contrary has precedence over all others.)
# The structure is the same as in "patlist" (except for the elements which
# relate to search pattern and options, which are unused and undefined here)
fmt_find = ["", 0, 0, "", "", "", "#faee0a", "", 0, 0, 0, "", "", "", 1, 0]
fmt_findinc = ["", 0, 0, "", "", "", "#c8ff00", "", 0, 0, 0, "", "", "", 1, 0]
fmt_selection = ["", 0, 0, "", "", "", "#c3c3c3", "", 0, 0, 0, "gray50", "", "raised", 2, 0]

# These variables define the initial geometry of the main and dialog windows.
win_geom = { "main_win": "684x480",
             "dlg_help": "648x480",
             "dlg_mark": "500x250",
             "dlg_tags": "400x300",
             "dlg_hist": "400x250",
             "dlg_srch": "648x250" }

# This list contains pre-defined colors values for color-highlighting.
col_palette = [
  "#000000",
  "#4acbb5",
  "#94ff80",
  "#b4e79c",
  "#bee1be",
  "#bfffb3",
  "#b3beff",
  "#96d9ff",
  "#b3fff3",
  "#ccffff",
  "#dab3d9",
  "#dab3ff",
  "#c180ff",
  "#e7b3ff",
  "#e6b3ff",
  "#e6ccff",
  "#e6b3d9",
  "#e73c39",
  "#ff6600",
  "#ff7342",
  "#ffb439",
  "#efbf80",
  "#ffbf80",
  "#ffd9b3",
  "#f0b0a0",
  "#ffb3be",
  "#e9ff80",
  "#f2ffb3",
  "#eeee8e",
  "#ffffff"]

# These variables hold patterns which are used to parse frame numbers or
# timestamps out of the text content, for use in the "Search list" dialog: The
# first two are regular expressions, the third a plain string; set patterns to
# empty strings to disable the feature.  Either specify both patterns as
# non-empty, or only the first. Both patterns have to include a capture (i.e.
# parenthesis), which will be used to extract the portion of text to be
# displayed as marker, when enabled via "Options" menu in the search list
# dialog. The extracted text may also be non-numerical, but then the "delta"
# feature will obviously not work. Regular expressions may only use syntax
# that is valid both in Tcl/Tk "regexp" (see "re_syntax" documentation) and
# Python "re".
tick_pat_num = ""
tick_pat_sep = ""
tick_str_prefix = ""

# This list stores text patterns and associated colors for color-highlighting
# in the main text window. Each list entry is again a list:
# 0: sub-string or regular expression
# 1: reg.exp. yes/no:=1/0
# 2: match case yes/no:=1/0
# 3: group tag (currently unused)
# 4: text tag name (arbitrary, but unique in list)
# 5: reserved, empty
# 6: background color
# 7: foreground color
# 8: bold yes/no:=1/0
# 9: underline yes/no:=1/0
# 10: overstrike yes/no:=1/0
# 11: background bitmap name ("", gray75, gray50, gray25, gray12)
# 12: foreground bitmap name ("", gray75, gray50, gray25, gray12)
# 13: relief: "", raised, sunken, ridge, groove
# 14: relief borderwidth: 1,2,...,9
# 15: spacing: 0,1,2,...
patlist = [
  [": Failure", False, True, "default", "tag0", "", "#e73c39", "", True, False, False, "", "", "", 1, 0],
  ["^\\[ ", True, True, "default", "tag1", "", "#b4e79c", "", False, False, False, "", "", "", 1, 0]
]

# This variable contains the mode and limit for file load. The mode can be
# 0 for "head" or 1 for "tail" (i.e. load data from end of the file). The
# values can be changed by the "head" and "tail" command line options.
load_file_mode = 0
load_buf_size = 0x100000

# define RC file version limit for forwards compatibility
rcfile_compat = 0x02000001
rcfile_version = 0x02010000
rc_file_error = 0

#
# Main
#
try:
  tk = Tk(className="trowser")
  # withdraw main window until fully populated below; needed in case of error popup during startup
  tk.wm_withdraw()
except:
  # this error occurs when the display connection is refused etc.
  print("Tk initialization failed", file=sys.stderr)
  sys.exit(1)

# convert into Tk variables (i.e. variables accessed by widgets or vwait)
tlb_case = BooleanVar(tk, tlb_case)
tlb_regexp = BooleanVar(tk, tlb_regexp)
tlb_hall = BooleanVar(tk, tlb_hall)
tlb_find = StringVar(tk, tlb_find)
font_content = tkf.nametofont(font_content_default)

# Parse command line parameters & load configuration options
myrcfile = GetRcFilePath()
ParseArgv()
LoadRcFile()

InitResources()
CreateMainWindow()
HighlightCreateTags()
tk.wm_deiconify()

if sys.argv[-1] == "-":
  cur_filename = ""
  load_pipe = LoadPipe()
  load_pipe.LoadPipe_Start()
else:
  LoadFile(sys.argv[-1])

# done - all following actions are event-driven
# the application exits when the main window is closed
try:
  tk.mainloop()
except KeyboardInterrupt:
  pass
