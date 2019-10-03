#!/bin/sh
# the next line restarts using wish \
exec wish "$0" -- "$@"

# ------------------------------------------------------------------------ #
# Copyright (C) 2007-2009 Tom Zoerner
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
# $Id: trowser.tcl,v 1.30 2009/03/12 21:11:08 tom Exp $
# ------------------------------------------------------------------------ #


#
# This function is used during start-up to define fonts, colors and
# global event binding tags.
#
proc InitResources {} {
  global font_normal font_bold

  # override default font for the Tk message box
  option add *Dialog.msg.font {helvetica 9 bold} userDefault

  # fonts for text and label widgets
  set font_normal {helvetica 9 normal}
  set font_bold {helvetica 9 bold}

  # bindings to allow scrolling a text widget with the mouse wheel
  bind TextWheel <Button-4>     {%W yview scroll -3 units}
  bind TextWheel <Button-5>     {%W yview scroll 3 units}
  bind TextWheel <MouseWheel>   {%W yview scroll [expr {- (%D / 120) * 3}] units}

  # bindings for a read-only text widget
  # copy allowed bindings from the regular text widget (i.e. move, mark, copy)
  foreach event {<ButtonPress-1> <ButtonRelease-1> <B1-Motion> <Double-Button-1> <Shift-Button-1> \
                 <Triple-Button-1> <Triple-Shift-Button-1> <Button-2> <B2-Motion> \
                 <<Copy>> <<Clear>> <Shift-Key-Tab> <Control-Key-Tab> <Control-Shift-Key-Tab> \
                 <Key-Prior> <Key-Next> <Key-Down> <Key-Up> <Key-Left> <Key-Right> \
                 <Shift-Key-Left> <Shift-Key-Right> <Shift-Key-Up> <Shift-Key-Down> \
                 <Shift-Key-Next> <Shift-Key-Prior> <Control-Key-Down> <Control-Key-Up> \
                 <Control-Key-Left> <Control-Key-Right> <Control-Key-Next> <Control-Key-Prior> \
                 <Key-Home> <Key-End> <Shift-Key-Home> <Shift-Key-End> <Control-Key-Home> \
                 <Control-Key-End> <Control-Shift-Key-Home> <Control-Shift-Key-End> \
                 <Control-Shift-Key-Left> <Control-Shift-Key-Right> <Control-Shift-Key-Down>
                 <Control-Shift-Key-Up> <Control-Key-slash>} {
    bind TextReadOnly $event [bind Text $event]
  }
  # bindings for a selection text widget (listbox emulation)
  # (uses non-standard cursor movement event bindings, hence not added here)
  foreach event {<Button-2> <B2-Motion> <Key-Prior> <Key-Next> \
                 <Shift-Key-Tab> <Control-Key-Tab> <Control-Shift-Key-Tab>} {
    bind TextSel $event [bind Text $event]
  }
  foreach event {<Button-4> <Button-5> <MouseWheel>} {
    bind TextReadOnly $event [bind TextWheel $event]
    bind TextSel $event [bind TextWheel $event]
  }
  bind TextReadOnly <Control-Key-c> [bind Text <<Copy>>]
  bind TextReadOnly <Key-Tab> [bind Text <Control-Key-Tab>]
  bind TextSel <Key-Tab> [bind Text <Control-Key-Tab>]

  # bookmark image which is inserted into the text widget
  global img_marker
  set img_marker [image create photo -data R0lGODlhBwAHAMIAAAAAuPj8+Hh8+JiYmDAw+AAAAAAAAAAAACH5BAEAAAEALAAAAAAHAAcAAAMUGDGsSwSMJ0RkpEIG4F2d5DBTkAAAOw==]
}


#
# This function creates the main window of the trace browser, including the
# menu at the top, the text area and a vertical scrollbar in the middle, and
# the search control dialog at the bottom.
#
proc CreateMainWindow {} {
  global font_content col_bg_content col_fg_content fmt_selection
  global main_win_geom fmt_find fmt_findinc
  global tlb_find tlb_hall tlb_case tlb_regexp

  # menubar at the top of the main window
  menu .menubar
  . config -menu .menubar
  .menubar add cascade -label "Control" -menu .menubar.ctrl -underline 0
  .menubar add cascade -label "Search" -menu .menubar.search -underline 0
  .menubar add cascade -label "Bookmarks" -menu .menubar.mark -underline 0
  .menubar add cascade -label "Help" -menu .menubar.help
  menu .menubar.ctrl -tearoff 0 -postcommand MenuPosted
  .menubar.ctrl add command -label "Open file..." -command MenuCmd_OpenFile
  .menubar.ctrl add command -label "Reload current file" -state disabled -command MenuCmd_Reload
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Discard above cursor..." -command {MenuCmd_Discard 0}
  .menubar.ctrl add command -label "Discard below cursor..." -command {MenuCmd_Discard 1}
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Font selection..." -command FontList_OpenDialog
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Quit" -command {UserQuit; update}
  menu .menubar.search -tearoff 0 -postcommand MenuPosted
  .menubar.search add command -label "Search history..." -command SearchHistory_Open
  .menubar.search add command -label "Edit highlight patterns..." -command TagList_OpenDialog
  .menubar.search add separator
  .menubar.search add command -label "List all search matches..." -command {SearchAll 1 0} -accelerator "ALT-a"
  .menubar.search add command -label "List all matches above..." -command {SearchAll 1 -1} -accelerator "ALT-P"
  .menubar.search add command -label "List all matches below..." -command {SearchAll 1 1} -accelerator "ALT-N"
  .menubar.search add separator
  .menubar.search add command -label "Clear search highlight" -command {SearchHighlightClear} -accelerator "&"
  .menubar.search add separator
  .menubar.search add command -label "Goto line number..." -command {KeyCmd_OpenDialog goto}
  menu .menubar.mark -tearoff 0 -postcommand MenuPosted
  .menubar.mark add command -label "Toggle bookmark" -accelerator "m" -command Mark_ToggleAtInsert
  .menubar.mark add command -label "List bookmarks" -command MarkList_OpenDialog
  .menubar.mark add command -label "Delete all bookmarks" -command Mark_DeleteAll
  .menubar.mark add separator
  .menubar.mark add command -label "Jump to prev. bookmark" -command {Mark_JumpNext 0} -accelerator "'-"
  .menubar.mark add command -label "Jump to next bookmark" -command {Mark_JumpNext 1} -accelerator "'+"
  .menubar.mark add separator
  .menubar.mark add command -label "Read bookmarks from file..." -command Mark_ReadFileFrom
  .menubar.mark add command -label "Save bookmarks to file..." -command Mark_SaveFileAs
  menu .menubar.help -tearoff 0 -postcommand MenuPosted
  .menubar.help add command -label "About" -command OpenAboutDialog

  # frame #1: text widget and scrollbar
  frame .f1
  text .f1.t -width 1 -height 1 -wrap none -yscrollcommand {.f1.sb set} \
             -font $font_content -background $col_bg_content -foreground $col_fg_content \
             -cursor top_left_arrow -relief flat -exportselection 1
  pack .f1.t -side left -fill both -expand 1
  scrollbar .f1.sb -orient vertical -command {.f1.t yview} -takefocus 0
  pack .f1.sb -side left -fill y
  pack .f1 -side top -fill both -expand 1

  focus .f1.t
  # note: order is important: "find" must be lower than highlighting tags
  eval [linsert [HighlightConfigure $fmt_find] 0 .f1.t tag configure find]
  eval [linsert [HighlightConfigure $fmt_findinc] 0 .f1.t tag configure findinc]
  .f1.t tag configure margin -lmargin1 17
  .f1.t tag configure bookmark -lmargin1 0
  eval [linsert [HighlightConfigure $fmt_selection] 0 .f1.t tag configure sel]
  .f1.t tag lower sel

  bindtags .f1.t {.f1.t TextReadOnly . all}

  # commands to scroll the X/Y view
  KeyBinding_UpDown .f1.t
  KeyBinding_LeftRight .f1.t

  # commands to move the cursor
  bind .f1.t <Key-Home> {if {%s == 0} {CursorSetColumn .f1.t left; KeyClr; break}}
  bind .f1.t <Key-End> {if {%s == 0} {CursorSetColumn .f1.t right; KeyClr; break}}
  bind .f1.t <Key-space> {event generate %W <Key-Right>; KeyClr; break}
  bind .f1.t <Key-BackSpace> {event generate %W <Key-Left>; KeyClr; break}
  KeyCmdBind .f1.t "h" {event generate .f1.t <Left>}
  KeyCmdBind .f1.t "l" {event generate .f1.t <Right>}
  KeyCmdBind .f1.t "Return" {CursorMoveLine .f1.t 1}
  KeyCmdBind .f1.t "w" {CursorMoveWord 1 0 0}
  KeyCmdBind .f1.t "e" {CursorMoveWord 1 0 1}
  KeyCmdBind .f1.t "b" {CursorMoveWord 0 0 0}
  KeyCmdBind .f1.t "W" {CursorMoveWord 1 1 0}
  KeyCmdBind .f1.t "E" {CursorMoveWord 1 1 1}
  KeyCmdBind .f1.t "B" {CursorMoveWord 0 1 0}
  KeyCmdBind .f1.t "ge" {CursorMoveWord 0 0 1}
  KeyCmdBind .f1.t "gE" {CursorMoveWord 0 1 1}
  KeyCmdBind .f1.t ";" {SearchCharInLine {} 1}
  KeyCmdBind .f1.t "," {SearchCharInLine {} -1}
  # commands for searching & repeating
  KeyCmdBind .f1.t "/" {SearchEnter 1}
  KeyCmdBind .f1.t "?" {SearchEnter 0}
  KeyCmdBind .f1.t "n" {SearchNext 1}
  KeyCmdBind .f1.t "N" {SearchNext 0}
  KeyCmdBind .f1.t "*" {SearchWord 1}
  KeyCmdBind .f1.t "#" {SearchWord 0}
  KeyCmdBind .f1.t "&" {SearchHighlightClear}
  bind .f1.t <Alt-Key-f> {focus .f2.e; KeyClr; break}
  bind .f1.t <Alt-Key-n> {SearchNext 1; KeyClr; break}
  bind .f1.t <Alt-Key-p> {SearchNext 0; KeyClr; break}
  bind .f1.t <Alt-Key-h> {SearchHighlightOnOff; KeyClr; break}
  bind .f1.t <Alt-Key-a> {SearchAll 0 0; KeyClr; break}
  bind .f1.t <Alt-Key-N> {SearchAll 0 1; KeyClr; break}
  bind .f1.t <Alt-Key-P> {SearchAll 0 -1; KeyClr; break}
  # misc
  KeyCmdBind .f1.t "i" {SearchList_Open 0; SearchList_CopyCurrentLine}
  KeyCmdBind .f1.t "u" SearchList_Undo
  bind .f1.t <Control-Key-r> SearchList_Redo
  bind .f1.t <Control-Key-g> {DisplayLineNumber; KeyClr; break}
  bind .f1.t <Control-Key-o> {CursorJumpHistory .f1.t -1; KeyClr; break}
  bind .f1.t <Control-Key-i> {CursorJumpHistory .f1.t 1; KeyClr; break}
  bind .f1.t <Double-Button-1> {if {%s == 0} {Mark_ToggleAtInsert; KeyClr; break}}
  KeyCmdBind .f1.t "m" {Mark_ToggleAtInsert}
  bind .f1.t <Alt-Key-w> {ToggleLineWrap; break}
  bind .f1.t <Control-plus> {ChangeFontSize 1; KeyClr}
  bind .f1.t <Control-minus> {ChangeFontSize -1; KeyClr}
  bind .f1.t <Control-Alt-Delete> DebugDumpAllState
  # catch-all (processes "KeyCmdBind" from above)
  bind .f1.t <FocusIn> {KeyClr}
  bind .f1.t <Return> {if {[KeyCmd .f1.t Return]} break}
  bind .f1.t <KeyPress> {if {[KeyCmd .f1.t %A]} break}

  # frame #2: search controls
  frame .f2 -borderwidth 2 -relief raised
  label .f2.l -text "Find:" -underline 0
  entry .f2.e -width 20 -textvariable tlb_find -exportselection false
  menu .f2.mh -tearoff 0
  button .f2.bn -text "Next" -command {SearchNext 1} -underline 0 -pady 2
  button .f2.bp -text "Prev." -command {SearchNext 0} -underline 0 -pady 2
  button .f2.bl -text "All" -command {SearchAll 1 0} -underline 0 -pady 2
  checkbutton .f2.bh -text "Highlight all" -variable tlb_hall -command SearchHighlightSettingChange -underline 0
  checkbutton .f2.cb -text "Match case" -variable tlb_case -command SearchHighlightSettingChange -underline 6
  checkbutton .f2.re -text "Reg.Exp." -variable tlb_regexp -command SearchHighlightSettingChange -underline 4
  pack .f2.l .f2.e .f2.bn .f2.bp .f2.bl .f2.bh .f2.cb .f2.re -side left -anchor w -padx 1
  pack configure .f2.e -fill x -expand 1
  pack .f2 -side top -fill x

  bind .f2.e <Escape> {SearchAbort; break}
  bind .f2.e <Return> {SearchReturn; break}
  bind .f2.e <FocusIn> {SearchInit}
  bind .f2.e <FocusOut> {SearchLeave}
  bind .f2.e <Control-n> {SearchIncrement 1 0; break}
  bind .f2.e <Control-N> {SearchIncrement 0 0; break}
  bind .f2.e <Key-Up> {Search_BrowseHistory 1; break}
  bind .f2.e <Key-Down> {Search_BrowseHistory 0; break}
  bind .f2.e <Control-d> {Search_Complete; break}
  bind .f2.e <Control-D> {Search_CompleteLeft; break}
  bind .f2.e <Control-x> {Search_RemoveFromHistory; break}
  bind .f2.e <Control-c> {SearchAbort; break}
  # disabled in v1.2 because of possible conflict with misconfigured backspace key
  #bind .f2.e <Control-h> {TagList_AddSearch .; break}
  #bind .f2.e <Control-H> {SearchHistory_Open; break}
  bind .f2.e <Alt-Key-n> {SearchNext 1; break}
  bind .f2.e <Alt-Key-p> {SearchNext 0; break}
  bind .f2.e <Alt-Key-a> {SearchAll 0 0; break}
  bind .f2.e <Alt-Key-N> {SearchAll 0 1; break}
  bind .f2.e <Alt-Key-P> {SearchAll 0 -1; break}
  bind .f2.e <Alt-Key-c> {set tlb_case [expr {!$tlb_case}]; SearchHighlightSettingChange; break}
  bind .f2.e <Alt-Key-e> {set tlb_regexp [expr {!$tlb_regexp}]; SearchHighlightSettingChange; break}
  trace add variable tlb_find write SearchVarTrace

  wm protocol . WM_DELETE_WINDOW UserQuit
  wm geometry . $main_win_geom
  wm positionfrom . user
  bind .f1.t <Configure> {ToplevelResized %W . .f1.t main_win_geom}
}


#
# This function creates the requested bitmaps if they don't exist yet
#
proc CreateButtonBitmap {args} {
  foreach img $args {
    if {[catch {image height $img}] != 0} {
      switch -exact $img {
        img_dropdown {
          # image for drop-down menu copied from combobox.tcl by Bryan Oakley
          image create bitmap img_dropdown -data \
            "#define down_arrow_width 15
            #define down_arrow_height 15
            static char down_arrow_bits[] = {
            0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,
            0x00,0x80,0xf8,0x8f,0xf0,0x87,0xe0,0x83,
            0xc0,0x81,0x80,0x80,0x00,0x80,0x00,0x80,
            0x00,0x80,0x00,0x80,0x00,0x80};"
        }
        img_down {
          image create bitmap img_down -data \
            "#define ptr_down_width 16
            #define ptr_down_height 14
            static unsigned char ptr_down_bits[] = {
            0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,
            0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,
            0xc0,0x01,0xf8,0x0f,0xf0,0x07,0xe0,0x03,
            0xc0,0x01,0x80,0x00};"
        }
        img_up {
          image create bitmap img_up -data \
            "#define ptr_up_width 16
            #define ptr_up_height 14
            static unsigned char ptr_up_bits[] = {
            0x80,0x00,0xc0,0x01,0xe0,0x03,0xf0,0x07,
            0xf8,0x0f,0xc0,0x01,0xc0,0x01,0xc0,0x01,
            0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,
            0xc0,0x01,0xc0,0x01};"
        }
      }
    }
  }
}


# ----------------------------------------------------------------------------
#
# This function is called during start-up to create tags for all color
# highlights.
#
proc HighlightCreateTags {} {
  global patlist

  foreach w $patlist {
    set tagnam [lindex $w 4]

    set cfg [HighlightConfigure $w]
    eval [linsert $cfg 0 .f1.t tag configure $tagnam]
    .f1.t tag lower $tagnam find
  }
}


#
# This function is called after loading a new text to apply the color
# highlighting to the complete text. This means all matches on all
# highlight patterns has to be searched for. Since this can take some
# time, the operation done in the background to avoid blocking the user.
# The CPU is given up voluntarily after each pattern and after max. 100ms
#
proc HighlightInit {} {
  global patlist tid_high_init

  if {[info commands .hipro] ne ""} {
    destroy .hipro
  }

  if {[llength $patlist] > 0} {
    # create a progress bar as overlay to the main window
    frame .hipro -takefocus 0 -relief sunken -borderwidth 2
    canvas .hipro.c -width 100 -height 10 -highlightthickness 0 -takefocus 0
    pack .hipro.c
    set cid [.hipro.c create rect 0 0 0 12 -fill {#0b1ff7} -outline {}]
    place .hipro -in .f1.t -anchor nw -x 0 -y 0

    .f1.t tag add margin 1.0 end

    # trigger highlighting for the 1st pattern in the background
    set tid_high_init [after 50 HighlightInitBg 0 $cid 0 0]
    .f1.t configure -cursor watch

    # apply highlighting on the text in the visible area (this is quick)
    # use the yview callback to redo highlighting in case the user scrolls
    Highlight_YviewRedirect 1
  }
}


#
# This function is a slave-function of proc HighlightInit. The function
# loops across all members in the global pattern list to apply color
# the respective highlighting. The loop is broken up by installing each
# new iteration as an idle event (and limiting each step to 100ms)
#
proc HighlightInitBg {pat_idx cid line loop_cnt} {
  global block_bg_tasks tid_search_inc tid_search_hall tid_search_list
  global patlist tid_high_init

  if {$block_bg_tasks || ($tid_search_inc ne {}) || ($tid_search_hall ne {}) || ($tid_search_list ne {})} {
    # background tasks are suspended - re-schedule with timer
    set tid_high_init [after 100 [list HighlightInitBg $pat_idx $cid $line 0]]
  } elseif {$loop_cnt > 10} {
    # insert a small timer delay to allow for idle-driven interactive tasks (e.g. selections)
    set tid_high_init [after 10 [list HighlightInitBg $pat_idx $cid $line 0]]

  } elseif {$pat_idx < [llength $patlist]} {
    set w [lindex $patlist $pat_idx]
    set tagnam [lindex $w 4]
    set opt [Search_GetOptions [lindex $w 0] [lindex $w 1] [lindex $w 2]]
    incr loop_cnt

    # here we do the actual work:
    # apply the tag to all matching lines of text
    set line [HighlightLines [lindex $w 0] $tagnam $opt $line]

    if {$line >= 0} {
      # not done yet - reschedule
      set tid_high_init [after idle [list HighlightInitBg $pat_idx $cid $line $loop_cnt]]

    } else {
      # trigger next tag
      incr pat_idx
      set tid_high_init [after idle [list HighlightInitBg $pat_idx $cid 1 $loop_cnt]]

      # update the progress bar
      catch {.hipro.c coords $cid 0 0 [expr {int(100*$pat_idx/[llength $patlist])}] 12}
    }
  } else {
    catch {destroy .hipro}
    .f1.t configure -cursor top_left_arrow
    Highlight_YviewRedirect 0
    set tid_high_init {}
  }
}


#
# This function searches for all lines in the main text widget which match the
# given pattern and adds the given tag to them.  If the loop doesn't complete
# within 100ms, the search is paused and the function returns the number of the
# last searched line.  In this case the caller must invoke the funtion again
# (as an idle event, to allow user-interaction in-between.)
#
proc HighlightLines {pat tagnam opt line} {
  set pos [.f1.t index end]
  scan $pos "%d.%d" max_line char
  set start_t [clock clicks -milliseconds]
  while {($line < $max_line) &&
         ([set pos [eval .f1.t search $opt -- {$pat} "$line.0" end]] ne {})} {
    # match found, highlight this line
    scan $pos "%d.%d" line char
    .f1.t tag add $tagnam "$line.0" "[expr {$line + 1}].0"
    # trigger the search result list dialog in case the line is included there too
    SearchList_HighlightLine $tagnam $line
    incr line

    # limit the runtime of the loop - return start line number for the next invocation
    if {([clock clicks -milliseconds] >= $start_t + 100) && ($line < $max_line)} {
      return $line
    }
  }
  # all done for this pattern
  return -1
}


#
# This helper function schedules the line highlight function until highlighting
# is complete for the given pattern.  This function is used to add highlighting
# for single tags (e.g. modified highlight patterns or colors; currently not used
# for search highlighting because a separate "cancel ID" is required.)
#
proc HighlightAll {pat tagnam opt {line 1}} {
  global tid_high_init block_bg_tasks

  if {$block_bg_tasks} {
    # background tasks are suspended - re-schedule with timer
    set tid_high_init [after 100 [list HighlightAll $pat $tagnam $opt $line]]
  } else {

    set line [HighlightLines $pat $tagnam $opt $line]
    if {$line >= 0} {
      set tid_high_init [after idle [list HighlightAll $pat $tagnam $opt $line]]
    } else {
      .f1.t configure -cursor top_left_arrow
      set tid_high_init {}
    }
  }
}


#
# This function searches the currently visible text content for all lines
# which contain the given sub-string and marks these lines with the given tag.
#
proc HighlightVisible {pat tagnam opt} {
  set start_pos [.f1.t index {@1,1}]
  set end_pos [.f1.t index "@[expr {[winfo width .f1.t] - 1}],[expr {[winfo height .f1.t] - 1}]"]
  scan $start_pos "%d.%d" line char
  scan $end_pos "%d.%d" max_line char
  #puts "visible $start_pos...$end_pos: $pat $opt"

  while {($line < $max_line) &&
         ([set pos [eval .f1.t search $opt -- {$pat} "$line.0" end]] ne {})} {
    scan $pos "%d.%d" line char
    .f1.t tag add $tagnam "$line.0" "[expr {$line + 1}].0"
    incr line
  }
}

#
# This callback is installed to the main text widget's yview. It is used
# to detect changes in the view to update highlighting if the initial
# highlighting task is not complete yet. The event is forwarded to the
# vertical scrollbar.
#
proc Highlight_YviewCallback {frac1 frac2} {
  global tid_high_init patlist

  if {$tid_high_init ne ""} {
    foreach w $patlist {
      set opt [Search_GetOptions [lindex $w 0] [lindex $w 1] [lindex $w 2]]
      HighlightVisible [lindex $w 0] [lindex $w 4] $opt
    }
  }
  .f1.sb set $frac1 $frac2
}

#
# This function redirect the yview callback from the scrollbar into the
# above function, or to undo the change. This is used to install a
# redirection for the duration of the initial highlighting task.
#
proc Highlight_YviewRedirect {enable} {
  if {$enable} {
    .f1.t configure -yscrollcommand Highlight_YviewCallback
  } else {
    .f1.t configure -yscrollcommand {.f1.sb set}
  }
}


#
# This function creates or updates a text widget tag with the options of
# a color highlight entry.  The function is called during start-up for all
# highlight patterns, and by the highlight edit dialog (also used for the
# sample text widget.)
#
proc HighlightConfigure {w} {
  global font_content

  set cfg {}
  if {[lindex $w 8]} {
    lappend cfg -font [DeriveFont $font_content 0 bold]
  } else {
    lappend cfg -font {}
  }
  if {[lindex $w 9]} {
    lappend cfg -underline [lindex $w 9]
  } else {
    lappend cfg -underline {}
  }
  if {[lindex $w 10]} {
    lappend cfg -overstrike [lindex $w 10]
  } else {
    lappend cfg -overstrike {}
  }
  if {[lindex $w 13] ne {}} {
    lappend cfg -relief [lindex $w 13]
    lappend cfg -borderwidth [lindex $w 14]
  } else {
    lappend cfg -relief {} -borderwidth {}
  }
  if {[lindex $w 15] > 0} {
    lappend cfg -spacing1 [lindex $w 15] -spacing3 [lindex $w 15]
  } else {
    lappend cfg -spacing1 {} -spacing3 {}
  }
  lappend cfg -background [lindex $w 6]
  lappend cfg -foreground [lindex $w 7]
  lappend cfg -bgstipple [lindex $w 11]
  lappend cfg -fgstipple [lindex $w 12]

  return $cfg
}


#
# This function clears the current search color highlighting without
# resetting the search string. It's bound to the "&" key, but also used
# during regular search reset.
#
proc SearchHighlightClear {} {
  global tlb_last_hall tid_search_hall

  after cancel $tid_search_hall
  set tid_search_hall {}
  .f1.t configure -cursor top_left_arrow

  .f1.t tag remove find 1.0 end
  .f1.t tag remove findinc 1.0 end
  set tlb_last_hall {}

  SearchList_HighlightClear
}


#
# This function triggers color highlighting of all lines of text which match
# the current search string.  The function is called when global highlighting
# is en-/disabled, when the search string is modified or when search options
# are changed.
#
proc SearchHighlightUpdate {} {
  global tlb_find tlb_regexp tlb_case tlb_hall tlb_last_hall
  global tid_search_hall tlb_last_hall

  if {$tlb_find ne ""} {
    if {$tlb_hall} {
      if {[SearchExprCheck $tlb_find $tlb_regexp 1]} {
        set opt [Search_GetOptions $tlb_find $tlb_regexp $tlb_case]

        HighlightVisible $tlb_find find $opt

        if {$tlb_last_hall ne $tlb_find} {
          if {[focus -displayof .] ne ".f2.e"} {

            # display "busy" cursor until highlighting is finished
            .f1.t configure -cursor watch

            # kill background highlight process for obsolete pattern
            after cancel $tid_search_hall

            # start highlighting in the background
            set tlb_last_hall {}
            set tid_search_hall [after 10 [list SearchHighlightAll $tlb_find find $opt]]
          }
        } else {
          HighlightVisible $tlb_find find $opt
        }
      }
    } else {
      SearchHighlightClear
    }
  }
}


#
# This helper function calls the global search highlight function until
# highlighting is complete.
#
proc SearchHighlightAll {pat tagnam opt {line 1}} {
  global tid_search_hall tlb_last_hall

  set line [HighlightLines $pat $tagnam $opt $line]
  if {$line >= 0} {
    set tid_search_hall [after idle [list SearchHighlightAll $pat $tagnam $opt $line]]
  } else {
    set tid_search_hall {}
    .f1.t configure -cursor top_left_arrow
    set tlb_last_hall $pat
  }
}



#
# This function is bound to the "Highlight all" checkbutton to en- or disable
# global highlighting.
#
proc SearchHighlightOnOff {} {
  global tlb_hall

  set tlb_hall [expr {!$tlb_hall}]
  UpdateRcAfterIdle

  SearchHighlightUpdate
}


#
# This function is invoked after a change in search settings (i.e. case
# match, reg.exp. or global highlighting.)  The changed settings are
# stored in the RC file and a possible search highlighting is removed
# or updated (the latter only if global highlighting is enabled)
#
proc SearchHighlightSettingChange {} {
  global tlb_hall

  UpdateRcAfterIdle

  SearchHighlightClear
  if {$tlb_hall} {
    SearchHighlightUpdate
  }
}


#
# This function is invoked when the user enters text in the "find" entry field.
# In contrary to the "atomic" search, this function only searches a small chunk
# of text, then re-schedules itself as an "idle" task.  The search can be aborted
# at any time by canceling the task.
#
proc Search_Background {pat is_fwd opt start is_changed callback} {
  global block_bg_tasks tid_search_inc tid_search_list
  global tid_search_inc

  if {$block_bg_tasks} {
    # background tasks are suspended - re-schedule with timer
    set tid_search_inc [after 100 [list Search_Background $pat $is_fwd $opt $start $is_changed $callback]]
    return
  }

  if {$is_fwd} {
    set end [.f1.t index end]
  } else {
    set end "1.0"
  }
  if {$start ne $end} {
    if {$is_fwd} {
      set next [.f1.t index [concat $start + 5000 lines lineend]]
    } else {
      set next [.f1.t index [concat $start - 5000 lines linestart]]
    }
    # invoke the actual search in the text widget content
    set pos [eval .f1.t search $opt -count match_len -- {$pat} $start $next]

    if {$pos eq ""} {
      set tid_search_inc [after idle [list Search_Background $pat $is_fwd $opt $next $is_changed $callback]]
    } else {
      set tid_search_inc {}
      Search_HandleMatch $pos $match_len $pat $is_changed
      eval [list $callback $pos $pat $is_fwd $is_changed]
    }
  } else {
    set tid_search_inc {}
    Search_HandleMatch "" 0 $pat $is_changed
    eval [list $callback "" $pat $is_fwd $is_changed]
  }
}


#
# This function searches the main text content for the expression in the
# search entry field, starting at the current cursor position. When a match
# is found, the cursor is moved there and the line is highlighed.
#
proc Search_Atomic {pat is_re use_case is_fwd is_changed} {
  global tlb_hall tlb_last_hall tlb_last_dir

  set pos ""
  if {($pat ne "") && [SearchExprCheck $pat $is_re 1]} {

    set tlb_last_dir $is_fwd
    set search_opt [Search_GetOptions $pat $is_re $use_case $tlb_last_dir]
    set start_pos [Search_GetBase $is_fwd 0]
    CursorJumpPushPos .f1.t

    if {$is_fwd} {
      set search_range [list $start_pos [.f1.t index end]]
    } else {
      set search_range [list $start_pos "1.0"]
    }
    set match_len 0

    if {$start_pos ne [lindex $search_range 1]} {
      # invoke the actual search in the text widget content
      while 1 {
        set pos [eval .f1.t search $search_opt -count match_len -- {$pat} $search_range]

        # work-around for backwards search:
        # make sure the matching text is entirely to the left side of the cursor
        if {($pos ne "") && [SearchOverlapCheck $is_fwd $start_pos $pos $match_len]} {
          # match overlaps: search further backwards
          set search_range [lreplace $search_range 0 0 $pos]
          continue
        }
        break
      }
    } else {
      set pos ""
    }
    # update cursor position and highlight
    Search_HandleMatch $pos $match_len $pat $is_changed

  } else {
    # empty or invalid expression: just remove old highlights
    SearchReset
  }
  return $pos
}


#
# This helper function checks if the match returned for a backwards search
# overlaps the search start position (e.g. the match is 1 char left of the
# start pos, but 2 chars long)
#
proc SearchOverlapCheck {is_fwd start_pos pos match_len} {
  if {$is_fwd == 0} {
    # convert start position into numerical (e.g. may be "insert")
    if {([scan $start_pos "%d.%d" line1 char1] == 2) ||
        ([scan [.f1.t index $start_pos] "%d.%d" line1 char1] == 2)} {
      if {[scan $pos "%d.%d" line2 char2] == 2} {
        if {($line1 == $line2) && ($char2 + $match_len > $char1)} {
          return 1
        }
      }
    }
  }
  return 0
}


#
# This function handles the result of a text search in the main window.
# If a match was found, the cursor is moved to the start of the match and
# the word, line are highlighted. Optionally, a background process to
# highlight all matches is started.  If no match is found, any previously
# applies highlights are removed.
#
proc Search_HandleMatch {pos match_len pat is_changed} {
  global tlb_find_line tlb_hall tlb_last_hall

  if {($pos ne "") || $is_changed} {
    if {!$tlb_hall || ($tlb_last_hall ne $pat)} {
      SearchHighlightClear
    } else {
      .f1.t tag remove findinc 1.0 end
    }
  }
  if {$pos ne ""} {
    scan $pos "%d" tlb_find_line
    .f1.t see $pos
    .f1.t mark set insert $pos
    .f1.t tag add find "$tlb_find_line.0" "[expr {$tlb_find_line + 1}].0"
    if {$match_len > 0} {
      .f1.t tag add findinc $pos "$pos + $match_len chars"
    }

    SearchList_HighlightLine find $tlb_find_line
    SearchList_MatchView $tlb_find_line
  }
  if {$tlb_hall} {
    SearchHighlightUpdate
  }
}


#
# This function is bound to all changes of the search text in the
# "find" entry field. It's called when the user enters new text and
# triggers an incremental search.
#
proc SearchVarTrace {name1 name2 op} {
  global tid_search_inc
  global tlb_last_dir

  after cancel $tid_search_inc
  set tid_search_inc [after 50 SearchIncrement $tlb_last_dir 1]
}


#
# This function performs a so-called "incremental" search after the user
# has modified the search text. This means searches are started already
# while the user is typing.
#
proc SearchIncrement {is_fwd is_changed} {
  global tlb_find tlb_regexp tlb_case tlb_last_dir tlb_inc_base tlb_inc_view
  global tid_search_inc

  set tid_search_inc {}

  if {[focus -displayof .] eq ".f2.e"} {
    if {($tlb_find ne {}) && [SearchExprCheck $tlb_find $tlb_regexp 0]} {

      if {![info exists tlb_inc_base]} {
        set tlb_inc_base [Search_GetBase $is_fwd 1]
        set tlb_inc_view [list [lindex [.f1.t xview] 0] [lindex [.f1.t yview] 0]]
        CursorJumpPushPos .f1.t
      }
      if {$is_changed} {
        .f1.t tag remove findinc 1.0 end
        .f1.t tag remove find 1.0 end
        set start_pos $tlb_inc_base
        #.f1.t xview moveto [lindex $tlb_inc_view 0]
        #.f1.t yview moveto [lindex $tlb_inc_view 1]
        #.f1.t mark set insert $tlb_inc_base
        #.f1.t see insert
      } else {
        set start_pos [Search_GetBase $is_fwd 0]
      }
      set opt [Search_GetOptions $tlb_find $tlb_regexp $tlb_case $is_fwd]

      Search_Background $tlb_find $is_fwd $opt $start_pos $is_changed Search_IncMatch

    } else {
      SearchReset

      if {$tlb_find ne {}} {
        DisplayStatusLine search error "Incomplete or invalid reg.exp."
      } else {
        ClearStatusLine search
      }
    }
  }
}


#
# This function is invoked as callback after a background search for the
# incremental search in the entry field is completed.  (Before this call,
# cursor position and search highlights are already updated.)
#
proc Search_IncMatch {pos pat is_fwd is_changed} {
  global tlb_inc_base tlb_inc_view tlb_history tlb_hist_pos tlb_hist_prefix

  if {($pos eq "") && [info exists tlb_inc_base]} {
    if {$is_changed} {
      .f1.t xview moveto [lindex $tlb_inc_view 0]
      .f1.t yview moveto [lindex $tlb_inc_view 1]
      .f1.t mark set insert $tlb_inc_base
      .f1.t see insert
    }

    if {$is_fwd} {
      DisplayStatusLine search warn "No match until end of file"
    } else {
      DisplayStatusLine search warn "No match until start of file"
    }
  } else {
    ClearStatusLine search
  }

  if {[info exists tlb_hist_pos]} {
    set hl [lindex $tlb_history $tlb_hist_pos]
    if {$pat ne [lindex $hl 0]} {
      unset tlb_hist_pos tlb_hist_prefix
    }
  }
}


#
# This function checks if the search pattern syntax is valid
#
proc SearchExprCheck {pat is_re display} {
  global tlb_find tlb_regexp

  if {$is_re && [catch {regexp -- $pat ""} cerr]} {
    if {$display} {
      set pos [string last ":" $cerr]
      if {$pos >= 0} {
        set cerr [string trim [string range $cerr [expr {$pos + 1}] end]]
      }
      DisplayStatusLine search error "Syntax error in search expression: $cerr"
    }
    return 0
  } else {
    return 1
  }
}


#
# This function returns the start address for a search.  The first search
# starts at the insertion cursor. If the cursor is not visible, the search
# starts at the top or bottom of the visible text. When a search is repeated,
# the search must behind the previous match (for a forward search) to prevent
# finding the same word again, or finding an overlapping match. (For backwards
# searches overlaps cannot be handled via search arguments; such results are
# filtered out when a match is found.)
#
proc Search_GetBase {is_fwd is_init} {
  if {[.f1.t bbox insert] eq ""} {
    if {$is_fwd} {
      .f1.t mark set insert {@1,1}
    } else {
      .f1.t mark set insert "@[expr {[winfo width .f1.t] - 1}],[expr {[winfo height .f1.t] - 1}]"
    }
    set start_pos insert
  } else {
    if {$is_init} {
      set start_pos insert
    } elseif $is_fwd {
      set start_pos [list insert + 1 chars]
    } else {
      set start_pos insert
    }
  }
  set start_pos [.f1.t index $start_pos]

  # move start position for forward search after the end of the previous match
  if {$is_fwd} {
    # search for tag which marks the previous match (would have been cleared if the pattern changed)
    set pos12 [.f1.t tag nextrange findinc [concat $start_pos linestart] [concat $start_pos lineend]]
    if {$pos12 ne ""} {
      # check if the start position (i.e. the cursor) is still inside of the area of the match
      if {[scan $start_pos "%d.%d" line1 char1] == 2} {
        if {[scan $pos12 "%d.%d %*d.%d" line2 char2 char3] >= 3} {
          if {($line1 == $line2) && ($char1 >= $char2) && ($char1 < $char3)} {
            set start_pos [lindex $pos12 1]
          }
        }
      }
    }
  }
  return $start_pos
}


#
# This function translates user-options into search options for the text widget.
#
proc Search_GetOptions {pat is_re use_case {is_fwd -1}} {

  set search_opt {}
  if {$is_re} {
    if {[regexp {[\.\\\*\+\?\(\[\{\^\$]} $pat]} {
      lappend search_opt {-regexp}
    }
  }
  if {$use_case == 0} {
    lappend search_opt {-nocase}
  }
  if {$is_fwd != -1} {
    if {$is_fwd} {
      lappend search_opt {-forwards}
    } else {
      lappend search_opt {-backwards}
    }
  }
  return $search_opt
}


#
# This function is used by the various key bindings which repeat a
# previous search.
#
proc SearchNext {is_fwd} {
  global tlb_find tlb_regexp tlb_case tlb_history

  ClearStatusLine search

  if {$tlb_find eq ""} {
    # empty expression: repeat last search
    if {[llength $tlb_history] > 0} {
      set hl [lindex $tlb_history 0]
      set tlb_find [lindex $hl 0]
    }
  }

  if {$tlb_find ne ""} {
    set found [Search_Atomic $tlb_find $tlb_regexp $tlb_case $is_fwd 0]
    if {$found eq ""} {
      if {$is_fwd} {
        DisplayStatusLine search warn "No match until end of file"
      } else {
        DisplayStatusLine search warn "No match until start of file"
      }
    }
  } else {
    DisplayStatusLine search error "No pattern defined for search repeat"
    set found ""
  }
  return $found
}


#
# This function is used by the "All" or "List all" buttons and assorted
# keyboard shortcuts to list all text lines matching the current search
# expression in a separate dialog window.
#
proc SearchAll {raise_win direction} {
  global tlb_find tlb_regexp tlb_case tlb_last_wid tlb_find_focus

  if {[SearchExprCheck $tlb_find $tlb_regexp 1]} {
    set pat $tlb_find
    set is_re $tlb_regexp
    set use_case $tlb_case

    Search_AddHistory $tlb_find $tlb_regexp $tlb_case

    # make focus return and cursor jump back to original position
    if {$tlb_find_focus} {
      SearchReset

      # note more clean-up is triggered via the focus-out event
      focus .f1.t

      if {$tlb_last_wid ne ""} {
        focus $tlb_last_wid
        # raise the caller's window above the main window
        if {[regsub {^(\.[^\.]*).*} $tlb_last_wid {\1} top_wid]} {
          raise $top_wid .
        }
      }
    }

    SearchList_Open $raise_win
    SearchList_SearchMatches 1 $pat $is_re $use_case $direction
  }
}


#
# This function resets the state of the search engine.  It is called when
# the search string is empty or a search is aborted with the Escape key.
#
proc SearchReset {} {
  global tlb_find tlb_last_hall tlb_last_dir tlb_inc_base tlb_inc_view

  .f1.t tag remove find 1.0 end
  .f1.t tag remove findinc 1.0 end
  set tlb_last_hall {}

  if {[info exists tlb_inc_base]} {
    .f1.t xview moveto [lindex $tlb_inc_view 0]
    .f1.t yview moveto [lindex $tlb_inc_view 1]
    .f1.t mark set insert $tlb_inc_base
    .f1.t see insert
    unset tlb_inc_base tlb_inc_view
  }
  ClearStatusLine search
}


#
# This function is called when the "find" entry field receives keyboard focus
# to intialize the search state machine for a new search.
#
proc SearchInit {} {
  global tlb_find tlb_find_focus tlb_hist_pos tlb_hist_prefix

  if {$tlb_find_focus == 0} {
    set tlb_find_focus 1
    unset -nocomplain tlb_hist_pos tlb_hist_prefix

    ClearStatusLine search
  }
}


#
# This function is called to move keyboard focus into the search entry field.
# The focus change will trigger the "init" function.  The caller can pass a
# widget to which focus is passed when leaving the search via the Return or
# Escape keys.
#
proc SearchEnter {is_fwd {wid ""}} {
  global tlb_find tlb_last_dir tlb_last_wid

  set tlb_last_dir $is_fwd
  set tlb_find {}
  focus .f2.e

  set tlb_last_wid $wid
  if {$tlb_last_wid ne ""} {
    # raise the search entry field above the caller's window
    if {[regsub {^(\.[^\.]*).*} $tlb_last_wid {\1} top_wid]} {
      raise . $top_wid
    }
  }
}


#
# This function is bound to the FocusOut event in the search entry field.
# It resets the incremental search state.
#
proc SearchLeave {} {
  global tlb_find tlb_regexp tlb_case tlb_hall
  global tlb_inc_base tlb_inc_view tlb_find_focus tlb_last_wid
  global tlb_hist_pos tlb_hist_prefix
  global tid_search_inc

  if {$tid_search_inc ne {}} {
    after cancel $tid_search_inc
    set tid_search_inc {}
  }

  # ignore if the keyboard focus is leaving towards another application
  if {[focus -displayof .] ne {}} {

    unset -nocomplain tlb_inc_base tlb_inc_view
    unset -nocomplain tlb_hist_pos tlb_hist_prefix
    Search_AddHistory $tlb_find $tlb_regexp $tlb_case
    set tlb_last_wid {}
    set tlb_find_focus 0
  }

  if {$tlb_hall} {
    if {[SearchExprCheck $tlb_find $tlb_regexp 0]} {
      SearchHighlightUpdate
    }
  }
}


#
# This function is called when the search window is left via "Escape" key.
# The search highlighting is removed and the search text is deleted.
#
proc SearchAbort {} {
  global tlb_find tlb_regexp tlb_case tlb_last_wid

  Search_AddHistory $tlb_find $tlb_regexp $tlb_case
  set tlb_find {}
  SearchReset
  # note more clean-up is triggered via the focus-out event
  focus .f1.t

  if {$tlb_last_wid ne ""} {
    focus $tlb_last_wid
    # raise the caller's window above the main window
    if {[regsub {^(\.[^\.]*).*} $tlb_last_wid {\1} top_wid]} {
      raise $top_wid .
    }
  }
}


#
# This function is bound to the Return key in the search entry field.
# If the search pattern is invalid (reg.exp. syntax) an error message is
# displayed and the focus stays in the entry field. Else, the keyboard
# focus is switched to the main window.
#
proc SearchReturn {} {
  global tlb_find tlb_find tlb_regexp tlb_history tlb_last_dir tlb_last_wid
  global tid_search_inc

  if {$tid_search_inc ne {}} {
    after cancel $tid_search_inc
    set tid_search_inc {}
    set restart 1
  } else {
    set restart 0
  }

  if {$tlb_find eq ""} {
    # empty expression: repeat last search
    if {[llength $tlb_history] > 0} {
      set hl [lindex $tlb_history 0]
      set tlb_find [lindex $hl 0]
      set restart 1
    } else {
      DisplayStatusLine search error "No pattern defined for search repeat"
    }
  }

  if {[SearchExprCheck $tlb_find $tlb_regexp 1]} {
    if {$restart} {
      # incremental search not completed -> start regular search
      if {[SearchNext $tlb_last_dir] eq ""} {
        global tlb_inc_view tlb_inc_base
        if {[info exists tlb_inc_base]} {
          .f1.t xview moveto [lindex $tlb_inc_view 0]
          .f1.t yview moveto [lindex $tlb_inc_view 1]
          .f1.t mark set insert $tlb_inc_base
          .f1.t see insert
        }
      }
    }

    # note this implicitly triggers the leave event
    focus .f1.t
    if {$tlb_last_wid ne ""} {
      focus $tlb_last_wid
      # raise the caller's window above the main window
      if {[regsub {^(\.[^\.]*).*} $tlb_last_wid {\1} top_wid]} {
        raise $top_wid .
      }
    }
  }
}


#
# This function add the given search string to the search history stack.
# If the string is already on the stack, it's moved to the top. Note: top
# of the stack is the front of the list.
#
proc Search_AddHistory {txt is_re use_case} {
  global tlb_history tlb_hist_maxlen

  if {$txt ne ""} {
    set old_sel [SearchHistory_StoreSel]

    # search for the expression in the history (options not compared)
    set idx 0
    foreach hl $tlb_history {
      if {[lindex $hl 0] eq $txt} break
      incr idx
    }
    # remove the element if already in the list
    if {$idx < [llength $tlb_history]} {
      set tlb_history [lreplace $tlb_history $idx $idx]
    }

    # insert the element at the top of the stack
    set hl [list $txt $is_re $use_case [clock seconds]]
    set tlb_history [linsert $tlb_history 0 $hl]

    # maintain max. stack depth
    if {[llength $tlb_history] > $tlb_hist_maxlen} {
      set tlb_history [lrange $tlb_history 0 [expr {$tlb_hist_maxlen - 1}]]
    }

    UpdateRcAfterIdle

    SearchHistory_Fill
    SearchHistory_RestoreSel $old_sel
  }
}


#
# This function is bound to the up/down cursor keys in the search entry
# field. The function is used to iterate through the search history stack.
# The "up" key starts with the most recently used pattern, the "down" key
# with the oldest. When the end of the history is reached, the original
# search string is displayed again.
#
proc Search_BrowseHistory {is_up} {
  global tlb_find tlb_history tlb_hist_pos tlb_hist_prefix

  if {[llength $tlb_history] > 0} {
    if {![info exists tlb_hist_pos]} {
      set tlb_hist_prefix $tlb_find
      if {$is_up} {
        set tlb_hist_pos 0
      } else {
        set tlb_hist_pos [expr {[llength $tlb_history] - 1}]
      }
    } elseif $is_up {
      if {$tlb_hist_pos + 1 < [llength $tlb_history]} {
        incr tlb_hist_pos 1
      } else {
        set tlb_hist_pos -1
      }
    } else {
      incr tlb_hist_pos -1
    }

    if {[string length $tlb_hist_prefix] > 0} {
      set tlb_hist_pos [Search_HistoryComplete [expr {$is_up ? 1 : -1}]]
    }

    if {$tlb_hist_pos >= 0} {
      set hl [lindex $tlb_history $tlb_hist_pos]
      set tlb_find [lindex $hl 0]
      .f2.e icursor end
    } else {
      # end of history reached -> reset
      set tlb_find $tlb_hist_prefix
      unset tlb_hist_pos tlb_hist_prefix
      .f2.e icursor end
    }
  }
}


#
# This helper function searches the search history stack for a search
# string with a given prefix.
#
proc Search_HistoryComplete {step} {
  global tlb_find tlb_hist_prefix tlb_history tlb_hist_pos tlb_hist_prefix

  set len [string length $tlb_hist_prefix]
  for {set idx $tlb_hist_pos} {($idx >= 0) && ($idx < [llength $tlb_history])} {incr idx $step} {
    set hl [lindex $tlb_history $idx]
    if {[string compare -length $len $tlb_find [lindex $hl 0]] == 0} {
      return $idx
    }
  }
  return -1
}


#
# This function is bound to "CTRL-d" in the "Find" entry field and
# performs auto-completion of a search text by adding any following
# characters in the word matched by the current expression.
#
proc Search_Complete {} {
  global tlb_find tlb_regexp tlb_case

  set pos [.f1.t index insert]
  if {$pos ne ""} {
    set dump [ExtractText $pos [concat $pos lineend]]
    set off 0

    if {$tlb_regexp} {
      if {$tlb_case} {
        set opt {-nocase}
      } else {
        set opt {--}
      }
      if {[SearchExprCheck $tlb_find $tlb_regexp 1]} {
        if {[regexp $opt "^${tlb_find}" $dump word]} {
          set off [string length $word]
        }
      } else {
        return
      }
    } else {
      set off [string length $tlb_find]
    }

    if {[regexp {^(?:\W+|\w+)} [string range $dump $off end] word]} {
      set word [Search_EscapeSpecialChars $word $tlb_regexp]
      append tlb_find $word
      .f2.e selection clear
      .f2.e selection range [expr {[string length $tlb_find] - [string length $word]}] end
      .f2.e icursor end
      .f2.e xview moveto 1
    }
  }
}


#
# This function is bound to "CTRL-SHIFT-D" in the "Find" entry field and
# performs auto-completion to the left by adding any preceding characters
# before the current cursor position.
#
proc Search_CompleteLeft {} {
  global tlb_find tlb_regexp

  set pos [.f1.t index insert]
  if {$pos ne ""} {
    set dump [ExtractText [concat $pos linestart] $pos]

    if {[regexp {(?:\W+|\w+)$} $dump word]} {
      set word [Search_EscapeSpecialChars $word $tlb_regexp]
      set tlb_find "$word$tlb_find"
      .f2.e selection clear
      .f2.e selection range 0 [string length $word]
      .f2.e icursor [string length $word]
      .f2.e xview moveto 0
    }
  }
}


#
# This function if bound to "*" and "#" in the main text window (as in VIM)
# These keys allow to search for the word under the cursor in forward and
# backwards direction respectively.
#
proc SearchWord {is_fwd} {
  global tlb_find tlb_regexp tlb_case

  set pos [.f1.t index insert]
  if {$pos ne ""} {

    # extract word to the right starting at the cursor position
    set dump [ExtractText $pos [concat $pos lineend]]
    if {[regexp {^[\w\-]+} $dump word]} {

      # complete word to the left
      set dump [ExtractText [concat $pos linestart] $pos]
      if {[regexp {[\w\-]+$} $dump word2]} {
        set word "$word2$word"
      }
      set word [Search_EscapeSpecialChars $word $tlb_regexp]

      # add regexp to match on word boundaries
      if {$tlb_regexp} {
        set nword {}
        append nword {\m} $word {\M}
        set word $nword
      }

      set tlb_find $word
      Search_AddHistory $tlb_find $tlb_regexp $tlb_case

      ClearStatusLine search
      set found [Search_Atomic $tlb_find $tlb_regexp $tlb_case $is_fwd 1]

      if {$found eq ""} {
        if {$is_fwd} {
          DisplayStatusLine search warn "No match until end of file"
        } else {
          DisplayStatusLine search warn "No match until start of file"
        }
      }
    }
  }
}


#
# This function moves the cursor onto the next occurence of the given
# character in the current line.
#
proc SearchCharInLine {char dir} {
  global last_inline_char last_inline_dir

  ClearStatusLine search_inline
  if {$char ne ""} {
    set last_inline_char $char
    set last_inline_dir $dir
  } else {
    if {[info exists last_inline_char]}  {
      set char $last_inline_char
      set dir [expr {$dir * $last_inline_dir}]
    } else {
      DisplayStatusLine search_inline error "No previous in-line character search"
      return
    }
  }

  set pos [.f1.t index insert]
  if {$pos ne ""} {
    if {$dir > 0} {
      set dump [ExtractText $pos [concat $pos lineend]]
      set idx [string first $char $dump 1]
      if {$idx != -1} {
        .f1.t mark set insert [list insert + $idx chars]
        .f1.t see insert
      } else {
        DisplayStatusLine search_inline warn "Character \"$char\" not found until line end"
      }
    } else {
      set dump [ExtractText [concat $pos linestart] $pos]
      set len [string length $dump]
      set idx [string last $char $dump]
      if {$idx != -1} {
        .f1.t mark set insert [list insert - [expr {$len - $idx}] chars]
        .f1.t see insert
      } else {
        DisplayStatusLine search_inline warn "Character \"$char\" not found until line start"
      }
    }
  }
}


#
# This helper function escapes characters with special semantics in
# regular expressions in a given word. The function is used for adding
# arbitrary text to the search string.
#
proc Search_EscapeSpecialChars {word is_re} {
  if {$is_re} {
    regsub -all {[^[:alnum:][:blank:]_\-\:\=\%\"\!\'\;\,\#\/\<\>]\@} $word {\\&} word
  }
  return $word
}


#
# This function is bound to "CTRL-x" in the "Find" entry field and
# removes the current entry from the search history.
#
proc Search_RemoveFromHistory {} {
  global tlb_history tlb_hist_pos tlb_hist_prefix

  if {[info exists tlb_hist_pos] && ($tlb_hist_pos < [llength $tlb_history])} {
    set old_sel [SearchHistory_StoreSel]

    set tlb_history [lreplace $tlb_history $tlb_hist_pos $tlb_hist_pos]
    UpdateRcAfterIdle

    SearchHistory_Fill
    SearchHistory_RestoreSel $old_sel

    set new_len [llength $tlb_history]
    if {$new_len == 0} {
      unset tlb_hist_pos tlb_hist_prefix
    } elseif {$tlb_hist_pos >= $new_len} {
      set tlb_hist_pos [expr {$new_len - 1}]
    }
  }
}


# ----------------------------------------------------------------------------
#
# This function creates a small overlay which displays a temporary status
# message.
#
proc DisplayStatusLine {topic type msg} {
  global col_bg_content tid_status_line status_line_topic

  switch -glob [focus -displayof .] {
    {.dlg_srch*}        {set wid .dlg_srch.f1.l}
    {.dlg_hist*}        {set wid .dlg_hist.f1.l}
    {.dlg_tags*}        {set wid .dlg_tags.f1.l}
    default             {set wid .f1.t}
  }

  switch -exact $type {
    error {set col {#ff6b6b}}
    warn {set col {#ffcc5d}}
    default {set col $col_bg_content}
  }

  if {[info commands ${wid}.stline] eq ""} {
    frame ${wid}.stline -background $col_bg_content -relief ridge -borderwidth 2 -takefocus 0

    label ${wid}.stline.l -text $msg -background $col -anchor w
    pack ${wid}.stline.l -side left -fill both -expand 1

    set wh [winfo height $wid]
    set fh [font metrics [${wid}.stline.l cget -font] -linespace]
    set relh [expr {double($fh + 10) / ($wh + 1)}]
    place ${wid}.stline -in $wid -anchor sw -bordermode inside \
                        -x 0 -y [expr {$wh - $fh}] -relheight $relh

  } else {
    ${wid}.stline.l configure -text $msg -background $col
  }

  after cancel $tid_status_line
  set tid_status_line [after 4000 [list destroy ${wid}.stline]]
  set status_line_topic $topic
}


#
# This function removes the status message display if it's currently visible
# and displays a message on the given topic.
#
proc ClearStatusLine {topic} {
  global tid_status_line status_line_topic

  if {[info exists status_line_topic] && ($topic eq $status_line_topic)} {

    # destroy all possible locations because focus may have moved since creation
    catch {destroy .dlg_srch.f1.l.stline}
    catch {destroy .dlg_hist.f1.l.stline}
    catch {destroy .dlg_tags.f1.l.stline}
    catch {destroy .f1.t.stline}

    unset -nocomplain status_line_topic
    after cancel $tid_status_line
    set tid_status_line {}
  }
}


#
# This function is bound to CTRL-G in the main window. It displays the
# current line number and fraction of lines above the cursor in percent
# (i.e. same as VIM)
#
proc DisplayLineNumber {} {
  global cur_filename

  set pos [.f1.t bbox insert]
  if {[llength $pos] == 4} {
    set pos [.f1.t index insert]
    if {[scan $pos "%d.%d" line char] == 2} {
      set pos [.f1.t index end]
      scan $pos "%d.%d" end_line char
      # if the last line is properly terminated with a newline char,
      # Tk inserts an empty line below - this should not be counted
      if {$char == 0} {
        incr end_line -1
      }

      if {$end_line > 0} {
        set val [expr {int(100.0*$line/$end_line + 0.5)}]
        set perc " ($val%)"
      } else {
        set perc ""
      }

      set name $cur_filename
      if {$name eq ""} {set name "STDIN"}

      DisplayStatusLine line_query msg "$name: line $line of $end_line lines$perc"
    }
  }
}


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
proc ToplevelResized {wid top cmp var} {
  upvar {#0} $var geom

  if {$wid eq $cmp} {
    set new_size [wm geometry $top]

    if {![info exists geom] || ($new_size ne $geom)} {
      set geom $new_size
      UpdateRcAfterIdle
    }
  }
}


#
# Helper function to modify a font's size or appearance
#
proc DeriveFont {afont delta_size {style {}}} {
  if {$style eq ""} {
    set style [lindex $afont 2]
  }
  set size [lindex $afont 1]
  if {$size < 0} {
    set size [expr {$size - $delta_size}]
  } else {
    incr size $delta_size
  }

  return [list [lindex $afont 0] $size $style]
}


#
# This function is bound to the Control +/- keys as a way to quickly
# adjust the content font size (as in web browsers)
#
proc ChangeFontSize {delta} {
  global font_content

  set new [DeriveFont $font_content $delta]

  set cerr [ApplyFont $new]
  if {$cerr ne ""} {
    DisplayStatusLine font error "Failed to apply the new font: $cerr"
  } else {
    ClearStatusLine font
  }
}


#
# This function is called after a new fonct has been configured to apply
# the new font in the main window, text highlight tags and dialog texts.
# The function returns 1 on success and saves the font setting in the RC.
#
proc ApplyFont {name} {
  global font_content

  if {[catch {.f1.t configure -font $name} cerr] == 0} {
    set cerr {}

    # save to rc
    set font_content $name
    UpdateRcAfterIdle

    # apply font change to dialogs
    catch {.dlg_mark.l configure -font $font_content}
    catch {.dlg_hist.f1.l configure -font $font_content}
    catch {.dlg_srch.f1.l configure -font $font_content}
    catch {.dlg_tags.f1.l configure -font $font_content}

    # update font in highlight tags (in case some contain font modifiers)
    HighlightCreateTags
    SearchList_CreateHighlightTags
    MarkList_CreateHighlightTags
  }
  return $cerr
}


#
# This function is bound to ALT-w and toggles wrapping of long lines
# in the main window.
#
proc ToggleLineWrap {} {
  set cur [.f1.t cget -wrap]
  if {$cur eq "none"} {
    .f1.t configure -wrap char
  } else {
    .f1.t configure -wrap none
  }
}


#
# This function adjusts the view so that the line holding the cursor is
# placed at the top, center or bottom of the viewable area, if possible.
#
proc YviewSet {wid where col} {
  global font_content

  $wid see insert
  set pos [$wid bbox insert]
  if {[llength $pos] == 4} {
    set fh [font metrics $font_content -linespace]
    set wh [winfo height $wid]
    set bbox_y [lindex $pos 1]
    set bbox_h [lindex $pos 3]

    if {$where eq "top"} {
      set delta [expr {int($bbox_y / $fh)}]

    } elseif {$where eq "center"} {
      set delta [expr {0 - int(($wh/2 - $bbox_y + $bbox_h/2) / $fh)}]

    } elseif {$where eq "bottom"} {
      set delta [expr {0 - int(($wh - $bbox_y - $bbox_h) / $fh)}]

    } else {
      set delta 0
    }

    if {$delta > 0} {
      $wid yview scroll $delta units
    } elseif {$delta < 0} {
      $wid yview scroll $delta units
    }

    if {$col == 0} {
      $wid xview moveto 0
      $wid mark set insert [list insert linestart]
      CursorMoveLine $wid 0
    } else {
      $wid see insert
    }
  }

  # synchronize the search result list (if open) with the main text
  scan [.f1.t index insert] "%d" line
  SearchList_MatchView $line
}


#
# This function scrolls the view vertically by the given number of lines.
# When the line holding the cursor is scrolled out of the window, the cursor
# is placed in the last visible line in scrolling direction.
#
proc YviewScroll {wid delta} {
  global font_content

  $wid yview scroll $delta units

  set fh [font metrics $font_content -linespace]
  set pos [$wid bbox insert]

  # check if cursor is fully visible
  if {([llength $pos] != 4) || ([lindex $pos 3] < $fh)} {
    if {$delta < 0} {
      $wid mark set insert [list "@1,[winfo height $wid]" - 1 lines linestart]
    } else {
      $wid mark set insert {@1,1}
    }
  }
}


#
# This function scrolls the view vertically by half the screen height
# in the given direction.
#
proc YviewScrollHalf {wid dir} {
  global font_content

  set wh [winfo height $wid]
  set fh [font metrics $font_content -linespace]
  if {$fh > 0} {
    set wh [expr {int(($wh + $fh/2) / $fh)}]

    YviewScroll $wid [expr {int($wh/2 * $dir)}]
  }
}


#
# This function moves the cursor into a given line in the current view.
#
proc CursorSetLine {wid where off} {
  global font_content

  CursorJumpPushPos $wid

  if {$where eq "top"} {
    set index [$wid index [list {@1,1} + $off lines]]
    if {($off > 0) && ![IsRowFullyVisible $wid $index]} {
      # offset out of range - set to bottom instead
      return [CursorSetLine $wid bottom 0]
    } else {
      $wid mark set insert $index
    }

  } elseif {$where eq "center"} {
    # note the offset parameter is not applicable in this case
    $wid mark set insert "@1,[expr {int([winfo height $wid] / 2)}]"

  } elseif {$where eq "bottom"} {
    set index [$wid index [list "@1,[winfo height $wid]" linestart - $off lines]]
    if {![IsRowFullyVisible $wid $index]} {
      if {$off == 0} {
        # move cursor to the last fully visible line to avoid scrolling
        set index [$wid index [list $index - 1 lines]]
      } else {
        # offset out of range - set to top instead
        return [CursorSetLine $wid top 0]
      }
    }
    $wid mark set insert $index

  } else {
    $wid mark set insert [list insert linestart]
  }
  # place cursor on first non-blank character in the selected row
  CursorMoveLine $wid 0
}


#
# This function moves the cursor by the given number of lines and places
# the cursor on the first non-blank character in that line. The delta may
# be zero (e.g. to just place the cursor onto the first non-blank)
#
proc CursorMoveLine {wid delta} {
  if {$delta > 0} {
    $wid mark set insert [list insert linestart + $delta lines]
  } elseif {$delta < 0} {
    $wid mark set insert [list insert linestart $delta lines]
  }
  $wid xview moveto 0

  # forward to the first non-blank character
  set dump [ExtractText insert [list insert lineend]]
  if {[regexp {^\s*} $dump word]} {
    $wid mark set insert [list insert + [string length $word] chars]
  }
  $wid see insert
}


#
# This function moves the cursor to the start or end of the main text.
# Additionally the cursor position prior to the jump is remembered in
# the jump stack.
#
proc CursorGotoLine {wid where} {
  CursorJumpPushPos .f1.t
  if {$where eq "start"} {
    $wid mark set insert 1.0
  } elseif {$where eq "end"} {
    $wid mark set insert {end -1 lines linestart}
  } elseif {$where >= 0} {
    catch {$wid mark set insert "$where.0"}
  } else {
    catch {.f1.t mark set insert "end - [expr {1 - $where}] lines linestart"}
  }
  # place the cursor on the first character in the line and make it visible
  CursorMoveLine $wid 0
}


#
# This function scrolls the view horizontally by the given number of characters.
# When the cursor is scrolled out of the window, it's placed in the last visible
# column in scrolling direction.
#
proc XviewScroll {wid how delta dir} {
  set pos_old [$wid bbox insert]

  if {$how eq "scroll"} {
    $wid xview scroll [expr {$dir * $delta}] units
  } else {
    $wid xview moveto $delta
  }

  if {$pos_old ne ""} {
    set pos_new [$wid bbox insert]

    # check if cursor is fully visible
    if {([llength $pos_new] != 4) || ([lindex $pos_new 2] == 0)} {
      set ycoo [expr {[lindex $pos_old 1] + int([lindex $pos_old 3] / 2)}]
      if {$dir < 0} {
        $wid mark set insert "@[winfo width $wid],$ycoo"
      } else {
        $wid mark set insert [list "@1,$ycoo" + 1 chars]
      }
    }
  }
}


#
# This function scrolls the view horizontally by half the screen width
# in the given direction.
#
proc XviewScrollHalf {wid dir} {
  set xpos [$wid xview]
  set w [winfo width $wid]
  if {$w != 0} {
    set fract_visible [expr {[lindex $xpos 1] - [lindex $xpos 0]}]
    set off [expr {[lindex $xpos 0] + $dir * (0.5 * $fract_visible)}]
    if {$off > 1} {set off 1}
    if {$off < 0} {set off 0}
  }
  XviewScroll $wid moveto $off $dir
}


#
# This function adjusts the view so that the column holding the cursor is
# placed at the left or right of the viewable area, if possible.
#
proc XviewSet {wid where} {
  set xpos [$wid xview]
  set coo [$wid bbox insert]
  set w [winfo width $wid]
  if {($coo ne "") && ($w != 0)} {
    set fract_visible [expr {[lindex $xpos 1] - [lindex $xpos 0]}]
    set fract_insert [expr {double(2 + [lindex $coo 0] + [lindex $coo 2]) / $w}]

    if {$where eq "left"} {
      set off [expr {[lindex $xpos 0] + ($fract_insert * $fract_visible)}]
      if {$off > 1.0} {set off 1.0}
    } else {
      set off [expr {[lindex $xpos 0] - ((1 - $fract_insert) * $fract_visible)}]
      if {$off < 0.0} {set off 0.0}
    }
    $wid xview moveto $off
    $wid see insert
  }
}


#
# This function moves the cursor into a given column in the current view.
#
proc CursorSetColumn {wid where} {
  if {$where eq "left"} {
    $wid xview moveto 0
    $wid mark set insert [list insert linestart]

  } elseif {$where eq "right"} {
    $wid mark set insert [list insert lineend]
    $wid see insert
  }
}


#
# This function moves the cursor onto the next or previous word.
# (Same as "w", "b" et.al. in vim)
#
proc CursorMoveWord {is_fwd spc_only to_end} {
  set pos [.f1.t index insert]
  if {$pos ne ""} {
    if {$is_fwd} {
      set dump [ExtractText $pos [concat $pos lineend]]
      if {$spc_only} {
        if {$to_end} {
          set match [regexp {^\s*\S*} $dump word]
        } else {
          set match [regexp {^\S*\s*} $dump word]
        }
      } else {
        if {$to_end} {
          set match [regexp {^\W*\w*} $dump word]
        } else {
          set match [regexp {^\w*\W*} $dump word]
        }
      }
      if {$match && (([string length $word] < [string length $dump]) || $to_end)} {
        .f1.t mark set insert [list insert + [string length $word] chars]
      } else {
        .f1.t mark set insert [list insert linestart + 1 lines]
      }
    } else {
      set dump [ExtractText [concat $pos linestart] $pos]
      set word ""
      if {$spc_only} {
        if {$to_end} {
          set match [regexp {\s(\s+)$} $dump foo word]
        } else {
          set match [regexp {\S+\s*$} $dump word]
        }
      } else {
        if {$to_end} {
          set match [regexp {\w(\W+\w*)$} $dump foo word]
        } else {
          set match [regexp {(\w+|\w+\W+)$} $dump word]
        }
      }
      if {$match} {
        .f1.t mark set insert [list insert - [string length $word] chars]
      } else {
        .f1.t mark set insert [list insert - 1 lines lineend]
      }
    }
    .f1.t see insert
  }
}


#
# Helper function which determines if the text at the given index in
# the given window is fully visible.
#
proc IsRowFullyVisible {wid index} {
  global font_content

  set fh [font metrics $font_content -linespace]
  set bbox [$wid bbox $index]

  if {([llength $bbox] != 4) || ([lindex $bbox 3] < $fh)} {
    return 0
  } else {
    return 1
  }
}

#
# Helper function to extrace a range of characters from the content.
#
proc ExtractText {pos1 pos2} {
  set dump {}
  foreach {key val idx} [.f1.t dump -text $pos1 $pos2] {
    if {$key eq "text"} {
      append dump $val
    }
  }
  return $dump
}


#
# This function is called by all key bindings which make a large jump to
# push the current cusor position onto the jump stack. Both row and column
# are stored.  If the position is already on the stack, this entry is
# deleted (note for this comparison only the line number is considered.)
#
proc CursorJumpPushPos {wid} {
  global cur_jump_stack cur_jump_idx

  if {$wid eq ".f1.t"} {
    # remove the line if already on the stack
    scan [$wid index insert] "%d" line
    set idx 0
    foreach pos $cur_jump_stack {
      scan $pos "%d" prev_line
      if {$prev_line == $line} {
        set cur_jump_stack [lreplace $cur_jump_stack $idx $idx]
        break
      }
      incr idx
    }
    # append to the stack
    lappend cur_jump_stack [$wid index insert]
    set cur_jump_idx -1

    # limit size of the stack
    if {[llength $cur_jump_stack] > 100} {
      set cur_jump_stack [lrange $cur_jump_stack 0 99]
    }
  }
}


#
# This function is bound to command "''" (i.e. two apostrophes) in the main
# window. The command makes the cursor jump back to the origin of the last
# jump (NOT to the target of the last jump, which may be confusing.) The
# current position is pushed to the jump stack, if not already on the stack.
#
proc CursorJumpToggle {wid} {
  global cur_jump_stack cur_jump_idx

  if {[llength $cur_jump_stack] > 0} {
    ClearStatusLine keycmd

    # push current position to the stack
    CursorJumpPushPos $wid

    if {[llength $cur_jump_stack] > 1} {
      set cur_jump_idx [expr {[llength $cur_jump_stack] - 2}]
      set pos [lindex $cur_jump_stack $cur_jump_idx]

      # FIXME this moves the cursor the the first char instead of the stored position
      catch {$wid mark set insert $pos}
      CursorMoveLine $wid 0
      scan $pos "%d" line
      SearchList_MatchView $line
    } else {
      DisplayStatusLine keycmd warn "Already on the mark."
    }
  } else {
    DisplayStatusLine keycmd error "Jump stack is empty."
  }
}


#
# This function is bound to the CTRL-O and CTRL-I commands in the main
# window. The function traverses backwards or forwards respectively
# through the jump stack. During the first call the current cursor
# position is pushed to the stack.
#
proc CursorJumpHistory {wid rel} {
  global cur_jump_stack cur_jump_idx

  ClearStatusLine keycmd
  if {[llength $cur_jump_stack] > 0} {
    if {$cur_jump_idx < 0} {
      # push current position to the stack
      CursorJumpPushPos $wid
      if {($rel < 0) && ([llength $cur_jump_stack] >= 2)} {
        set cur_jump_idx [expr {[llength $cur_jump_stack] - 2}]
      } else {
        set cur_jump_idx 0
      }
    } else {
      incr cur_jump_idx $rel
      if {$cur_jump_idx < 0} {
        set cur_jump_idx [expr {[llength $cur_jump_stack] - 1}]
        DisplayStatusLine keycmd warn "Jump stack wrapped from oldest to newest."
      } elseif {$cur_jump_idx >= [llength $cur_jump_stack]} {
        DisplayStatusLine keycmd warn "Jump stack wrapped from newest to oldest."
        set cur_jump_idx 0
      }
    }
    set pos [lindex $cur_jump_stack $cur_jump_idx]
    catch {$wid mark set insert $pos}
    CursorMoveLine $wid 0

    scan $pos "%d" line
    SearchList_MatchView $line

  } else {
    DisplayStatusLine keycmd error "Jump stack is empty."
  }
}


#
# This helper function is used during start-up to store key bindings in a
# hash. These bindings are evaluated by function KeyCmd, which receives all
# "plain" key press events from the main window (i.e. non-control keys)
#
proc KeyCmdBind {wid char cmd} {
  set var_name "keys"
  append var_name [regsub -all {\.} $wid "_"]
  upvar #0 $var_name key_hash

  set key_hash($char) $cmd
}


#
# This function is bound to key presses in the main window. It's called
# when none of the single-key bindings match. It's intended to handle
# complex key sequences, but also has to handle single key bindings for
# keys which can be part of sequences (e.g. "b" due to "zb")
#
proc KeyCmd {wid char} {
  global last_key_char

  set var_name "keys"
  append var_name [regsub -all {\.} $wid "_"]
  upvar #0 $var_name key_hash

  set result 0
  if {$char ne ""} {
    if {$last_key_char eq "'"} {
      # single quote char: jump to marker or bookmark
      ClearStatusLine keycmd
      if {$char eq "'"} {
        CursorJumpToggle $wid
      } elseif {$char eq "^"} {
        # '^ and '$ are from less
        CursorGotoLine $wid start
      } elseif {$char eq "$"} {
        CursorGotoLine $wid end
      } elseif {$char eq "+"} {
        Mark_JumpNext 1
      } elseif {$char eq "-"} {
        Mark_JumpNext 0
      } else {
        DisplayStatusLine keycmd error "Undefined key sequence \"'$char\""
      }
      set last_key_char {}
      set result 1

    } elseif {($last_key_char eq "z") || ($last_key_char eq "g")} {
      ClearStatusLine keycmd
      set char "$last_key_char$char"
      if {[info exists key_hash($char)]} {
        uplevel {#0} $key_hash($char)
      } else {
        DisplayStatusLine keycmd error "Undefined key sequence \"$char\""
      }
      set last_key_char {}
      set result 1

    } elseif {$last_key_char eq "f"} {
      SearchCharInLine $char 1
      set last_key_char {}
      set result 1

    } elseif {$last_key_char eq "F"} {
      SearchCharInLine $char -1
      set last_key_char {}
      set result 1

    } else {
      set last_key_char {}

      if {[info exists key_hash($char)]} {
        uplevel {#0} $key_hash($char)

      } elseif {[regexp {[1-9]} $char]} {
        KeyCmd_OpenDialog any $char
        set last_key_char {}
        set result 1

      } elseif {[regexp {[z'fFg]} $char]} {
        set last_key_char $char
        set result 1
      }
    }
  }
  # return 1 if the key was consumed, else 0
  return $result
}


#
# This function is called for all explicit key bindings to forget about
# any previously buffered partial multi-keypress commands.
#
proc KeyClr {} {
  global last_key_char
  set last_key_char {}
}


#
# This function adds key bindings for scrolling vertically
# to the given text widget.
#
proc KeyBinding_UpDown {wid} {
  bind $wid <Control-Up> {YviewScroll %W -1; KeyClr; break}
  bind $wid <Control-Down> {YviewScroll %W 1; KeyClr; break}
  bind $wid <Control-f> {event generate %W <Key-Next>; KeyClr; break}
  bind $wid <Control-b> {event generate %W <Key-Prior>; KeyClr; break}
  bind $wid <Control-e> {YviewScroll %W 1; KeyClr; break}
  bind $wid <Control-y> {YviewScroll %W -1; KeyClr; break}
  bind $wid <Control-d> {YviewScrollHalf %W 1; KeyClr; break}
  bind $wid <Control-u> {YviewScrollHalf %W -1; KeyClr; break}

  KeyCmdBind $wid "z-" [list YviewSet $wid bottom 0]
  KeyCmdBind $wid "zb" [list YviewSet $wid bottom 1]
  KeyCmdBind $wid "z." [list YviewSet $wid center 0]
  KeyCmdBind $wid "zz" [list YviewSet $wid center 1]
  KeyCmdBind $wid "zReturn" [list YviewSet $wid top 0]
  KeyCmdBind $wid "zt" [list YviewSet $wid top 1]

  KeyCmdBind $wid "+" [list CursorMoveLine $wid 1]
  KeyCmdBind $wid "-" [list CursorMoveLine $wid -1]
  KeyCmdBind $wid "k" [list event generate $wid <Up>]
  KeyCmdBind $wid "j" [list event generate $wid <Down>]
  KeyCmdBind $wid "H" [list CursorSetLine $wid top 0]
  KeyCmdBind $wid "M" [list CursorSetLine $wid center 0]
  KeyCmdBind $wid "L" [list CursorSetLine $wid bottom 0]

  KeyCmdBind $wid "G" [list CursorGotoLine $wid end]
  KeyCmdBind $wid "gg" [list CursorGotoLine $wid start]
}


#
# This function adds key bindings for scrolling horizontally
# to the given text widget.
#
proc KeyBinding_LeftRight {wid} {
  bind $wid <Control-Left> {XviewScroll %W scroll 1 -1; KeyClr; break}
  bind $wid <Control-Right> {XviewScroll %W scroll 1 1; KeyClr; break}

  KeyCmdBind $wid "zl" [list XviewScroll $wid scroll 1 1]
  KeyCmdBind $wid "zh" [list XviewScroll $wid scroll 1 -1]
  KeyCmdBind $wid "zL" [list XviewScrollHalf $wid 1]
  KeyCmdBind $wid "zH" [list XviewScrollHalf $wid -1]
  KeyCmdBind $wid "zs" [list XviewSet $wid left]
  KeyCmdBind $wid "ze" [list XviewSet $wid right]

  KeyCmdBind $wid "0" [list CursorSetColumn $wid left]
  KeyCmdBind $wid "^" [list CursorSetColumn $wid left; CursorMoveLine $wid 0]
  KeyCmdBind $wid "$" [list CursorSetColumn $wid right]
}


# ----------------------------------------------------------------------------
#
# This function opens a tiny "overlay" dialog which allows to enter a line
# number.  The dialog is placed into the upper left corner of the text
# widget in the main window.
#
proc KeyCmd_OpenDialog {type {txt {}}} {
  global keycmd_ent

  PreemptBgTasks
  if {[llength [info commands .dlg_key.e]] == 0} {
    frame .dlg_key -borderwidth 2 -relief raised

    if {$type eq "goto"} {
      set cmd_text "Goto line:"
    } else {
      set cmd_text "Command:"
    }
    set keycmd_ent $txt

    label .dlg_key.l -text $cmd_text
    pack .dlg_key.l -side left -padx 5
    entry .dlg_key.e -width 12 -textvariable keycmd_ent -exportselection false
    pack .dlg_key.e -side left -padx 5

    if {$type eq "goto"} {
      bind .dlg_key.e <Return> {KeyCmd_ExecGoto; break}
    } else {
      # line goto key binding
      bind .dlg_key.e <Key-g> {KeyCmd_ExecGoto; break}
      bind .dlg_key.e <Shift-Key-G> {KeyCmd_ExecGoto; break}
      # vertical cursor movement binding
      bind .dlg_key.e <Key-minus> {KeyCmd_ExecCursorUpDown 0; break}
      bind .dlg_key.e <Key-plus> {KeyCmd_ExecCursorUpDown 1; break}
      bind .dlg_key.e <Return> {KeyCmd_ExecCursorUpDown 1; break}
      bind .dlg_key.e <Key-bar> {KeyCmd_ExecAbsColumn; break}
      bind .dlg_key.e <Key-k> {KeyCmd_ExecCursorUpDown 0; break}
      bind .dlg_key.e <Key-j> {KeyCmd_ExecCursorUpDown 1; break}
      bind .dlg_key.e <Key-H> {KeyCmd_ExecCursorVertSet top; break}
      bind .dlg_key.e <Key-M> {KeyCmd_ExecCursorVertSet center; break}
      bind .dlg_key.e <Key-L> {KeyCmd_ExecCursorVertSet bottom; break}
      # horizontal/in-line cursor movement binding
      bind .dlg_key.e <Key-w> {KeyCmd_ExecCursorMove <Key-w>; break}
      bind .dlg_key.e <Key-e> {KeyCmd_ExecCursorMove <Key-e>; break}
      bind .dlg_key.e <Key-b> {KeyCmd_ExecCursorMove <Key-b>; break}
      bind .dlg_key.e <Key-W> {KeyCmd_ExecCursorMove <Key-W>; break}
      bind .dlg_key.e <Key-E> {KeyCmd_ExecCursorMove <Key-E>; break}
      bind .dlg_key.e <Key-B> {KeyCmd_ExecCursorMove <Key-B>; break}
      bind .dlg_key.e <Key-colon> {KeyCmd_ExecCursorMove <Key-colon>; break}
      bind .dlg_key.e <Key-semicolon> {KeyCmd_ExecCursorMove <Key-semicolon>; break}
      bind .dlg_key.e <Key-space> {KeyCmd_ExecCursorMove <Key-space>; break}
      bind .dlg_key.e <Key-BackSpace> {KeyCmd_ExecCursorMove <Key-BackSpace>; break}
      bind .dlg_key.e <Key-h> {KeyCmd_ExecCursorMove <Key-h>; break}
      bind .dlg_key.e <Key-l> {KeyCmd_ExecCursorMove <Key-l>; break}
      bind .dlg_key.e <Key-semicolon> {KeyCmd_ExecCursorMove <Key-semicolon>; break}
      # search key binding
      bind .dlg_key.e <Key-n> {KeyCmd_ExecSearch 1; break}
      bind .dlg_key.e <Key-p> {KeyCmd_ExecSearch 0; break}
      bind .dlg_key.e <Shift-Key-N> {KeyCmd_ExecSearch 0; break}
      # catch-all
      bind .dlg_key.e <KeyPress> {
        if {"%A" eq "|"} {
          # work-around: keysym <Key-bar> doesn't work on German keyboard
          KeyCmd_ExecAbsColumn
          break
        } elseif {![regexp {[[:digit:]]} %A] && [regexp {[[:graph:][:space:]]} %A]} {
          break
        }
      }
    }
    bind .dlg_key.e <Escape> {KeyCmd_Leave; break}
    bind .dlg_key.e <Leave> {KeyCmd_Leave; break}
    bind .dlg_key.e <FocusOut> {KeyCmd_Leave; break}
    bind .dlg_key <Leave> {destroy .dlg_key}
    .dlg_key.e icursor end

    place .dlg_key -in .f1.t -anchor nw -x 0 -y 0
    focus .dlg_key.e
  }
  ResumeBgTasks
}


#
# This function is bound to all events which signal an exit of the goto
# dialog window. The window is destroyed.
#
proc KeyCmd_Leave {} {
  focus .f1.t
  destroy .dlg_key
  unset -nocomplain keycmd_ent
}


#
# This function scrolls the text in the main window to the line number
# entered in the "goto line" dialog window and closes the dialog. Line
# numbers start with 1. If the line numbers is negative, -1 refers to
# the last line.
#
proc KeyCmd_ExecGoto {} {
  global keycmd_ent

  # check if the content is a valid line number
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    ClearStatusLine keycmd
    CursorJumpPushPos .f1.t

    CursorGotoLine .f1.t $keycmd_ent

    KeyCmd_Leave
  } else {
    DisplayStatusLine keycmd error "Input is not a valid line number: \"$keycmd_ent\""
  }
}


#
# This function moves the cursor up or down by a given number of lines.
#
proc KeyCmd_ExecCursorUpDown {is_fwd} {
  global keycmd_ent

  # check if the content is a valid line number
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    ClearStatusLine keycmd
    if {$is_fwd} {
      catch {.f1.t mark set insert [list insert linestart + $keycmd_ent lines]}
    } else {
      catch {.f1.t mark set insert [list insert linestart - $keycmd_ent lines]}
    }
    .f1.t xview moveto 0
    .f1.t see insert
    KeyCmd_Leave
  } else {
    DisplayStatusLine keycmd error "Cursor movement commands require numeric input."
  }
}


#
# This function sets the cursor into a given row, relative to top, or bottom.
# Placement into the middle is also supported, but without offset.
#
proc KeyCmd_ExecCursorVertSet {where} {
  global keycmd_ent

  # check if the content is a valid line number
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    ClearStatusLine keycmd
    CursorSetLine .f1.t $where $keycmd_ent
    KeyCmd_Leave
  } else {
    DisplayStatusLine keycmd error "Cursor movement commands require numeric input."
  }
}


#
# This function sets the cursor into a given column
#
proc KeyCmd_ExecAbsColumn {} {
  global keycmd_ent

  # check if the content is a valid line number
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    ClearStatusLine keycmd
    # prevent running beyond the end of the line
    scan [.f1.t index {insert lineend}] "%*d.%d" max_col
    if {$keycmd_ent < $max_col} {
      catch {.f1.t mark set insert [list insert linestart + $keycmd_ent chars]}
    } else {
      catch {.f1.t mark set insert [list insert lineend]}
    }
    .f1.t see insert
    KeyCmd_Leave
  } else {
    DisplayStatusLine keycmd error "Cursor column placement requires numeric input."
  }
}


#
# This function starts a search from within the command popup window.
#
proc KeyCmd_ExecSearch {is_fwd} {
  global tlb_history tlb_find tlb_regexp tlb_case
  global keycmd_ent

  # check if the content is a repeat count
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    KeyCmd_Leave
    ClearStatusLine keycmd
    ClearStatusLine search

    if {$tlb_find eq ""} {
      # empty expression: repeat last search
      if {[llength $tlb_history] > 0} {
        set hl [lindex $tlb_history 0]
        set tlb_find [lindex $hl 0]
      }
    }
    if {$tlb_find ne ""} {
      set count 0
      for {set idx 0} {$idx < $keycmd_ent} {incr idx} {
        set found [Search_Atomic $tlb_find $tlb_regexp $tlb_case $is_fwd 0]
        if {$found eq ""} {
          if {$is_fwd} {set limit "end"} else {set limit "start"}
          if {$count == 0} {
            DisplayStatusLine search warn "No match until $limit of file"
          } else {
            DisplayStatusLine search warn "Only $count of $keycmd_ent matches until $limit of file"
          }
        }
        incr count
      }
    } else {
      DisplayStatusLine search error "No pattern defined for search repeat"
    }
  } else {
    DisplayStatusLine keycmd error "Search repetition requires a numeric value as input."
  }
}


#
# This function moves the cursor as if the given key had been pressed
# the number of times specified in the number entry field.
#
proc KeyCmd_ExecCursorMove {key} {
  global keycmd_ent

  # check if the content is a repeat count
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    if {$keycmd_ent < 10000} {
      ClearStatusLine keycmd
      KeyCmd_Leave
      for {set idx 0} {$idx < $keycmd_ent} {incr idx} {
        event generate .f1.t $key
      }
    } else {
      DisplayStatusLine keycmd error "Repetition value too large: $keycmd_ent"
    }
  } else {
    DisplayStatusLine keycmd error "Cursor movement commands require numeric input."
  }
}


# ----------------------------------------------------------------------------
#
# This function retrieves the "frame number" (timestamp) to which a given line
# of text belongs via pattern matching. Two methods for retrieving the number
# can be used, depending on which patterns are defined.
#
proc ParseFrameTickNo {pos {cache_ref {}}} {
  global tick_pat_sep tick_pat_num tick_str_prefix

  # query the cache before parsing for the frame number
  if {$cache_ref ne ""} {
    upvar $cache_ref fn_cache
    if {[info exists fn_cache($pos)]} {
      # FN of this line is already known
      return $fn_cache($pos)
    } elseif {[info exists fn_cache(-1)]} {
      set info $fn_cache(-1)
      scan [.f1.t index $pos] "%d" line
      if {($line >= [lindex $info 0]) && ($line < [lindex $info 1])} {
        # line is within the range of the most recently parsed frame
        set fn_cache($pos) [lindex $info 2]
        return [lindex $info 2]
      }
    }
  }

  # catch because patterns are supplied by user
  if {[catch {
    if {$tick_pat_sep ne ""} {
      # determine frame number by searching forwards and backwards for frame boundaries
      # marked by a frame separator pattern; then within these boundaries search for FN
      set prefix ""
      set pos1 [.f1.t search -regexp -backwards -- $tick_pat_sep [list $pos lineend] 1.0]
      set pos2 [.f1.t search -regexp -forwards -- $tick_pat_sep [list $pos lineend] end]
      if {$pos1 eq ""} {set pos1 "1.0"}
      if {$pos2 eq ""} {set pos2 end}
      set pos3 [.f1.t search -regexp -count match_len -- $tick_pat_num $pos1 $pos2]
      if {$pos3 ne ""} {
        set dump [ExtractText $pos3 [list $pos3 + $match_len chars]]
        if {[regexp -- $tick_pat_num $dump foo fn]} {
          set prefix $fn

          if {$cache_ref ne ""} {
            # add result to the cache
            set fn_cache($pos) $fn
            # add a special entry to the cache remembering the extent of the current frame
            scan [.f1.t index $pos1] "%d" line1
            scan [.f1.t index $pos2] "%d" line2
            set fn_cache(-1) [list $line1 $line2 $fn]
          }
        }
      }
    } elseif {$tick_pat_num ne ""} {
      # determine frame number by searching backwards for the line holding the FN
      set prefix ""
      set pos3 [.f1.t search -regexp -backwards -count match_len -- $tick_pat_num [concat $pos lineend] 1.0]
      if {$pos3 ne ""} {
        set dump [ExtractText $pos3 [list $pos3 + $match_len chars]]
        if {[regexp -- $tick_pat_num $dump foo fn]} {
          set prefix $fn

          if {$cache_ref ne ""} {
            set fn_cache($pos) $fn
          }
        }
      }
    } else {
      # FN parsing is disabled: omit the prefix
      set prefix ""
    }
  } cerr]} {
    puts stderr "Warning: tick pattern match error '$cerr'"
  }
  return $prefix
}

#
# This function adds or removes a bookmark at the given text line.
# The line is marked by inserting an image in the text and the bookmark
# is added to the bookmark list dialog, if it's currently open.
#
proc Mark_Toggle {line {txt {}}} {
  global img_marker mark_list mark_list_modified
  global tick_str_prefix

  if {![info exists mark_list($line)]} {
    if {$txt eq ""} {
      set tickno [ParseFrameTickNo insert]
      set txt [ExtractText "$line.0" "$line.0 lineend"]
      set txt [string trim $txt]
      set mark_list($line) "$tick_str_prefix$tickno $txt"
    } else {
      set mark_list($line) $txt
    }
    .f1.t image create "$line.0" -image $img_marker -padx 5
    .f1.t tag add bookmark "$line.0"

    # extend highlighting tags to the inserted bookmark char
    foreach {key val idx} [.f1.t dump -tag "$line.1"] {
      if {$key eq "tagon"} {
        .f1.t tag add $val "$line.0" $idx
      }
    }
    MarkList_Add $line

  } else {
    unset mark_list($line)
    .f1.t delete "$line.0" "$line.1"
    MarkList_Delete $line
  }
  SearchList_MarkLine $line
  set mark_list_modified 1
}


#
# This function adds or removes a bookmark at the current cursor position.
# The function is used to set bookmarks via key bindings.
#
proc Mark_ToggleAtInsert {} {
  set pos [.f1.t index insert]
  if {$pos ne ""} {
    scan $pos "%d.%d" line char
    Mark_Toggle $line
  }
}


#
# This function moves the cursor into the given line and highlights the entire
# line. The "line" parameter is a text widget line number, starting at 1.
#
proc Mark_Line {line} {
  global tlb_last_hall

  CursorJumpPushPos .f1.t

  # move the cursor into the specified line
  .f1.t mark set insert "$line.0"
  .f1.t see insert

  # remove a possible older highlight
  .f1.t tag remove find 1.0 end
  .f1.t tag remove findinc 1.0 end
  set tlb_last_hall ""

  # highlight the specified line
  .f1.t tag add find "$line.0" "[expr {$line + 1}].0"
}


#
# This function moves the cursor onto the next bookmark in the given
# direction.
#
proc Mark_JumpNext {is_fwd} {
  global mark_list

  set pos [.f1.t index insert]
  if {$pos ne ""} {
    scan $pos "%d.%d" line char
    if {$is_fwd} {
      foreach mark_line [lsort -integer [array names mark_list]] {
        if {$mark_line > $line} {
          set goto $mark_line
          break
        }
      }
    } else {
      foreach mark_line [lsort -integer -decreasing [array names mark_list]] {
        if {$mark_line < $line} {
          set goto $mark_line
          break
        }
      }
    }
    if {[info exists goto]} {
      CursorJumpPushPos .f1.t
      .f1.t mark set insert "${goto}.0"
      .f1.t see insert
      .f2.e xview moveto 0

    } else {
      if {[array size mark_list] == 0} {
        DisplayStatusLine bookmark error "No bookmarks have been defined yet"
      } elseif {$is_fwd} {
        DisplayStatusLine bookmark warn "No more bookmarks until end of file"
      } else {
        DisplayStatusLine bookmark warn "No more bookmarks until start of file"
      }
    }
  }
}


#
# This function deletes all bookmarks. It's called via the main menu.
# The function is intended esp. if a large number of bookmarks was imported
# previously from a file.
#
proc Mark_DeleteAll {} {
  global mark_list mark_list_modified

  set count [array size mark_list]
  if {$count > 0} {
    if {$count == 1} {set pls ""} else {set pls "s"}
    set answer [tk_messageBox -icon question -type okcancel -parent . \
                              -message "Really delete $count bookmark${pls}?"]
    if {$answer eq "ok"} {
      foreach line [array names mark_list] {
        Mark_Toggle $line
      }
      set mark_list_modified 0
    }
  } else {
    tk_messageBox -icon info -type ok -parent . -message "Your bookmark list is already empty."
  }
}


#
# This function reads a list of line numbers and tags from a file and
# adds them to the bookmark list. (Note already existing bookmarks are
# not discarded, hence there's no warning when bookmarks already exist.)
#
proc Mark_ReadFile {filename} {
  global mark_list mark_list_modified

  if {[catch {set file [open $filename r]} cerr] == 0} {
    set line_num 1
    set bol {}
    while {[gets $file line] >= 0} {
      set txt {}
      if {[regexp {^[[:space:]]#} $line]} {
        # skip comment in file
      } elseif {[regexp {^(\d+)([ \t\:\.\,\;\=\'\/](.*))?$} $line foo num foo2 txt]} {
        lappend bol $num [string trim $txt]
      } else {
        tk_messageBox -icon error -type ok -parent . \
                      -message "Parse error in line $line_num: line is not starting with a digit: \"[string range $line 0 40]\"."
        set line_num -1
        break
      }
      incr line_num
    }
    close $file

    if {$line_num > 0} {
      set modif [expr {$mark_list_modified || ([array size mark_list] != 0)}]

      set pos [.f1.t index end]
      scan [lindex $pos 0] "%d.%d" max_line foo
      set warned 0

      foreach {line txt} $bol {
        if {($line < 0) || ($line > $max_line)} {
          if {$warned == 0} {
            set answer [tk_messageBox -icon warning -type okcancel -parent . \
                          -message "Invalid line number $line in bookmarks file \"$filename\" (should be in range 0...$max_line)"]
            if {$answer eq "cancel"} {
              break
            }
          }
        } else {
          if {![info exists mark_list($line)]} {
            Mark_Toggle $line $txt
          }
        }
      }
      set mark_list_modified $modif

      # update bookmark list dialog window, if opened
      MarkList_Fill
    }
  } else {
    tk_messageBox -icon error -type ok -parent . -message "Failed to read bookmarks file: $cerr"
  }
}


#
# This function stores the bookmark list in a file.
#
proc Mark_SaveFile {filename} {
  global mark_list mark_list_modified

  if {[catch {set file [open $filename w]} cerr] == 0} {
    if {[catch {
      foreach line [lsort -integer [array names mark_list]] {
        puts $file "$line $mark_list($line)"
      }
      close $file
      set mark_list_modified 0

    } cerr] != 0} {
      tk_messageBox -icon error -type ok -parent . \
                    -message "Error while writing bookmarks into \"$filename\": $cerr"
    }
  } else {
    tk_messageBox -icon error -type ok -parent . -message "Failed to save bookmarks: $cerr"
  }
}


#
# This function is called by menu entry "Read bookmarks from file"
# The user is asked to select a file; if he does so it's content is read.
#
proc Mark_ReadFileFrom {} {
  global cur_filename

  set def_name [Mark_DefaultFile $cur_filename]
  if {![file readable $def_name]} {
    set def_name ""
  }
  set filename [tk_getOpenFile -parent . -filetypes {{Bookmarks {*.bok}} {all {*}}} \
                               -title "Select bookmark file" \
                               -initialfile [file tail $def_name] \
                               -initialdir [file dirname $def_name]]
  if {$filename ne ""} {
    Mark_ReadFile $filename
  }
}


#
# This function automatically reads a previously stored bookmark list
# for a newly loaded file, if the bookmark file is named by the default
# naming convention, i.e. with ".bok" extension.
#
proc Mark_ReadFileAuto {} {
  global cur_filename

  set bok_name [Mark_DefaultFile $cur_filename]
  if {$bok_name ne ""} {
    Mark_ReadFile $bok_name
  }
}


#
# This helper function determines the default filename for reading bookmarks.
# Default is the trace file name or base file name plus ".bok". The name is
# only returned if a file with this name actually exists and is not older
# than the trace file.
#
proc Mark_DefaultFile {trace_name} {
  set bok_name ""
  if {$trace_name ne ""} {
    # must use catch around call to "mtime"
    catch {
      set cur_mtime [file mtime $trace_name]
    }
    if {[info exists cur_mtime]} {
      set name "${trace_name}.bok"
      catch {
        if {[file readable $name]} {
          if {[file mtime $name] >= $cur_mtime} {
            set bok_name $name
          } else {
            puts stderr "$::argv0: warning: bookmark file $name is older than content - not loaded"
          }
        }
      }
      if {$bok_name eq ""} {
        if {[regsub {\.[^\.]+$} $trace_name {.bok} name] && ($name ne $trace_name)} {
          catch {
            if {[file readable $name] && ([file mtime $name] >= $cur_mtime)} {
              set bok_name $name
            }
          }
        }
      }
    }
  }
  return $bok_name
}


#
# This function is called by menu entry "Save bookmarks to file".
# The user is asked to select a file; if he does so the bookmarks are written to it.
#
proc Mark_SaveFileAs {} {
  global mark_list cur_filename

  if {[array size mark_list] > 0} {
    if {$cur_filename ne ""} {
      set def_name "${cur_filename}.bok"
    } else {
      set def_name ""
    }
    set filename [tk_getSaveFile -parent . -filetypes {{Bookmarks {*.bok}} {all {*}}} \
                                 -title "Select bookmark file" \
                                 -initialfile [file tail $def_name] \
                                 -initialdir [file dirname $def_name]]
    if {$filename ne ""} {
      Mark_SaveFile $filename
    }
  } else {
    tk_messageBox -icon info -type ok -parent . -message "Your bookmark list is empty."
  }
}


#
# This function offers to store the bookmark list into a file if the list was
# modified.  The function is called when the application is closed or a new file
# is loaded.
#
proc Mark_OfferSave {} {
  global mark_list mark_list_modified

  if {$mark_list_modified && ([array size mark_list] > 0)} {
    set answer [tk_messageBox -icon question -type yesno -parent . \
                  -message "Store changes in the bookmark list?"]

    if {$answer eq "yes"} {
      Mark_SaveFileAs

      # give some positive feedback via a popup message
      # (and don't annoy the user by forcing him to press an "ok" button to close the message)
      if {$mark_list_modified == 0} {
        toplevel .minfo
        wm transient .minfo .
        wm geometry .minfo "+[expr {[winfo rootx .] + 100}]+[expr {[winfo rooty .] + 100}]"
        wm title .minfo "Bookmarks saved"

        button  .minfo.b -bitmap info -relief flat
        pack .minfo.b -side left -padx 10 -pady 20
        label .minfo.t -text "Bookmarks have been saved..."
        pack .minfo.t -side left -padx 10 -pady 20

        set ::minfo_shown 1
        bind .minfo <Button-1> {destroy .minfo}
        bind .minfo <Return> {destroy .minfo}
        bind .minfo.t <Destroy> {unset -nocomplain minfo_shown}
        grab .minfo

        # remove the popup message after 3 seconds (or when the user clicks into it)
        set id [after 3000 {catch {destroy .minfo}}]
        vwait ::minfo_shown
        after cancel $id
        catch {destroy .minfo}
      }
    }
  }
}


# ----------------------------------------------------------------------------
#
# This function inserts a bookmark text into the listbox and copies
# color highlight tags from the main window so that the text displays
# in the same way.
#
proc MarkList_Insert {idx line} {
  global patlist mark_list

  set tag_list {}
  foreach tag [.f1.t tag names "$line.1"] {
    if {[regexp {^tag\d+$} $tag]} {
      lappend tag_list $tag
    }
  }

  set txt $mark_list($line)
  append txt "\n"

  # insert text (prepend space to improve visibility of selection)
  .dlg_mark.l insert "[expr {$idx + 1}].0" "  " margin $txt $tag_list
}


#
# This function fills the bookmark list dialog window with all bookmarks.
#
proc MarkList_Fill {} {
  global dlg_mark_shown dlg_mark_list mark_list

  if {[info exists dlg_mark_shown]} {
    set dlg_mark_list {}
    .dlg_mark.l delete 1.0 end

    set idx 0
    foreach line [lsort -integer [array names mark_list]] {
      lappend dlg_mark_list $line

      MarkList_Insert $idx $line
      incr idx
    }
  }
}


#
# This function is called after a bookmark was added to insert the text
# into the bookmark list dialog window.
#
proc MarkList_Add {line} {
  global dlg_mark_shown mark_list dlg_mark_list dlg_mark_sel

  if {[info exists dlg_mark_shown]} {
    set idx 0
    foreach l $dlg_mark_list {
      if {$l > $line} {
        break
      }
      incr idx
    }
    set dlg_mark_list [linsert $dlg_mark_list $idx $line]
    MarkList_Insert $idx $line
    .dlg_mark.l see "[expr {$idx + 1}].0"
    TextSel_SetSelection dlg_mark_sel $idx
  }
}


#
# This function is called after a bookmark was deleted to remove the text
# from the bookmark list dialog window.
#
proc MarkList_Delete {line} {
  global dlg_mark_shown mark_list dlg_mark_list dlg_mark_sel

  if {[info exists dlg_mark_shown]} {
    set idx [lsearch -exact -integer $dlg_mark_list $line]
    if {$idx >= 0} {
      set dlg_mark_list [lreplace $dlg_mark_list $idx $idx]
      .dlg_mark.l delete "[expr {$idx + 1}].0" "[expr {$idx + 2}].0"
      TextSel_SetSelection dlg_mark_sel {}
    }
  }
}


#
# This function is bound to the "delete" key and context menu entry to
# allow the user to remove a bookmark via the bookmark list dialog.
#
proc MarkList_RemoveSelected {} {
  global dlg_mark_list dlg_mark_sel

  set sel [TextSel_GetSelection dlg_mark_sel]

  # translate list indices to text lines, because indices change during removal
  set line_list {}
  foreach idx $sel {
    lappend line_list [lindex $dlg_mark_list $idx]
  }
  # remove bookmarks on all selected lines
  foreach line $line_list {
    Mark_Toggle $line
  }
}


#
# This function is bound to the "insert" key and "rename" context menu
# entry to allow the user to edit the tag assigned to the selected bookmark.
# The function opens an "overlay" window with an entry field.
#
proc MarkList_RenameSelected {} {
  global dlg_mark_sel

  set sel [TextSel_GetSelection dlg_mark_sel]
  if {[llength $sel] == 1} {
    MarkList_OpenRename $sel
  }
}


#
# This function is bound to changes of the selection in the bookmark list,
# i.e. it's called when the user uses the cursor keys or mouse button to
# select an entry.  The view in the main window is set to display the line
# which contains the bookmark. If more than one bookmark nothing is done.
#
proc MarkList_SelectionChange {sel} {
  global dlg_mark_list

  if {[llength $sel] >= 1} {
    set line [lindex $dlg_mark_list [lindex $sel 0]]
    Mark_Line $line
    SearchList_MatchView $line
  }
  if {[llength $sel] > 1} {
    foreach idx [lrange $sel 1 end] {
      set line [lindex $dlg_mark_list $idx]
      .f1.t tag add find "$line.0" "[expr {$line + 1}].0"
    }
  }
}


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
proc MarkList_GetLen {} {
  global dlg_mark_list
  return [llength $dlg_mark_list]
}


#
# This function must be called when portions of the text in the main window
# have been deleted to update references to text lines. Paramater meaning:
# + top_l: this is the first line which is not deleted, or 1 if none
# + bottom_l: this line and all below have been removed, or 0 if none
#
proc MarkList_AdjustLineNums {top_l bottom_l} {
  global mark_list

  foreach {line title} [array get mark_list] {
    unset mark_list($line)
    if {($line >= $top_l) && (($line < $bottom_l) || ($bottom_l == 0))} {
      set mark_list([expr {$line - $top_l + 1}]) $title
    }
  }
  MarkList_Fill
}


#
# This function assigns the given text to a bookmark with the given index
# in the bookmark list dialog.  The function is called when the user has
# closed the bookmark text entry dialog with "Return"
#
proc MarkList_Rename {idx txt} {
  global dlg_mark_shown dlg_mark_list dlg_mark_sel mark_list mark_list_modified

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    set line [lindex $dlg_mark_list $idx]
    if {$txt ne ""} {
      set mark_list($line) $txt
      set mark_list_modified 1

      .dlg_mark.l delete "[expr {$idx + 1}].0" "[expr {$idx + 2}].0"
      MarkList_Insert $idx $line
      .dlg_mark.l see "[expr {$idx + 1}].0"
      TextSel_SetSelection dlg_mark_sel $idx
    }
  }
}


#
# This function pops up a context menu for the bookmark list dialog.
#
proc MarkList_ContextMenu {xcoo ycoo} {
  global dlg_mark_list dlg_mark_sel

  TextSel_ContextSelection dlg_mark_sel $xcoo $ycoo

  set sel [TextSel_GetSelection dlg_mark_sel]
  if {[llength $sel] > 0} {
    .dlg_mark.ctxmen delete 0 end
    if {[llength $sel] == 1} {
      .dlg_mark.ctxmen add command -label "Rename marker" -command [list MarkList_OpenRename $sel]
    }
    .dlg_mark.ctxmen add command -label "Remove marker" -command MarkList_RemoveSelected

    set rootx [expr {[winfo rootx .dlg_mark] + $xcoo}]
    set rooty [expr {[winfo rooty .dlg_mark] + $ycoo}]
    tk_popup .dlg_mark.ctxmen $rootx $rooty 0
  }
}


#
# This function creates or raises the bookmark list dialog. This dialog shows
# all currently defined bookmarks.
#
proc MarkList_OpenDialog {} {
  global font_content col_bg_content col_fg_content
  global cur_filename dlg_mark_shown dlg_mark_sel dlg_mark_geom

  PreemptBgTasks
  if {![info exists dlg_mark_shown]} {
    toplevel .dlg_mark
    if {$cur_filename ne ""} {
      wm title .dlg_mark "Bookmark list - $cur_filename"
    } else {
      wm title .dlg_mark "Bookmark list"
    }
    wm group .dlg_mark .

    text .dlg_mark.l -width 1 -height 1 -wrap none -font $font_content -cursor top_left_arrow \
                     -foreground $col_fg_content -background $col_bg_content \
                     -exportselection 0 -insertofftime 0 -yscrollcommand {.dlg_mark.sb set} \
                     -insertwidth [expr {2 * [font measure $font_content " "]}]
    pack .dlg_mark.l -side left -fill both -expand 1
    scrollbar .dlg_mark.sb -orient vertical -command {.dlg_mark.l yview} -takefocus 0
    pack .dlg_mark.sb -side left -fill y

    bindtags .dlg_mark.l {.dlg_mark.l TextSel . all}
    TextSel_Init .dlg_mark.l dlg_mark_sel MarkList_SelectionChange MarkList_GetLen "browse"

    menu .dlg_mark.ctxmen -tearoff 0

    bind .dlg_mark.l <Insert> {MarkList_RenameSelected; break}
    bind .dlg_mark.l <Delete> {MarkList_RemoveSelected; break}
    bind .dlg_mark.l <Escape> {destroy .dlg_mark; break}
    bind .dlg_mark.l <ButtonRelease-3> {MarkList_ContextMenu %x %y}
    focus .dlg_mark.l

    set dlg_mark_shown 1
    bind .dlg_mark.l <Destroy> {+ MarkList_Quit 1}
    bind .dlg_mark <Configure> {ToplevelResized %W .dlg_mark .dlg_mark dlg_mark_geom}
    wm protocol .dlg_mark WM_DELETE_WINDOW {MarkList_Quit 0}
    wm geometry .dlg_mark $dlg_mark_geom
    wm positionfrom .dlg_mark user

    MarkList_CreateHighlightTags
    MarkList_Fill

  } else {
    wm deiconify .dlg_mark
    raise .dlg_mark
    focus .dlg_mark.l
  }
  ResumeBgTasks
}


#
# This function creates the tags for selection and color highlighting.
# This is used for initialisation and after editing highlight tags.
#
proc MarkList_CreateHighlightTags {} {
  global patlist fmt_selection

  foreach w $patlist {
    set tagnam [lindex $w 4]
    eval [linsert [HighlightConfigure $w] 0 .dlg_mark.l tag configure $tagnam]
  }
  eval [linsert [HighlightConfigure $fmt_selection] 0 .dlg_mark.l tag configure sel]
  .dlg_mark.l tag configure margin -lmargin1 10
  .dlg_mark.l tag lower sel
}


#
# This function is called after removal of tags in the Tag list dialog.
#
proc MarkList_DeleteTag {tag} {
  global dlg_mark_shown

  if {[info exists dlg_mark_shown]} {
    .dlg_mark.l tag delete $tag
  }
}


#
# This function is bound to destroy
#
proc MarkList_Quit {is_destroyed} {
  global dlg_mark_shown

  unset -nocomplain dlg_mark_shown
  if {!$is_destroyed} {
    destroy .dlg_mark
  }
  catch {destroy .dlg_mark.mren}
}


# ----------------------------------------------------------------------------
#
# This function opens a tiny dialog window which allows to enter a new text
# tag for a selected bookmark. The dialog window is sized and placed so that
# it exactly covers the respective entry in the bookmark list dialog to make
# it appear as if the listbox entry could be edited directly.
#
proc MarkList_OpenRename {idx} {
  global dlg_mark_shown dlg_mark_list mark_list
  global mark_rename mark_rename_idx
  global font_normal font_bold tick_str_prefix tick_pat_num

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    .dlg_mark.l see "[expr {$idx + 1}].0"
    set coo [.dlg_mark.l dlineinfo "[expr {$idx + 1}].0"]
    if {[llength $coo] == 5} {
      catch {destroy .dlg_mark.mren}
      frame .dlg_mark.mren -takefocus 0 -borderwidth 2 -relief raised

      set line [lindex $dlg_mark_list $idx]
      set mark_rename $mark_list($line)
      set mark_rename_idx $idx

      entry .dlg_mark.mren.e -width 12 -textvariable mark_rename -exportselection false -font $font_normal
      pack .dlg_mark.mren.e -side left -fill both -expand 1
      bind .dlg_mark.mren.e <Return> {MarkList_Rename $mark_rename_idx $mark_rename; MarkList_LeaveRename; break}
      bind .dlg_mark.mren.e <Escape> {MarkList_LeaveRename; break}

      button .dlg_mark.mren.b -text "X" -padx 2 -font $font_bold -command MarkList_LeaveRename
      pack .dlg_mark.mren.b -side left

      .dlg_mark.mren.e selection clear
      set off 0
      if {$tick_pat_num ne ""} {
        set skip [string length $tick_str_prefix]
        if {([string compare -length $skip $tick_str_prefix $mark_rename] == 0) &&
            ([regexp -start $skip {^[\d:,\.]+ *} $mark_rename match])} {
          set off [string length $match]
        }
      }
      .dlg_mark.mren.e selection from $off
      .dlg_mark.mren.e selection to end
      .dlg_mark.mren.e icursor $off

      set xcoo [expr {[lindex $coo 0] - 3}]
      set ycoo [expr {[lindex $coo 1] - 3}]
      set w [winfo width .dlg_mark.l]
      set h [expr {[lindex $coo 3] + 6}]
      place .dlg_mark.mren -in .dlg_mark.l -anchor nw -x $xcoo -y $ycoo -relwidth 1.0

      focus .dlg_mark.mren.e
      grab .dlg_mark.mren
    }
  }
}


#
# This function is bound to all events which signal an exit of the rename
# dialog window. The window is destroyed.
#
proc MarkList_LeaveRename {} {
  unset -nocomplain mark_rename mark_rename_idx
  catch {destroy .dlg_mark.mren}
  catch {focus .dlg_mark.l}
}


# ----------------------------------------------------------------------------
#
# This function creates or raises a dialog window which contains the
# search history, i.e. a list of previously used search expressions.
#
proc SearchHistory_Open {} {
  global font_content col_bg_content col_fg_content cur_filename
  global dlg_hist_shown dlg_hist_sel dlg_hist_geom

  PreemptBgTasks
  if {![info exists dlg_hist_shown]} {
    toplevel .dlg_hist
    if {$cur_filename ne ""} {
      wm title .dlg_hist "Search history - $cur_filename"
    } else {
      wm title .dlg_hist "Search history"
    }
    wm group .dlg_hist .

    frame .dlg_hist.f1
    text .dlg_hist.f1.l -width 1 -height 1 -wrap none -font $font_content -cursor top_left_arrow \
                        -foreground $col_fg_content -background $col_bg_content \
                        -exportselection 0 -insertofftime 0 -yscrollcommand {.dlg_hist.f1.sb set}
    pack .dlg_hist.f1.l -side left -fill both -expand 1
    scrollbar .dlg_hist.f1.sb -orient vertical -command {.dlg_hist.f1.l yview} -takefocus 0
    pack .dlg_hist.f1.sb -side left -fill y
    pack .dlg_hist.f1 -side top -fill both -expand 1

    frame  .dlg_hist.f2
    label  .dlg_hist.f2.lab_one -text "Find:"
    grid   .dlg_hist.f2.lab_one -sticky w -column 0 -row 0 -padx 5
    button .dlg_hist.f2.but_next -text "Next" -command {SearchHistory_Search 1} -underline 0 -state disabled -pady 2
    grid   .dlg_hist.f2.but_next -sticky we -column 1 -row 0
    button .dlg_hist.f2.but_prev -text "Prev." -command {SearchHistory_Search 0} -underline 0 -state disabled -pady 2
    grid   .dlg_hist.f2.but_prev -sticky we -column 2 -row 0
    button .dlg_hist.f2.but_all -text "All" -command {SearchHistory_SearchList 0} -underline 0 -state disabled -pady 2
    grid   .dlg_hist.f2.but_all -sticky we -column 3 -row 0
    button .dlg_hist.f2.but_blw -text "All below" -command {SearchHistory_SearchList 1} -state disabled -pady 2
    grid   .dlg_hist.f2.but_blw -sticky we -column 4 -row 0
    button .dlg_hist.f2.but_abve -text "All above" -command {SearchHistory_SearchList -1} -state disabled -pady 2
    grid   .dlg_hist.f2.but_abve -sticky we -column 5 -row 0
    pack   .dlg_hist.f2 -side top -anchor w -pady 2

    TextSel_Init .dlg_hist.f1.l dlg_hist_sel SearchHistory_SelectionChange SearchHistory_GetLen "extended"

    bindtags .dlg_hist.f1.l {.dlg_hist.f1.l TextSel . all}
    bind .dlg_hist.f1.l <Double-Button-1> {SearchHistory_CopyToSearch; SearchHistory_Search $tlb_last_dir; break}
    bind .dlg_hist.f1.l <ButtonRelease-3> {SearchHistory_ContextMenu %x %y; break}
    bind .dlg_hist.f1.l <Delete> {SearchHistory_Remove; break}
    bind .dlg_hist.f1.l <Key-n> {SearchHistory_Search 1; break}
    bind .dlg_hist.f1.l <Key-N> {SearchHistory_Search 0; break}
    bind .dlg_hist.f1.l <Key-a> {SearchHistory_SearchList 0; break}
    bind .dlg_hist.f1.l <Key-ampersand> {SearchHighlightClear; break}
    bind .dlg_hist.f1.l <Alt-Key-n> {SearchHistory_Search 1; break}
    bind .dlg_hist.f1.l <Alt-Key-p> {SearchHistory_Search 0; break}
    bind .dlg_hist.f1.l <Alt-Key-a> {SearchHistory_SearchList 0; break}
    bind .dlg_hist.f1.l <Alt-Key-P> {SearchHistory_SearchList -1; break}
    bind .dlg_hist.f1.l <Alt-Key-N> {SearchHistory_SearchList 1; break}
    focus .dlg_hist.f1.l

    menu .dlg_hist.ctxmen -tearoff 0

    set dlg_hist_shown 1
    bind .dlg_hist.f1.l <Destroy> {+ SearchHistory_Close}
    bind .dlg_hist <Configure> {ToplevelResized %W .dlg_hist .dlg_hist dlg_hist_geom}
    wm geometry .dlg_hist $dlg_hist_geom
    wm positionfrom .dlg_hist user

    set cw1 [font measure $font_content "reg.exp."]
    set cw2 [font measure $font_content "ign.case"]
    set tab_pos [list [expr {$cw1/2 + 5}] center \
                      [expr {$cw1 + $cw2/2 + 5}] center \
                      [expr {$cw1+$cw2 + 10}] left]
    .dlg_hist.f1.l configure -tabs $tab_pos
    .dlg_hist.f1.l tag configure small -font [DeriveFont $font_content -2]

    SearchHistory_Fill

  } else {
    wm deiconify .dlg_hist
    raise .dlg_hist
    focus .dlg_hist.f1.l
  }
  ResumeBgTasks
}


#
# This function is bound to destruction events on the search history dialog.
# The function releases all dialog resources.
#
proc SearchHistory_Close {} {
  global dlg_hist_sel dlg_hist_shown
  unset -nocomplain dlg_hist_sel dlg_hist_shown
}


#
# This function pops up a context menu for the search history dialog.
#
proc SearchHistory_ContextMenu {xcoo ycoo} {
  global dlg_hist_sel

  TextSel_ContextSelection dlg_hist_sel $xcoo $ycoo
  set sel [TextSel_GetSelection dlg_hist_sel]

  .dlg_hist.ctxmen delete 0 end

  set c 0
  if {[llength $sel] > 0} {
    if {$c > 0} {.dlg_hist.ctxmen add separator}
    .dlg_hist.ctxmen add command -label "Remove selected expressions" -command SearchHistory_Remove
    incr c 1
  }
  if {[llength $sel] == 1} {
    if {$c > 0} {.dlg_hist.ctxmen add separator}
    .dlg_hist.ctxmen add command -label "Copy to the search entry field" -command SearchHistory_CopyToSearch
  }

  if {$c > 0} {
    set rootx [expr {[winfo rootx .dlg_hist] + $xcoo}]
    set rooty [expr {[winfo rooty .dlg_hist] + $ycoo}]
    tk_popup .dlg_hist.ctxmen $rootx $rooty 0
  }
}


#
# This function returns all currently selected patterns. This is used only
# to save the selection across history modifications.
#
proc SearchHistory_StoreSel {} {
  global dlg_hist_shown dlg_hist_sel tlb_history

  set copy {}
  if {[info exists dlg_hist_shown]} {
    set sel [TextSel_GetSelection dlg_hist_sel]
    foreach idx $sel {
      lappend copy [lindex $tlb_history $idx]
    }
  }
  return $copy
}


#
# This function selects all patterns in the given list. This is used only
# to restore a previous selection after history modifications (e.g. sort
# order change due to use of a pattern for a search)
#
proc SearchHistory_RestoreSel {copy} {
  global dlg_hist_shown dlg_hist_sel tlb_history

  if {[info exists dlg_hist_shown]} {
    set sel {}
    foreach cphl $copy {
      set idx 0
      foreach hl $tlb_history {
        if {([lindex $hl 0] eq [lindex $cphl 0]) &&
            ([lindex $hl 1] eq [lindex $cphl 1]) &&
            ([lindex $hl 2] eq [lindex $cphl 2])} {
          lappend sel $idx
          break
        }
        incr idx
      }
    }
    TextSel_SetSelection dlg_hist_sel $sel
  }
}


#
# This function fills the search history dialog with all search expressions
# in the search history stack.  The last used expression is placed on top.
#
proc SearchHistory_Fill {} {
  global dlg_hist_shown tlb_history

  if {[info exists dlg_hist_shown]} {

    .dlg_hist.f1.l delete 1.0 end

    foreach hl $tlb_history {
      .dlg_hist.f1.l insert end "\t" {} [expr {[lindex $hl 1] ? "reg.exp.":"-"}] small \
                                "\t" {} [expr {[lindex $hl 2] ? "-":"ign.case"}] small \
                                "\t" {} [lindex $hl 0] {} "\n" {}
    }

    TextSel_SetSelection dlg_hist_sel {}
  }
}


#
# This function is bound to the "Remove selected lines" command in the
# search history list dialog's context menu.  All currently selected text
# lines are removed from the search list.
#
proc SearchHistory_Remove {} {
  global dlg_hist_sel tlb_history

  set sel [TextSel_GetSelection dlg_hist_sel]
  set sel [lsort -integer -decreasing -uniq $sel]
  foreach idx $sel {
    set line "[expr {$idx + 1}].0"
    set tlb_history [lreplace $tlb_history $idx $idx]
    .dlg_hist.f1.l delete $line [list $line + 1 lines]
  }
  TextSel_SetSelection dlg_hist_sel {}
  UpdateRcAfterIdle
}


#
# This function is invoked by the "Copy to search field" command in the
# search history list's context menu. (Note an almost identical menu entry
# exists in the tag list dialog.)
#
proc SearchHistory_CopyToSearch {} {
  global tlb_history tlb_find tlb_regexp tlb_case tlb_find_focus
  global dlg_hist_sel

  set sel [TextSel_GetSelection dlg_hist_sel]
  if {[llength $sel] == 1} {
    # force focus into find entry field & suppress "Enter" event
    SearchInit
    set tlb_find_focus 1
    focus .f2.e

    set hl [lindex $tlb_history [lindex $sel 0]]

    SearchHighlightClear
    set tlb_find [lindex $hl 0]
    set tlb_regexp [lindex $hl 1]
    set tlb_case [lindex $hl 2]
  }
}


#
# This function starts a search in the main text content for the selected
# expression, i.e. as if the word had been entered to the search text
# entry field. TODO: currently only one expression at a time can be searched
#
proc SearchHistory_Search {is_fwd} {
  global dlg_hist_sel tlb_history

  ClearStatusLine search

  set sel [TextSel_GetSelection dlg_hist_sel]
  if {[llength $sel] == 1} {
    set hl [lindex $tlb_history [lindex $sel 0]]

    set pat [lindex $hl 0]
    set is_re [lindex $hl 1]
    set use_case [lindex $hl 2]

    Search_AddHistory $pat $is_re $use_case

    if {[SearchExprCheck $pat $is_re 1]} {

      set found [Search_Atomic $pat $is_re $use_case $is_fwd 1]
      if {$found eq ""} {
        if {$is_fwd} {
          DisplayStatusLine search warn "No match until end of file"
        } else {
          DisplayStatusLine search warn "No match until start of file"
        }
      }
    }
  } else {
    DisplayStatusLine search error "No expression selected"
  }
}


#
# This function is bound to the "list all" button in the search history
# dialog. The function opens the search result list window and starts a
# search for the expression which is currently selected in the history list.
#
proc SearchHistory_SearchList {direction} {
  global tlb_history
  global dlg_hist_sel

  ClearStatusLine search

  set sel [TextSel_GetSelection dlg_hist_sel]
  if {[llength $sel] > 0} {
    set pat_list {}
    foreach idx $sel {
      set hl [lindex $tlb_history $idx]

      lappend pat_list [lrange $hl 0 2]

      Search_AddHistory [lindex $hl 0] [lindex $hl 1] [lindex $hl 2]
    }

    SearchList_Open 0
    SearchList_StartSearchAll $pat_list 1 $direction
  } else {
    DisplayStatusLine search error "No expression selected"
  }
}


#
# This function is a callback for selection changes in the search history dialog.
#
proc SearchHistory_SelectionChange {sel} {
  if {[llength $sel] == 1} {
    .dlg_hist.f2.but_next configure -state normal
    .dlg_hist.f2.but_prev configure -state normal
  } else {
    .dlg_hist.f2.but_next configure -state disabled
    .dlg_hist.f2.but_prev configure -state disabled
  }
  if {[llength $sel] > 0} {
    .dlg_hist.f2.but_all configure -state normal
    .dlg_hist.f2.but_abve configure -state normal
    .dlg_hist.f2.but_blw configure -state normal
  } else {
    .dlg_hist.f2.but_all configure -state disabled
    .dlg_hist.f2.but_abve configure -state disabled
    .dlg_hist.f2.but_blw configure -state disabled
  }
}


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
proc SearchHistory_GetLen {} {
  global tlb_history
  return [llength $tlb_history]
}


# ----------------------------------------------------------------------------
#
# This function creates or raises a dialog window which collects text lines
# matching one or more search expressions.  The user can also freely add or
# remove lines from the list.
#
proc SearchList_Open {raise_win} {
  global font_content col_bg_content col_fg_content cur_filename
  global dlg_srch_shown dlg_srch_geom dlg_srch_sel dlg_srch_lines dlg_srch_fn_cache
  global dlg_srch_highlight dlg_srch_show_fn dlg_srch_fn_delta dlg_srch_fn_root
  global dlg_srch_undo dlg_srch_redo

  PreemptBgTasks
  if {![info exists dlg_srch_shown]} {
    toplevel .dlg_srch
    if {$cur_filename ne ""} {
      wm title .dlg_srch "Search matches - $cur_filename"
    } else {
      wm title .dlg_srch "Search matches"
    }
    wm group .dlg_srch .

    menu .dlg_srch.menubar
    .dlg_srch config -menu .dlg_srch.menubar
    .dlg_srch.menubar add cascade -label "Control" -menu .dlg_srch.menubar.ctrl -underline 0
    .dlg_srch.menubar add cascade -label "Edit" -menu .dlg_srch.menubar.edit -underline 0
    .dlg_srch.menubar add cascade -label "Search" -menu .dlg_srch.menubar.search -underline 0
    .dlg_srch.menubar add cascade -label "Options" -menu .dlg_srch.menubar.options -underline 0
    menu .dlg_srch.menubar.ctrl -tearoff 0 -postcommand MenuPosted
    .dlg_srch.menubar.ctrl add command -label "Load line numbers..." -command {SearchList_LoadFrom}
    .dlg_srch.menubar.ctrl add command -label "Save text as..." -command {SearchList_SaveFileAs 0}
    .dlg_srch.menubar.ctrl add command -label "Save line numbers..." -command {SearchList_SaveFileAs 1}
    .dlg_srch.menubar.ctrl add separator
    .dlg_srch.menubar.ctrl add command -label "Clear all" -command SearchList_Clear
    .dlg_srch.menubar.ctrl add command -label "Close" -command {destroy .dlg_srch}
    menu .dlg_srch.menubar.edit -tearoff 0 -postcommand SearchList_MenuPosted
    .dlg_srch.menubar.edit add command -label "Undo" -command SearchList_Undo
    .dlg_srch.menubar.edit add command -label "Redo" -command SearchList_Redo
    .dlg_srch.menubar.edit add separator
    .dlg_srch.menubar.edit add command -label "Import selected lines from main window" -command SearchList_CopyCurrentLine
    .dlg_srch.menubar.edit add separator
    .dlg_srch.menubar.edit add command -label "Remove selected lines" -accelerator "Del" -command SearchList_RemoveSelection
    menu .dlg_srch.menubar.search -tearoff 0 -postcommand MenuPosted
    .dlg_srch.menubar.search add command -label "Search history..." -command SearchHistory_Open
    .dlg_srch.menubar.search add command -label "Edit highlight patterns..." -command TagList_OpenDialog
    .dlg_srch.menubar.search add separator
    .dlg_srch.menubar.search add command -label "Insert all search matches..." -command {SearchAll 1 0} -accelerator "ALT-a"
    .dlg_srch.menubar.search add command -label "Insert all matches above..." -command {SearchAll 1 -1} -accelerator "ALT-P"
    .dlg_srch.menubar.search add command -label "Insert all matches below..." -command {SearchAll 1 1} -accelerator "ALT-N"
    .dlg_srch.menubar.edit add separator
    .dlg_srch.menubar.edit add command -label "Add main window search matches" -command {SearchList_AddMatches 0}
    .dlg_srch.menubar.edit add command -label "Remove main window search matches" -command {SearchList_RemoveMatches 0}
    .dlg_srch.menubar.search add separator
    .dlg_srch.menubar.search add command -label "Clear search highlight" -command {SearchHighlightClear} -accelerator "&"
    menu .dlg_srch.menubar.options -tearoff 0 -postcommand MenuPosted
    .dlg_srch.menubar.options add checkbutton -label "Show frame number" -command SearchList_ToggleTickNo -variable dlg_srch_show_fn -accelerator "ALT-f"
    .dlg_srch.menubar.options add checkbutton -label "Show frame no. delta" -command SearchList_ToggleTickNo -variable dlg_srch_fn_delta -accelerator "ALT-d"
    .dlg_srch.menubar.options add checkbutton -label "Highlight search" -command SearchList_ToggleHighlight -variable dlg_srch_highlight -accelerator "ALT-h"
    .dlg_srch.menubar.options add separator
    .dlg_srch.menubar.options add command -label "Select line as origin for FN delta" -command SearchList_SetFnRoot -accelerator "ALT-o"

    frame .dlg_srch.f1
    text .dlg_srch.f1.l -width 1 -height 1 -wrap none -font $font_content -cursor top_left_arrow \
                        -foreground $col_fg_content -background $col_bg_content \
                        -exportselection 0 -insertofftime 0 \
                        -insertwidth [expr {2 * [font measure $font_content " "]}] \
                        -yscrollcommand {.dlg_srch.f1.sb set}
    pack .dlg_srch.f1.l -side left -fill both -expand 1
    scrollbar .dlg_srch.f1.sb -orient vertical -command {.dlg_srch.f1.l yview} -takefocus 0
    pack .dlg_srch.f1.sb -side left -fill y
    pack .dlg_srch.f1 -side top -fill both -expand 1

    TextSel_Init .dlg_srch.f1.l dlg_srch_sel SearchList_SelectionChange SearchList_GetLen "browse"

    bindtags .dlg_srch.f1.l {.dlg_srch.f1.l TextSel . all}
    bind .dlg_srch.f1.l <ButtonRelease-3> {SearchList_ContextMenu %x %y; break}
    bind .dlg_srch.f1.l <Delete> {SearchList_RemoveSelection; break}
    bind .dlg_srch.f1.l <Control-plus> {ChangeFontSize 1; KeyClr; break}
    bind .dlg_srch.f1.l <Control-minus> {ChangeFontSize -1; KeyClr; break}
    bind .dlg_srch.f1.l <Control-Key-g> {SearchList_DisplayStats; KeyClr; break}
    KeyCmdBind .dlg_srch.f1.l "/" {SearchEnter 1 .dlg_srch.f1.l}
    KeyCmdBind .dlg_srch.f1.l "?" {SearchEnter 0 .dlg_srch.f1.l}
    KeyCmdBind .dlg_srch.f1.l "n" {SearchList_SearchNext 1}
    KeyCmdBind .dlg_srch.f1.l "N" {SearchList_SearchNext 0}
    KeyCmdBind .dlg_srch.f1.l "&" {SearchHighlightClear}
    KeyCmdBind .dlg_srch.f1.l "m" SearchList_ToggleMark
    KeyCmdBind .dlg_srch.f1.l "u" SearchList_Undo
    bind .dlg_srch.f1.l <Control-Key-r> SearchList_Redo
    bind .dlg_srch.f1.l <space> {SearchList_SelectionChange [TextSel_GetSelection dlg_srch_sel]}
    bind .dlg_srch.f1.l <Escape> {SearchList_SearchAbort 0}
    bind .dlg_srch.f1.l <Alt-Key-h> {set dlg_srch_highlight [expr {!$dlg_srch_highlight}]; SearchList_ToggleHighlight; break}
    bind .dlg_srch.f1.l <Alt-Key-f> {set dlg_srch_show_fn [expr {!$dlg_srch_show_fn}]; SearchList_ToggleTickNo; break}
    bind .dlg_srch.f1.l <Alt-Key-d> {set dlg_srch_fn_delta [expr {!$dlg_srch_fn_delta}]; SearchList_ToggleTickNo; break}
    bind .dlg_srch.f1.l <Alt-Key-o> {SearchList_SetFnRoot; break}
    bind .dlg_srch.f1.l <Alt-Key-n> {SearchNext 1; break}
    bind .dlg_srch.f1.l <Alt-Key-p> {SearchNext 0; break}
    bind .dlg_srch.f1.l <Alt-Key-a> {SearchAll 0 0; break}
    bind .dlg_srch.f1.l <Alt-Key-N> {SearchAll 0 1; break}
    bind .dlg_srch.f1.l <Alt-Key-P> {SearchAll 0 -1; break}
    focus .dlg_srch.f1.l

    menu .dlg_srch.ctxmen -tearoff 0

    set dlg_srch_shown 1
    bind .dlg_srch.f1.l <Destroy> {+ SearchList_Close}
    bind .dlg_srch <Configure> {ToplevelResized %W .dlg_srch .dlg_srch dlg_srch_geom}
    wm geometry .dlg_srch $dlg_srch_geom
    wm positionfrom .dlg_srch user

    # reset options to default values
    set dlg_srch_show_fn 0
    set dlg_srch_fn_delta 0
    set dlg_srch_highlight 0

    SearchList_Init
    SearchList_CreateHighlightTags

  } elseif {$raise_win} {
    wm deiconify .dlg_srch
    raise .dlg_srch
  }
  ResumeBgTasks
}


#
# This function is bound to destruction events on the search list dialog window.
# The function stops background processes and releases all dialog resources.
#
proc SearchList_Close {} {
  global dlg_srch_sel dlg_srch_lines dlg_srch_fn_cache dlg_srch_shown

  SearchList_SearchAbort 0

  unset -nocomplain dlg_srch_sel dlg_srch_lines dlg_srch_shown
  unset -nocomplain dlg_srch_undo dlg_srch_redo
  array unset dlg_srch_fn_cache
}


#
# This function removes all content in the search list.
#
proc SearchList_Clear {} {
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel
  global dlg_srch_undo dlg_srch_redo

  if {[info exists dlg_srch_shown]} {
    SearchList_SearchAbort 0

    if {[llength $dlg_srch_lines] > 0} {
      lappend dlg_srch_undo [list -1 $dlg_srch_lines]
      set dlg_srch_redo {}
    }
    set dlg_srch_lines {}
    .dlg_srch.f1.l delete 1.0 end

    TextSel_SetSelection dlg_srch_sel {}
  }
}


#
# This function clears the content and resets the dialog state variables.
# The function is used when the window is newly opened or a new file is loaded.
#
proc SearchList_Init {} {
  global dlg_srch_shown dlg_srch_lines dlg_srch_fn_cache dlg_srch_fn_root
  global dlg_srch_undo dlg_srch_redo

  if {[info exists dlg_srch_shown]} {
    set dlg_srch_lines {}
    set dlg_srch_undo {}
    set dlg_srch_redo {}
    set dlg_srch_fn_root -1
    array unset dlg_srch_fn_cache
  }
}


#
# This function is called when the "edit" menu in the search list dialog is opened.
#
proc SearchList_MenuPosted {} {
  global dlg_srch_undo dlg_srch_redo

  if {[llength $dlg_srch_undo] > 0} {
    .dlg_srch.menubar.edit entryconfigure "Undo*" -state normal
    set cmd [lindex $dlg_srch_undo end]
    if {[lindex $cmd] > 0} { set op "addition" } else { set op "removal" }
    .dlg_srch.menubar.edit entryconfigure "Undo*" -label "Undo ($op of [llength [lindex $cmd 1]] lines)"
  } else {
    .dlg_srch.menubar.edit entryconfigure "Undo*" -state disabled -label "Undo"
  }
  if {[llength $dlg_srch_redo] > 0} {
    .dlg_srch.menubar.edit entryconfigure "Redo*" -state normal
    set cmd [lindex $dlg_srch_redo end]
    if {[lindex $cmd] > 0} { set op "addition" } else { set op "removal" }
    .dlg_srch.menubar.edit entryconfigure "Redo*" -label "Redo ($op of [llength [lindex $cmd 1]] lines)"
  } else {
    .dlg_srch.menubar.edit entryconfigure "Redo*" -state disabled -label "Redo"
  }
  MenuPosted
}


#
# This function pops up a context menu for the search list dialog.
#
proc SearchList_ContextMenu {xcoo ycoo} {
  global tlb_find
  global dlg_srch_sel dlg_srch_lines
  global dlg_srch_show_fn dlg_srch_fn_delta dlg_srch_fn_root
  global tick_pat_sep tick_pat_num tick_str_prefix dlg_srch_fn_cache

  TextSel_ContextSelection dlg_srch_sel $xcoo $ycoo
  set sel [TextSel_GetSelection dlg_srch_sel]

  .dlg_srch.ctxmen delete 0 end

  set c 0
  if {$tlb_find ne ""} {
    .dlg_srch.ctxmen add command -label "Import matches on current search" \
                                 -command {SearchList_AddMatches 0}
    incr c 1
  }
  if {[.f1.t tag nextrange sel 1.0] ne ""} {
    .dlg_srch.ctxmen add command -label "Import main window selection" \
                                 -command SearchList_AddMainSelection
    incr c 1
  }

  if {([llength $sel] == 1) && (($tick_pat_sep ne "") || ($tick_pat_num ne ""))} {
    set line [lindex $dlg_srch_lines [lindex $sel 0]]
    set fn [ParseFrameTickNo "$line.0" dlg_srch_fn_cache]
    if {$fn ne ""} {
      if {$c > 0} {.dlg_srch.ctxmen add separator}
      .dlg_srch.ctxmen add command -label "Select line as origin for FN delta" \
                                   -command SearchList_SetFnRoot
      set c 1
    }
  }

  if {[llength $sel] > 0} {
    if {$c > 0} {.dlg_srch.ctxmen add separator}
    .dlg_srch.ctxmen add command -label "Remove selected lines" \
                                 -command SearchList_RemoveSelection
    set c 1
  }

  if {$c > 0} {
    set rootx [expr {[winfo rootx .dlg_srch] + $xcoo}]
    set rooty [expr {[winfo rooty .dlg_srch] + $ycoo}]
    tk_popup .dlg_srch.ctxmen $rootx $rooty 0
  }
}


#
# This function is bound to the "Remove selected lines" command in the
# search list dialog's context menu.  All currently selected text lines
# are removed from the search list.
#
proc SearchList_RemoveSelection {} {
  global dlg_srch_sel dlg_srch_lines
  global dlg_srch_undo dlg_srch_redo

  if {[SearchList_SearchAbort]} {
    set sel [TextSel_GetSelection dlg_srch_sel]
    set sel [lsort -integer -decreasing -uniq $sel]
    if {[llength $sel] > 0} {
      set new_lines $dlg_srch_lines
      set line_list {}
      foreach idx $sel {
        lappend line_list [lindex $new_lines $idx]
        #too slow: set dlg_srch_lines [lreplace $new_lines $idx $idx]
        lset new_lines $idx -1

        set line "[expr {$idx + 1}].0"
        .dlg_srch.f1.l delete $line "$line +1 lines"
      }
      set dlg_srch_lines [lsearch -all -inline -exact -integer -not $new_lines -1]

      lappend dlg_srch_undo [list -1 $line_list]
      set dlg_srch_redo {}

      TextSel_SetSelection dlg_srch_sel {}
    }
  }
}


#
# This function is bound to "n", "N" in the search filter dialog. The function
# starts a regular search in the main window, but repeats until a matching
# line is found which is also listed in the filter dialog.
#
proc SearchList_SearchNext {is_fwd} {
  global dlg_srch_sel dlg_srch_lines

  set old_yview [.f1.t yview]
  set old_cpos [.f1.t index insert]

  if {$is_fwd} {
    .f1.t mark set insert {insert lineend}
  } else {
    .f1.t mark set insert {insert linestart}
  }
  set found_any 0

  while 1 {
    set found [SearchNext $is_fwd]
    if {$found ne ""} {
      set found_any 1
      # check if the found line is also listed in the search list
      scan $found "%d" line
      set idx [lsearch -exact -integer -sorted -increasing $dlg_srch_lines $line]
      if {$idx >= 0} {
        TextSel_SetSelection dlg_srch_sel $idx
        break
      }
    } else {
      break
    }
  }

  # if none found, set the cursor back to the original position
  if {$found eq ""} {
    .f1.t mark set insert $old_cpos
    .f1.t yview moveto [lindex $old_yview 0]

    if {$found_any} {
      # match found in main window, but not in search result window
      DisplayStatusLine search warn "No match in search result list"
    }
  }
}


#
# This function is bound to the "Show frame number" checkbutton which toggles
# the display of frame numbers in front of each line on/off.
#
proc SearchList_ToggleTickNo {} {
  global dlg_srch_sel dlg_srch_lines dlg_srch_fn_cache tick_pat_sep tick_pat_num
  global dlg_srch_show_fn dlg_srch_fn_delta dlg_srch_fn_root

  if {($tick_pat_sep ne "") || ($tick_pat_num ne "")} {
    if {[SearchList_SearchAbort]} {
      if {$dlg_srch_fn_delta && ($dlg_srch_fn_root == -1)} {
        set sel [TextSel_GetSelection dlg_srch_sel]
        if {[llength $sel] > 0} {
          set line [lindex $dlg_srch_lines [lindex $sel 0]]
          set fn [ParseFrameTickNo "$line.0" dlg_srch_fn_cache]
          if {$fn ne ""} {
            set dlg_srch_fn_root $fn
          } else {
            DisplayStatusLine search warn "Failed to extract a frame number from the selected line"
          }
        } else {
          DisplayStatusLine search warn "Please select a line as origin for FN deltas"
        }
      }

      SearchList_Refill
    }

  } else {
    DisplayStatusLine search error "No patterns defined in the RC file for parsing frame numbers"
    set dlg_srch_fn_delta 0
    set dlg_srch_show_fn 0
  }
}


#
# This function is bound to ALT-o in the search result list and to the
# "Select root FN" context menu command. The function sets the currently
# selected line as origin for frame number delta calculations and enables
# frame number delta display, which requires a complete refresh of the list.
#
proc SearchList_SetFnRoot {} {
  global dlg_srch_sel dlg_srch_lines dlg_srch_fn_cache tick_pat_sep tick_pat_num
  global dlg_srch_fn_delta dlg_srch_fn_root

  if {($tick_pat_sep ne "") || ($tick_pat_num ne "")} {
    if {[SearchList_SearchAbort]} {
      set sel [TextSel_GetSelection dlg_srch_sel]
      if {[llength $sel] > 0} {
        set line [lindex $dlg_srch_lines [lindex $sel 0]]
        # extract the frame number from the text in the main window around the referenced line
        set fn [ParseFrameTickNo "$line.0" dlg_srch_fn_cache]
        if {$fn ne ""} {
          set dlg_srch_fn_delta 1
          set dlg_srch_fn_root $fn
          SearchList_Refill

        } else {
          DisplayStatusLine search error "Select a line as origin for FN deltas"
        }
      } else {
        DisplayStatusLine search error "Select a line as origin for FN deltas"
      }
    }
  } else {
    DisplayStatusLine search error "No patterns defined in the RC file for parsing frame numbers"
  }
}


#
# This function is bound to the "Highlight search" checkbutton which toggles
# highlighting of lines matching searches in the main window on/off.
#
proc SearchList_ToggleHighlight {} {
  global dlg_srch_highlight tlb_last_hall

  if {$dlg_srch_highlight} {
    # search highlighting was enabled:
    # force update of global highlighting (in main and search result windows)
    set tlb_last_hall ""
    SearchHighlightUpdate
  } else {
    # search highlighting was enabled: remove highlight tag in the search list dialog
    .dlg_srch.f1.l tag remove find 1.0 end
  }
}


#
# This function is bound to the "Undo" menu command any keyboard shortcut.
# This reverts the last modification of the line list (i.e. last removal or
# addition, either via search or manually.)
#
proc SearchList_Undo {} {
  global dlg_srch_shown dlg_srch_undo dlg_srch_redo
  global tid_search_list

  ClearStatusLine search
  if {[info exists dlg_srch_shown]} {
    if {[llength $dlg_srch_undo] > 0} {
      if {[SearchList_SearchAbort]} {
        set cmd [lindex $dlg_srch_undo end]
        set dlg_srch_undo [lrange $dlg_srch_undo 0 end-1]

        set tid_search_list [after 10 [list SearchList_BgUndoRedoLoop \
                                            [lindex $cmd 0] [lindex $cmd 1] -1 0]]
      }
    } else {
      DisplayStatusLine search warn "Already at oldest change in search list"
    }
  }
}


#
# This function is bound to the "Redo" menu command any keyboard shortcut.
# This reverts the last "undo", if any.
#
proc SearchList_Redo {} {
  global dlg_srch_shown dlg_srch_undo dlg_srch_redo
  global tid_search_list

  ClearStatusLine search
  if {[info exists dlg_srch_shown]} {
    if {[llength $dlg_srch_redo] > 0} {
      if {[SearchList_SearchAbort]} {

        set cmd [lindex $dlg_srch_redo end]
        set dlg_srch_redo [lrange $dlg_srch_redo 0 end-1]

        set tid_search_list [after 10 [list SearchList_BgUndoRedoLoop \
                                            [lindex $cmd 0] [lindex $cmd 1] 1 0]]
      }
    } else {
      DisplayStatusLine search warn "Already at newest change in search list"
    }
  }
}


#
# This function acts as background process for undo and redo operations.
# Each iteration of this task works on at most 250-500 lines.
#
proc SearchList_BgUndoRedoLoop {op line_list mode off} {
  global block_bg_tasks tid_search_inc tid_search_hall tid_search_list
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel
  global dlg_srch_undo dlg_srch_redo

  if {$block_bg_tasks || ($tid_search_inc ne {}) || ($tid_search_hall ne {})} {
    # background tasks are suspended - re-schedule with timer
    set tid_search_list [after 100 [list SearchList_BgUndoRedoLoop $op $line_list $mode $off]]

  } elseif [info exists dlg_srch_shown] {
    set anchor [SearchList_GetViewAnchor]
    set line_frag [lrange $line_list $off [expr {$off + 399}]]
    incr off 400

    SearchList_InvertCmd $op $line_frag $mode

    set do_add [expr {$op > 0}]
    if {$mode < 0} {
      SearchList_BgSearch_AppendUndoList dlg_srch_redo $do_add $line_frag
    } else {
      SearchList_BgSearch_AppendUndoList dlg_srch_undo $do_add $line_frag
    }

    # select previously selected line again
    SearchList_SeeViewAnchor $anchor

    if {$off <= [llength $line_list]} {
      # create or update the progress bar
      set ratio [expr {int(100.0 * $off / [llength $line_list])}]
      SearchList_SearchProgress $ratio

      set tid_search_list [after idle [list SearchList_BgUndoRedoLoop $op $line_list $mode $off]]

    } else {
      if {$mode < 0} {
        SearchList_BgSearch_FinalizeUndoList dlg_srch_redo
      } else {
        SearchList_BgSearch_FinalizeUndoList dlg_srch_undo
      }

      set tid_search_list {}
      catch {destroy .dlg_srch.slpro}
      catch {destroy .srch_abrt}
      .dlg_srch.f1.l configure -cursor top_left_arrow
    }
  } else {
    set tid_search_list {}
  }
}


#
# This helper function performs a command for "undo" and "redo"
#
proc SearchList_InvertCmd {op line_list mode} {
  global dlg_srch_lines

  if {$op * $mode < 0} {
    # undo insertion, i.e. delete lines again
    set new_lines $dlg_srch_lines
    #set count 0
    foreach line [lsort -integer -decreasing $line_list] {
      #set idx [SearchList_GetLineIdx $line]
      #if {[lindex $dlg_srch_lines $idx] == $line}
      set idx [lsearch -exact -integer -sorted -increasing $dlg_srch_lines $line]
      if {$idx >= 0} {
        set pos "[expr {$idx + 1}].0"
        .dlg_srch.f1.l delete $pos "$pos +1 lines"

        #too slow: set dlg_srch_lines [lreplace $new_lines $idx $idx]
        #also slow: set new_lines [lreplace $new_lines $idx $idx -1]
        lset new_lines $idx -1
        #incr count
      }
    }
    #set dlg_srch_lines [lrange [lsort -integer -increasing $new_lines] $count end]
    set dlg_srch_lines [lsearch -all -inline -exact -integer -not $new_lines -1]

  } elseif {$op * $mode > 0} {
    # re-insert previously removed lines
    set new_lines {}
    foreach line [lsort -integer -decreasing $line_list] {
      set idx [SearchList_GetLineIdx $line]
      if {[lindex $dlg_srch_lines $idx] != $line} {
        SearchList_InsertLine $line "[expr {$idx + 1}].0"

        #too slow: set dlg_srch_lines [linsert $dlg_srch_lines $idx $line]
        lappend new_lines $line
      }
    }
    set dlg_srch_lines [lsort -integer -increasing [concat $dlg_srch_lines $new_lines]]
  }
}


#
# Wrapper functions to simplify external interfaces
#
proc SearchList_AddMatches {direction} {
  global tlb_find tlb_regexp tlb_case
  SearchList_SearchMatches 1 $tlb_find $tlb_regexp $tlb_case $direction
}

proc SearchList_RemoveMatches {direction} {
  global tlb_find tlb_regexp tlb_case
  SearchList_SearchMatches 0 $tlb_find $tlb_regexp $tlb_case $direction
}


#
# This function is the external interface to the search list for adding
# or removing lines matching the given search pattern.  The search is
# performed in a background task, i.e. it's not completed when this
# function returns.
#
proc SearchList_SearchMatches {do_add pat is_re use_case direction} {
  global dlg_srch_sel

  if {$pat ne ""} {
    if {[SearchExprCheck $pat $is_re 1]} {
      set hl [list $pat $is_re $use_case]
      set pat_list [list $hl]
      SearchList_StartSearchAll $pat_list $do_add $direction
    }
  }
}


#
# Helper function which performs a binary search in the sorted line index
# list for the first value which is larger or equal to the given value.
# Returns the index of the element, or the length of the list if all
# values in the list are smaller.
#
proc SearchList_GetLineIdx {ins_line} {
  global dlg_srch_lines

  set end [llength $dlg_srch_lines]
  set min -1
  set max $end
  if {$end > 0} {
    set idx [expr {$end >> 1}]
    incr end -1
    while 1 {
      set el [lindex $dlg_srch_lines $idx]
      if {$el < $ins_line} {
        set min $idx
        set idx [expr {($idx + $max) >> 1}]
        if {($idx >= $max) || ($idx <= $min)} {
          break
        }
      } elseif {$el > $ins_line} {
        set max $idx
        set idx [expr {($min + $idx) >> 1}]
        if {$idx <= $min} {
          break
        }
      } else {
        set max $idx
        break
      }
    }
  }
  return $max
}


#
# This function starts the search in the main text content for all matches
# to a given pattern.  Matching lines are either inserted or removed from the
# search list. The search is performed in the background and NOT finished when
# this function returns.  Possibly still running older searches are aborted.
#
proc SearchList_StartSearchAll {pat_list do_add direction} {
  global dlg_srch_redo tid_search_list

  if {[SearchList_SearchAbort]} {
    if {$direction == 0} {
      set line 1
    } else {
      scan [.f1.t index insert] "%d" line
    }
    # reset redo list
    set dlg_srch_redo {}

    set tid_search_list [after 10 [list SearchList_BgSearchLoop $pat_list $do_add \
                                                                $direction $line 0 0]]
  }
}


#
# This function acts as background process to fill the search list window.
# The search loop continues for at most 100ms, then the function re-schedules
# itself as idle task.
#
proc SearchList_BgSearchLoop {pat_list do_add direction line pat_idx loop_cnt} {
  global block_bg_tasks tid_search_inc tid_search_hall tid_search_list
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel dlg_srch_undo

  if {$block_bg_tasks || ($tid_search_inc ne {}) || ($tid_search_hall ne {})} {
    # background tasks are suspended - re-schedule with timer
    set tid_search_list [after 100 [list SearchList_BgSearchLoop $pat_list $do_add $direction $line $pat_idx 0]]

  } elseif {$loop_cnt > 10} {
    set tid_search_list [after 10 [list SearchList_BgSearchLoop $pat_list $do_add $direction $line $pat_idx 0]]

  } elseif [info exists dlg_srch_shown] {
    scan [.f1.t index end] "%d" max_line
    set anchor [SearchList_GetViewAnchor]
    set stop_t [expr {[clock clicks -milliseconds] + 100}]
    set hl [lindex $pat_list $pat_idx]
    set pat [lindex $hl 0]
    set opt [Search_GetOptions $pat [lindex $hl 1] [lindex $hl 2] [expr {$direction < 0 ? 0 : 1}]]
    set line_list {}
    set off 0
    if {$direction >= 0} {
      set last_line end
    } else {
      set last_line "1.0"
    }

    while {($line < $max_line) &&
           ([set pos [eval .f1.t search $opt -- {$pat} "$line.0" $last_line]] ne "")} {
      scan $pos "%d" line

      if {$do_add} {
        set idx [SearchList_GetLineIdx $line]
        if {[lindex $dlg_srch_lines $idx] != $line} {
          SearchList_InsertLine $line "[expr {$idx + $off + 1}].0"
          lappend line_list $line
          # linsert is extremely slow on large lists, so use append and sort instead (outside of the loop)
          #set dlg_srch_lines [linsert $dlg_srch_lines $idx $line]
          if {$direction >= 0} {
            incr off
          }
        }
      } else {
        set idx [lsearch -exact -integer -sorted -increasing $dlg_srch_lines $line]
        if {$idx >= 0} {
          lappend line_list $line
          set dlg_srch_lines [lreplace $dlg_srch_lines $idx $idx]
          .dlg_srch.f1.l delete "[expr {$idx + 1}].0" "[expr {$idx + 2}].0"
        }
      }

      if {$direction >= 0} {
        incr line
      }

      if {[clock clicks -milliseconds] >= $stop_t} {
        break
      }
    }

    if {[llength $line_list] > 0} {
      SearchList_BgSearch_AppendUndoList dlg_srch_undo $do_add $line_list
      if {$do_add} {
        set dlg_srch_lines [lsort -integer -increasing [concat $dlg_srch_lines $line_list]]
      }
      # select previously selected line again
      SearchList_SeeViewAnchor $anchor
    }

    if {($line < $max_line) && ($pos ne "")} {
      # create or update the progress bar
      if {$direction == 0} {
        set ratio [expr {double($line) / $max_line}]
      } elseif {$direction < 0} {
        scan [.f1.t index insert] "%d" thresh
        set ratio [expr {1 - (double($line) / $thresh)}]
      } else {
        scan [.f1.t index insert] "%d" thresh
        set ratio [expr {double($line) / ($max_line - $thresh)}]
      }
      set ratio [expr {int(100.0*($ratio + $pat_idx)/[llength $pat_list])}]
      SearchList_SearchProgress $ratio

      incr loop_cnt
      set tid_search_list [after idle [list SearchList_BgSearchLoop $pat_list $do_add $direction $line $pat_idx $loop_cnt]]

    } else {
      SearchList_BgSearch_FinalizeUndoList dlg_srch_undo
      incr pat_idx
      if {$pat_idx < [llength $pat_list]} {
        incr loop_cnt
        if {$direction == 0} {
          set line 1
        } else {
          scan [.f1.t index insert] "%d" line
        }
        set tid_search_list [after idle [list SearchList_BgSearchLoop $pat_list $do_add $direction $line $pat_idx $loop_cnt]]
      } else {
        set tid_search_list {}
        catch {destroy .dlg_srch.slpro}
        catch {destroy .srch_abrt}
        .dlg_srch.f1.l configure -cursor top_left_arrow
      }
    }
  } else {
    set tid_search_list {}
  }
}


#
# This function inserts all lines of text tagged with one of the given
# tags in the main window content into the search list.
#
proc SearchList_StartSearchTags {tag_list direction} {
  global tid_search_list

  if {[SearchList_SearchAbort]} {
    if {$direction == 1} {
      scan [.f1.t index insert] "%d" line
    } else {
      set line 1
    }
    # reset redo list
    set dlg_srch_redo {}

    set tid_search_list [after 10 [list SearchList_BgSearchTagsLoop $tag_list 0 $direction $line 0]]
  }
}


#
# This function acts as background process to fill the search list window with
# matches on highlight tags.
#
proc SearchList_BgSearchTagsLoop {tag_list tag_idx direction line loop_cnt} {
  global block_bg_tasks tid_search_inc tid_search_hall tid_search_list
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel dlg_srch_undo

  if {$block_bg_tasks || ($tid_search_inc ne {}) || ($tid_search_hall ne {})} {
    # background tasks are suspended - re-schedule with timer
    set tid_search_list [after 100 [list SearchList_BgSearchTagsLoop $tag_list $tag_idx $direction $line 0]]

  } elseif {$loop_cnt > 10} {
    set tid_search_list [after 10 [list SearchList_BgSearchTagsLoop $tag_list $tag_idx $direction $line 0]]

  } elseif [info exists dlg_srch_shown] {
    if {$direction < 0} {
      set last_line [.f1.t index insert]
    } else {
      set last_line [.f1.t index end]
    }
    set anchor [SearchList_GetViewAnchor]
    set stop_t [expr {[clock clicks -milliseconds] + 100}]
    set tagnam [lindex $tag_list $tag_idx]
    set line_list {}
    set off 0

    while {[set pos12 [.f1.t tag nextrange $tagnam "$line.0" $last_line]] ne ""} {
      scan [lindex $pos12 0] "%d" line

      set idx [SearchList_GetLineIdx $line]
      if {[lindex $dlg_srch_lines $idx] != $line} {
        SearchList_InsertLine $line "[expr {$idx + $off + 1}].0"
        lappend line_list $line
        # linsert is extremely slow on large lists... (see also comments in other search loop)
        #set dlg_srch_lines [linsert $dlg_srch_lines $idx $line]
        incr off
      }
      incr line

      if {[clock clicks -milliseconds] >= $stop_t} {
        break
      }
    }

    if {[llength $line_list] > 0} {
      SearchList_BgSearch_AppendUndoList dlg_srch_undo 1 $line_list
      set dlg_srch_lines [lsort -integer -increasing [concat $dlg_srch_lines $line_list]]

      # select previously selected line again
      SearchList_SeeViewAnchor $anchor
    }

    if {$pos12 ne ""} {
      # create or update the progress bar
      scan $last_line "%d" max_line
      scan [.f1.t index insert] "%d" thresh
      if {$direction == 0} {
        set ratio [expr {double($line) / $max_line}]
      } elseif {$direction < 0} {
        set ratio [expr {double($line) / $thresh}]
      } else {
        set ratio [expr {double($line) / ($max_line - $thresh)}]
      }
      set ratio [expr {int(100.0*($ratio + $tag_idx)/[llength $tag_list])}]
      SearchList_SearchProgress $ratio

      incr loop_cnt
      set tid_search_list [after idle [list SearchList_BgSearchTagsLoop $tag_list $tag_idx $direction $line $loop_cnt]]

    } else {
      SearchList_BgSearch_FinalizeUndoList dlg_srch_undo
      incr tag_idx
      if {$tag_idx < [llength $tag_list]} {
        incr loop_cnt
        if {$direction == 1} {
          scan [.f1.t index insert] "%d" line
        } else {
          set line 1
        }
        set tid_search_list [after idle [list SearchList_BgSearchTagsLoop $tag_list $tag_idx $line $direction $loop_cnt]]
      } else {
        set tid_search_list {}
        catch {destroy .dlg_srch.slpro}
        catch {destroy .srch_abrt}
        .dlg_srch.f1.l configure -cursor top_left_arrow
      }
    }
  } else {
    set tid_search_list {}
  }
}


#
# This function is called by the background search process to create and
# update the progress bar.
#
proc SearchList_SearchProgress {ratio} {
  if {[info commands .dlg_srch.slpro] eq ""} {
    frame .dlg_srch.slpro -takefocus 0 -relief sunken -borderwidth 2

    canvas .dlg_srch.slpro.c -width 100 -height 10 -highlightthickness 0 -takefocus 0
    .dlg_srch.slpro.c create rect 0 0 $ratio 12 -fill {#0b1ff7} -outline {}
    pack .dlg_srch.slpro.c
    place .dlg_srch.slpro -in .dlg_srch.f1.l -anchor nw -x 0 -y 0

    .dlg_srch.f1.l configure -cursor watch

  } else {
    .dlg_srch.slpro.c coords all 0 0 $ratio 12
  }
}


#
# This function stops a possibly ongoing background search in the search
# list dialog. Optionally the user is asked it he really wants to abort.
# The function returns 0 and does not abort the background action if the
# user selects "Cancel", else it returns 1.  The caller MUST check the
# return value if parameter "do_warn" is TRUE.
#
proc SearchList_SearchAbort {{do_warn 1}} {
  global tid_search_list dlg_srch_undo dlg_srch_redo

  if {$tid_search_list ne ""} {
    if {$do_warn} {
      PreemptBgTasks
      toplevel .srch_abrt
      wm group .srch_abrt .
      wm geometry .srch_abrt "+[expr {[winfo rootx .dlg_srch] + 100}]+[expr {[winfo rooty .dlg_srch] + 100}]"
      wm title .srch_abrt "Confirm abort of search"

      frame  .srch_abrt.f1
      button .srch_abrt.f1.icon -bitmap question -relief flat
      pack   .srch_abrt.f1.icon -side left -padx 10 -pady 20
      label  .srch_abrt.f1.msg -justify left \
                            -text "This command will abort the ongoing search operation.\nPlease confirm, or wait until this popup disappears."
      pack   .srch_abrt.f1.msg -side left -padx 10 -pady 20
      pack   .srch_abrt.f1 -side top

      frame  .srch_abrt.f3
      button .srch_abrt.f3.cancel -text "Cancel" -command {set ::vwait_search_complete 0}
      button .srch_abrt.f3.ok -text "Ok" -default active -command {set ::vwait_search_complete 1}
      pack   .srch_abrt.f3.cancel .srch_abrt.f3.ok -side left -padx 10 -pady 5
      pack   .srch_abrt.f3 -side top

      bindtags .srch_abrt.f1.icon {.srch_abrt all}
      bind   .srch_abrt <Destroy> {destroy .srch_abrt}
      bind   .srch_abrt <Return> {set ::vwait_search_complete 1}
      bind   .srch_abrt.f1 <Destroy> {if {$::vwait_search_complete == -1} {set ::vwait_search_complete -2}}
      focus  .srch_abrt.f3.ok
      grab   .srch_abrt

      ResumeBgTasks

      # block here until the user responds or the background task finishes
      set ::vwait_search_complete -1
      vwait ::vwait_search_complete
      catch {destroy .srch_abrt}

      set cancel [expr {$::vwait_search_complete == 0}]
      unset -nocomplain ::vwait_search_complete

      if {$cancel} {
        return 0
      }
    } else {
      catch {destroy .srch_abrt}
    }
  } else {
    catch {destroy .srch_abrt}
  }

  if {$tid_search_list ne ""} {
    SearchList_BgSearch_FinalizeUndoList dlg_srch_undo
    SearchList_BgSearch_FinalizeUndoList dlg_srch_redo

    # stop the background process
    after cancel $tid_search_list
    set tid_search_list {}

    # remove the progress bar
    catch {destroy .dlg_srch.slpro}
    catch {.dlg_srch.f1.l configure -cursor top_left_arrow}

    DisplayStatusLine search warn "Search list operation was aborted"
  }
  return 1
}


#
# This helper function is called before modifications of the search result
# list by the various background tasks to determine a line which can serve
# as "anchor" for the view, i.e. which will be made visible again after the
# insertions or removals (which may lead to scrolling.)
#
proc SearchList_GetViewAnchor {} {
  global dlg_srch_sel dlg_srch_lines

  set sel [TextSel_GetSelection dlg_srch_sel]
  if {[llength $sel] > 0} {
    # keep selection visible
    return [list 1 [lindex $dlg_srch_lines [lindex $sel 0]]]
  } else {
    # no selection - check if line near cursor in main win is visible
    scan [.f1.t index insert] "%d" line
    set idx [SearchList_GetLineIdx $line]
    if {($idx < [llength $dlg_srch_lines]) &&
        ([.dlg_srch.f1.l bbox "[expr {$idx + 1}].0"] ne "")} {
      return [list 0 [lindex $dlg_srch_lines $idx]]
    }
  }
  return {0 -1}
}


#
# This helper function is called after modifications of the search result
# list by the various background tasks to make the previously determines
# "anchor" line visible and to adjust the selection.
#
proc SearchList_SeeViewAnchor {info} {
  global dlg_srch_sel dlg_srch_lines

  set anchor [lindex $info 1]
  if {($anchor >= 0) &&
      ([set idx [lsearch -exact -integer -sorted -increasing $dlg_srch_lines $anchor]] >= 0)} {
    .dlg_srch.f1.l see "[expr {$idx + 1}].0"
    if {[lindex $info 0]} {
      TextSel_SetSelection dlg_srch_sel $idx
    }
  } else {
    TextSel_SetSelection dlg_srch_sel {}
  }
}


#
# This function is during background tasks which fill the search match dialog
# after adding new matches. The function adds the respective line numbers to
# the undo list. If there's already an undo item for the current search, the
# numbers are merged into it.
#
proc SearchList_BgSearch_AppendUndoList {list_ref do_add line_list} {
  upvar $list_ref undo_list

  if {[llength $undo_list] > 0} {
    set prev_undo [lindex $undo_list end]
    set prev_op [lindex $prev_undo 0]
    if {$prev_op == [expr {$do_add ? 2 : -2}]} {
      set prev_l [lindex $prev_undo 1]
      set undo_list [lreplace $undo_list end end [list $prev_op [concat $prev_l $line_list]]]
    } else {
      lappend undo_list [list [expr {$do_add ? 2 : -2}] $line_list]
    }
  } else {
    lappend undo_list [list [expr {$do_add ? 2 : -2}] $line_list]
  }
}


#
# This function is invoked at the end of background tasks which fill the
# search list window to mark the entry on the undo list as closed (so that
# future search matches go into a new undo element.)
#
proc SearchList_BgSearch_FinalizeUndoList {list_ref} {
  upvar $list_ref undo_list

  if {[llength $undo_list] > 0} {
    set prev_undo [lindex $undo_list end]
    set prev_op [lindex $prev_undo 0]
    if {($prev_op == 2) || ($prev_op == -2)} {
      set prev_l [lindex $prev_undo 1]
      set prev_op [expr {($prev_op == -2) ? -1 : 1}]
      set undo_list [lreplace $undo_list end end [list $prev_op $prev_l]]
    }
  }
}


#
# This function inserts all selected lines in the main window into the list.
#
proc SearchList_AddMainSelection {} {
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel
  global dlg_srch_undo dlg_srch_redo

  if {[info exists dlg_srch_shown] &&
      [SearchList_SearchAbort]} {

    set pos12 {1.0 1.0}
    while {[set pos12 [.f1.t tag nextrange sel [lindex $pos12 1]]] ne ""} {
      scan [lindex $pos12 0] "%d" line_1
      scan [lindex $pos12 1] "%d.%d" line_2 char
      if {$char == 0} {incr line_2 -1}

      set line_list {}
      set idx_list {}
      for {set line $line_1} {$line <= $line_2} {incr line} {
        set idx [SearchList_GetLineIdx $line]
        set pos "[expr {$idx + 1}].0"
        if {[lindex $dlg_srch_lines $idx] != $line} {
          set dlg_srch_lines [linsert $dlg_srch_lines $idx $line]
          SearchList_InsertLine $line $pos
          lappend line_list $line
          lappend idx_list $idx
        }
        .dlg_srch.f1.l see $pos
      }

      if {[llength $line_list] > 0} {
        lappend dlg_srch_undo [list 1 $line_list]
        set dlg_srch_redo {}
        TextSel_SetSelection dlg_srch_sel $idx_list 0
      }
    }
  }
}


#
# This function inserts either the line in the main window holding the cursor
# or all selected lines into the search result list.  It's bound to the "i" key
# press event in the main window.
#
proc SearchList_CopyCurrentLine {} {
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel
  global dlg_srch_undo dlg_srch_redo

  if {[info exists dlg_srch_shown] &&
      [SearchList_SearchAbort]} {

    set pos12 [.f1.t tag nextrange sel 1.0]
    # ignore selection if not visible (because it can be irritating when "i"
    # inserts some random line instead of the one holding the cursor)
    if {($pos12 ne "") && ([.f1.t bbox [lindex $pos12 0]] ne "")} {
      # selection exists: add all selected lines
      SearchList_AddMainSelection
    } else {
      # get line number of the cursor position
      scan [.f1.t index insert] "%d" line
      set idx [SearchList_GetLineIdx $line]
      if {[lindex $dlg_srch_lines $idx] != $line} {
        set dlg_srch_lines [linsert $dlg_srch_lines $idx $line]
        set pos "[expr {$idx + 1}].0"
        SearchList_InsertLine $line $pos
        .dlg_srch.f1.l see $pos
        TextSel_SetSelection dlg_srch_sel $idx 0

        lappend dlg_srch_undo [list 1 $line]
        set dlg_srch_redo {}

      } else {
        # line is already included - make it visible & select it
        SearchList_MatchView $line
      }
    }
  }
}


#
# This function creates the tags for selection and color highlighting.
# This is used for initialisation and after editing highlight tags.
#
proc SearchList_CreateHighlightTags {} {
  global patlist fmt_find fmt_selection
  global dlg_srch_sel dlg_srch_shown

  if {[info exists dlg_srch_shown]} {
    # create highlight tags
    foreach w $patlist {
      eval [linsert [HighlightConfigure $w] 0 .dlg_srch.f1.l tag configure [lindex $w 4]]
    }

    # create text tag for search highlights
    eval [linsert [HighlightConfigure $fmt_find] 0 .dlg_srch.f1.l tag configure find]
    .dlg_srch.f1.l tag raise find

    # raise sel above highlight tag, but below find
    eval [linsert [HighlightConfigure $fmt_selection] 0 .dlg_srch.f1.l tag configure sel]
    .dlg_srch.f1.l tag lower sel

    # create tag to invisibly mark prefixes (used to exclude the text from search and mark-up)
    # padding is added to align bookmarked lines (note padding is smaller than
    # in the main window because there's extra space in front anyways)
    .dlg_srch.f1.l tag configure prefix -lmargin1 11
    .dlg_srch.f1.l tag configure bookmark -lmargin1 0
  }
}


#
# This function is called after removal of tags in the Tag list dialog.
#
proc SearchList_DeleteTag {tag} {
  global dlg_srch_shown

  if {[info exists dlg_srch_shown]} {
    .dlg_srch.f1.l tag delete $tag
  }
}


#
# This function is called out of the main window's highlight loop for every line
# to which a highlight is applied.
#
proc SearchList_HighlightLine {tag line} {
  global dlg_srch_shown dlg_srch_highlight dlg_srch_lines
  global tid_high_init

  if {[info exists dlg_srch_shown]} {
    if {$dlg_srch_highlight || ($tid_high_init ne "")} {
      set idx [SearchList_GetLineIdx $line]
      if {[lindex $dlg_srch_lines $idx] == $line} {
        catch {.dlg_srch.f1.l tag add $tag "[expr {$idx + 1}].0" "[expr {$idx + 2}].0"}
      }
    }
  }
}


#
# This function is bound to the "Toggle highlight" checkbutton in the
# search list dialog's menu.  The function enables or disables search highlight.
#
proc SearchList_HighlightClear {} {
  global dlg_srch_shown

  if {[info exists dlg_srch_shown]} {
    .dlg_srch.f1.l tag remove find 1.0 end
  }
}


#
# This function adjusts the view in the search result list so that the given
# main window's text line becomes visible.
#
proc SearchList_MatchView {line} {
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel

  if {[info exists dlg_srch_shown]} {
    set idx [SearchList_GetLineIdx $line]
    if {$idx < [llength $dlg_srch_lines]} {
      .dlg_srch.f1.l see "$idx.0"
      .dlg_srch.f1.l see "[expr {$idx + 1}].0"
      .dlg_srch.f1.l mark set insert "[expr {$idx + 1}].0"

      # move selection onto the line; clear selection if line is not in the list
      if {[lindex $dlg_srch_lines $idx] == $line} {
        TextSel_SetSelection dlg_srch_sel $idx 0
      } else {
        TextSel_SetSelection dlg_srch_sel {}
      }
    } else {
      .dlg_srch.f1.l see end
      .dlg_srch.f1.l mark set insert end
    }
  }
}


#
# This function is called when a bookmark is added or removed in the main
# window.  The function displays the bookmark in the respective line in
# the search filter dialog, if the line is currently visible.  (Note this
# function is not used to mark lines which are newly inserted into the
# dialog and which already have a bookmark; see the insert function below)
#
proc SearchList_MarkLine {line} {
  global dlg_srch_shown dlg_srch_lines
  global img_marker mark_list

  if {[info exists dlg_srch_shown]} {
    set idx [lsearch -exact -integer $dlg_srch_lines $line]
    if {$idx != -1} {
      set pos "[expr {$idx + 1}].0"
      if {[info exists mark_list($line)]} {
        .dlg_srch.f1.l image create $pos -image $img_marker -padx 2
        .dlg_srch.f1.l tag add bookmark $pos
        .dlg_srch.f1.l see $pos

        scan [.dlg_srch.f1.l index insert] "%d.%d" lil lic
        if {$lic > 0} {
          .dlg_srch.f1.l mark set insert "$lil.0"
        }
      } else {
        .dlg_srch.f1.l delete $pos
      }
    }
  }
}


#
# This function is bound to the "m" key in the search filter dialog.
# The function adds or removes a bookmark on the currently selected
# line (but only if exactly one line is selected.)
#
proc SearchList_ToggleMark {} {
  global dlg_srch_shown dlg_srch_lines dlg_srch_sel

  set sel [TextSel_GetSelection dlg_srch_sel]
  if {[llength $sel] == 1} {
    set line [lindex $dlg_srch_lines $sel]
    Mark_Toggle $line
  }
}


#
# This functions copies a line of text (including highlighting tags and
# bookmark marker) from the main window into the the search filter dialog.
#
proc SearchList_InsertLine {txt_line ins_pos} {
  global dlg_srch_fn_cache dlg_srch_show_fn dlg_srch_fn_delta dlg_srch_fn_root
  global tick_str_prefix img_marker mark_list

  # copy text content and tags out of the main window
  set pos "$txt_line.0"
  set dump [ExtractText [list $pos linestart] [list $pos lineend]]
  set tag_list [lsearch -all -inline -glob [.f1.t tag names $pos] "tag*"]

  if {$dlg_srch_fn_delta || $dlg_srch_show_fn} {
    set fn [ParseFrameTickNo $pos dlg_srch_fn_cache]
    if {[catch {expr {$fn + 0}}]} {
      set fn 0
    }
    if {$dlg_srch_fn_delta && $dlg_srch_show_fn} {
      set prefix "   $tick_str_prefix$fn:[expr {$fn - $dlg_srch_fn_root}] "
    } elseif $dlg_srch_show_fn {
      set prefix "   $tick_str_prefix$fn "
    } elseif $dlg_srch_fn_delta {
      set prefix "   $tick_str_prefix[expr {$fn - $dlg_srch_fn_root}] "
    }
  } else {
    set prefix "   "
  }

  # display the text
  .dlg_srch.f1.l insert $ins_pos $prefix prefix "$dump\n" $tag_list

  # add bookmark, if this line is marked
  if {[info exists mark_list($txt_line)]} {
    .dlg_srch.f1.l image create $ins_pos -image $img_marker -padx 2
    .dlg_srch.f1.l tag add bookmark $ins_pos
  }
}


#
# This function fills the search list dialog with all text lines indicated in
# the dialog's line number list (note the first line has number 1)
#
proc SearchList_Refill {} {
  # WARNING: caller must invoke SearchList_SearchAbort
  set tid_search_list [after 10 [list SearchList_BgRefillLoop 0]]
}


#
# This function acts as background process to refill the search list window
# with previous content, but in a different format (e.g. with added frame nums)
#
proc SearchList_BgRefillLoop {off} {
  global block_bg_tasks tid_search_inc tid_search_hall tid_search_list
  global dlg_srch_shown dlg_srch_lines

  if {$block_bg_tasks || ($tid_search_inc ne {}) || ($tid_search_hall ne {})} {
    # background tasks are suspended - re-schedule with timer
    set tid_search_list [after 100 [list SearchList_BgRefillLoop $off]]

  } elseif [info exists dlg_srch_shown] {
    set end_off [expr {$off + 399}]
    if {$end_off >= [llength $dlg_srch_lines]} {
      set end_off [expr {[llength $dlg_srch_lines] - 1}]
    }
    set line_frag [lrange $dlg_srch_lines $off $end_off]

    # replace each line separately with the new format
    for {} {$off <= $end_off} {incr off} {
      set txt_line [lindex $dlg_srch_lines $off]
      set pos "[expr {$off + 1}].0"

      .dlg_srch.f1.l delete $pos "$pos +1 line"
      SearchList_InsertLine $txt_line $pos
    }

    # refresh the selection
    TextSel_ShowSelection dlg_srch_sel

    if {$off < [llength $dlg_srch_lines]} {
      # create or update the progress bar
      set ratio [expr {int(100.0*$off/[llength $dlg_srch_lines])}]
      SearchList_SearchProgress $ratio

      set tid_search_list [after idle [list SearchList_BgRefillLoop $off]]

    } else {
      set tid_search_list {}
      catch {destroy .dlg_srch.slpro}
      catch {destroy .srch_abrt}
      .dlg_srch.f1.l configure -cursor top_left_arrow
    }
  } else {
    set tid_search_list {}
  }
}


#
# This function is a callback for selection changes in the search list dialog.
# If a single line is selected, the view in the main window is changed to
# display the respective line.
#
proc SearchList_SelectionChange {sel} {
  global dlg_srch_sel dlg_srch_lines

  if {[llength $sel] == 1} {
    set idx [lindex $sel 0]
    if {$idx < [llength $dlg_srch_lines]} {
      Mark_Line [lindex $dlg_srch_lines $idx]
    }
  }
}


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
proc SearchList_GetLen {} {
  global dlg_srch_sel dlg_srch_lines
  return [llength $dlg_srch_lines]
}


#
# This function must be called when portions of the text in the main window
# have been deleted to update references to text lines. Paramater meaning:
# + top_l: this is the first line which is not deleted, or 1 if none
# + bottom_l: this line and all below have been removed, or 0 if none
#
proc SearchList_AdjustLineNums {top_l bottom_l} {
  global dlg_srch_sel dlg_srch_lines dlg_srch_undo dlg_srch_redo dlg_srch_fn_cache

  if {[info exists dlg_srch_lines]} {
    set dlg_srch_lines [SearchList_RemoveLineFromList $dlg_srch_lines $top_l $bottom_l]

    set tmp2 {}
    foreach cmd $dlg_srch_undo {
      set tmpl [SearchList_RemoveLineFromList [lindex $cmd 1] $top_l $bottom_l]
      if {[llength $tmpl] > 0} {
        lappend tmp2 [list [lindex $cmd 0] $tmpl]
      }
    }
    set dlg_srch_undo $tmp2

    set dlg_srch_redo {}

    array unset dlg_srch_fn_cache

    SearchList_SearchAbort 0
    SearchList_Refill
  }
}

# this helper function removes numbers in a given range from a list
# and adjusts the line number
proc SearchList_RemoveLineFromList {lines top_l bottom_l} {
  set tmpl {}
  foreach line $lines {
    if {($line >= $top_l) && (($line < $bottom_l) || ($bottom_l == 0))} {
      lappend tmpl [expr {$line - $top_l + 1}]
    }
  }
  return $tmpl
}

#
# This function stores all text lines in the search result window into a file.
#
proc SearchList_SaveFile {filename lnum_only} {
  global dlg_srch_lines

  if {[catch {set file [open $filename w]} cerr] == 0} {
    if {[catch {
      if {$lnum_only} {
        # save line numbers only (i.e. line numbers in main window)
        foreach line $dlg_srch_lines {
          puts $file $line
        }
      } else {
        # save text content
        foreach line $dlg_srch_lines {
          puts $file [ExtractText "$line.0" "$line.0 lineend"]]
        }
      }
      close $file

    } cerr] != 0} {
      tk_messageBox -icon error -type ok -parent .dlg_srch \
                    -message "Error writing \"$filename\": $cerr"
      catch {close $file}
      catch {file delete $file}
    }
  } else {
    tk_messageBox -icon error -type ok -parent .dlg_srch \
                  -message "Failed to create output file: $cerr"
  }
}


#
# This function is called by menu entry "Save as..." in the search dialog.
# The user is asked to select an output file; if he does so the list
# content is written into it, in the format selected by the user.
#
proc SearchList_SaveFileAs {lnum_only} {
  global dlg_srch_lines

  PreemptBgTasks
  if {[llength $dlg_srch_lines] > 0} {
    set def_name ""

    set filename [tk_getSaveFile -parent .dlg_srch -filetypes {{all {*}} {Text {*.txt}}} \
                                 -title "Select output file" \
                                 -initialfile [file tail $def_name] \
                                 -initialdir [file dirname $def_name]]
    if {$filename ne ""} {
      SearchList_SaveFile $filename $lnum_only
    }
  } else {
    tk_messageBox -icon info -type ok -parent .dlg_srch \
                  -message "The search results list is empty."
  }
  ResumeBgTasks
}


#
# This function is called by menu entry "Load line numbers..." in the search
# result dialog. The user is asked to select an input file; if he does so a
# list of line numbers is extracted from it and lines with these numbers is
# copied from the main window.
#
proc SearchList_LoadFrom {} {
  global tid_search_list dlg_srch_shown

  if {[info exists dlg_srch_shown] &&
      [SearchList_SearchAbort]} {
    PreemptBgTasks
    set def_name ""
    set filename [tk_getOpenFile -parent .dlg_srch -filetypes {{all {*}} {Text {*.txt}}}]
    ResumeBgTasks

    if {$filename ne ""} {
      if {[catch {set file [open $filename r]} cerr] == 0} {
        if {[catch {
          scan [.f1.t index end] "%d" max_line
          set answer ""
          set skipped 0
          set synerr 0
          set line_list {}
          while {[gets $file line_str] >= 0} {
            if {[regexp {^\d+} $line_str line]} {
              if {($line > 0) && ($line < $max_line)} {
                lappend line_list $line
              } else {
                incr skipped
              }
            } elseif {![regexp {^(\s*$|\s*#)} $line_str line]} {
              incr synerr
            }
          }
          close $file

          if {($skipped > 0) || ($synerr > 0)} {
            set msg "Ignored"
            if {$skipped > 0} {
              append msg " $skipped lines with a value outside of the allowed range 1 .. $max_line"
            }
            if {$synerr > 0} {
              append msg " $synerr non-empty lines without a number value"
            }
            append msg "."
            set answer [tk_messageBox -icon warning -type okcancel -parent .dlg_srch -message $msg]
          }

          if {$answer ne "cancel"} {
            if {[llength $line_list] > 0} {
              # sort & remove duplicate line numbers
              set line_list [lsort -integer -increasing -unique $line_list]

              set tid_search_list [after 10 [list SearchList_BgLoadLoop $line_list 0]]

            } else {
              tk_messageBox -icon error -type ok -parent .dlg_srch \
                            -message "No valid line numbers found in \"$filename\""
            }
          }
        } cerr] != 0} {
          tk_messageBox -icon error -type ok -parent .dlg_srch \
                        -message "Error reading from \"$filename\": $cerr"
          catch {close $file}
        }
      } else {
        tk_messageBox -icon error -type ok -parent .dlg_srch \
                      -message "Failed to open file: $cerr"
      }
    }
  }
}


#
# This function acts as background process to fill the search list window
# with a given list of line indices.
#
proc SearchList_BgLoadLoop {line_list off} {
  global block_bg_tasks tid_search_inc tid_search_hall tid_search_list
  global dlg_srch_shown dlg_srch_lines dlg_srch_undo dlg_srch_redo

  if {$block_bg_tasks || ($tid_search_inc ne {}) || ($tid_search_hall ne {})} {
    # background tasks are suspended - re-schedule with timer
    set tid_search_list [after 100 [list SearchList_BgLoadLoop $line_list $off]]

  } elseif [info exists dlg_srch_shown] {
    set anchor [SearchList_GetViewAnchor]
    set start_off $off
    set end_off [expr {$off + 399}]
    if {$end_off >= [llength $line_list]} {
      set end_off [expr {[llength $line_list] - 1}]
    }
    set ins_off 0
    for {} {$off <= $end_off} {incr off} {
      set line [lindex $line_list $off]
      set idx [SearchList_GetLineIdx $line]
      if {[lindex $dlg_srch_lines $idx] != $line} {
        SearchList_InsertLine $line "[expr {$idx + $ins_off + 1}].0"
        incr ins_off
      }
    }
    SearchList_BgSearch_AppendUndoList dlg_srch_undo 1 [lrange $line_list $start_off $end_off]
    set dlg_srch_lines [lsort -integer -increasing [concat $dlg_srch_lines [lrange $line_list $start_off $end_off]]]
    set dlg_srch_redo {}

    SearchList_SeeViewAnchor $anchor

    if {$off < [llength $line_list]} {
      # create or update the progress bar
      set ratio [expr {int(100.0*$off/[llength $line_list])}]
      SearchList_SearchProgress $ratio

      set tid_search_list [after idle [list SearchList_BgLoadLoop $line_list $off]]

    } else {
      SearchList_BgSearch_FinalizeUndoList dlg_srch_undo
      set tid_search_list {}
      catch {destroy .dlg_srch.slpro}
      catch {destroy .srch_abrt}
      .dlg_srch.f1.l configure -cursor top_left_arrow
    }
  } else {
    set tid_search_list {}
  }
}


#
# This function is bound to CTRL-g in the search list and displays stats
# about the content of the search result list.
#
proc SearchList_DisplayStats {} {
  global dlg_srch_sel dlg_srch_lines

  set sel [TextSel_GetSelection dlg_srch_sel]
  if {[llength $sel] == 1} {
    set line_idx [expr {[lindex $sel 0] + 1}]
    set msg "line $line_idx of [llength $dlg_srch_lines] in the search list"
  } else {
    set msg "[llength $dlg_srch_lines] lines in the search list"
  }
  DisplayStatusLine line_query msg $msg
}


# ----------------------------------------------------------------------------
#
# This function creates or raises the color highlighting tags list dialog.
# This dialog shows all currently defined tag assignments.
#
proc TagList_OpenDialog {} {
  global font_content col_bg_content col_fg_content
  global dlg_tags_shown dlg_tags_geom dlg_tags_sel

  PreemptBgTasks
  if {![info exists dlg_tags_shown]} {
    toplevel .dlg_tags
    wm title .dlg_tags "Color highlights list"
    wm group .dlg_tags .

    CreateButtonBitmap img_up img_down

    menu .dlg_tags.menubar
    .dlg_tags config -menu .dlg_tags.menubar
    .dlg_tags.menubar add cascade -label "Edit" -menu .dlg_tags.menubar.edit -underline 0
    .dlg_tags.menubar add command -label "Move Up" -command TagList_ShiftUp -state disabled
    .dlg_tags.menubar add command -label "Move Down" -command TagList_ShiftDown -state disabled
    menu .dlg_tags.menubar.edit -tearoff 0 -postcommand {TagList_ContextPopulate .dlg_tags.menubar.edit 1}

    frame .dlg_tags.f1
    text .dlg_tags.f1.l -width 1 -height 1 -wrap none -font $font_content -cursor top_left_arrow \
                        -foreground $col_fg_content -background $col_bg_content \
                        -exportselection 0 -insertofftime 0 -yscrollcommand {.dlg_tags.f1.sb set} \
                        -insertwidth [expr {2 * [font measure $font_content " "]}]
    pack .dlg_tags.f1.l -side left -fill both -expand 1
    scrollbar .dlg_tags.f1.sb -orient vertical -command {.dlg_tags.f1.l yview} -takefocus 0
    pack .dlg_tags.f1.sb -side left -fill y
    pack .dlg_tags.f1.sb -side left -fill y
    pack .dlg_tags.f1 -side top -fill both -expand 1

    TextSel_Init .dlg_tags.f1.l dlg_tags_sel TagList_SelectionChange TagList_GetLen "extended"
    bindtags .dlg_tags.f1.l {.dlg_tags.f1.l TextSel . all}

    frame .dlg_tags.f2
    label  .dlg_tags.f2.l -text "Find:"
    grid   .dlg_tags.f2.l -sticky we -column 0 -row 0
    button .dlg_tags.f2.but_next -text "Next" -command {TagList_Search 1} -underline 0 -state disabled
    grid   .dlg_tags.f2.but_next -sticky we -column 1 -row 0
    button .dlg_tags.f2.but_prev -text "Prev." -command {TagList_Search 0} -underline 0 -state disabled
    grid   .dlg_tags.f2.but_prev -sticky we -column 2 -row 0
    button .dlg_tags.f2.but_all -text "All" -command {TagList_SearchList 0} -underline 0 -state disabled
    grid   .dlg_tags.f2.but_all -sticky we -column 3 -row 0
    button .dlg_tags.f2.but_blw -text "All below" -command {TagList_SearchList 1} -state disabled
    grid   .dlg_tags.f2.but_blw -sticky we -column 4 -row 0
    button .dlg_tags.f2.but_abve -text "All above" -command {TagList_SearchList -1} -state disabled
    grid   .dlg_tags.f2.but_abve -sticky we -column 5 -row 0
    pack .dlg_tags.f2 -side top -anchor w -pady 2

    menu .dlg_tags.ctxmen -tearoff 0

    bind .dlg_tags.f1.l <Double-Button-1> {TagList_DoubleClick %x %y; break}
    bind .dlg_tags.f1.l <ButtonRelease-3> {TagList_ContextMenu %x %y; break}
    bind .dlg_tags.f1.l <Delete> {TagList_RemoveSelection; break}
    bind .dlg_tags.f1.l <Key-slash> {SearchEnter 1 .dlg_tags.f1.l; break}
    bind .dlg_tags.f1.l <Key-question> {SearchEnter 0 .dlg_tags.f1.l; break}
    bind .dlg_tags.f1.l <Key-ampersand> {SearchHighlightClear; break}
    bind .dlg_tags.f1.l <Key-n> {TagList_Search 1; break}
    bind .dlg_tags.f1.l <Key-N> {TagList_Search 0; break}
    bind .dlg_tags.f1.l <Alt-Key-n> {TagList_Search 1; break}
    bind .dlg_tags.f1.l <Alt-Key-p> {TagList_Search 0; break}
    bind .dlg_tags.f1.l <Alt-Key-a> {TagList_SearchList 0; break}
    bind .dlg_tags.f1.l <Alt-Key-N> {TagList_SearchList 1; break}
    bind .dlg_tags.f1.l <Alt-Key-P> {TagList_SearchList -1; break}
    focus .dlg_tags.f1.l

    set dlg_tags_shown 1
    bind .dlg_tags.f1.l <Destroy> {+ unset -nocomplain dlg_tags_shown}
    bind .dlg_tags <Configure> {ToplevelResized %W .dlg_tags .dlg_tags dlg_tags_geom}
    wm geometry .dlg_tags $dlg_tags_geom
    wm positionfrom .dlg_tags user

    TagList_Fill

  } else {
    wm deiconify .dlg_tags
    raise .dlg_tags
  }
  ResumeBgTasks
}


#
# This function is bound to right mouse clicks in the highlight tag list and pops
# up a context menu. If the mouse click occurred outside of the current selection
# the selection is updated. Then the menu is populated and shown at the mouse
# coordinates.
#
proc TagList_ContextMenu {xcoo ycoo} {
  global dlg_tags_sel

  TextSel_ContextSelection dlg_tags_sel $xcoo $ycoo

  TagList_ContextPopulate .dlg_tags.ctxmen 0

  set rootx [expr {[winfo rootx .dlg_tags] + $xcoo}]
  set rooty [expr {[winfo rooty .dlg_tags] + $ycoo}]
  tk_popup .dlg_tags.ctxmen $rootx $rooty 0
}


#
# This function is used both to populate the "Edit" menu and the context menu.
# The contents depend on the number of selected items.
#
proc TagList_ContextPopulate {wid show_all} {
  global tlb_find dlg_tags_sel

  set sel [TextSel_GetSelection dlg_tags_sel]
  set sel_cnt [llength $sel]

  set state_find [expr {($tlb_find eq "") ? "disabled" : "normal"}]
  set state_sel_1 [expr {($sel_cnt != 1) ? "disabled" : "normal"}]
  set state_sel_n0 [expr {($sel_cnt == 0) ? "disabled" : "normal"}]

  $wid delete 0 end
  if {($state_sel_1 eq "normal") || $show_all} {
    $wid add command -label "Change background color" -command [list TagList_PopupColorPalette $sel 0] -state $state_sel_1
    $wid add command -label "Edit markup..." -command [list Markup_OpenDialog $sel] -state $state_sel_1
    $wid add separator
    $wid add command -label "Copy to search field" -command [list TagList_CopyToSearch $sel] -state $state_sel_1
    $wid add command -label "Update from search field" -command [list TagList_CopyFromSearch $sel] -state $state_find
  }
  $wid add command -label "Add current search" -command {TagList_AddSearch .dlg_tags} -state $state_find
  if {($state_sel_n0 eq "normal") || $show_all} {
    $wid add separator
    $wid add command -label "Remove selected entries" -command [list TagList_Remove $sel] -state $state_sel_n0
  }
}


#
# This function is bound to double mouse button clicks onto an entry in
# the highlight list. The function opens the markup editor dialog.
#
proc TagList_DoubleClick {xcoo ycoo} {
  global patlist dlg_tags_sel

  set sel [TextSel_GetSelection dlg_tags_sel]
  if {[llength $sel] == 1} {
    Markup_OpenDialog $sel
  }
}


#
# This function is bound to the "up" button next to the color highlight list.
# Each selected item (selection may be non-consecutive) is shifted up by one line.
#
proc TagList_ShiftUp {} {
  global patlist dlg_tags_sel

  set sel [TextSel_GetSelection dlg_tags_sel]
  set sel [lsort -integer -increasing $sel]
  if {[lindex $sel 0] > 0} {
    set new_sel {}
    foreach idx $sel {
      # remove the item in the listbox widget above the shifted one
      TagList_DisplayDelete [expr {$idx - 1}]
      # re-insert the just removed item below the shifted one
      TagList_DisplayInsert $idx [expr {$idx - 1}]

      # perform the same exchange in the associated list
      set patlist [lreplace $patlist [expr {$idx - 1}] $idx \
                            [lindex $patlist $idx] \
                            [lindex $patlist [expr {$idx - 1}]]]

      lappend new_sel [expr {$idx - 1}]
    }

    # redraw selection
    TextSel_SetSelection dlg_tags_sel $new_sel
  }
}


#
# This function is bound to the "down" button next to the color highlight
# list.  Each selected item is shifted down by one line.
#
proc TagList_ShiftDown {} {
  global patlist dlg_tags_sel

  set sel [TextSel_GetSelection dlg_tags_sel]
  set sel [lsort -integer -decreasing $sel]
  if {[lindex $sel 0] < [llength $patlist] - 1} {
    set new_sel {}
    foreach idx $sel {
      TagList_DisplayDelete [expr {$idx + 1}]
      TagList_DisplayInsert $idx [expr {$idx + 1}]

      set patlist [lreplace $patlist $idx [expr {$idx + 1}] \
                            [lindex $patlist [expr {$idx + 1}]] \
                            [lindex $patlist $idx]]

      lappend new_sel [expr {$idx + 1}]
    }
    TextSel_SetSelection dlg_tags_sel $new_sel
  }
}


#
# This function is bound to the next/prev buttons below the highlight tags
# list. The function searches for the next line which is tagged with one of
# the selected highlighting tags (i.e. no text search is performed!) When
# multiple tags are searched for, the closest match is used.
#
proc TagList_Search {is_fwd} {
  global patlist dlg_tags_sel

  set min_line -1
  set sel [TextSel_GetSelection dlg_tags_sel]

  foreach pat_idx $sel {
    set w [lindex $patlist $pat_idx]

    Search_AddHistory [lindex $w 0] [lindex $w 1] [lindex $w 2]

    set tagnam [lindex $w 4]
    set start_pos [Search_GetBase $is_fwd 0]

    if {$is_fwd} {
      set pos12 [.f1.t tag nextrange $tagnam $start_pos]
    } else {
      set pos12 [.f1.t tag prevrange $tagnam $start_pos]
    }
    if {$pos12 ne ""} {
      scan [lindex $pos12 0] "%d" line
      if {($min_line == -1) ||
          ($is_fwd ? ($line < $min_line) : ($line > $min_line))} {
        set min_line $line
      }
    }
  }

  if {$min_line > 0} {
    ClearStatusLine search
    Mark_Line $min_line
    SearchList_HighlightLine find $min_line
    SearchList_MatchView $min_line
  } else {
    if {[llength $sel] == 0} {
      DisplayStatusLine search error "No pattern is selected in the list"
    } elseif {$is_fwd} {
      DisplayStatusLine search warn "No match until end of file"
    } else {
      DisplayStatusLine search warn "No match until start of file"
    }
  }
}


#
# This function is bound to the "List all" button in the color tags dialog.
# The function opens the search result window and adds all lines matching
# the pattern for the currently selected color tags.
#
proc TagList_SearchList {direction} {
  global patlist dlg_tags_sel

  set min_line -1
  set sel [TextSel_GetSelection dlg_tags_sel]
  if {[llength $sel] > 0} {
    set tag_list {}
    foreach pat_idx $sel {
      set w [lindex $patlist $pat_idx]
      lappend tag_list [lindex $w 4]
    }
    ClearStatusLine search
    SearchList_Open 0
    SearchList_StartSearchTags $tag_list $direction

  } else {
    DisplayStatusLine search error "No pattern is selected in the list"
  }
}


#
# This function is bound to changes of the selection in the color tags list.
#
proc TagList_SelectionChange {sel} {
  global dlg_tags_sel

  if {[llength $sel] > 0} {
    set state normal
  } else {
    set state disabled
  }

  .dlg_tags.f2.but_next configure -state $state
  .dlg_tags.f2.but_prev configure -state $state
  .dlg_tags.f2.but_all configure -state $state
  .dlg_tags.f2.but_blw configure -state $state
  .dlg_tags.f2.but_abve configure -state $state

  .dlg_tags.menubar entryconfigure 2 -state $state
  .dlg_tags.menubar entryconfigure 3 -state $state
}


#
# This callback is used by the selection "library" to query the number of
# elements in the list to determine the possible selection range.
#
proc TagList_GetLen {} {
  global patlist
  return [llength $patlist]
}


#
# This function updates a color tag text in the listbox.
#
proc TagList_Update {pat_idx} {
  global dlg_tags_shown patlist

  if {[info exists dlg_tags_shown]} {
    if {$pat_idx < [llength $patlist]} {
      set w [lindex $patlist $pat_idx]

      TagList_Fill
      TextSel_SetSelection dlg_tags_sel {}

      .dlg_tags.f1.l see "[expr {$pat_idx + 1}].0"
    }
  }
}


#
# This function removes an entry from the listbox.
#
proc TagList_DisplayDelete {pat_idx} {
  .dlg_tags.f1.l delete "[expr {$pat_idx + 1}].0" "[expr {$pat_idx + 2}].0"
}


#
# This function inserts a color tag text into the listbox and applies its
# highlight format options.
#
proc TagList_DisplayInsert {pos pat_idx} {
  global patlist

  set w [lindex $patlist $pat_idx]

  set txt [lindex $w 0]
  append txt "\n"

  # insert text (prepend space to improve visibility of selection)
  .dlg_tags.f1.l insert "[expr {$pos + 1}].0" "  " margin $txt [lindex $w 4]
}


#
# This function fills the highlight pattern list dialog window with all
# list entries.
#
proc TagList_Fill {} {
  global dlg_tags_shown patlist fmt_selection

  if {[info exists dlg_tags_shown]} {
    .dlg_tags.f1.l delete 1.0 end

    set idx 0
    foreach w $patlist {
      set tagnam [lindex $w 4]
      eval [linsert [HighlightConfigure $w] 0 .dlg_tags.f1.l tag configure $tagnam]

      TagList_DisplayInsert $idx $idx
      incr idx
    }

    # configure appearance of selected rows
    .dlg_tags.f1.l tag configure margin -lmargin1 6
    eval [linsert [HighlightConfigure $fmt_selection] 0 .dlg_tags.f1.l tag configure sel]
    .dlg_tags.f1.l tag lower sel
  }
}


#
# This function allows to edit a color assigned to a tags entry.
#
proc TagList_PopupColorPalette {pat_idx is_fg} {
  global patlist

  .dlg_tags.f1.l see "[expr {$pat_idx + 1}].0"

  set cool [.dlg_tags.f1.l dlineinfo "[expr {$pat_idx + 1}].0"]
  set rootx [expr {[winfo rootx .dlg_tags] + [lindex $cool 0]}]
  set rooty [expr {[winfo rooty .dlg_tags] + [lindex $cool 1]}]

  set w [lindex $patlist $pat_idx]
  set col_idx [expr {$is_fg ? 7 : 6}]
  set def_col [lindex $w $col_idx]

  PaletteMenu_Popup .dlg_tags $rootx $rooty \
                    [list TagList_UpdateColor $pat_idx $is_fg] [lindex $w $col_idx]
}


#
# This function is invoked after a direct color change via the popup color palette.
# The new color is saved in the highlight list and applied to the main window
# and the highlight dialog's list. NOTE: the color value my be an empty string
# (color "none" refers to the default fore- and background colors)
#
proc TagList_UpdateColor {pat_idx is_fg col} {
  global patlist dlg_tags_sel

  set w [lindex $patlist $pat_idx]
  set col_idx [expr {$is_fg ? 7 : 6}]

  set w [lreplace $w $col_idx $col_idx $col]
  if {[catch {.dlg_tags.f1.l tag configure [lindex $w 4] -background [lindex $w 6] -foreground [lindex $w 7]}] == 0} {
    # clear selection so that the color becomes visible
    TextSel_SetSelection dlg_tags_sel {}

    set patlist [lreplace $patlist $pat_idx $pat_idx $w]
    UpdateRcAfterIdle

    .f1.t tag configure [lindex $w 4] -background [lindex $w 6] -foreground [lindex $w 7]

    SearchList_CreateHighlightTags
    MarkList_CreateHighlightTags
  }
}


#
# This function is invoked by the "Add current search" entry in the highlight
# list's context menu.
#
proc TagList_AddSearch {parent} {
  global tlb_find tlb_regexp tlb_case
  global dlg_tags_shown dlg_tags_sel patlist fmt_find

  if {$tlb_find ne ""} {
    # search a free tag index
    set dup_idx -1
    set nam_idx 0
    set idx 0
    foreach w $patlist {
      scan [lindex $w 4] "tag%d" tag_idx
      if {$tag_idx >= $nam_idx} {
        set nam_idx [expr {$tag_idx + 1}]
      }
      if {[lindex $w 0] eq $tlb_find} {
        set dup_idx $idx
      }
      incr idx
    }

    set answer "no"
    if {$dup_idx >= 0} {
      set answer [tk_messageBox -type yesnocancel -icon warning -parent $parent \
                     -message "The same search expression is already used - overwrite this entry?"]
      if {$answer eq "cancel"} return
    }
    if {$answer eq "no"}  {
      # append new entry
      set pat_idx [llength $patlist]

      set w [list $tlb_find $tlb_regexp $tlb_case default "tag$nam_idx" {}]
      set w [concat $w [lrange $fmt_find 6 end]]
      lappend patlist $w

    } else {
      # replace pattern and search options in existing entry
      set pat_idx $dup_idx
      set w [lindex $patlist $pat_idx]
      set w [lreplace $w 0 2 $tlb_find $tlb_regexp $tlb_case]
      set patlist [lreplace $patlist $pat_idx $pat_idx $w]
    }

    # add the tag to the main window text widget
    HighlightCreateTags

    # tag matching lines in the main window
    .f1.t configure -cursor watch
    set opt [Search_GetOptions [lindex $w 0] [lindex $w 1] [lindex $w 2]]
    HighlightAll [lindex $w 0] [lindex $w 4] $opt
    SearchHighlightClear

    if {[info exists dlg_tags_shown]} {
      # insert the entry into the listbox
      TagList_Fill
      TextSel_SetSelection dlg_tags_sel $pat_idx
      .dlg_tags.f1.l see "[expr {$pat_idx + 1}].0"
    } else {
      TagList_OpenDialog
    }

  } else {

    tk_messageBox -type ok -icon error -parent $parent \
      -message "First enter a search text or regular expression in the main window's \"Find\" field."
  }
}


#
# This function is invoked by the "Copy to search field" command in the
# highlight list's context menu.
#
proc TagList_CopyToSearch {pat_idx} {
  global patlist
  global tlb_find_focus tlb_find tlb_regexp tlb_case

  set w [lindex $patlist $pat_idx]

  # force focus into find entry field & suppress "Enter" event
  SearchInit
  set tlb_find_focus 1
  focus .f2.e

  SearchHighlightClear
  set tlb_find [lindex $w 0]
  set tlb_regexp [lindex $w 1]
  set tlb_case [lindex $w 2]
}


#
# This function is invoked by the "Update from search field" command in the
# highlight list's context menu.
#
proc TagList_CopyFromSearch {pat_idx} {
  global tlb_find_focus tlb_find tlb_regexp tlb_case
  global patlist

  set answer [tk_messageBox -type okcancel -icon question -parent .dlg_tags \
                -message "Please confirm overwriting the search pattern for this entry? This cannot be undone"]
  if {$answer eq "ok"} {
    set w [lindex $patlist $pat_idx]
    set w [lreplace $w 0 2 $tlb_find $tlb_regexp $tlb_case]
    set patlist [lreplace $patlist $pat_idx $pat_idx $w]
    UpdateRcAfterIdle

    # apply the tag to the text content
    .f1.t tag remove [lindex $w 4] 1.0 end
    set opt [Search_GetOptions [lindex $w 0] [lindex $w 1] [lindex $w 2]]
    HighlightAll [lindex $w 0] [lindex $w 4] $opt

    TagList_DisplayDelete $pat_idx
    TagList_DisplayInsert $pat_idx $pat_idx
    TextSel_SetSelection dlg_tags_sel $pat_idx
    .dlg_tags.f1.l see "[expr {$pat_idx + 1}].0"
  }
}


#
# This function is invoked by the "Remove entry" command in the highlight
# list's context menu.
#
proc TagList_Remove {pat_sel} {
  global patlist

  set cnt [llength $pat_sel]
  if {$cnt > 0} {
    if {$cnt == 1} {
      set msg "Really remove this entry? This cannot be undone"
    } else {
      set msg "Really remove all $cnt selected entries? This cannot be undone"
    }
    set answer [tk_messageBox -type yesno -icon question -parent .dlg_tags -message $msg]
    if {$answer eq "yes"} {

      foreach idx [lsort -integer -decreasing -unique $pat_sel] {
        set w [lindex $patlist $idx]
        set patlist [lreplace $patlist $idx $idx]

        # remove the highlight in the main window
        .f1.t tag delete [lindex $w 4]

        # remove the highlight in other dialogs, if currently open
        SearchList_DeleteTag [lindex $w 4]
        MarkList_DeleteTag [lindex $w 4]
      }
      UpdateRcAfterIdle

      # remove the entry in the listbox
      TagList_Fill
      TextSel_SetSelection dlg_tags_sel {}
    }
  }
}


#
# This function is bound to the "Delete" key in the highlight list.
#
proc TagList_RemoveSelection {} {
  global dlg_tags_sel

  set sel [TextSel_GetSelection dlg_tags_sel]
  if {[llength $sel] > 0} {
    TagList_Remove $sel
  }
}


# ----------------------------------------------------------------------------
#
# This function creates or raises the font selection dialog.
#
proc FontList_OpenDialog {} {
  global font_normal font_content dlg_font_shown
  global dlg_font_fams dlg_font_size dlg_font_bold

  PreemptBgTasks
  if {![info exists dlg_font_shown]} {
    set dlg_font_shown 1
    toplevel .dlg_font
    wm title .dlg_font "Font selection"
    wm group .dlg_font .

    # frame #1: listbox with all available fonts
    frame .dlg_font.f1
    listbox .dlg_font.f1.fams -width 40 -height 10 -font $font_normal -exportselection false \
                        -cursor top_left_arrow -yscrollcommand {.dlg_font.f1.sb set} \
                        -selectmode browse
    pack .dlg_font.f1.fams -side left -fill both -expand 1
    scrollbar .dlg_font.f1.sb -orient vertical -command {.dlg_font.f1.fams yview} -takefocus 0
    pack .dlg_font.f1.sb -side left -fill y
    pack .dlg_font.f1 -side top -fill both -expand 1 -padx 5 -pady 5
    bind .dlg_font.f1.fams <<ListboxSelect>> {FontList_Selection; break}

    # frame #2: size and weight controls
    frame .dlg_font.f2
    label .dlg_font.f2.lab_sz -text "Font size:"
    pack .dlg_font.f2.lab_sz -side left
    spinbox .dlg_font.f2.sz -from 1 -to 99 -width 3 \
                            -textvariable dlg_font_size -command FontList_Selection
    pack .dlg_font.f2.sz -side left
    checkbutton .dlg_font.f2.bold -text "bold" \
                            -variable dlg_font_bold -command FontList_Selection
    pack .dlg_font.f2.bold -side left -padx 15
    pack .dlg_font.f2 -side top -fill x -padx 5 -pady 5

    # frame #3: demo text
    text .dlg_font.demo -width 20 -height 4 -wrap none -exportselection false \
                        -relief ridge -takefocus 0
    pack .dlg_font.demo -side top -fill x -expand 1 -padx 15 -pady 10
    bindtags .dlg_font.demo {.dlg_font.demo TextReadOnly . all}

    .dlg_font.demo insert end "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
    .dlg_font.demo insert end "abcdefghijklmnopqrstuvwxyz\n"
    .dlg_font.demo insert end "0123456789\n"
    .dlg_font.demo insert end "AAA ,,,...---;;;:::___+++=== AAA\n"

    # frame #4: ok/abort buttons
    frame .dlg_font.f3
    button .dlg_font.f3.abort -text "Abort" -command {FontList_Quit 0}
    button .dlg_font.f3.ok -text "Ok" -default active -command {FontList_Quit 1}
    pack .dlg_font.f3.abort .dlg_font.f3.ok -side left -padx 10 -pady 5
    pack .dlg_font.f3 -side top

    bind .dlg_font.f1 <Destroy> {unset -nocomplain dlg_font_shown}
    bind .dlg_font.f3.ok <Return> {event generate .dlg_font.f3.ok <space>}
    bind .dlg_font <Escape> {event generate .dlg_font.f3.abort <space>}
    focus .dlg_font.f1.fams

    # fill font list and select current font
    FontList_Fill
    set dlg_font_bold [expr {![string equal [font actual $font_content -weight] "normal"]}]
    set dlg_font_size [font actual $font_content -size]
    set idx [lsearch -exact $dlg_font_fams [font actual $font_content -family]]
    if {$idx >= 0} {
      .dlg_font.f1.fams selection set $idx
      .dlg_font.f1.fams see $idx
    }

  } else {
    wm deiconify .dlg_font
    raise .dlg_font
    focus .dlg_font.f1.fams
  }
  ResumeBgTasks
}


#
# This function fills the font selection listbox with  list of all
# font families which are available on the system.
#
proc FontList_Fill {} {
  global dlg_font_fams dlg_font_size dlg_font_bold

  set dlg_font_fams [lsort [font families -displayof .]]

  # move known fonts to the front
  set known_fonts {arial courier fixed helvetica times}
  foreach known $known_fonts {
    set idx [lsearch -exact $dlg_font_fams $known]
    if {$idx >= 0} {
      set dlg_font_fams [lreplace $dlg_font_fams $idx $idx]
      set dlg_font_fams [linsert $dlg_font_fams 0 $known]
    }
  }

  foreach f $dlg_font_fams {
    lappend dlg_font_fams $f
    .dlg_font.f1.fams insert end $f
  }
}


#
# This function is bound to changes of the selection in the font list
# or changes in the size and weight controls.  The function applies
# the selection to the demo text.
#
proc FontList_Selection {} {
  global dlg_font_fams dlg_font_size dlg_font_bold

  set sel [.dlg_font.f1.fams curselection]
  if {([llength $sel] == 1) && ($sel < [llength $dlg_font_fams])} {
    set name [list [lindex $dlg_font_fams $sel]]
    lappend name $dlg_font_size
    if {$dlg_font_bold} {
      lappend name bold
    }
    if {[catch {.dlg_font.demo configure -font $name}] != 0} {
      catch {.dlg_font.demo configure -font fixed}
    }
  }
}


#
# This function is bound to the "Ok" and "Abort" command buttons.
# In case of OK the function checks and stores the selection.
# In case of abort, the function just closes the dialog.
#
proc FontList_Quit {do_store} {
  global dlg_font_fams dlg_font_size dlg_font_bold
  global font_content

  if {$do_store} {
    set sel [.dlg_font.f1.fams curselection]
    if {([llength $sel] == 1) && ($sel < [llength $dlg_font_fams])} {
      set name [list [lindex $dlg_font_fams $sel]]
      lappend name $dlg_font_size
      if {$dlg_font_bold} {
        lappend name bold
      }

      set cerr [ApplyFont $name]
      if {$cerr ne ""} {
        tk_messageBox -type ok -icon error -parent .dlg_font \
                      -message "Selected font is unavailable: $cerr"
      }
    } else {
      tk_messageBox -type ok -icon error -parent .dlg_font \
                    -message "No font is selected - Use \"Abort\" to leave without changes."
      return
    }
  }
  unset -nocomplain dlg_font_fams dlg_font_size dlg_font_bold
  destroy .dlg_font
}


# ----------------------------------------------------------------------------
#
# This function creates or raises the color color highlight edit dialog.
#
proc Markup_OpenDialog {pat_idx} {
  global font_normal font_bold font_content col_bg_content col_fg_content
  global fmt_selection dlg_fmt_shown dlg_fmt
  global patlist col_palette

  if {![info exists dlg_fmt_shown]} {
    toplevel .dlg_fmt
    wm title .dlg_fmt "Markup editor"
    wm group .dlg_fmt .dlg_tags

    entry .dlg_fmt.epat -width 12 -textvariable dlg_fmt(pat) -exportselection false \
                        -justify center -font $font_bold -relief groove -borderwidth 2
    pack .dlg_fmt.epat -side top -fill x -expand 1 -padx 20 -pady 4 -anchor c
    frame .dlg_fmt.fop
    checkbutton .dlg_fmt.fop.mcase -text "Match case" -variable dlg_fmt(mcase) -font $font_normal
    checkbutton .dlg_fmt.fop.regexpt -text "Reg.Exp." -variable dlg_fmt(regexp) -font $font_normal
    pack .dlg_fmt.fop.mcase .dlg_fmt.fop.regexpt -side left -padx 2
    pack .dlg_fmt.fop -side top

    text .dlg_fmt.sample -height 5 -width 35 -font $font_content -wrap none \
                         -foreground $col_fg_content -background $col_bg_content \
                         -relief sunken -borderwidth 1 -takefocus 0 -highlightthickness 0 \
                         -exportselection 0 -insertofftime 0 \
                         -insertwidth [expr {2 * [font measure $font_content " "]}]
    pack .dlg_fmt.sample -side top -padx 5 -pady 6
    bindtags .dlg_fmt.sample {.dlg_fmt.sample TextReadOnly .dlg_fmt all}

    set lh [font metrics $font_content -linespace]
    .dlg_fmt.sample tag configure spacing -spacing1 $lh
    .dlg_fmt.sample tag configure margin -lmargin1 17
    eval [linsert [HighlightConfigure $fmt_selection] 0 .dlg_fmt.sample tag configure sel]
    .dlg_fmt.sample tag lower sel
    .dlg_fmt.sample tag configure sample
    .dlg_fmt.sample insert 1.0 "Text line above\n" {margin spacing} \
                               "Text sample ... sample text\n" {margin sample} \
                               "Text line below\n" {margin}

    set row 0
    frame   .dlg_fmt.mb
    label   .dlg_fmt.mb.fnt_lab -text "Font:"
    grid    .dlg_fmt.mb.fnt_lab -sticky w -column 0 -row $row
    frame   .dlg_fmt.mb.fnt_f
    checkbutton .dlg_fmt.mb.fnt_f.chk_bold -text "bold" -variable dlg_fmt(bold) \
                                      -command Markup_UpdateFormat -font $font_normal
    checkbutton .dlg_fmt.mb.fnt_f.chk_underline -text "underline" -variable dlg_fmt(underline) \
                                           -command Markup_UpdateFormat -font $font_normal
    checkbutton .dlg_fmt.mb.fnt_f.chk_overstrike -text "overstrike" -variable dlg_fmt(overstrike) \
                                            -command Markup_UpdateFormat -font $font_normal
    pack    .dlg_fmt.mb.fnt_f.chk_bold \
            .dlg_fmt.mb.fnt_f.chk_underline \
            .dlg_fmt.mb.fnt_f.chk_overstrike -side left -padx 5
    grid    .dlg_fmt.mb.fnt_f -sticky w -column 1 -row $row -columnspan 4 -pady 2
    incr row

    label   .dlg_fmt.mb.bg_lab -text "Background:"
    grid    .dlg_fmt.mb.bg_lab -sticky w -column 0 -row $row
    label   .dlg_fmt.mb.bgcol_lab -text "Color:" -font $font_normal
    grid    .dlg_fmt.mb.bgcol_lab -sticky w -column 1 -row $row
    Markup_ImageButton .dlg_fmt.mb.bgcol_mb bgcol
    grid    .dlg_fmt.mb.bgcol_mb -sticky w -column 2 -row $row -padx 10 -pady 2
    label   .dlg_fmt.mb.bgpat_lab -text "Pattern:" -font $font_normal
    grid    .dlg_fmt.mb.bgpat_lab -sticky w -column 3 -row $row
    Markup_ImageButton .dlg_fmt.mb.bgpat_mb bgpat
    grid    .dlg_fmt.mb.bgpat_mb -sticky w -column 4 -row $row -padx 10 -pady 2
    incr row

    label   .dlg_fmt.mb.fgc_lab -text "Text:"
    grid    .dlg_fmt.mb.fgc_lab -sticky w -column 0 -row $row
    label   .dlg_fmt.mb.fgcol_lab -text "Color:" -font $font_normal
    grid    .dlg_fmt.mb.fgcol_lab -sticky w -column 1 -row $row
    Markup_ImageButton .dlg_fmt.mb.fgc_mb fgcol
    grid    .dlg_fmt.mb.fgc_mb -sticky w -column 2 -row $row -padx 10 -pady 2
    label   .dlg_fmt.mb.fgpat_lab -text "Pattern:" -font $font_normal
    grid    .dlg_fmt.mb.fgpat_lab -sticky w -column 3 -row $row
    Markup_ImageButton .dlg_fmt.mb.fgpat_mb fgpat
    grid    .dlg_fmt.mb.fgpat_mb -sticky w -column 4 -row $row -padx 10 -pady 2
    incr row

    menu    .dlg_fmt.mb.bgpat_mb.men -tearoff 0
    .dlg_fmt.mb.bgpat_mb.men add radiobutton -label "none - 100% filled" \
                     -value "none" -variable dlg_fmt(bgpat) -command Markup_UpdateFormat
    foreach cmd {"75" "50" "25" "12"} {
      .dlg_fmt.mb.bgpat_mb.men add radiobutton -compound left -label "  $cmd% filled" \
                     -value "gray$cmd" -variable dlg_fmt(bgpat) -bitmap "gray$cmd" -command Markup_UpdateFormat
    }

    menu    .dlg_fmt.mb.fgpat_mb.men -tearoff 0
    .dlg_fmt.mb.fgpat_mb.men add radiobutton -label "none - 100% filled" \
                     -value "none" -variable dlg_fmt(fgpat) -command Markup_UpdateFormat
    foreach cmd {"75" "50" "25" "12"} {
      .dlg_fmt.mb.fgpat_mb.men add radiobutton -compound left -label "  $cmd% filled" \
                     -value "gray$cmd" -variable dlg_fmt(fgpat) -bitmap "gray$cmd" -command Markup_UpdateFormat
    }

    label   .dlg_fmt.mb.bd_lab -text "Border:"
    grid    .dlg_fmt.mb.bd_lab -sticky w -column 0 -row $row
    label   .dlg_fmt.mb.ref_lab -text "Relief:" -font $font_normal
    grid    .dlg_fmt.mb.ref_lab -sticky w -column 1 -row $row
    Markup_ImageButton .dlg_fmt.mb.ref_mb relief
    grid    .dlg_fmt.mb.ref_mb -sticky w -column 2 -row $row -padx 10 -pady 2

    menu    .dlg_fmt.mb.ref_mb.men -tearoff 0
    foreach cmd {flat raised sunken ridge groove solid} {
      .dlg_fmt.mb.ref_mb.men add radiobutton -label $cmd -variable dlg_fmt(relief) -value $cmd \
                                             -command Markup_UpdateFormat
    }

    label   .dlg_fmt.mb.bwd_lab -text "Width:" -font $font_normal
    grid    .dlg_fmt.mb.bwd_lab -sticky w -column 3 -row $row
    spinbox .dlg_fmt.mb.bdw_sb -from 1 -to 9 -width 3 -borderwidth 1 \
                               -textvariable dlg_fmt(border) -command Markup_UpdateFormat
    bind    .dlg_fmt.mb.bdw_sb <Return> Markup_UpdateFormat
    grid    .dlg_fmt.mb.bdw_sb -sticky w -column 4 -row $row -padx 10 -pady 2
    incr row

    label   .dlg_fmt.mb.lsp_lab -text "Line spacing:"
    grid    .dlg_fmt.mb.lsp_lab -sticky w -column 0 -row $row
    label   .dlg_fmt.mb.lsp2_lab -text "Distance:" -font $font_normal
    grid    .dlg_fmt.mb.lsp2_lab -sticky w -column 3 -row $row
    spinbox .dlg_fmt.mb.lsp_sb -from 0 -to 999 -width 3 -borderwidth 1 \
                               -textvariable dlg_fmt(spacing) -command Markup_UpdateFormat
    bind    .dlg_fmt.mb.lsp_sb <Return> Markup_UpdateFormat
    grid    .dlg_fmt.mb.lsp_sb -sticky w -column 4 -row $row -padx 10 -pady 2
    incr row
    pack .dlg_fmt.mb -side top -padx 5 -pady 3 -anchor nw

    button .dlg_fmt.cop -text "Edit color palette..." -command Palette_OpenDialog \
                        -borderwidth 0 -relief flat -font [DeriveFont $font_normal -2 underline] \
                        -foreground {#0000ff} -activeforeground {#0000ff} -padx 0 -pady 0
    pack .dlg_fmt.cop -side top -anchor w -padx 5 -pady 4

    frame .dlg_fmt.f2
    button .dlg_fmt.f2.abort -text "Abort" -command {Markup_Save 0 1}
    button .dlg_fmt.f2.apply -text "Apply" -command {Markup_Save 1 0}
    button .dlg_fmt.f2.ok -text "Ok" -default active -command {Markup_Save 1 1}
    pack .dlg_fmt.f2.abort .dlg_fmt.f2.apply .dlg_fmt.f2.ok -side left -padx 10 -pady 4
    pack .dlg_fmt.f2 -side top

    bind .dlg_fmt.mb <Destroy> {+ unset -nocomplain dlg_fmt_shown}
    set dlg_fmt_shown 1

  } else {
    wm deiconify .dlg_fmt
    raise .dlg_fmt
  }

  Markup_InitConfig $pat_idx
  Markup_UpdateFormat
  .dlg_fmt.epat selection clear
  .dlg_fmt.epat icursor end
}


#
# This function is called when the mark-up dialog is opened to copy the
# format parameters from the global patlist into the dialog's has array.
#
proc Markup_InitConfig {pat_idx} {
  global patlist dlg_fmt

  set w [lindex $patlist $pat_idx]
  set dlg_fmt(pat) [lindex $w 0]
  set dlg_fmt(regexp) [lindex $w 1]
  set dlg_fmt(mcase) [lindex $w 2]
  set dlg_fmt(tagnam) [lindex $w 4]
  set dlg_fmt(bgcol) [lindex $w 6]
  set dlg_fmt(fgcol) [lindex $w 7]
  set dlg_fmt(bold) [lindex $w 8]
  set dlg_fmt(underline) [lindex $w 9]
  set dlg_fmt(overstrike) [lindex $w 10]
  set dlg_fmt(bgpat) [lindex $w 11]
  set dlg_fmt(fgpat) [lindex $w 12]
  set dlg_fmt(relief) [lindex $w 13]
  set dlg_fmt(border) [lindex $w 14]
  set dlg_fmt(spacing) [lindex $w 15]
  if {$dlg_fmt(bgpat) eq ""} {set dlg_fmt(bgpat) "none"}
  if {$dlg_fmt(fgpat) eq ""} {set dlg_fmt(fgpat) "none"}
  if {$dlg_fmt(relief) eq ""} {set dlg_fmt(relief) "flat"}
  if {$dlg_fmt(relief) eq ""} {set dlg_fmt(relief) "flat"}
}


#
# This function is called when the mark-up dialog is closed to build a
# parameter list from the dialog's temporary hash array.
#
proc Markup_GetConfig {pat_idx} {
  global dlg_fmt patlist

  if {$pat_idx >= 0} {
    set w [lindex $patlist $pat_idx]
  } else {
    set w [list {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}]
  }

  set bgpat $dlg_fmt(bgpat)
  set fgpat $dlg_fmt(fgpat)
  set relief $dlg_fmt(relief)
  set border $dlg_fmt(border)
  set spacing $dlg_fmt(spacing)
  if {$bgpat eq "none"} {set bgpat ""}
  if {$fgpat eq "none"} {set fgpat ""}
  if {$relief eq "flat"} {set relief ""}
  if {![regexp {^\d+$} $border]} {set border 1}
  if {![regexp {^\d+$} $spacing]} {set spacing 0}

  set w [lreplace $w 0 2 \
    $dlg_fmt(pat) \
    $dlg_fmt(regexp) \
    $dlg_fmt(mcase)]
  set w [lreplace $w 6 15 \
    $dlg_fmt(bgcol) \
    $dlg_fmt(fgcol) \
    $dlg_fmt(bold) \
    $dlg_fmt(underline) \
    $dlg_fmt(overstrike) \
    $bgpat \
    $fgpat \
    $relief \
    $border \
    $spacing]

  return $w
}


#
# This function is bound to the "Ok" and "Abort" buttons in the mark-up dialog.
#
proc Markup_Save {do_save do_quit} {
  global dlg_fmt patlist

  if {$do_save} {
    # determine the edited pattern's index in the list (use the unique tag
    # which doesn't change even if the list is reordered)
    set tagnam $dlg_fmt(tagnam)
    set idx 0
    foreach w $patlist {
      if {[lindex $w 4] eq $tagnam} {
        set pat_idx $idx
        break
      }
      incr idx
    }
    if {[info exists pat_idx]} {
      set old_w [lindex $patlist $pat_idx]
      set w [Markup_GetConfig $pat_idx]
      set patlist [lreplace $patlist $pat_idx $pat_idx $w]
      UpdateRcAfterIdle

      # update hightlight color listbox
      TagList_Update $pat_idx

      # update tag in the search result dialog
      SearchList_CreateHighlightTags
      MarkList_CreateHighlightTags

      # update tag in the main window
      set cfg [HighlightConfigure $w]
      eval [linsert $cfg 0 .f1.t tag configure $tagnam]

      # check if the search pattern changed
      if {([lindex $old_w 0] ne [lindex $w 0]) ||
          ([lindex $old_w 1] ne [lindex $w 1]) ||
          ([lindex $old_w 2] ne [lindex $w 2])} {

        # remove the tag and re-apply to the text
        .f1.t tag remove $tagnam 1.0 end

        set opt [Search_GetOptions [lindex $w 0] [lindex $w 1] [lindex $w 2]]
        HighlightAll [lindex $w 0] $tagnam $opt
      }

    } else {
      tk_messageBox -type ok -icon error -parent .dlg_fmt \
                    -message "This element has already been deleted."
      return
    }
  }
  if {$do_quit} {
    unset -nocomplain dlg_fmt
    destroy .dlg_fmt
  }
}


#
# This function is called whenever a format parameter is changed to update
# the sample text and the control widgets.
#
proc Markup_UpdateFormat {} {
  global dlg_fmt font_content col_bg_content col_fg_content

  set cfg [HighlightConfigure [Markup_GetConfig -1]]
  eval [linsert $cfg 0 .dlg_fmt.sample tag configure sample]

  if {$dlg_fmt(relief) ne "none"} {
    .dlg_fmt.mb.bdw_sb conf -state normal
  } else {
    .dlg_fmt.mb.bdw_sb conf -state disabled
  }

  # adjust spacing above first line to center the content vertically
  set lh [font metrics $font_content -linespace]
  set spc [expr {$lh - $dlg_fmt(spacing)}]
  if {$spc < 0} {set spc 0}
  .dlg_fmt.sample tag configure spacing -spacing1 $spc

  # update the entry widgets
  if {$dlg_fmt(bgcol) ne {}} {
    .dlg_fmt.mb.bgcol_mb.c configure -background $dlg_fmt(bgcol)
  } else {
    .dlg_fmt.mb.bgcol_mb.c configure -background $col_bg_content
  }
  if {$dlg_fmt(fgcol) ne {}} {
    .dlg_fmt.mb.fgc_mb.c configure -background $dlg_fmt(fgcol)
  } else {
    .dlg_fmt.mb.fgc_mb.c configure -background $col_fg_content
  }
  if {$dlg_fmt(bgpat) ne "none"} {
    .dlg_fmt.mb.bgpat_mb.c itemconfigure all -bitmap $dlg_fmt(bgpat)
  } else {
    .dlg_fmt.mb.bgpat_mb.c itemconfigure all -bitmap {}
  }
  if {$dlg_fmt(fgpat) ne "none"} {
    .dlg_fmt.mb.fgpat_mb.c itemconfigure all -bitmap $dlg_fmt(fgpat)
  } else {
    .dlg_fmt.mb.fgpat_mb.c itemconfigure all -bitmap {}
  }
  .dlg_fmt.mb.ref_mb.c.w configure -relief $dlg_fmt(relief)
}


#
# This function is used during creation of the markup editor dialog to
# create the widgets for color, pattern and relief selection. The widget
# consists of a rectangle which displays the current choice and a button
# which triggers a popup menu when pressed.
#
proc Markup_ImageButton {wid type} {
  global dlg_fmt

  CreateButtonBitmap img_dropdown
  frame ${wid} -relief sunken -borderwidth 1
  canvas ${wid}.c -width [expr {[image width img_dropdown] + 4}] -height [image height img_dropdown] \
                 -highlightthickness 0 -takefocus 0 -borderwidth 0
  pack ${wid}.c -fill both -expand 1 -side left
  button ${wid}.b -image img_dropdown -highlightthickness 1 -borderwidth 1 -relief raised
  pack ${wid}.b -side left

  if {[string match {*col} $type]} {
    ${wid}.b configure -command [list Markup_PopupColorPalette $wid $type]
  } elseif {[string match {*pat} $type]} {
    ${wid}.c create bitmap 2 2 -anchor nw
    ${wid}.b configure -command [list Markup_PopupPatternMenu ${wid}]
  } elseif {[string equal {relief} $type]} {
    frame ${wid}.c.w -width 10 -height 10 -borderwidth 2 -relief flat
    ${wid}.c create window 3 3 -anchor nw -window ${wid}.c.w -width 12 -height 12
    ${wid}.b configure -command [list Markup_PopupPatternMenu ${wid}]
  }
}


#
# This helper function is invoked when the "drop down" button is pressed
# on a color selction widget: it opens the color palette menu directly
# below the widget.
#
proc Markup_PopupColorPalette {wid type} {
  global dlg_fmt

  set rootx [winfo rootx $wid]
  set rooty [expr {[winfo rooty $wid] + [winfo height $wid]}]
  PaletteMenu_Popup .dlg_fmt $rootx $rooty [list Markup_UpdateColor $type] $dlg_fmt($type)
}


#
# This helper function is invoked when the "drop down" button is pressed
# on a pattern selction widget: it opens the associated menu directly
# below the widget.
#
proc Markup_PopupPatternMenu {wid} {
  set rootx [winfo rootx $wid]
  set rooty [expr {[winfo rooty $wid] + [winfo height $wid]}]
  tk_popup ${wid}.men $rootx $rooty {}
}


#
# This helper function is invoked as callback after a color was selected
# in the palette popup menu.
#
proc Markup_UpdateColor {type col} {
  set ::dlg_fmt($type) $col
  Markup_UpdateFormat
}



# ----------------------------------------------------------------------------
#
# This function creates or raises the color palette dialog which allows to
# add, delete, modify or reorder colors used for highlighting.
#
proc Palette_OpenDialog {} {
  global font_normal dlg_cols_shown dlg_cols_palette dlg_cols_cid
  global col_palette

  if {![info exists dlg_cols_shown]} {
    toplevel .dlg_cols
    wm title .dlg_cols "Color palette"
    wm group .dlg_cols .

    label .dlg_cols.lab_hd -text "Pre-define a color palette for quick selection\nwhen changing colors. Use the context menu\nor drag-and-drop to modify the palette:" \
                           -font $font_normal -justify left
    pack .dlg_cols.lab_hd -side top -anchor w -pady 5 -padx 5

    canvas .dlg_cols.c -width 1 -height 1 -background [.dlg_cols cget -background] \
                       -cursor top_left_arrow
    pack .dlg_cols.c -side top -padx 10 -pady 10 -anchor w

    bind .dlg_cols.c <ButtonRelease-3> {Palette_ContextMenu %x %y}
    bind .dlg_cols.c <Destroy> {+ unset -nocomplain dlg_cols_shown}
    set dlg_cols_shown 1

    frame .dlg_cols.f2
    button .dlg_cols.f2.abort -text "Abort" -command {Palette_Save 0}
    button .dlg_cols.f2.ok -text "Ok" -default active -command {Palette_Save 1}
    pack .dlg_cols.f2.abort .dlg_cols.f2.ok -side left -padx 10 -pady 5
    pack .dlg_cols.f2 -side top

    menu .dlg_cols.ctxmen -tearoff 0

    set dlg_cols_palette $col_palette
    Palette_Fill .dlg_cols.c $dlg_cols_palette

  } else {
    wm deiconify .dlg_cols
    raise .dlg_cols
  }
}


#
# This functions fills the color palette canvas with rectangles which
# each display one of the currently defined colors. Each rectangle gets
# mouse bindings for a context menu and changing the order of colors.
#
proc Palette_Fill {wid pal {sz 20} {sel_cmd {}}} {
  global dlg_cols_cid

  $wid delete all
  set dlg_cols_cid {}

  set x 2
  set y 2
  set col_idx 0
  set idx 0
  foreach col $pal {
    set cid [$wid create rect $x $y [expr {$x + $sz}] [expr {$y + $sz}] \
                                     -outline black -fill $col \
                                     -activeoutline black -activewidth 2]
    lappend dlg_cols_cid $cid

    if {$sel_cmd eq ""} {
      $wid bind $cid <Double-Button-1> [list Palette_EditColor $idx $cid]
      $wid bind $cid <B1-Motion> [list Palette_MoveColor $idx $cid %x %y]
      $wid bind $cid <ButtonRelease-1> [list Palette_MoveColorEnd $idx $cid %x %y]
    } else {
      $wid bind $cid <Button-1> [linsert $sel_cmd end $col]
    }

    incr x $sz
    incr col_idx 1
    if {$col_idx >= 10} {
      incr y $sz
      set x 2
      set col_idx 0
    }
    incr idx
  }

  $wid configure -width [expr {10 * $sz + 3+3}] \
                 -height [expr {(int([llength $pal] + (10-1)) / 10) * $sz + 2+2}]
}


#
# This function is bound to right mouse clicks on color items.
#
proc Palette_ContextMenu {xcoo ycoo} {
  global dlg_cols_palette dlg_cols_cid

  set cid [.dlg_cols.c find closest $xcoo $ycoo]
  if {$cid ne ""} {
    set idx [lsearch -exact -integer $dlg_cols_cid $cid]
    if {$idx != -1} {

      .dlg_cols.ctxmen delete 0 end
      .dlg_cols.ctxmen add command -label "" -background [lindex $dlg_cols_palette $idx] -state disabled
      .dlg_cols.ctxmen add separator
      .dlg_cols.ctxmen add command -label "Change this color..." -command [list Palette_EditColor $idx $cid]
      .dlg_cols.ctxmen add command -label "Duplicate this color" -command [list Palette_DuplicateColor $idx $cid]
      .dlg_cols.ctxmen add command -label "Insert new color (white)" -command [list Palette_InsertColor $idx $cid]
      .dlg_cols.ctxmen add separator
      .dlg_cols.ctxmen add command -label "Remove this color" -command [list Palette_RemoveColor $idx]

      set rootx [expr {[winfo rootx .dlg_cols] + $xcoo}]
      set rooty [expr {[winfo rooty .dlg_cols] + $ycoo}]
      tk_popup .dlg_cols.ctxmen $rootx $rooty 0
    }
  }
}


#
# This function is bound to the "remove this color" menu item in the
# color palette context menu.
#
proc Palette_RemoveColor {idx} {
  global dlg_cols_palette

  if {$idx < [llength $dlg_cols_palette]} {
    set dlg_cols_palette [lreplace $dlg_cols_palette $idx $idx]
    Palette_Fill .dlg_cols.c $dlg_cols_palette
  }
}


#
# This function is bound to the "insert new color" menu item in the
# color palette entries. It inserts an white color entry at the mouse
# pointer position.
#
proc Palette_InsertColor {idx cid} {
  global dlg_cols_palette

  set dlg_cols_palette [linsert $dlg_cols_palette $idx {#ffffff}]
  Palette_Fill .dlg_cols.c $dlg_cols_palette
}

proc Palette_DuplicateColor {idx cid} {
  global dlg_cols_palette

  if {$idx < [llength $dlg_cols_palette]} {
    set col [lindex $dlg_cols_palette $idx]
    set dlg_cols_palette [linsert $dlg_cols_palette $idx $col]
    Palette_Fill .dlg_cols.c $dlg_cols_palette
  }
}


#
# This function is bound to the "edit this color" menu item in the
# color palette context menu.
#
proc Palette_EditColor {idx cid} {
  global dlg_cols_palette

  set col [lindex $dlg_cols_palette $idx]
  set col [tk_chooseColor -initialcolor $col -parent .dlg_cols -title "Select color"]
  if {$col ne ""} {
    set dlg_cols_palette [lreplace $dlg_cols_palette $idx $idx $col]
    .dlg_cols.c itemconfigure $cid -fill $col
  }
}


#
# This function is bound to motion events on color palette entries while
# the left mouse button is helt down.
#
proc Palette_MoveColor {idx cid xcoo ycoo} {
  set sz 20
  set sz_2 [expr {0 - ($sz /2)}]
  incr xcoo $sz_2
  incr ycoo $sz_2
  .dlg_cols.c raise $cid
  .dlg_cols.c coords $cid $xcoo $ycoo [expr {$xcoo + $sz}] [expr {$ycoo + $sz}]
}


#
# This function is bound to the mouse button release event on color palette
# entries. It's used to change the order of colors by drag-and-drop.
#
proc Palette_MoveColorEnd {idx cid xcoo ycoo} {
  global dlg_cols_palette

  set sz 20
  incr xcoo -2
  incr ycoo -2
  if {$xcoo < 0} {set col_idx 0} else {set col_idx [expr {int($xcoo / $sz)}]}
  if {$ycoo < 0} {set row_idx 0} else {set row_idx [expr {int($ycoo / $sz)}]}

  set new_idx [expr {($row_idx * 10) + $col_idx}]
  set col [lindex $dlg_cols_palette $idx]
  set dlg_cols_palette [lreplace $dlg_cols_palette $idx $idx]
  set dlg_cols_palette [linsert $dlg_cols_palette $new_idx $col]

  Palette_Fill .dlg_cols.c $dlg_cols_palette
}


#
# This function is bound to the "ok" and "abort" buttons. Ths function
# closes the color palette dialog. In case of "ok" the edited palette
# is stored.
#
proc Palette_Save {do_save} {
  global col_palette dlg_cols_palette dlg_cols_cid

  if {$do_save} {
    set col_palette $dlg_cols_palette
    UpdateRcAfterIdle
  }
  unset -nocomplain dlg_cols_palette dlg_cols_cid
  destroy .dlg_cols
}


#
# This function creates a menu with all the colors. It's usually used as
# sub-menu (i.e. cascade) in other menus.
#
proc PaletteMenu_Popup {parent rootx rooty cmd col_def} {
  global col_palette font_normal

  toplevel .colsel -highlightthickness 0
  wm title .colsel "Color selection menu"
  wm transient .colsel $parent
  wm geometry .colsel "+$rootx+$rooty"
  wm resizable .colsel 0 0

  canvas .colsel.c -width 1 -height 1 -background [.colsel cget -background] \
                   -cursor top_left_arrow -highlightthickness 0
  pack .colsel.c -side top

  frame .colsel.f1
  button .colsel.f1.b_other -text "Other..." -command [list PaletteMenu_OtherColor $parent $cmd $col_def] \
                         -borderwidth 0 -relief flat -font [DeriveFont $font_normal -2 underline] \
                         -foreground {#0000ff} -activeforeground {#0000ff} -padx 0 -pady 0
  pack .colsel.f1.b_other -side left -expand 1 -anchor w
  button .colsel.f1.b_none -text "None" -command [linsert $cmd end {}] \
                       -borderwidth 0 -relief flat -font [DeriveFont $font_normal -2 underline] \
                       -foreground {#0000ff} -activeforeground {#0000ff} -padx 0 -pady 0
  pack .colsel.f1.b_none -side left -expand 1 -anchor e
  pack .colsel.f1 -side top -fill x -expand 1

  Palette_Fill .colsel.c $col_palette 15 $cmd

  bind .colsel <ButtonRelease-1> {destroy .colsel}
  focus .colsel.c
  grab .colsel
}


#
# This helper function is bound to "Other..." in the palette popup menu.
# This function opens the color editor and returns the selected color to
# the owner of the palette popup, if any.
#
proc PaletteMenu_OtherColor {parent cmd col_def} {
  destroy .colsel
  if {$col_def eq ""} {
    set col_def {#000000}
  }
  set col [tk_chooseColor -initialcolor $col_def -parent $parent -title "Select color"]
  if {$col ne ""} {
    eval [linsert $cmd end $col]
  }
}


# ----------------------------------------------------------------------------
#
# This functions creates the "About" dialog with copyleft info
#
proc OpenAboutDialog {} {
  global dlg_about_shown font_normal

  PreemptBgTasks
  if {![info exists dlg_about_shown]} {
    toplevel .about
    wm title .about "About"
    wm group .about .
    wm transient .about .
    wm resizable .about 1 1

    label .about.name -text "Trace Browser"
    pack .about.name -side top -pady 8

    label .about.copyr1 -text "Copyright (C) 2007-2009 Tom Zoerner" -font $font_normal
    pack .about.copyr1 -side top

    message .about.m -font $font_normal -text {
This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License Version 3 as published by the Free Software Foundation at http://www.fsf.org/

THIS PROGRAM IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL, BUT WITHOUT ANY WARRANTY; WITHOUT EVEN THE IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
    }
    pack .about.m -side top

    button .about.dismiss -text "Close" -command {destroy .about}
    pack .about.dismiss -pady 10

    bind  .about.dismiss <Destroy> {+ unset -nocomplain dlg_about_shown}
    bind  .about.dismiss <Return> {event generate .about.dismiss <space>}
    bind  .about.dismiss <Escape> {event generate .about.dismiss <space>}
    focus .about.dismiss
    set dlg_about_shown 1

  } else {
    wm deiconify .about
    raise .about
    focus .about.dismiss
  }
  ResumeBgTasks
}


# ----------------------------------------------------------------------------
#
# The following set of functions form a "library" which allow to use a text
# widget in the way of a listbox, i.e. allow to select one or more lines.
# The mouse bindings are similar to the listbox "extended" mode. The cursor
# key bindings differ from the listbox, as there is no "active" element
# (i.e. there's no separate cursor from the selection.)
#

#
# This function is called after a text widget is created to add event
# bindings for handling the selection and to initialize a global variable
# with the selection state.
#
proc TextSel_Init {wid var cb_proc len_proc mode} {
  upvar $var state

  # initialize selection state struct:
  # 0- text widget command name
  # 1- callback to invoke after selection changes
  # 2- callback which provides the content list length
  # 3- ID of event handler for scrolling, or empty
  # 4- scrolling speed
  # 5- anchor element index OR last selection cursor pos
  # 6- list of selected element indices
  set state [list $wid $cb_proc $len_proc {} 0 -1 {}]

  bind $wid <Control-ButtonPress-1> "TextSel_Pick $var %x %y; break"
  bind $wid <Shift-ButtonPress-1> "TextSel_Resize $var %x %y; break"
  bind $wid <ButtonPress-1> "TextSel_Button $var %x %y; break"
  bind $wid <ButtonRelease-1> "TextSel_MotionEnd $var; break"
  if {$mode eq "browse"} {
    bind $wid <B1-Motion> "TextSel_Button $var %x %y; break"
  } else {
    bind $wid <B1-Motion> "TextSel_Motion $var %x %y; break"
  }

  bind $wid <Shift-Key-Up> "TextSel_KeyResize $var -1; break"
  bind $wid <Shift-Key-Down> "TextSel_KeyResize $var 1; break"
  bind $wid <Key-Up> "TextSel_KeyUpDown $var -1; break"
  bind $wid <Key-Down> "TextSel_KeyUpDown $var 1; break"
  bind $wid <Shift-Key-Home> "TextSel_KeyHomeEnd $var 0 1; break"
  bind $wid <Shift-Key-End> "TextSel_KeyHomeEnd $var 1 1; break"
  bind $wid <Key-Home> "TextSel_KeyHomeEnd $var 0 0; break"
  bind $wid <Key-End> "TextSel_KeyHomeEnd $var 1 0; break"

  #KeyBinding_UpDown $wid
  #KeyBinding_LeftRight $wid
  bind $wid <FocusIn> {KeyClr}
  #bind $wid <Return> "if {\[KeyCmd $wid Return\]} break"
  bind $wid <KeyPress> "if {\[KeyCmd $wid %A\]} break"

  bind $wid <Key-Prior> "TextSel_SetSelection $var {}"
  bind $wid <Key-Next> "TextSel_SetSelection $var {}"
}


#
# This is an interface function which allows outside users to retrieve a
# list of selected elements (i.e. a list of indices)
#
proc TextSel_GetSelection {var} {
  upvar #0 $var state

  return [lindex $state 6]
}


#
# This is an interface function which allows to modify the selection
# externally.
#
proc TextSel_SetSelection {var sel {callback 1}} {
  upvar #0 $var state

  if {[llength $sel] > 0} {
    set state [lreplace $state 5 6 [lindex $sel 0] $sel]
  } else {
    set state [lreplace $state 6 6 {}]
  }
  TextSel_ShowSelection $var

  if {$callback} {
    eval [list [lindex $state 1] [lindex $state 6]]
  }
}


#
# This is an interface function which is used by context menus to check
# if the item under the mouse pointer is included in the selection.
# If not, the selection is set to consist only of the pointed item.
#
proc TextSel_ContextSelection {var xcoo ycoo} {
  upvar #0 $var state

  set line [TextSel_Coo2Line $var $xcoo $ycoo]
  if {$line != -1} {
    set sel [lindex $state 6]
    if {[llength $sel] != 0} {
      if {[lsearch -exact -integer $sel $line] == -1} {
        # click was outside the current selection -> replace selection
        TextSel_SetSelection $var $line
      }
    } else {
      # nothing selected yet -> select element under the mouse pointer
      TextSel_SetSelection $var $line
    }
  } else {
    TextSel_SetSelection $var {}
  }
}


#
# This function is bound to button-press events in the text widget while
# neither Control nor Shift keys are pressed.  A previous selection is
# is cleared and the entry below the mouse (if any) is selected.
#
proc TextSel_Button {var xcoo ycoo} {
  upvar #0 $var state

  set line [TextSel_Coo2Line $var $xcoo $ycoo]
  set len_cb [lindex $state 2]
  set old_sel [lindex $state 6]
  if {($line != -1) && ($line < [$len_cb])} {
    # select the entry under the mouse pointer
    set state [lreplace $state 5 6 $line $line]
    set notify 1
  } else {
    # mouse pointer is not above a list entry -> clear selection
    set state [lreplace $state 6 6 {}]
    set notify 0
  }
  # update display if the selection changed
  if {$old_sel ne [lindex $state 6]} {
    TextSel_ShowSelection $var
    set notify 1
  }
  # invoke notification callback if an element was selected or de-selected
  if {$notify} {
    eval [list [lindex $state 1] [lindex $state 6]]
  }
  focus [lindex $state 0]
}


#
# This function is bound to mouse pointer motion events in the text widget
# while the mouse buttin is pressed down. This allows to change the extent
# of the selection. The originally selected item ("anchor") always remains
# selected.  If the pointer is moved above or below the widget borders,
# the text is scrolled.
#
proc TextSel_Motion {var xcoo ycoo} {
  upvar #0 $var state

  # the anchor element is the one above which the mouse button was pressed
  # (the check here is for fail-safety only, should always be fulfilled)
  set anchor [lindex $state 5]
  if {$anchor >= 0} {
    set wid [lindex $state 0]
    set wh [winfo height $wid]
    # check if the mouse is still inside of the widget area
    if {($ycoo >= 0) && ($ycoo < $wh)} {
      # identify the item under the mouse pointer
      set line [TextSel_Coo2Line $var $xcoo $ycoo]
      if {$line != -1} {
        # build list of all consecutive indices between the anchor and the mouse position
        set sel [TextSel_IdxRange $anchor $line]
        # update display and invoke notification callback if the selection changed
        if {$sel ne [lindex $state 6]} {
          set state [lreplace $state 6 6 $sel]
          TextSel_ShowSelection $var
          eval [list [lindex $state 1] [lindex $state 6]]
        }
      }
      # cancel scrolling timer, as the mouse is now back inside the widget
      if {[lindex $state 3] ne ""} {
        after cancel [lindex $state 3]
        set state [lreplace $state 3 3 ""]
      }

    } else {
      # mouse is outside of the text widget - start scrolling
      # scrolling speed is determined by how far the mouse is outside
      set fh [font metrics [$wid cget -font] -linespace]
      if {$ycoo < 0} {
        set delta [expr {0 - $ycoo}]
      } else {
        set delta [expr {$ycoo - $wh}]
      }
      set delay [expr {500 - int($delta / $fh) * 100}]
      if {$delay > 500} {set delay 500}
      if {$delay <  50} {set delay  50}
      if {[lindex $state 3] eq ""} {
        # start timer and remember it's ID to be able to cancel it later
        set tid [after $delay [list TextSel_MotionScroll $var [expr {($ycoo < 0) ? -1 : 1}]]]
        set state [lreplace $state 3 4 $tid $delay]
      } else {
        # timer already active - just update the delay
        set state [lreplace $state 4 4 $delay]
      }
    }
  }
}


#
# This timer event handler is activated when the mouse is moved outside of
# the text widget while the mouse button is pressed. The handler re-installs
# itself and is only stopped when the button is released or the mouse is
# moved back inside the widget area.  The function invariably scrolls the
# text by one line. Scrolling speed is varied by means of the delay time.
#
proc TextSel_MotionScroll {var delta} {
  upvar #0 $var state

  # check if the widget still exists
  set wid [lindex $state 0]
  if {[info exists state] && ([info commands $wid] ne "")} {

    # scroll up or down by one line
    $wid yview scroll $delta units

    # extend the selection to the end of the viewable area
    if {$delta < 0} {
      TextSel_Motion $var 0 0
    } else {
      TextSel_Motion $var 0 [expr {[winfo height $wid] - 1}]
    }

    # install the timer gain (possibly with a changed delay if the mouse was moved)
    set delay [lindex $state 4]
    set tid [after $delay [list TextSel_MotionScroll $var $delta]]
    set state [lreplace $state 3 3 $tid]
  }
}


#
# This function is boud to mouse button release events and stops a
# possible on-going scrolling timer.
#
proc TextSel_MotionEnd {var} {
  upvar #0 $var state

  if {[lindex $state 3] ne ""} {
    after cancel [lindex $state 3]
    set state [lreplace $state 3 3 ""]
  }
}


#
# This function is bound to mouse button events while the Control key is
# pressed. The item below the mouse pointer is toggled in the selection.
# Otherwise the selection is left unchanged.  Note this operation always
# clears the "anchor" element, i.e. the selection cannot be modified
# using "Shift-Click" afterwards.
#
proc TextSel_Pick {var xcoo ycoo} {
  upvar #0 $var state

  set line [TextSel_Coo2Line $var $xcoo $ycoo]
  if {$line != -1} {
    set sel [lindex $state 6]
    set sel_len [llength $sel]
    # check if the item is already selected
    set pick_idx [lsearch -exact -integer $sel $line]
    if {$pick_idx != -1} {
      # already selected -> remove from selection
      set sel [lreplace $sel $pick_idx $pick_idx]
    } else {
      lappend sel $line
    }
    if {[llength $sel] > 1} {
      set state [lreplace $state 6 6 $sel]
    } else {
      set state [lreplace $state 5 6 $pick_idx $sel]
    }
    TextSel_ShowSelection $var
    eval [list [lindex $state 1] [lindex $state 6]]
  }
}


#
# This function is bound to mouse button events while the Shift key is
# pressed. The selection is changed to cover all items starting at the
# anchor item and the item under the mouse pointer.  If no anchor is
# defined, the selection is reset and only the item under the mouse is
# selected.
#
proc TextSel_Resize {var xcoo ycoo} {
  upvar #0 $var state

  set line [TextSel_Coo2Line $var $xcoo $ycoo]
  if {$line != -1} {
    set anchor [lindex $state 5]
    if {$anchor != -1} {
      set sel [TextSel_IdxRange $anchor $line]
      set state [lreplace $state 6 6 $sel]
      TextSel_ShowSelection $var
      eval [list [lindex $state 1] [lindex $state 6]]
    } else {
      TextSel_Button $var $xcoo $ycoo
    }
  }
}


#
# This function is bound to the up/down cursor keys. If no selection
# exists, the viewable first item in cursor direction is selected.
# If a selection exists, it's cleared and the item next to the
# previous selection in cursor direction is selected.
#
proc TextSel_KeyUpDown {var delta} {
  upvar #0 $var state

  set wid [lindex $state 0]
  set len_cb [lindex $state 2]
  set content_len [$len_cb]
  if {$content_len > 0} {
    set sel [lsort -integer -increasing [lindex $state 6]]
    if {[llength $sel] != 0} {
      # selection already exists -> determine item below or above
      if {$delta < 0} {
        set line [lindex $sel 0]
      } else {
        set line [lindex $sel end]
      }
      # determine the newly selected item
      incr line $delta

      if {($line >= 0) && ($line < $content_len)} {
        # set selection on the new line
        set state [lreplace $state 5 6 $line $line]

        TextSel_ShowSelection $var
        $wid see "[expr {$line + 1}].0"
        eval [list [lindex $state 1] [lindex $state 6]]

      } elseif {[llength $sel] > 1} {
        # selection already includes last line - restrict selection to this single line
        if {$delta < 0} {
          set line 0
        } else {
          set line [expr {$content_len - 1}]
        }
        set state [lreplace $state 5 6 $line $line]

        TextSel_ShowSelection $var
        $wid see "[expr {$line + 1}].0"
        eval [list [lindex $state 1] [lindex $state 6]]
      }

    } else {
      # no selection exists yet -> use last anchor, or top/bottom visible line
      set anchor [lindex $state 5]
      if {($anchor >= 0) && ([$wid bbox "[expr {$anchor + 1}].0"] ne "")} {
        set idx "[expr {$anchor + 1}].0"
      } else {
        if {$delta > 0} {
          set idx {@1,1}
        } else {
          set idx "@1,[expr {[winfo height $wid] - 1}]"
        }
      }
      set pos [$wid index $idx]
      if {([scan $pos "%d.%d" line char] == 2) && ($line > 0)} {
        incr line -1
        if {$line >= $content_len} {set line [expr {$content_len - 1}]}
        set state [lreplace $state 5 6 $line $line]

        TextSel_ShowSelection $var
        $wid see "[expr {$line + 1}].0"
        eval [list [lindex $state 1] [lindex $state 6]]
      }
    }
  }
}


#
# This function is bound to the up/down cursor keys while the Shift key
# is pressed. The selection is changed to cover all items starting at the
# anchor item and the next item above or below the current selection.
#
proc TextSel_KeyResize {var delta} {
  upvar #0 $var state

  set wid [lindex $state 0]
  set len_cb [lindex $state 2]
  set content_len [$len_cb]
  set anchor [lindex $state 5]
  set sel [lindex $state 6]
  if {[llength $sel] > 0} {
    set sel [lsort -integer -increasing $sel]
    # decide if we manipulate the upper or lower end of the selection:
    # use the opposite side of the anchor element
    if {$anchor == [lindex $sel end]} {
      set line [lindex $sel 0]
    } else {
      set line [lindex $sel end]
    }
    incr line $delta
    if {($line >= 0) && ($line < $content_len)} {
      set sel [TextSel_IdxRange $anchor $line]
      set state [lreplace $state 6 6 $sel]

      TextSel_ShowSelection $var
      $wid see "[expr {$line + 1}].0"
      eval [list [lindex $state 1] [lindex $state 6]]
    }
  } else {
    TextSel_KeyUpDown $var $delta
  }
}


#
# This function is bound to the "Home" and "End" keys.  While the Shift
# key is not pressed, the first or last element in the list are selected.
# If the Shift key is pressed, the selection is extended to include all
# items between the anchor and the first or last item.
#
proc TextSel_KeyHomeEnd {var is_end is_resize} {
  upvar #0 $var state

  set wid [lindex $state 0]
  set len_cb [lindex $state 2]
  set anchor [lindex $state 5]

  set content_len [$len_cb]
  if {$content_len > 0} {
    if {$is_end} {
      set line [expr {$content_len - 1}]
    } else {
      set line 0
    }
    if {$is_resize == 0} {
      set state [lreplace $state 5 6 $line $line]
    } else {
      if {$anchor >= 0} {
        set sel [TextSel_IdxRange $anchor $line]
        set state [lreplace $state 6 6 $sel]
      }
    }
    TextSel_ShowSelection $var
    $wid see "[expr {$line + 1}].0"
    eval [list [lindex $state 1] [lindex $state 6]]
  }
}


#
# This helper function is used to build a list of all indices between
# (and including) two given values in increasing order.
#
proc TextSel_IdxRange {start end} {
  if {$start > $end} {
    set tmp $start
    set start $end
    set end $tmp
  }
  for {set idx $start} {$idx <= $end} {incr idx} {
    lappend sel $idx
  }
  return $sel
}


#
# This function displays a selection in the text widget by adding the
# "sel" tag to all selected lines.  (Note the view is not affected, i.e.
# the selection may be outside of the viewable area.)
#
proc TextSel_ShowSelection {var} {
  upvar #0 $var state

  set wid [lindex $state 0]
  set sel [lindex $state 6]
  set anchor [lindex $state 5]

  # first remove any existing highlight
  $wid tag remove sel "1.0" end

  # select each selected line (may be non-consecutive)
  foreach line $sel {
    $wid tag add sel "[expr {$line + 1}].0" "[expr {$line + 2}].0"
  }

  if {([llength $sel] == 0) || ($anchor == -1)} {
    $wid mark set insert end
  } else {
    $wid mark set insert "[expr {$anchor + 1}].0"
  }
}


#
# This function determines the line under the mouse pointer.
# If the pointer is not above a content line, -1 is returned.
#
proc TextSel_Coo2Line {var xcoo ycoo} {
  upvar #0 $var state

  set wid [lindex $state 0]
  set len_cb [lindex $state 2]

  set pos [$wid index "@$xcoo,$ycoo"]
  if {([scan $pos "%d.%d" line foo] == 2) &&
      ($line >= 1) && ($line - 1 < [$len_cb])} {
    incr line -1
  } else {
    set line -1
  }
  return $line
}


#
# This function has to be called when an item has been inserted into the
# list to adapt the selection: Indices following the insertion are
# incremented.  The new element is not included in the selection.
#
proc TextSel_AdjustInsert {var line} {
  upvar #0 $var state

  set sel {}
  foreach idx [lindex $state 6] {
    if {$idx >= $line} {
      lappend sel [expr {$idx + 1}]
    } else {
      lappend sel $idx
    }
  }
  set anchor [lindex $state 5]
  if {$anchor >= $line} {
    incr anchor 1
  }
  set state [lreplace $state 5 6 $anchor $sel]
}


#
# This function has to be called when an item has been deleted from the
# list (asynchronously, i.e. not via a command related to the selection)
# to adapt the list of selected lines: The deleted line is removed from
# the selection (if included) and following indices are decremented.
#
proc TextSel_AdjustDeletion {var line} {
  upvar #0 $var state

  set sel {}
  foreach idx [lindex $state 6] {
    if {$idx == $line} {
      # skip
    } elseif {$idx > $line} {
      lappend sel [expr {$idx - 1}]
    } else {
      lappend sel $idx
    }
  }
  set anchor [lindex $state 5]
  if {$anchor > $line} {
    incr anchor -1
  }
  set state [lreplace $state 5 6 $anchor $sel]
}


# ----------------------------------------------------------------------------
#
# This function opens the "Loading from STDIN" status dialog.
#
proc OpenLoadPipeDialog {stop} {
  global font_normal dlg_load_shown dlg_load_file_limit
  global load_buf_size load_buf_fill load_file_sum_str load_file_mode load_file_close

  if {![info exists dlg_load_shown]} {
    toplevel .dlg_load
    wm title .dlg_load "Loading from STDIN..."
    wm group .dlg_load .
    wm transient .dlg_load .
    set xcoo [expr {[winfo rootx .f1.t] + 50}]
    set ycoo [expr {[winfo rooty .f1.t] + 50}]
    wm geometry .dlg_load "+${xcoo}+${ycoo}"

    frame .dlg_load.f1
    set row 0
    label .dlg_load.f1.lab_total -text "Loaded data:"
    grid  .dlg_load.f1.lab_total -sticky w -column 0 -row $row
    label .dlg_load.f1.val_total -textvariable load_file_sum_str -font $font_normal
    grid  .dlg_load.f1.val_total -sticky w -column 1 -row $row -columnspan 2
    incr row
    label .dlg_load.f1.lab_bufil -text "Buffered data:"
    grid  .dlg_load.f1.lab_bufil -sticky w -column 0 -row $row
    label .dlg_load.f1.val_bufil -textvariable load_buf_fill -font $font_normal
    grid  .dlg_load.f1.val_bufil -sticky w -column 1 -row $row -columnspan 2
    incr row

    set dlg_load_file_limit [expr {int(($load_buf_size+(1024*1024-1))/(1024*1024))}]
    label .dlg_load.f1.lab_bufsz -text "Buffer size:"
    grid  .dlg_load.f1.lab_bufsz -sticky w -column 0 -row $row
    frame .dlg_load.f1.f11
    spinbox .dlg_load.f1.f11.val_bufsz -from 1 -to 999 -width 4 -textvariable dlg_load_file_limit
    pack  .dlg_load.f1.f11.val_bufsz -side left
    label .dlg_load.f1.f11.lab_bufmb -text {MByte} -font $font_normal
    pack  .dlg_load.f1.f11.lab_bufmb -side left -pady 5
    grid  .dlg_load.f1.f11 -sticky w -column 1 -row $row -columnspan 2
    incr row

    label .dlg_load.f1.lab_mode -text "Mode:"
    grid  .dlg_load.f1.lab_mode -sticky w -column 0 -row $row
    radiobutton .dlg_load.f1.ohead -text "head" -variable load_file_mode -value 0
    grid  .dlg_load.f1.ohead -sticky w -column 1 -row $row
    radiobutton .dlg_load.f1.otail -text "tail" -variable load_file_mode -value 1
    grid  .dlg_load.f1.otail -sticky w -column 2 -row $row
    pack .dlg_load.f1 -side top -padx 5 -pady 5
    incr row

    label .dlg_load.f1.lab_close -text "Close file:"
    grid  .dlg_load.f1.lab_close -sticky w -column 0 -row $row
    checkbutton .dlg_load.f1.val_close -variable load_file_close -text "close after read"
    grid  .dlg_load.f1.val_close -sticky w -column 1 -row $row -columnspan 2
    incr row

    frame .dlg_load.cmd
    button .dlg_load.cmd.stop
    button .dlg_load.cmd.ok -text "Ok"
    pack .dlg_load.cmd.stop .dlg_load.cmd.ok -side left -padx 10
    pack .dlg_load.cmd -side top -pady 5

    set dlg_load_shown 1
    bind .dlg_load.cmd <Destroy> {unset -nocomplain dlg_load_shown}
    wm protocol .dlg_load WM_DELETE_WINDOW {}
  }
  if {$stop} {
    LoadPipe_CmdStop
  } else {
    LoadPipe_CmdContinue 0
  }
}


#
# This function is bound to the "Abort" button in the "Load from pipe"
# dialog (note this button replaces "Stop" while loading is ongoing.)
# The function stops loading and closes the dialog. Note: data that
# already has been loaded is kept and displayed.
#
proc LoadPipe_CmdAbort {} {
  global load_file_complete
  set load_file_complete ""
  destroy .dlg_load
}


#
# This function is bound to the "Stop" button in the "Load from pipe"
# dialog (note this button replaces "Abort" while loading is ongoing.)
# The function temporarily suspends loading to allow the user to change
# settings or abort loading.
#
proc LoadPipe_CmdStop {} {
  # remove the read event handler to suspend loading
  fileevent stdin readable {}

  # switch buttons in the dialog
  .dlg_load.cmd.stop configure -text "Abort" -command LoadPipe_CmdAbort
  .dlg_load.cmd.ok configure -state normal -command {LoadPipe_CmdContinue 1}

  # allow the user to modify settings in the dialog
  grab .dlg_load
}


#
# This function is bound to the "Ok" button in the "Load from pipe"
#
proc LoadPipe_CmdContinue {is_user} {
  global load_file_mode load_buf_fill load_buf_size
  global load_file_complete
  global dlg_load_file_limit

  # apply possible change of buffer limit by the user
  if {[catch {set val [expr {1024*1024*$dlg_load_file_limit}]} cerr] == 0} {
    if {$val != $load_buf_size} {
      set load_buf_size $val
      UpdateRcAfterIdle
    }
  } elseif $is_user {
    after idle {tk_messageBox -type ok -icon error -message "Buffer size is not a number: \"$dlg_load_file_limit\""}
    return
  }

  if {$is_user && ($load_file_mode == 0) && ($load_buf_fill >= $load_buf_size)} {
    # "head" mode confirmed by user and buffer is full -> close the dialog
    set load_file_complete ""
    destroy .dlg_load
  } else {
    .dlg_load.cmd.stop configure -text "Stop" -command LoadPipe_CmdStop
    .dlg_load.cmd.ok configure -state disabled

    # install the read handler again to resume loading data
    fileevent stdin readable LoadDataFromPipe

    # prohibit modifications of settings; allow the "Stop" button only
    grab .dlg_load.cmd.stop
  }
}


#
# This function discards data in the load buffer queue if the length
# limit is exceeded.  The buffer queue is an array of character strings
# (each string the result of a "read" command.)  The function is called
# after each read in tail mode, so it must be efficient (i.e. esp. avoid
# copying large buffers.)
#
proc LoadPipe_LimitData {exact} {
  global load_buf_size load_buf_fill load_file_mode
  global load_file_data

  # tail mode: delete oldest data / head mode: delete newest data
  if {$load_file_mode == 0} {
    set lidx end
  } else {
    set lidx 0
  }

  # calculate how much data must be discarded
  set rest [expr {$load_buf_fill - $load_buf_size}]

  # unhook complete data buffers from the queue
  while {($rest > 0) &&
         ([llength $load_file_data] > 0) &&
         ([string length [lindex $load_file_data $lidx]] <= $rest)} {

    set len [string length [lindex $load_file_data $lidx]]
    set rest [expr {$rest - $len}]
    set load_buf_fill [expr {$load_buf_fill - $len}]
    set load_file_data [lreplace $load_file_data $lidx $lidx]
  }

  # truncate the last data buffer in the queue (only if exact limit is requested)
  if {($rest > 0) && $exact && ([llength $load_file_data] > 0)} {
    set len [string length [lindex $load_file_data $lidx]]
    set data [lindex $load_file_data $lidx]
    if {$load_file_mode == 0} {
      set data [string replace $data [expr {$len - $rest}] end]
    } else {
      set data [string replace $data 0 [expr {$rest - 1}]]
    }
    set load_file_data [lreplace $load_file_data $lidx $lidx $data]
    set load_buf_fill [expr {$load_buf_fill - $rest}]
  }
}


#
# This function is installed as handler for asynchronous read events
# when reading text data from STDIN, i.e. via a pipe.
#
proc LoadDataFromPipe {} {
  global load_buf_size load_buf_fill load_file_mode
  global load_file_sum_hi load_file_sum_lo load_file_sum_str
  global load_file_complete load_file_data load_file_close

  if {[catch {
    # limit read length to buffer size ("head" mode only)
    set size 100000
    if {($load_file_mode == 0) && ($load_buf_fill + $size > $load_buf_size)} {
      set size [expr {$load_buf_size - $load_buf_fill}]
    }
    if {$size > 0} {
      set data [read stdin $size]
      set len [string length $data]
      if {$len > 0} {
        set load_file_sum_hi [expr {(($load_file_sum_lo + $len) >> 20) + $load_file_sum_hi}]
        set load_file_sum_lo [expr {($load_file_sum_lo + $len) & 0xFFFFF}]
        if {($load_file_sum_hi >= 0xFFF) ||
            ((($load_file_sum_hi << 20) | $load_file_sum_hi) > 4 * $load_buf_size)} {
          set load_file_sum_str "$load_file_sum_hi MByte"
        }  else {
          set load_file_sum_str [expr {($load_file_sum_hi << 20) | $load_file_sum_hi}]
        }
        incr load_buf_fill $len
        # data chunk is added to an array (i.e. not a single char string) for efficiency
        lappend load_file_data $data

        # discard oldest data when buffer size limit is exceeded ("tail" mode only)
        if {($load_file_mode != 0) && ($load_buf_fill > $load_buf_size)} {
          LoadPipe_LimitData 0
        }
      } else {
        # end-of-file reached -> stop loading
        set load_file_complete ""
        set load_file_close 1
        fileevent stdin readable {}
        catch {destroy .dlg_load}
      }
    }
  } cerr] != 0} {
    # I/O error
    fileevent stdin readable {}
    set load_file_complete $cerr
    catch {destroy .dlg_load}
  }

  if {![info exists load_file_complete] &&
      ($load_file_mode == 0) && ($load_buf_fill >= $load_buf_size)} {
    OpenLoadPipeDialog 1
  }
}


#
# This function loads a text file from STDIN. A status dialog is opened
# if loading takes longer than a few seconds or if the current buffer
# size is exceeded.
#
proc LoadPipe {} {
  global cur_filename load_file_complete load_file_data
  global load_buf_size load_buf_fill load_file_mode load_file_close
  global load_file_sum_hi load_file_sum_lo load_file_sum_str

  set cur_filename ""
  if {![info exists load_file_mode]}  {
    set load_file_mode 0
  }
  if {![info exists load_file_close]} {
    set load_file_close 1
  }
  if {![info exists load_file_sum_lo]} {
    # split file length in HIGH and LOW (20 bit) as it may exceed 32-bit
    # (newer Tcl versions support 64-bit int, but this still fails when used as -textvariable)
    set load_file_sum_lo 0
    set load_file_sum_hi 0
    set load_file_sum_str 0
  }
  set load_file_data {}
  set load_buf_fill 0
  unset -nocomplain load_file_complete

  set tid_load_dlg [after 1000 OpenLoadPipeDialog 0]
  .f1.t configure -cursor watch

  # install an event handler to read the data asynchronously
  fconfigure stdin -blocking 0
  fileevent stdin readable LoadDataFromPipe

  # block here until all data has been read
  vwait load_file_complete

  if {$load_file_complete eq ""} {
    # success (no read error, although EOF may have been reached)
    # limit content length to the exact maximum (e.g. in case the user changed sizes)
    LoadPipe_LimitData 1

    # insert the data into the text widget
    foreach data $load_file_data {
      .f1.t insert end $data
    }
    if {$load_file_close} {
      catch {close stdin}
    }
  } else {
    tk_messageBox -type ok -icon error -message "Read error on STDIN: $load_file_complete"
    catch {close stdin}
  }
  after cancel $tid_load_dlg
  .f1.t configure -cursor top_left_arrow

  unset load_file_complete load_file_data load_buf_fill

  # finally initiate color highlighting etc.
  InitContent
}


#
# This function loads a text file (or parts of it) into the text widget.
#
proc LoadFile {filename} {
  global cur_filename load_buf_size

  set cur_filename $filename

  if {[catch {
    set file [open $filename r]

    # apply file length limit
    file stat $filename sta
    if {$sta(size) > $load_buf_size} {
      seek $file [expr {0 - $load_buf_size}] end
    }

    # insert the data into the text widget
    .f1.t insert end [read $file $load_buf_size]

    close $file
  } cerr] != 0} {
    tk_messageBox -type ok -icon error -message "Failed to load \"$filename\": $cerr"
  }

  InitContent
}


#
# This function initializes the text widget and control state for a
# newly loaded text.
#
proc InitContent {} {
  global tid_search_inc tid_search_hall tid_high_init
  global cur_filename dlg_mark_shown

  after cancel $tid_high_init
  after cancel $tid_search_inc
  after cancel $tid_search_hall
  set tid_high_init {}
  set tid_search_inc {}
  set tid_search_hall {}

  # window title and main menu
  if {$cur_filename ne ""} {
    wm title . "$cur_filename - Trace browser"
    if {[info exists dlg_mark_shown]} {
      wm title .dlg_mark "Bookmark list [$cur_filename]"
    }
    .menubar.ctrl entryconfigure 1 -state normal -label "Reload current file"
  } else {
    .menubar.ctrl entryconfigure 1 -label "Continue loading STDIN..."
    if {[file channels stdin] ne ""} {
      .menubar.ctrl entryconfigure 1 -state normal
    } else {
      .menubar.ctrl entryconfigure 1 -state disabled
    }
    wm title . "Trace browser"
  }

  # switch from "watch" to default cursor
  .f1.t configure -cursor top_left_arrow
  # set cursor to the end of file
  .f1.t mark set insert "end"
  CursorMoveLine .f1.t 0
  global cur_jump_stack
  set cur_jump_stack {}
  set cur_jump_idx -1
  # read bookmarks from the default file
  Mark_ReadFileAuto
  # start color highlighting in the background
  HighlightInit
}


#
# This procedure discards all text content and aborts all ongoing
# activity and timers. The function is called before new data is
# loaded.
#
proc DiscardContent {} {
  global patlist mark_list mark_list_modified

  # the following is a work-around for a performance issue in the text widget:
  # deleting text with large numbers of tags is extremely slow, so we clear the tags first
  set tag_idx 0
  foreach w $patlist {
    .f1.t tag delete [lindex $w 4]
    incr tag_idx
  }
  # discard the current trace content
  .f1.t delete 1.0 end
  # re-create the color tags
  HighlightCreateTags

  SearchReset

  array unset mark_list
  set mark_list_modified 0
  MarkList_Fill

  SearchList_Init
}


#
# This function is bound to the "Discard content" menu commands.
# The parameter specifies if content above or below the cursor is discarded.
#
proc MenuCmd_Discard {is_fwd} {
  global cur_filename

  if {$is_fwd} {
    # delete everything below the line holding the cursor
    scan [.f1.t index "insert +1 lines linestart"] "%d.%d" first_l first_c
    scan [.f1.t index end] "%d.%d" last_l last_c
    set count [expr {$last_l - $first_l}]
    if {($last_c == 0) && ($count > 0)} {
      incr count -1
    }
    if {$count == 0} {
      tk_messageBox -type ok -default ok -icon error -parent . -message "Already at the bottom"
      return
    }
  } else {
    # delete everything above the line holding the cursor
    set first_l 1
    set first_c 0
    scan [.f1.t index "insert linestart"] "%d.%d" last_l last_c
    set count [expr {$last_l - $first_l}]
    if {$count == 0} {
      tk_messageBox -type ok -default ok -icon error -parent . -message "Already at the top"
      return
    }
  }
  # ask for confirmation, as this cannot be undone
  if {$count > 0} {
    if {$count == 1} {set pl ""} else {set pl "s"}
    scan [.f1.t index end] "%d.%d" end_l end_c
    if {$end_l > 1} {
      if {$end_c == 0} {incr end_l -1}
      set tmp_val [expr {int(100.0*$count/($end_l-1) + 0.5)}]
      set perc " ($tmp_val%)"
    } else {
      set perc ""
    }
    set msg "Please confirm removing $count line$pl$perc"
    if {$cur_filename ne ""} {
      append msg "\n(The file will not be modified)"
    }
    set answer [tk_messageBox -type okcancel -icon question -parent . -message $msg]

    if {$answer eq "ok"} {

      # perform the removal
      .f1.t delete "${first_l}.${first_c}" "${last_l}.${last_c}"

      SearchReset
      global cur_jump_stack cur_jump_idx
      set cur_jump_stack {}
      set cur_jump_idx -1

      MarkList_AdjustLineNums [expr {$is_fwd ? 1 : $last_l}] [expr {$is_fwd ? $first_l : 0}]
      SearchList_AdjustLineNums [expr {$is_fwd ? 1 : $last_l}] [expr {$is_fwd ? $first_l : 0}]
    }
  }
}


#
# This function is bound to the "Reload current file" menu command.
#
proc MenuCmd_Reload {} {
  global cur_filename

  if {$cur_filename ne ""} {
    DiscardContent
    after idle [list LoadFile $cur_filename]
  } else {
    if {[file channels stdin] ne ""} {
      DiscardContent
      after idle [list LoadPipe]
    }
  }
}


#
# This function is bound to the "Load file" menu command.  The function
# allows to specify a file from which a new trace is read. The current browser
# contents are discarded and all bookmarks are cleared.
#
proc MenuCmd_OpenFile {} {
  # offer to save old bookmarks before discarding them below
  Mark_OfferSave

  set filename [tk_getOpenFile -parent . -filetypes {{"trace" {out.*}} {all {*}}}]
  if {$filename ne ""} {
    DiscardContent
    after idle [list LoadFile $filename]
  }
}


#
# This function is installed as callback for destroy requests on the
# main window to store the search history and bookmarks.
#
proc UserQuit {} {
  UpdateRcFile
  Mark_OfferSave
  destroy .
}


#
# This function sets a global flag which makes background tasks sleep
# for a short time so that an interactive task can be completed. It's
# essential that ResumeBgTasks is called afterwards and that the caller
# doesn't block.
#
proc PreemptBgTasks {} {
  global block_bg_tasks block_bg_caller tid_resume_bg

  lappend block_bg_caller LOCK [info level -1]
  if {$tid_resume_bg ne {}} {
    # no incr in this case b/c resume was called, but decr delayed
    set block_bg_tasks 1
    after cancel $tid_resume_bg
    set tid_resume_bg {}
  } else {
    incr block_bg_tasks
  }
}


#
# This function allows background tasks to resume after all pending events
# have been processed.  Note the extra delay via idle and additional timer
# is required to make sure all X events (e.g. from opening a new dialog
# window) have been processed.
#
proc ResumeBgTasks {} {
  global block_bg_tasks block_bg_caller tid_resume_bg

  if {$block_bg_tasks > 1}  {
    lappend block_bg_caller "DEC #$block_bg_tasks" [info level -1]
    incr block_bg_tasks -1
  } else {
    lappend block_bg_caller "UNLOCK" [info level -1]
    after cancel $tid_resume_bg
    set tid_resume_bg [after idle ClearBgTasks 1]
  }
}


#
# This function is installed as idle and timer event to finally allow
# background tasks to resume.  The handler once re-installs itself via
# a timer to make extra-sure all pending activity is done.  Note the
# whole procedure is similar to calling "update" (which is avoided
# though because it inflicts race conditions.)
#
proc ClearBgTasks {flag} {
  global block_bg_tasks tid_resume_bg

  if {$flag} {
    if {$block_bg_tasks == 0}  {
      puts stderr "Warning: nested call of ResumeBgTasks(?) - internal error"
    } else {
      incr block_bg_tasks -1
      unset -nocomplain block_bg_caller
    }
    set tid_resume_bg {}
  } else {
    set tid_resume_bg [after 250 ClearBgTasks 0]
  }
}


#
# This helper function is installed as post command for all menu popups
# so that idle-event driven background tasks are shortly suspended while
# a menu popup is displayed. Without this the GUI may freeze until the
# background task has finished.
#
proc MenuPosted {} {
  PreemptBgTasks
  ResumeBgTasks
}


#
# Debug only: This function dumps all global variables and their content
# to STDERR. Additionally info about all running tasks is printed.
#
proc DebugDumpAllState {} {
  puts stderr "#--- debug dump of scalars and lists ---#"
  foreach var [lsort [info globals]] {
    upvar #0 $var l
    if {![array exists "::$var"]} {
      # limit list output length
      puts stderr [string range "$var ##$l##" 0 10000]
    }
  }
  puts stderr "#--- debug dump of tasks ---#"
  foreach id [lsort [after info]] {
    if {[catch {puts stderr "$id [after info $id]"} err] != 0} {
      puts stderr "$id: $err"
    }
  }
}


# ----------------------------------------------------------------------------
#
# This functions reads configuration variables from the rc file.
# The function is called once during start-up.
#
proc LoadRcFile {} {
  global tlb_history tlb_hist_maxlen tlb_case tlb_regexp tlb_hall
  global dlg_mark_geom dlg_hist_geom dlg_srch_geom dlg_tags_geom main_win_geom
  global patlist col_palette tick_pat_sep tick_pat_num tick_str_prefix
  global font_content col_bg_content col_fg_content fmt_find fmt_findinc
  global load_buf_size
  global rcfile_version myrcfile

  set error 0
  set ver_check 0
  set line_no 0

  if {[catch {set rcfile [open $myrcfile "r"]} errmsg] == 0} {
    while {[gets $rcfile line] >= 0} {
      incr line_no
      if {[string compare $line "___END___"] == 0} {
        break;
      } elseif {([catch $line] != 0) && !$error} {
        tk_messageBox -type ok -default ok -icon error \
                      -message "Syntax error in rc file, line #$line_no: $line"
        set error 1

      } elseif {$ver_check == 0} {
        # check if the given rc file is from a newer version
        if {[info exists rc_compat_version] && [info exists rcfile_version]} {
          if {$rc_compat_version > $rcfile_version} {
            tk_messageBox -type ok -default ok -icon error \
               -message "rc file '$myrcfile' is from an incompatible, newer browser version ($rcfile_version) and cannot be loaded."

            # change name of rc file so that the newer one isn't overwritten
            append myrcfile "." $rcfile_version
            # abort loading further data (would overwrite valid defaults)
            return
          }
          set ver_check 1
        }
      }
    }
    close $rcfile
  }

  # override config var with command line options
  global load_buf_size_opt
  if {[info exists load_buf_size_opt]} {
    set load_buf_size $load_buf_size_opt
  }
}

#
# This functions writes configuration variables into the rc file
#
proc UpdateRcFile {} {
  global argv0 myrcfile rcfile_compat rcfile_version
  global tid_update_rc_sec tid_update_rc_min rc_file_error
  global tlb_history tlb_hist_maxlen tlb_case tlb_regexp tlb_hall
  global dlg_mark_geom dlg_hist_geom dlg_srch_geom dlg_tags_geom main_win_geom
  global patlist col_palette tick_pat_sep tick_pat_num tick_str_prefix
  global font_content col_bg_content col_fg_content fmt_find fmt_findinc
  global fmt_selection load_buf_size

  after cancel $tid_update_rc_sec
  after cancel $tid_update_rc_min
  set tid_update_rc_min {}

  expr {srand([clock clicks -milliseconds])}
  append tmpfile $myrcfile "." [expr {int(rand() * 1000000)}] ".tmp"

  if {[catch {set rcfile [open $tmpfile "w"]} errstr] == 0} {
    if {[catch {
      puts $rcfile "#"
      puts $rcfile "# trowser configuration file"
      puts $rcfile "#"
      puts $rcfile "# This file is automatically generated - do not edit"
      puts $rcfile "# Written at: [clock format [clock seconds] -format %c]"
      puts $rcfile "#"

      # dump software version
      puts $rcfile [list set rcfile_version $rcfile_version]
      puts $rcfile [list set rc_compat_version $rcfile_compat]
      puts $rcfile [list set rc_timestamp [clock seconds]]

      # dump highlighting patterns
      puts $rcfile [list set patlist {}]
      foreach val $patlist {
        puts $rcfile [list lappend patlist $val]
      }

      # dump color palette
      puts $rcfile [list set col_palette {}]
      foreach val $col_palette {
        puts $rcfile [list lappend col_palette $val]
      }

      # frame number parser patterns
      puts $rcfile [list set tick_pat_sep $tick_pat_sep]
      puts $rcfile [list set tick_pat_num $tick_pat_num]
      puts $rcfile [list set tick_str_prefix $tick_str_prefix]

      # dump search history
      # (renamed from "tlb_hist" in v1.3 due to format change)
      puts $rcfile [list set tlb_history {}]
      foreach val $tlb_history {
        puts $rcfile [list lappend tlb_history $val]
      }

      # dump search settings
      puts $rcfile [list set tlb_case $tlb_case]
      puts $rcfile [list set tlb_regexp $tlb_regexp]
      puts $rcfile [list set tlb_hall $tlb_hall]
      puts $rcfile [list set tlb_hist_maxlen $tlb_hist_maxlen]

      # dialog sizes
      puts $rcfile [list set dlg_mark_geom $dlg_mark_geom]
      puts $rcfile [list set dlg_hist_geom $dlg_hist_geom]
      puts $rcfile [list set dlg_srch_geom $dlg_srch_geom]
      puts $rcfile [list set dlg_tags_geom $dlg_tags_geom]
      puts $rcfile [list set main_win_geom $main_win_geom]

      # font and color settings
      puts $rcfile [list set font_content $font_content]
      puts $rcfile [list set col_bg_content $col_bg_content]
      puts $rcfile [list set col_fg_content $col_fg_content]
      puts $rcfile [list set fmt_find $fmt_find]
      puts $rcfile [list set fmt_findinc $fmt_findinc]
      puts $rcfile [list set fmt_selection $fmt_selection]

      # misc (note the head/tail mode is omitted intentionally)
      puts $rcfile [list set load_buf_size $load_buf_size]

      close $rcfile
    } errstr] == 0} {
      # copy attributes on the new file
      if {[catch {set att_perm [file attributes $myrcfile -permissions]}] == 0} {
        catch {file attributes $tmpfile -permissions $att_perm}
      }
      if {[catch {set att_grp [file attributes $myrcfile -group]}] == 0} {
        catch {file attributes $tmpfile -group $att_grp}
      }
      # move the new file over the old one
      if {[catch {file rename -force $tmpfile $myrcfile} errstr] != 0} {
        if {![info exists rc_file_error]} {
          tk_messageBox -type ok -default ok -icon error \
                        -message "Could not replace rc file $myrcfile: $errstr"
          set rc_file_error 1
        }
      } else {
        unset -nocomplain rc_file_error
      }
    } else {
      # write error - remove the file fragment, report to user
      catch {file delete $tmpfile}
      if {![info exists rc_file_error]} {
        tk_messageBox -type ok -default ok -icon error \
                      -message "Write error in file $myrcfile: $errstr"
        set rc_file_error 1
      }
    }

  } else {
    if {![info exists rc_file_error]} {
      tk_messageBox -type ok -default ok -icon error \
                    -message "Could not create temporary rc file $tmpfile: $errstr"
      set rc_file_error 1
    }
  }
}


#
# This function is used to trigger writing the RC file after changes.
# The write is delayed by a few seconds to avoid writing the file multiple
# times when multiple values are changed. This timer is restarted when
# another change occurs during the delay, however only up to a limit.
#
proc UpdateRcAfterIdle {} {
  global tid_update_rc_sec tid_update_rc_min

  after cancel $tid_update_rc_sec
  set tid_update_rc_sec [after 3000 UpdateRcFile]

  if {$tid_update_rc_min eq ""} {
    set tid_update_rc_min [after 60000 UpdateRcFile]
  }
}


# ----------------------------------------------------------------------------
#
# This function is called when the program is started with -help to list all
# possible command line options.
#
proc PrintUsage {argvn reason} {
  global argv0

  if {$argvn ne ""} {
    puts stderr "$argv0: $reason: $argvn"
  }

  puts stderr "Usage: $argv0 \[options\] {file|-}"

  if {$argvn ne ""} {
    puts "Use -\? or --help for a list of options"
  } else {
    puts stderr "The following options are available:"
    puts stderr "  --head=size\t\tLoad <size> bytes from the start of the file"
    puts stderr "  --tail=size\t\tLoad <size> bytes from the end of the file"
    puts stderr "  --rcfile=<path>\tUse alternate config file (default: ~/.trowserc)"
  }

  exit
}


#
# This helper function checks if a command line flag which requires an
# argument is followed by at least another word on the command line.
#
proc ParseArgvLenCheck {argv arg_idx} {
  if {$arg_idx + 1 >= [llength $argv]} {
    PrintUsage [lindex $argv $arg_idx] "this option requires an argument"
  }
}


#
# This helper function reads an integer value from a command line parameter
#
proc ParseArgInt {opt val var} {
  upvar $var assign

  if {([catch {expr {$val <= 0}}] == 0) && ($val > 0)} {
    set assign $val
  } else {
    PrintUsage $opt "\"$val\" is not a valid value for option"
  }
}


#
# This function parses and evaluates the command line arguments.
#
proc ParseArgv {} {
  global argv load_file_mode load_buf_size_opt myrcfile

  set file_seen 0
  for {set arg_idx 0} {$arg_idx < [llength $argv]} {incr arg_idx} {
    set arg [lindex $argv $arg_idx]

    if {[string match "-?*" $arg]} {
      switch -regexp -- $arg {
        {^-t$} {
          ParseArgvLenCheck $argv $arg_idx
          incr arg_idx
          ParseArgInt $arg [lindex $argv $arg_idx] load_buf_size_opt
          set load_file_mode 1
        }
        {^--tail.*$} {
          if {[regexp -all -- {--tail=(\d+)} $arg foo val]} {
            ParseArgInt $arg $val load_buf_size_opt
          } else {
            PrintUsage $arg "requires a numerical argument (e.g. $arg=10000000)"
          }
          set load_file_mode 1
        }
        {^-h$} {
          ParseArgvLenCheck $argv $arg_idx
          incr arg_idx
          ParseArgInt $arg [lindex $argv $arg_idx] load_buf_size_opt
          set load_file_mode 1
        }
        {^--head.*$} {
          if {[regexp -all -- {--head=(\d+)} $arg foo val]} {
            ParseArgInt $arg $val load_buf_size_opt
          } else {
            PrintUsage $arg "requires a numerical argument (e.g. $arg=10000000)"
          }
          set load_file_mode 0
        }
        {^-r$} {
          if {$arg_idx + 1 < [llength $argv]} {
            incr arg_idx
            set myrcfile [lindex $argv $arg_idx]
          } else {
            PrintUsage $arg "this option requires an argument"
          }
        }
        {^--rcfile.*$} {
          if {[regexp -all -- {--rcfile=(.+)} $arg foo val]} {
            set myrcfile $val
          } else {
            PrintUsage $arg "requires a path argument (e.g. --rcfile=foo/bar)"
          }
        }
        {^-\?$} -
        {^--help$} {
          PrintUsage {} {}
        }
        default {
          PrintUsage $arg "unknown option"
        }
      }
    } else {
      if {$arg_idx + 1 == [llength $argv]} {
        set file_seen 1
      } else {
        incr arg_idx
        PrintUsage [lindex $argv $arg_idx] "only one file name expected"
      }
    }
  }
  if {!$file_seen} {
    puts stderr "File name missing (use \"-\" for stdin)"
    PrintUsage {} {}
  }
}


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
set tlb_find {}

# This variable is a cache of the search string for which the last
# "highlight all" color highlighting was done. It's used to avoid unnecessarily
# repeating the search when the string is unchanged, because the search can
# take some time. The variable is empty when no highlights are shown.
set tlb_last_hall {}

# This variable contains the stack of previously used search expressions
# The top of the stack, aka the most recently used expression, is at the
# front of the list. Each element is a list with the following elements:
# 0: sub-string or regular expression
# 1: reg.exp. yes/no:=1/0
# 2: match case yes/no:=1/0
# 3: timestamp of last use
set tlb_history {}

# This variable defines the maximum length of the search history list.
# This configuration option currently can only be set here.
set tlb_hist_maxlen 50

# These variables contain search options which can be set by checkbuttons.
set tlb_case 1
set tlb_regexp 1
set tlb_hall 1

# This variable stores the search direction: 0:=backwards, 1:=forwards
set tlb_last_dir 1

# This variable indicates if the search entry field has keyboard focus.
set tlb_find_focus 0

# This variable contains the name of the widget which had input focus before
# the focus was moved into the search entry field. Focus will return there
# after Return or Escape
set tlb_last_wid {}

# These variables hold the cursor position and Y-view from before the start of an
# incremental search (they are used to move the cursor back to the start position)
#unset tlb_inc_base
#unset tlb_inc_view

# These variables are used when cycling through the search history via the up/down
# keys in the search text entry field. They hold the current index in the history
# stack and the prefix string (i.e. the text in the entry field from before
# opening the history.)
#unset tlb_hist_pos
#unset tlb_hist_prefix

# This variable is used to parse multi-key command sequences.
set last_key_char {}

# This list remembers cursor positions preceding "large jumps" (i.e. searches,
# or positioning commands "G", "H", "L", "M" etc.) Used to allow jumping back.
# The second variable is used when jumping back and forward inside the list.
set cur_jump_stack {}
set cur_jump_idx -1

# This hash array stores the bookmark list. Its indices are text line numbers,
# the values are the bookmark text (i.e. initially a copy of the text line)
array set mark_list {}

# This variable tracks if the marker list was changed since the last save.
# It's used to offer automatic save upon quit.
set mark_list_modified 0

# These variables are used by the bookmark list dialog.
#set dlg_mark_list {}
#unset dlg_mark_shown

# These variables hold IDs of timers and background tasks (i.e. scripts delayed
# by "after")  They are used to cancel the scripts when necessary.
set tid_search_inc {}
set tid_search_list {}
set tid_search_hall {}
set tid_high_init {}
set tid_update_rc_sec {}
set tid_update_rc_min {}
set tid_status_line {}
set tid_resume_bg {}

# This variable is set to 1 to temporarily suspend background tasks while an
# interactive operation is performed (e.g. a dialog window is opened.)
set block_bg_tasks 0

# These variables hold the font and color definitions for the main text content.
set font_content {helvetica 9 normal}
set col_bg_content {#e2e2e8}
set col_fg_content {#000000}

# These variables hold the markup definitions for search match highlighting
# and selected text in the main window and dialogs. (Note the selection mark-up
# has lower precedence than color highlighting for mark-up types which are
# used in both. Search highlighting in contrary has precedence over all others.)
# The structure is the same as in "patlist" (except for the elements which
# relate to search pattern and options, which are unused and undefined here)
set fmt_find {{} 0 0 {} {} {} #faee0a {} 0 0 0 {} {} {} 1 0}
set fmt_findinc {{} 0 0 {} {} {} #c8ff00 {} 0 0 0 {} {} {} 1 0}
set fmt_selection {{} 0 0 {} {} {} #c3c3c3 {} 0 0 0 gray50 {} raised 2 0}

# These variables define the initial geometry of the main and dialog windows.
set main_win_geom "684x480"
set dlg_mark_geom "500x250"
set dlg_tags_geom "400x300"
set dlg_hist_geom "400x250"
set dlg_srch_geom "648x250"

# This list contains pre-defined colors values for color-highlighting.
set col_palette [list \
  {#000000} \
  {#4acbb5} \
  {#94ff80} \
  {#b4e79c} \
  {#bee1be} \
  {#bfffb3} \
  {#b3beff} \
  {#96d9ff} \
  {#b3fff3} \
  {#ccffff} \
  {#dab3d9} \
  {#dab3ff} \
  {#c180ff} \
  {#e7b3ff} \
  {#e6b3ff} \
  {#e6ccff} \
  {#e6b3d9} \
  {#e73c39} \
  {#ff6600} \
  {#ff7342} \
  {#ffb439} \
  {#efbf80} \
  {#ffbf80} \
  {#ffd9b3} \
  {#f0b0a0} \
  {#ffb3be} \
  {#e9ff80} \
  {#f2ffb3} \
  {#eeee8e} \
  {#ffffff}]

# These variables hold patterns which are used to parse timestamps out of
# the text content: the first two are regular expressions, the third a
# plain string; set patterns to empty strings to disable the feature
set tick_pat_sep {}
set tick_pat_num {}
set tick_str_prefix ""

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
set patlist {
  {{----} 0 1 default tag0 {} #000000 #FFFFFF 1 0 0 {} {} {} 1 0}
  {{^ *#} 1 1 default tag1 {} {} #008800 1 0 0 {} {} {} 1 0}
}

# This variable contains the limit for file load
# The value can be changed by the "head" and "tail" command line options.
set load_buf_size 2000000

# define RC file version limit for forwards compatibility
set rcfile_compat 0x01000000
set rcfile_version 0x01030000
set myrcfile "~/.trowserc"

#
# Main
#
if {[catch {tk appname "trowser"}]} {
  # this error occurs when the display connection is refused etc.
  puts stderr "Tk initialization failed"
  exit
}
ParseArgv
LoadRcFile
InitResources
CreateMainWindow
HighlightCreateTags
update

if {[lindex $argv end] eq "-"} {
  LoadPipe
} else {
  LoadFile [lindex $argv end]
}

# done - all following actions are event-driven
# the application exits when the main window is closed
