#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

# ------------------------------------------------------------------------ #
# Copyright (C) 2007 Thorsten Zoerner. All rights reserved.
# ------------------------------------------------------------------------ #
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License Version 2 as
#  published by the Free Software Foundation.
#
#  THIS PROGRAM IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL,
#  BUT WITHOUT ANY WARRANTY; WITHOUT EVEN THE IMPLIED WARRANTY OF
#  MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# ------------------------------------------------------------------------ #
#  Revision Information :
#     File name: trowser.tcl
#     Version:   /main/gsm/gprs/0
#     Date:   new
# ------------------------------------------------------------------------ #
#
# DESCRIPTION:  Tcl/Tk script to browse Layer 1 hosttest trace files.      #
#
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
  bind TextWheel    <Button-4>     {%W yview scroll -3 units}
  bind TextWheel    <Button-5>     {%W yview scroll 3 units}
  bind TextWheel    <MouseWheel>   {%W yview scroll [expr {- (%D / 120) * 3}] units}

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
  foreach event {<Button-4> <Button-5> <MouseWheel>} {
    bind TextReadOnly $event [bind TextWheel $event]
  }
  bind TextReadOnly <Control-Key-c> [bind Text <<Copy>>]
  bind TextReadOnly <Key-Tab> [bind Text <Control-Key-Tab>]

  # bookmark image which is inserted into the text widget
  global img_marker
  set img_marker [image create photo -data R0lGODlhBwAHAMIAAAAAuPj8+Hh8+JiYmDAw+AAAAAAAAAAAACH5BAEAAAEALAAAAAAHAAcAAAMUGDGsSwSMJ0RkpEIG4F2d5DBTkAAAOw==]

  # image for drop-down menu copied from combobox.tcl by Bryan Oakley
  image create bitmap img_dropdown -data \
    "#define down_arrow_width 15\n#define down_arrow_height 15\n
    static char down_arrow_bits[] = {
    0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x80,
    0x00,0x80,0xf8,0x8f,0xf0,0x87,0xe0,0x83,
    0xc0,0x81,0x80,0x80,0x00,0x80,0x00,0x80,
    0x00,0x80,0x00,0x80,0x00,0x80};"

  image create bitmap img_down -data \
    "#define ptr_down_width 16\n#define ptr_down_height 14\n
    static unsigned char ptr_down_bits[] = {
    0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,
    0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,
    0xc0,0x01,0xf8,0x0f,0xf0,0x07,0xe0,0x03,
    0xc0,0x01,0x80,0x00};"

  image create bitmap img_up -data \
    "#define ptr_up_width 16\n#define ptr_up_height 14\n
    static unsigned char ptr_up_bits[] = {
    0x80,0x00,0xc0,0x01,0xe0,0x03,0xf0,0x07,
    0xf8,0x0f,0xc0,0x01,0xc0,0x01,0xc0,0x01,
    0xc0,0x01,0xc0,0x01,0xc0,0x01,0xc0,0x01,
    0xc0,0x01,0xc0,0x01};"
}


#
# This function creates the main window of the trace browser.
#
proc CreateMainWindow {} {
  global font_content col_bg_content col_fg_content main_win_geom
  global col_bg_find col_bg_findinc
  global tlb_find tlb_hall tlb_case tlb_regexp

  # menubar at the top of the window
  menu .menubar -relief raised
  . config -menu .menubar
  .menubar add cascade -label "Control" -menu .menubar.ctrl -underline 0
  .menubar add cascade -label "Bookmarks" -menu .menubar.mark -underline 0
  .menubar add cascade -label "Help" -menu .menubar.help -underline 0
  menu .menubar.ctrl -tearoff 0
  .menubar.ctrl add command -label "Open file..." -command MenuCmd_OpenFile
  .menubar.ctrl add command -label "Reload current file" -command MenuCmd_Reload
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Read bookmarks from file..." -command Mark_ReadFileFrom
  .menubar.ctrl add command -label "Save bookmarks to file..." -command Mark_SaveFileAs
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Edit color highlighting..." -command Tags_OpenDialog
  .menubar.ctrl add command -label "Font selection..." -command FontList_OpenDialog
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Quit" -command {destroy .; update}
  menu .menubar.mark -tearoff 0
  .menubar.mark add command -label "Toggle bookmark" -accelerator "m" -command Mark_ToggleAtInsert
  .menubar.mark add command -label "List bookmarks" -command MarkList_OpenDialog
  .menubar.mark add command -label "Delete all bookmarks" -command Mark_DeleteAll
  .menubar.mark add separator
  .menubar.mark add command -label "Goto line..." -command {KeyCmd_OpenDialog goto}
  menu .menubar.help -tearoff 0
  .menubar.help add command -label "About" -command OpenAboutDialog

  # frame #1: text widget and scrollbar
  frame .f1
  text .f1.t -width 1 -height 1 -wrap none -undo 0 \
          -font $font_content -background $col_bg_content -foreground $col_fg_content \
          -cursor top_left_arrow -relief flat -exportselection false \
          -yscrollcommand {.f1.sb set}
  pack .f1.t -side left -fill both -expand 1
  scrollbar .f1.sb -orient vertical -command {.f1.t yview} -takefocus 0
  pack .f1.sb -side left -fill y
  pack .f1 -side top -fill both -expand 1

  focus .f1.t
  # note: order is important: find must be "lowest" for tags created by color highlighting
  .f1.t tag configure find -background $col_bg_find
  .f1.t tag configure findinc -background $col_bg_findinc
  .f1.t tag configure margin -lmargin1 17
  .f1.t tag configure bookmark -lmargin1 0
  .f1.t tag configure sel -bgstipple gray50
  .f1.t tag lower sel
  bindtags .f1.t {.f1.t TextReadOnly . all}
  # Ctrl+cursor and Ctrl-E/Y allow to shift the view up/down
  bind .f1.t <Control-Up> {YviewScroll -1; KeyClr; break}
  bind .f1.t <Control-Down> {YviewScroll 1; KeyClr; break}
  bind .f1.t <Control-e> {YviewScroll 1; KeyClr; break}
  bind .f1.t <Control-y> {YviewScroll -1; KeyClr; break}
  bind .f1.t <Control-f> {event generate %W <Key-Next>; KeyClr; break}
  bind .f1.t <Control-b> {event generate %W <Key-Prior>; KeyClr; break}
  bind .f1.t <Key-H> {CursorSetLine top; KeyClr; break}
  bind .f1.t <Key-M> {CursorSetLine center; KeyClr; break}
  bind .f1.t <Key-L> {CursorSetLine bottom; KeyClr; break}
  # goto line or column
  bind .f1.t <G> {.f1.t mark set insert end; .f1.t see insert; KeyClr; break}
  bind .f1.t <Key-dollar> {.f1.t mark set insert "insert lineend"; .f1.t see insert; KeyClr; break}
  bind .f1.t <Control-g> {DisplayLineNumer; KeyClr; break}
  # search with "/", "?"; repeat search with n/N
  bind .f1.t <Key-slash> {set tlb_last_dir 1; focus .f2.e; KeyClr; break}
  bind .f1.t <Key-question> {set tlb_last_dir 0; focus .f2.e; KeyClr; break}
  bind .f1.t <Key-n> {Search 1 0; KeyClr; break}
  bind .f1.t <Key-p> {Search 0 0; KeyClr; break}
  bind .f1.t <Key-N> {Search 0 0; KeyClr; break}
  bind .f1.t <Key-asterisk> {SearchWord 1; KeyClr; break}
  bind .f1.t <Key-numbersign> {SearchWord 0; KeyClr; break}
  bind .f1.t <Key-ampersand> {SearchHighlightClear; KeyClr; break}
  bind .f1.t <Alt-Key-f> {focus .f2.e; KeyClr; break}
  bind .f1.t <Alt-Key-n> {Search 1 0; KeyClr; break}
  bind .f1.t <Alt-Key-p> {Search 0 0; KeyClr; break}
  bind .f1.t <Alt-Key-h> {SearchHighlightOnOff; KeyClr; break}
  # bookmarks
  bind .f1.t <Double-Button-1> {Mark_ToggleAtInsert; KeyClr; break}
  bind .f1.t <Key-m> {Mark_ToggleAtInsert; KeyClr; break}
  # catch-all
  bind .f1.t <FocusIn> {KeyClr}
  bind .f1.t <Return> {if {[KeyCmd return]} break}
  bind .f1.t <KeyPress> {if {[KeyCmd %A]} break}

  # frame #2: search controls
  frame .f2 -borderwidth 2 -relief raised
  label .f2.l -text "Find:" -underline 0
  entry .f2.e -width 20 -textvariable tlb_find -exportselection false
  menu .f2.mh -tearoff 0
  button .f2.bn -text "Find next" -command {Search 1 0} -underline 5 -pady 2
  button .f2.bp -text "Find previous" -command {Search 0 0} -underline 5 -pady 2
  checkbutton .f2.bh -text "Highlight all" -variable tlb_hall -command SearchHighlightSettingChange -underline 0
  checkbutton .f2.cb -text "Match case" -variable tlb_case -command SearchHighlightSettingChange
  checkbutton .f2.re -text "Reg.Exp." -variable tlb_regexp -command SearchHighlightSettingChange
  pack .f2.l .f2.e .f2.bn .f2.bp .f2.bh .f2.cb .f2.re -side left -anchor w
  pack configure .f2.e -fill x -expand 1
  pack .f2 -side top -fill x

  bind .f2.e <Escape> {SearchAbort; break}
  bind .f2.e <Return> {SearchReturn; break}
  bind .f2.e <FocusIn> {SearchInit}
  bind .f2.e <FocusOut> {SearchLeave}
  bind .f2.e <Control-n> {Search 1 0; break}
  bind .f2.e <Control-N> {Search 0 0; break}
  bind .f2.e <Key-Up> {Search_BrowseHistory 1; break}
  bind .f2.e <Key-Down> {Search_BrowseHistory 0; break}
  bind .f2.e <Control-d> {Search_Complete; break}
  bind .f2.e <Control-D> {Search_CompleteLeft; break}
  bind .f2.e <Control-x> {Search_RemoveFromHistory; break}
  bind .f2.e <Control-c> {SearchAbort; break}
  bind .f2.e <Control-h> {TagsList_AddSearch .; break}
  trace add variable tlb_find write SearchVarTrace

  wm protocol . WM_DELETE_WINDOW UserQuit
  wm geometry . $main_win_geom
  bind .f1.t <Configure> {TestCaseList_Resize %W . .f1.t main_win_geom}
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
# highlighting to the complete text. Since this can take some time,
# the operation done separately for each pattern with a 10ms pause
# in-between to allow user-interaction. Additionally, a progress bar
# is shown.
#
proc HighlightInit {} {
  global patlist tid_high_init

  if {[info commands .hipro] ne ""} {
    destroy .hipro
  }

  if {[llength $patlist] > 0} {
    toplevel .hipro -takefocus 0 -relief sunken -borderwidth 2
    wm transient .hipro .
    wm geometry .hipro "+[expr [winfo rootx .f1.t] + 1]+[expr [winfo rooty .f1.t] + 1]"

    canvas .hipro.c -width 100 -height 10 -highlightthickness 0 -takefocus 0
    pack .hipro.c
    set cid [.hipro.c create rect 0 0 0 12 -fill {#0b1ff7} -outline {}]

    .f1.t tag add margin 1.0 end

    foreach w $patlist {
      HighlightVisible [lindex $w 0] [lindex $w 4] {}
    }

    # trigger highlighting for the 1st and following patterns
    set tid_high_init [after 10 HighlightInitBg 0 $cid 0]
    .f1.t configure -cursor watch
  }
}


#
# This function is a slave-function of proc HighlightInit. The function
# loops across all patterns to apply color highlights. The loop is broken
# up by means of a 10ms timer.
#
proc HighlightInitBg {pat_idx cid line} {
  global patlist tid_high_init

  if {$pat_idx < [llength $patlist]} {
    set w [lindex $patlist $pat_idx]
    set tagnam [lindex $w 4]
    set opt [Search_GetOptions [lindex $w 1] [lindex $w 2]]

    # apply the tag to all matching lines of text
    set line [HighlightLines [lindex $w 0] $tagnam $opt $line]
    if {$line >= 0} {
      # not done yet - reschedule
      set tid_high_init [after idle [list HighlightInitBg $pat_idx $cid $line]]

    } else {
      # trigger next tag
      incr pat_idx
      set tid_high_init [after 10 [list HighlightInitBg $pat_idx $cid 1]]

      # update the progress bar
      catch {.hipro.c coords $cid 0 0 [expr int(100*$pat_idx/[llength $patlist])] 12}
    }
  } else {
    catch {destroy .hipro}
    .f1.t configure -cursor top_left_arrow
  }
}


#
# This function searches for all text lines which contain the given
# sub-string and marks these lines with the given tag.
#
proc HighlightLines {pat tagnam opt line} {
  set pos [.f1.t index end]
  scan $pos "%d.%d" max_line char
  set start_t [clock clicks -milliseconds]
  while {($line < $max_line) &&
         ([set pos [eval .f1.t search $opt -- {$pat} "$line.0" end]] ne {})} {
    scan $pos "%d.%d" line char
    .f1.t tag add $tagnam "$line.0" "[expr $line + 1].0"
    incr line

    if {([clock clicks -milliseconds] >= $start_t + 100) && ($line < $max_line)} {
      return $line
    }
  }
  return -1
}


#
# This helper function calls the line highlight function until highlighting
# is complete.  It's used to add highlighting for single tags and for the
# search highlighting.
#
proc HighlightAll {pat tagnam opt {line 1}} {
  set line [HighlightLines $pat $tagnam $opt $line]
  if {$line >= 0} {
    after idle [list HighlightAll $pat $tagnam $opt $line]
  } else {
    .f1.t configure -cursor top_left_arrow
  }
}


#
# This function searches for all text lines which contain the given
# sub-string and marks these lines with the given tag.
#
proc HighlightVisible {pat tagnam opt} {
  set start_pos [.f1.t index {@1,1}]
  set end_pos [.f1.t index "@[expr [winfo width .f1.t] - 1],[expr [winfo height .f1.t] - 1]"]
  scan $start_pos "%d.%d" line char
  scan $end_pos "%d.%d" max_line char
  #puts "visible $start_pos...$end_pos: $pat $opt"

  while {($line < $max_line) &&
         ([set pos [eval .f1.t search $opt -- {$pat} "$line.0" end]] ne {})} {
    scan $pos "%d.%d" line char
    .f1.t tag add $tagnam "$line.0" "[expr $line + 1].0"
    incr line
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
  if [lindex $w 8] {
    lappend cfg -font [DeriveFont $font_content 0 bold]
  } else {
    lappend cfg -font {}
  }
  if [lindex $w 9] {
    lappend cfg -underline [lindex $w 9]
  } else {
    lappend cfg -underline {}
  }
  if [lindex $w 10] {
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

  .f1.t tag remove find 1.0 end
  .f1.t tag remove findinc 1.0 end
  set tlb_last_hall {}
}


#
# This function triggers color highlighting of all lines of text which match
# the current search string.  The function is called when global highlighting
# is en-/disabled, when the search string is modified or when search options
# are changed.
#
proc SearchHighlightUpdate {} {
  global tlb_find tlb_regexp tlb_case tlb_hall tlb_last_hall
  global tid_search_hall

  if {$tlb_find ne ""} {
    if $tlb_hall {
      after cancel $tid_search_hall

      if {$tlb_last_hall ne $tlb_find} {
        if [SearchExprCheck] {
          set opt [Search_GetOptions $tlb_regexp $tlb_case]

          HighlightVisible $tlb_find find $opt

          if {[focus -displayof .] ne ".f2.e"} {

            # display "busy" cursor until highlighting is finished
            .f1.t configure -cursor watch

            # start highlighting in the background
            set tid_search_hall [after 10 [list HighlightAll $tlb_find find $opt]]

            set tlb_last_hall $tlb_find
          }
        }
      }
    } else {
      SearchHighlightClear
    }
  }
}


#
# This function is bound to the "Highlight all" checkbutton to en- or
# disable global highlighting.
#
proc SearchHighlightOnOff {} {
  global tlb_hall

  set tlb_hall [expr !$tlb_hall]
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
  if $tlb_hall {
    SearchHighlightUpdate
  }
}


#
# This is the main search function which is invoked when the user
# enters text in the "find" entry field or repeats a previous search.
#
proc Search {is_fwd is_changed {start_pos {}}} {
  global tlb_find tlb_hall tlb_case tlb_regexp tlb_last_hall tlb_last_dir

  set found 0
  if {($tlb_find ne "") && [SearchExprCheck]} {

    set tlb_last_dir $is_fwd
    set search_opt [Search_GetOptions $tlb_regexp $tlb_case $tlb_last_dir]
    if {$start_pos eq {}} {
      set start_pos [Search_GetBase $is_fwd 0]
      Mark_JumpPos
    }
    if $is_fwd {
      set search_range [list $start_pos end]
    } else {
      set search_range [list $start_pos "0.0"]
    }

    set pos [eval .f1.t search $search_opt -count match_len -- {$tlb_find} $search_range]
    if {($pos ne "") || $is_changed} {
      if {!$tlb_hall || ($tlb_last_hall ne $tlb_find)} {
        SearchHighlightClear
      } else {
        .f1.t tag remove findinc 1.0 end
      }
    }
    if {$pos ne ""} {
      scan $pos "%d.%d" tlb_find_line char
      .f1.t see $pos
      .f1.t mark set insert $pos
      .f1.t tag add findinc $pos "$pos + $match_len chars"
      .f1.t tag add find "$tlb_find_line.0" "[expr $tlb_find_line + 1].0"
      set found 1
    }
    if $tlb_hall {
      SearchHighlightUpdate
    }

  } else {
    SearchReset
  }
  return $found
}


#
# This function is bound to all changes of the search text in the
# "find" entry field. It's called when the user enters new text and
# triggers an incremental search.
#
proc SearchVarTrace {name1 name2 op} {
  global tid_search_inc tid_search_hall

  after cancel $tid_search_inc
  after cancel $tid_search_hall

  set tid_search_inc [after 10 SearchIncrement]
}


#
# This function performs an so-called "incremental" search after the user
# has modified the search text. This means searches are started already
# while the user is typing.
#
proc SearchIncrement {} {
  global tlb_find tlb_last_dir tlb_inc_base tlb_hist tlb_hist_pos tlb_hist_base

  if {[focus -displayof .] eq ".f2.e"} {
    if {($tlb_find ne {}) &&
        ([catch {regexp -- $tlb_find ""}] == 0)} {

      if {![info exists tlb_inc_base]} {
        set tlb_inc_base [Search_GetBase $tlb_last_dir 1]
        Mark_JumpPos
      }

      set found [Search $tlb_last_dir 1 $tlb_inc_base]

      if {($found == 0) && [info exists tlb_inc_base]} {
        .f1.t mark set insert $tlb_inc_base
        .f1.t see insert
      }

      if {[info exists tlb_hist_pos] &&
          ($tlb_find ne [lindex $tlb_hist $tlb_hist_pos])} {
        unset tlb_hist_pos tlb_hist_base
      }
    } else {
      SearchReset
    }
  }
}


#
# This function checks if the search pattern syntax is valid
#
proc SearchExprCheck {} {
  global tlb_find tlb_regexp

  if {$tlb_regexp && [catch {regexp -- $tlb_find ""} cerr]} {
    set str $tlb_find
    tk_messageBox -icon error -type ok -parent . \
                  -message "Search has invalid regular expression: $cerr"
    return 0
  } else {
    return 1
  }
}


#
# This function returns the start address for a search.  The first search
# starts at the insertion cursor. If the cursor is not visible, the search
# starts at the top or bottom of the visible text. When a search is repeated,
# the search must start at line start or end to avoid finding the same line
# again.
#
proc Search_GetBase {is_fwd is_init} {
  if {[.f1.t bbox insert] eq ""} {
    if $is_fwd {
      .f1.t mark set insert {@1,1}
    } else {
      .f1.t mark set insert "@[expr [winfo width .f1.t] - 1],[expr [winfo height .f1.t] - 1]"
    }
    set start_pos insert
  } else {
    if $is_init {
      set start_pos insert
    } elseif $is_fwd {
      set start_pos [list insert + 1 chars]
    } else {
      set start_pos insert
    }
  }
  return [.f1.t index $start_pos]
}


#
# This function translates user-options into search options for the text widget.
#
proc Search_GetOptions {is_re match_case {is_fwd -1}} {
  global tlb_case tlb_regexp tlb_last_dir

  set search_opt {}
  if $is_re {
    lappend search_opt {-regexp}
  }
  if {$match_case == 0} {
    lappend search_opt {-nocase}
  }
  if {$is_fwd != -1} {
    if $is_fwd {
      lappend search_opt {-forwards}
    } else {
      lappend search_opt {-backwards}
    }
  }
  return $search_opt
}


#
# This function resets the state of the search engine.  It is called when
# the search string is empty or a search is aborted with the Escape key.
#
proc SearchReset {} {
  global tlb_find tlb_last_hall tlb_last_dir tlb_inc_base

  .f1.t tag remove find 1.0 end
  .f1.t tag remove findinc 1.0 end
  set tlb_last_hall {}

  if [info exists tlb_inc_base] {
    .f1.t mark set insert $tlb_inc_base
    .f1.t see insert
    unset tlb_inc_base
  }
}


#
# This function is called when the "find" entry field receives keyboard focus
# to intialize the search state machine for a new search.
#
proc SearchInit {} {
  global tlb_find tlb_find_focus tlb_hist_pos tlb_hist_base

  if {$tlb_find_focus == 0} {
    set tlb_find_focus 1
    set tlb_find {}
    unset -nocomplain tlb_hist_pos tlb_hist_base
  }
}


#
# This function is bound to the FocusOut event in the search entry field.
# It resets the incremental search state.
#
proc SearchLeave {} {
  global tlb_find tlb_hall tlb_inc_base tlb_inc_yview tlb_find_focus
  global tlb_hist_pos tlb_hist_base

  # ignore if the keyboard focus is leaving towards another application
  if {[focus -displayof .] ne {}} {

    unset -nocomplain tlb_inc_base tlb_inc_yview tlb_hist_pos tlb_hist_base
    .f1.t tag remove findinc 1.0 end
    Search_AddHistory $tlb_find
    set tlb_find_focus 0
  }

  if $tlb_hall {
    SearchHighlightUpdate
  }
}


#
# This function is called when the search window is left via "Escape" key.
# The search highlighting is removed and the search text is deleted.
#
proc SearchAbort {} {
  global tlb_find

  Search_AddHistory $tlb_find
  set tlb_find {}
  SearchReset
  # note more clean-up is triggered via the focus-out event
  focus .f1.t
}


#
# This function is bound to the Return key in the search entry field.
# If the search pattern is invalid (reg.exp. syntax) an error message is
# displayed and the focus stays in the entry field. Else, the keyboard
# focus is switched to the main window.
#
proc SearchReturn {} {
  global tlb_find

  if [SearchExprCheck] {
    # note this implicitly triggers the leave event
    focus .f1.t
  }
}


#
# This function add the given search string to the search histroy stack.
# If the string is already on the stack, it's moved to the top. Note: top
# of the stack is the end of the list.
#
proc Search_AddHistory {txt} {
  global tlb_hist tlb_hist_maxlen

  if {$txt ne ""} {
    set idx [lsearch -exact $tlb_hist $txt]
    if {$idx != -1} {
      set tlb_hist [lreplace $tlb_hist $idx $idx]
    }
    lappend tlb_hist $txt

    # maintain max. stack depth
    if {[llength $tlb_hist] > $tlb_hist_maxlen} {
      set off [expr [llength $tlb_hist] - $tlb_hist_maxlen - 1]
      set tlb_hist [lreplace $tlb_hist 0 $off]
    }

    UpdateRcAfterIdle
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
  global tlb_find tlb_hist tlb_hist_pos tlb_hist_base

  if {[llength $tlb_hist] > 0} {
    if {![info exists tlb_hist_pos]} {
      set tlb_hist_base $tlb_find
      if {$is_up} {
        set tlb_hist_pos [expr [llength $tlb_hist] - 1]
      } else {
        set tlb_hist_pos 0
      }
    } elseif $is_up {
      incr tlb_hist_pos -1
    } else {
      if {$tlb_hist_pos + 1 < [llength $tlb_hist]} {
        incr tlb_hist_pos 1
      } else {
        set tlb_hist_pos -1
      }
    }

    if {[string length $tlb_hist_base] > 0} {
      set tlb_hist_pos [Search_HistoryComplete [expr $is_up ? -1 : 1]]
    }

    if {$tlb_hist_pos >= 0} {
      set tlb_find [lindex $tlb_hist $tlb_hist_pos]
      .f2.e icursor end
    } else {
      # end of history reached -> reset
      set tlb_find $tlb_hist_base
      unset tlb_hist_pos tlb_hist_base
      .f2.e icursor end
    }
  }
}


#
# This helper function searches the search history stack for a search
# string with a given prefix.
#
proc Search_HistoryComplete {step} {
  global tlb_find tlb_hist_base tlb_hist tlb_hist_pos tlb_hist_base

  set len [string length $tlb_hist_base]
  for {set idx $tlb_hist_pos} {($idx >= 0) && ($idx < [llength $tlb_hist])} {incr idx $step} {
    set hi [lindex $tlb_hist $idx]
    if {[string compare -length $len $tlb_find $hi] == 0} {
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
    set dump [ExtractText $pos [list $pos lineend]]
    set off 0

    if $tlb_regexp {
      if $tlb_case {
        set opt {-nocase}
      } else {
        set opt {--}
      }
      if [SearchExprCheck] {
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
      set word [Search_EscapeSpecialChars $word]
      append tlb_find $word
      .f2.e selection clear
      .f2.e selection range [expr [string length $tlb_find] - [string length $word]] end
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
  global tlb_find tlb_regexp tlb_case

  set pos [.f1.t index insert]
  if {$pos ne ""} {
    set dump [ExtractText [list $pos linestart] $pos]

    if {[regexp {(?:\W+|\w+)$} $dump word]} {
      set word [Search_EscapeSpecialChars $word]
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
  global tlb_find tlb_last_dir

  if {[.f1.t bbox insert] ne ""} {
    set pos [.f1.t index insert]
    if {$pos ne ""} {
      set dump [ExtractText $pos [list $pos lineend]]

      if {[regexp {^[\w\-]+} $dump word]} {
        set dump [ExtractText [list $pos linestart] $pos]
        if {[regexp {\w+$} $dump word2]} {
          set word "$word2$word"
        }

        set tlb_find [Search_EscapeSpecialChars $word]
        Search_AddHistory $tlb_find

        Search $is_fwd 1
      }
    }
  }
}


#
# This helper function escapes characters with special semantics in
# regular expressions in a given word. The function is used for adding
# arbitrary text to the search string.
#
proc Search_EscapeSpecialChars {word} {
  global tlb_regexp

  if $tlb_regexp {
    set word [regsub -all {[^[:alnum:][:blank:]_\-\:\=\%\"\!\'\;\,\#\/\<\>]\@} $word {\\&}]
  }
  return $word
}


#
# This function is bound to "CTRL-x" in the "Find" entry field and
# removes the current entry from the search history.
#
proc Search_RemoveFromHistory {} {
  global tlb_hist tlb_hist_pos tlb_hist_base

  if {[info exists tlb_hist_pos] && ($tlb_hist_pos < [llength $tlb_hist])} {
    set tlb_hist [lreplace $tlb_hist $tlb_hist_pos $tlb_hist_pos]
    UpdateRcAfterIdle

    if {[llength $tlb_hist] == 0} {
      unset tlb_hist_pos tlb_hist_base
    } elseif {$tlb_hist_pos >= [llength $tlb_hist]} {
      set tlb_hist_pos [expr [llength $tlb_hist] - 1]
    }
  }
}


# ----------------------------------------------------------------------------
#
# This function creates a small overlay which displays a temporary status
# message.
#
proc DisplayStatusLine {type msg} {
  global font_bold col_bg_content tid_status_line

  set old_focus [focus -displayof .]
  if {[info commands .stline] eq ""} {
    toplevel .stline -background $col_bg_content -relief ridge -borderwidth 2 \
                     -highlightthickness 0 -takefocus 0
    wm overrideredirect .stline 1
    wm group .stline .
    wm resizable . 0 0

    set fh [font metrics $font_bold -linespace]
    wm geometry .stline "+[winfo rootx .f1]+[expr [winfo rooty .f2] - $fh - 10]"

    label .stline.l -font $font_bold -text $msg -background $col_bg_content
    pack .stline.l -side left

  } else {
    raise .stline
    .stline.l configure -text $msg
  }

  after cancel $tid_status_line
  set tid_status_line [after 4000 {destroy .stline}]

  if {$old_focus ne ""} {
    focus -force $old_focus
  }
}


#
# This function is bound to CTRL-G in the main window. It displays the
# current line number (i.e. same as VIM)
#
proc DisplayLineNumer {} {
  global cur_filename

  set pos [.f1.t bbox insert]
  if {[llength $pos] == 4} {
    set pos [.f1.t index insert]
    if {[scan $pos "%d.%d" line char] == 2} {
      set pos [.f1.t index end]
      scan $pos "%d.%d" end_line char

      DisplayStatusLine msg "$cur_filename: line $line of $end_line lines"
    }
  }
}


#
# This function is bound to configure events on dialog windows, i.e. called
# when the window size or stacking changes. The function stores the new size
# so that the same size can be used when the window is closed and re-opened.
#
# Note: this event is installed on the toplevel window, but also called for
# all its childs when they are resized (due to the bindtag mechanism.) This
# is the reason for passing widget and compare parameters.
#
proc TestCaseList_Resize {wid top cmp var} {
  upvar {#0} $var size

  if {$wid eq $cmp} {
    set new_size "[winfo width $top]x[winfo height $top]"

    if {![info exists size] || ($new_size ne $size)} {
      set size $new_size
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
    set size [expr $size - $delta_size]
  } else {
    incr size $delta_size
  }

  return [list [lindex $afont 0] $size $style]
}


#
# This function adjusts the view so that the line holding the cursor is
# placed at the top, center or bottom of the viewable area, if possible.
#
proc YviewSet {where} {
  global font_normal

  .f1.t see insert
  set pos [.f1.t bbox insert]
  if {[llength $pos] == 4} {
    set fh [font metrics $font_normal -linespace]
    set wh [winfo height .f1.t]

    if {$where eq "top"} {
      set delta [expr [lindex $pos 1] / $fh]

    } elseif {$where eq "center"} {
      set delta [expr 0 - (($wh/2 - [lindex $pos 1] + [lindex $pos 3]/2) / $fh)]

    } elseif {$where eq "bottom"} {
      set delta [expr 0 - (($wh - [lindex $pos 1] - [lindex $pos 3]) / $fh)]

    } else {
      set delta 0
    }

    if {$delta > 0} {
      .f1.t yview scroll $delta units
      .f1.t see insert
    } elseif {$delta < 0} {
      .f1.t yview scroll $delta units
      .f1.t see insert
    }
  }
}


#
# This function scrolls the view vertically by the given number of lines.
# When the line holding the cursor is scrolled out of the window
#
proc YviewScroll {delta} {
  global font_normal

  .f1.t yview scroll $delta units

  set fh [font metrics $font_normal -linespace]
  set pos [.f1.t bbox insert]

  # check if cursor is fully visible
  if {([llength $pos] != 4) || ([lindex $pos 3] < $fh)} {
    if {$delta < 0} {
      .f1.t mark set insert [list "@1,[winfo height .f1.t]" - 1 lines linestart]
    } else {
      .f1.t mark set insert {@1,1}
    }
  }
}


#
# This function moves the cursor onto a given position in the current view.
#
proc CursorSetLine {where} {
  global font_normal

  if {$where eq "top"} {
    .f1.t mark set insert {@1,1}

  } elseif {$where eq "center"} {
    .f1.t mark set insert "@1,[expr [winfo height .f1.t] / 2]"

  } elseif {$where eq "bottom"} {
    set fh [font metrics $font_normal -linespace]
    .f1.t mark set insert "@1,[expr [winfo height .f1.t] - $fh/2]"

  } else {
    .f1.t mark set insert [list insert linestart]
  }
  .f1.t xview moveto 0
  .f1.t see insert
}


#
# Helper function to extrace a range of characters from the content.
#
proc ExtractText {pos1 pos2} {
  set dump {}
  foreach {key val idx} [.f1.t dump -text $pos1 $pos2] {
    append dump $val
  }
  return $dump
}


#
# This function stores the current cusor position and view before making
# a "large jump", i.e. performing a search or goto command.
#
proc Mark_JumpPos {} {
  global last_jump_orig

  set last_jump_orig [.f1.t index insert]
}


#
# This function is bound to key presses in the main window. It's called
# when none of the single-key bindings match. It's used to handle complex
# key press event sequences.
#
proc KeyCmd {char} {
  global last_key_char last_jump_orig

  set result 0
  if {$char ne ""} {
    if {$last_key_char eq "'"} {
      if {$char eq "'"} {
        set tmp $last_jump_orig
        Mark_JumpPos
        .f1.t mark set insert $tmp
        .f1.t see insert
      } elseif {($last_key_char eq "0") || ($last_key_char eq "^")} {
        .f1.t mark set insert "insert linestart"
      } elseif {$last_key_char eq "$"} {
        .f1.t mark set insert "insert lineend"
      }
      set last_key_char {}
      set result 1

    } elseif {$last_key_char eq "z"} {
      if {$char eq "-"} {
        YviewSet bottom
      } elseif {($char eq ".") || ($char eq "z")} {
        YviewSet center
      } elseif {($char eq "+") || ($char eq "return")} {
        YviewSet top
      }
      set last_key_char {}
      set result 1

    } else {
      if {[regexp {[0-9]} $char]} {
        KeyCmd_OpenDialog any $char
        set last_key_char {}
        set result 1

      } elseif {[regexp {[z']} $char]} {
        set last_key_char $char
        set result 1

      } elseif {$char eq "-"} {
        .f1.t mark set insert [list insert linestart - 1 lines]
        .f1.t xview moveto 0
        .f1.t see insert

      } elseif {$char eq "return"} {
        .f1.t mark set insert [list insert linestart + 1 lines]
        .f1.t xview moveto 0
        .f1.t see insert
      }
    }
  }
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


# ----------------------------------------------------------------------------
#
# This function opens a tiny "overlay" dialog which allows to enter a line
# number.  The dialog is placed into the upper left corner of the text
# widget in the main window.
#
proc KeyCmd_OpenDialog {type {txt {}}} {
  global keycmd_ent

  if {[llength [info commands .dlg_key.e]] == 0} {
    toplevel .dlg_key
    bind .dlg_key <Leave> {destroy .dlg_key}
    wm transient .dlg_key .
    wm geometry .dlg_key "+[winfo rootx .f1.t]+[winfo rooty .f1.t]"

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
      bind .dlg_key.e <g> {KeyCmd_ExecGoto; break}
      bind .dlg_key.e <G> {KeyCmd_ExecGoto; break}
      # cursor movement binding
      bind .dlg_key.e <Key-minus> {KeyCmd_ExecCursor 0; break}
      bind .dlg_key.e <Return> {KeyCmd_ExecCursor 1; break}
      bind .dlg_key.e <Key-bar> {KeyCmd_ExecColumn; break}
      # search key binding
      bind .dlg_key.e <n> {KeyCmd_ExecSearch 1; break}
      bind .dlg_key.e <N> {KeyCmd_ExecSearch 0; break}
      bind .dlg_key.e <p> {KeyCmd_ExecSearch 0; break}
      # catch-all
      bind .dlg_key.e <KeyPress> {
        if {"%A" eq "|"} {
          # work-around: keysym doesn't work on German keyboard
          KeyCmd_ExecColumn
          break
        } elseif {![regexp {[[:digit:]]} %A] && [regexp {[[:graph:][:space:]]} %A]} {
          break
        }
      }
    }
    bind .dlg_key.e <Escape> {KeyCmd_Leave; break}
    bind .dlg_key.e <Leave> {KeyCmd_Leave; break}
    bind .dlg_key.e <FocusOut> {KeyCmd_Leave; break}

    .dlg_key.e icursor end
    focus .dlg_key.e
  }
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
    # note: line range check not required, text widget does not complain
    if {$keycmd_ent >= 0} {
      .f1.t mark set insert "$keycmd_ent.0"
    } else {
      set keycmd_ent [expr 1 - $keycmd_ent]
      catch {.f1.t mark set insert "end - $keycmd_ent lines"}
    }
    .f1.t see insert
    KeyCmd_Leave
  }
}


#
# This function moves the cursor by a given amount of lines.
#
proc KeyCmd_ExecCursor {is_fwd} {
  global keycmd_ent

  # check if the content is a valid line number
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    if {$is_fwd} {
      catch {.f1.t mark set insert [list insert linestart + $keycmd_ent lines]}
    } else {
      catch {.f1.t mark set insert [list insert linestart - $keycmd_ent lines]}
    }
    .f1.t xview moveto 0
    .f1.t see insert
    KeyCmd_Leave
  }
}


#
# This function sets the cursor into a given column
#
proc KeyCmd_ExecColumn {} {
  global keycmd_ent

  # check if the content is a valid line number
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    # note: line range check not required, text widget does not complain
    catch {.f1.t mark set insert [list insert linestart + $keycmd_ent chars]}
    .f1.t see insert
    KeyCmd_Leave
  }
}


#
# This function starts a search from within the command popup window.
#
proc KeyCmd_ExecSearch {dir} {
  global keycmd_ent

  # check if the content is a repeat count
  set foo 0
  if {[catch {incr foo $keycmd_ent}] == 0} {
    KeyCmd_Leave
    for {set idx 0} {$idx < $keycmd_ent} {incr idx} {
      Search $dir 0
    }
  }
} 


# ----------------------------------------------------------------------------
#
# This function retrieves the TDMA frame number to which a given line of
# text belongs via pattern matching. The frame number is determined by
# searching for frame boundaries marked by "-- TDMA TICK --" and within
# these boundaries for the trace output by the FN increment function.
#
proc ParseTdmaFn {pos} {
  set pat {---- TDMA TICK ----}
  set pos1 [.f1.t search -backwards -- $pat [list $pos lineend] 1.0]
  set pos2 [.f1.t search -forwards -- $pat [list $pos lineend] end]
  if {$pos1 eq ""} {set pos1 "1.0"}
  if {$pos2 eq ""} {set pos2 end}
  set pat {^\d+:\d+ l1g.fint.tick}
  set pos3 [.f1.t search -regexp -count match_len -- $pat $pos1 $pos2]
  if {$pos3 ne ""} {
    set dump [ExtractText $pos3 [list $pos3 + $match_len chars]]
    if {[regexp {\d+:(\d+)} $dump foo fn] == 0} {
      set fn "???"
    }
  } else {
    set fn "???"
  }
  return $fn
}


#
# This function adds or removes a bookmark at the given text line.
# The line is marked by inserting an image in the text and the bookmark
# is added to the bookmark list dialog, if it's currently open.
#
proc Mark_Toggle {line {txt {}}} {
  global img_marker mark_list mark_list_modified

  if {![info exists mark_list($line)]} {
    if {$txt eq ""} {
      set fn [ParseTdmaFn insert]
      set mark_list($line) "FN:$fn "
      append mark_list($line) [ExtractText "$line.0" "$line.0 lineend"]
    } else {
      set mark_list($line) $txt
    }
    .f1.t image create "$line.0" -image $img_marker -padx 5
    .f1.t tag add bookmark "$line.0" "$line.1"
    MarkList_Add $line

  } else {
    unset mark_list($line)
    .f1.t delete "$line.0" "$line.1"
    MarkList_Delete $line
  }
  set mark_list_modified 1
}


#
# This function adds or removes a bookmark at the current cursor position.
#
proc Mark_ToggleAtInsert {} {
  if {[.f1.t bbox insert] ne ""} {
    set pos [.f1.t index insert]
    scan $pos "%d.%d" line char
    Mark_Toggle $line
  }
}


#
# This function deletes all bookmarks. It's called via the main menu.
# The function is intended esp. if a large number of bookmarks was imported
# previously from a file.
#
proc Mark_DeleteAll {} {
  global mark_list

  set count [array size mark_list]
  if {$count > 0} {
    if {$count == 1} {set pls ""} else {set pls "s"}
    set answer [tk_messageBox -icon question -type okcancel -parent . \
                              -message "Really delete $count bookmark${pls}?"]
    if {$answer eq "ok"} {
      foreach line [array names mark_list] {
        Mark_Toggle $line
      }
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
      } elseif {[regexp {^(\d+)([[:space:]:.,;\-][[:space:]]*(.*))?$} $line foo num txt]} {
        lappend bol $num $txt
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
      if {[array size mark_list] != 0} {
        set mark_list_modified 1
      }
      foreach {line txt} $bol {
        if {![info exists mark_list($line)]} {
          Mark_Toggle $line $txt
        }
      }
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
      foreach {line txt} [array get mark_list] {
        puts $file "$line $txt"
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
  set filename [tk_getOpenFile -parent . -filetypes {{Bookmarks {*.bok}} {all {*.*}}} \
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
  # must use catch around mtime
  catch {
    set cur_mtime [file mtime $trace_name]
  }
  if [info exists cur_mtime] {
    set name "${trace_name}.bok"
    catch {
      if {[file readable $name] && ([file mtime $name] >= $cur_mtime)} {
        set bok_name $name
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
  return $bok_name
}


#
# This function is called by menu entry "Save bookmarks to file".
# The user is asked to select a file; if he does so the bookmarks are written to it.
#
proc Mark_SaveFileAs {} {
  global mark_list cur_filename

  if {[array size mark_list] > 0} {
    set def_name "${cur_filename}.bok"
    set filename [tk_getSaveFile -parent . -filetypes {{Bookmarks {*.bok}} {all {*.*}}} \
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
        wm geometry .minfo "+[expr [winfo rootx .] + 100]+[expr [winfo rooty .] + 100]"
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
# This function inserts a bookmark text into the listbox and matches
# the text against the color highlight patterns to assign foreground
# and background colors.
#
proc MarkList_Insert {pos line} {
  global patlist mark_list

  .dlg_mark.l insert $pos $mark_list($line)

  foreach tag [.f1.t tag names "$line.1"] {
    if {[scan $tag "tag%d" tag_idx] == 1} {
      set fg_col [.f1.t tag cget $tag -foreground]
      set bg_col [.f1.t tag cget $tag -background]
      .dlg_mark.l itemconfigure $pos -background $bg_col -foreground $fg_col
      break
    }
  }
}


#
# This function fills the bookmark list dialog window with all bookmarks.
#
proc MarkList_Fill {} {
  global dlg_mark_shown dlg_mark_list mark_list

  if [info exists dlg_mark_shown] {
    set dlg_mark_list {}
    .dlg_mark.l delete 0 end

    foreach line [lsort -integer [array names mark_list]] {
      lappend dlg_mark_list $line

      MarkList_Insert end $line
    }
  }
}


#
# This function is called after a bookmark was added to insert the text
# into the bookmark list dialog window.
#
proc MarkList_Add {line} {
  global dlg_mark_shown mark_list dlg_mark_list

  if [info exists dlg_mark_shown] {
    set idx 0
    foreach l $dlg_mark_list {
      if {$l > $line} {
        break
      }
      incr idx
    }
    set dlg_mark_list [linsert $dlg_mark_list $idx $line]
    MarkList_Insert $idx $line
    .dlg_mark.l selection clear 0 end
    .dlg_mark.l selection set $idx
    .dlg_mark.l see $idx
  }
}


#
# This function is called after a bookmark was deleted to remove the text
# from the bookmark list dialog window.
#
proc MarkList_Delete {line} {
  global dlg_mark_shown mark_list dlg_mark_list

  if [info exists dlg_mark_shown] {
    set idx [lsearch -integer $dlg_mark_list $line]
    if {$idx >= 0} {
      set dlg_mark_list [lreplace $dlg_mark_list $idx $idx]
      .dlg_mark.l delete $idx
    }
  }
}


#
# This helper function determines which line is selected in the
# bookmark list dialog window.
#
proc MarkList_GetSelectedIdx {} {
  global dlg_mark_list

  set sel [.dlg_mark.l curselection]
  if {([llength $sel] == 1) && ($sel < [llength $dlg_mark_list])} {
    set idx $sel
  } else {
    set idx -1
  }
  return $idx
}


#
# This function is bound to the "delete" key and context menu entry to
# allow the user to remove a bookmark via the bookmark list dialog.
#
proc MarkList_DeleteSelection {} {
  set idx [MarkList_GetSelectedIdx]
  if {$idx >= 0} {
    MarkList_Remove $idx
  }
}


#
# This function is bound to the "insert" key and "rename" context menu
# entry to allow the user to edit the tag assigned to the selected bookmark.
# The function opens an "overlay" window with an entry field.
#
proc MarkList_RenameSelection {} {
  set idx [MarkList_GetSelectedIdx]
  if {$idx >= 0} {
    MarkList_OpenRename $idx
  }
}


#
# This function is bound to changes of the selection in the bookmark list,
# i.e. it's called when the user uses the cursor keys or mouse button to
# select an entry.  The listbox is configured so that the user can select
# at most one entry at a time. The text in the main window is scrolled to
# display the line which contains the bookmark.
#
proc MarkList_Selection {} {
  global dlg_mark_list

  set idx [MarkList_GetSelectedIdx]
  if {$idx >= 0} {
    set line [lindex $dlg_mark_list $idx]
    .f1.t mark set insert "$line.0"
    .f1.t see insert
    .f1.t tag remove sel 1.0 end
    .f1.t tag add sel "$line.0" "[expr $line + 1].0"
  }
}


#
# This function removes a bookmark with the given index in the bookmark
# list dialog.
#
proc MarkList_Remove {idx} {
  global dlg_mark_shown dlg_mark_list mark_list mark_list_modified

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    set line [lindex $dlg_mark_list $idx]

    set dlg_mark_list [lreplace $dlg_mark_list $idx $idx]
    .dlg_mark.l delete $idx

    set mark_list_modified 1
    unset mark_list($line)
    .f1.t delete "$line.0" "$line.1"
  }
}


#
# This function assigns the given text to a bookmark with the given index
# in the bookmark list dialog.  The function is called when the user has
# closed the bookmark text entry dialog with "Return"
#
proc MarkList_Rename {idx txt} {
  global dlg_mark_shown dlg_mark_list mark_list mark_list_modified

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    set line [lindex $dlg_mark_list $idx]
    if {$txt ne ""} {
      set mark_list($line) $txt
      set mark_list_modified 1

      .dlg_mark.l delete $idx
      MarkList_Insert $idx $line
      .dlg_mark.l selection clear 0 end
      .dlg_mark.l selection set $idx
      .dlg_mark.l see $idx
    }
  }
}


#
# This function pops up a context menu for the bookmark list dialog.
#
proc MarkList_ContextMenu {xcoo ycoo} {
  global dlg_mark_list

  set idx [.dlg_mark.l index "@$xcoo,$ycoo"]
  if {([llength $idx] > 0) && ($idx < [llength $dlg_mark_list])} {
    .dlg_mark.l selection clear 0 end
    .dlg_mark.l selection set $idx

    .dlg_mark.ctxmen delete 0 end
    .dlg_mark.ctxmen add command -label "Rename marker" -command [list MarkList_OpenRename $idx]
    .dlg_mark.ctxmen add command -label "Remove marker" -command [list MarkList_Remove $idx]

    set rootx [expr [winfo rootx .dlg_mark] + $xcoo]
    set rooty [expr [winfo rooty .dlg_mark] + $ycoo]
    tk_popup .dlg_mark.ctxmen $rootx $rooty 0
  }
}


#
# This function creates or raises the bookmark list dialog. This dialog shows
# all currently defined bookmarks.
#
proc MarkList_OpenDialog {} {
  global font_content col_bg_content col_fg_content
  global cur_filename dlg_mark_shown dlg_mark_size

  if {![info exists dlg_mark_shown]} {
    toplevel .dlg_mark
    if {$cur_filename ne ""} {
      wm title .dlg_mark "Bookmark list - $cur_filename"
    } else {
      wm title .dlg_mark "Bookmark list"
    }
    wm group .dlg_mark .

    listbox .dlg_mark.l -width 40 -height 10 -cursor top_left_arrow -font $font_content \
                        -background $col_bg_content -foreground $col_fg_content \
                        -selectmode browse -exportselection false \
                        -yscrollcommand {.dlg_mark.sb set}
    pack .dlg_mark.l -side left -fill both -expand 1
    scrollbar .dlg_mark.sb -orient vertical -command {.dlg_mark.l yview} -takefocus 0
    pack .dlg_mark.sb -side left -fill y

    menu .dlg_mark.ctxmen -tearoff 0

    bind .dlg_mark.l <<ListboxSelect>> {MarkList_Selection; break}
    bind .dlg_mark.l <Insert> {MarkList_RenameSelection; break}
    bind .dlg_mark.l <Delete> {MarkList_DeleteSelection; break}
    bind .dlg_mark.l <Escape> {destroy .dlg_mark; break}
    bind .dlg_mark.l <ButtonRelease-3> {MarkList_ContextMenu %x %y}
    focus .dlg_mark.l

    set dlg_mark_shown 1
    bind .dlg_mark.l <Destroy> {+ MarkList_Quit 1}
    bind .dlg_mark <Configure> {TestCaseList_Resize %W .dlg_mark .dlg_mark dlg_mark_size}
    wm protocol .dlg_mark WM_DELETE_WINDOW {MarkList_Quit 0}
    wm geometry .dlg_mark $dlg_mark_size

    MarkList_Fill

  } else {
    wm deiconify .dlg_mark
    raise .dlg_mark
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
  catch {destroy .mren}
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
  global font_normal font_bold

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    set coo [.dlg_mark.l bbox $idx]
    if {[llength $coo] > 0} {
      catch {destroy .mren}
      toplevel .mren
      wm transient .mren .dlg_mark

      set xcoo [expr [lindex $coo 0] + [winfo rootx .dlg_mark.l] - 3]
      set ycoo [expr [lindex $coo 1] + [winfo rooty .dlg_mark.l] - 3]
      set w [winfo width .dlg_mark.l]
      set h [expr [lindex $coo 3] + 6]
      wm geometry .mren "${w}x${h}+${xcoo}+${ycoo}"

      set line [lindex $dlg_mark_list $idx]
      set mark_rename $mark_list($line)
      set mark_rename_idx $idx

      entry .mren.e -width 12 -textvariable mark_rename -exportselection false -font $font_normal
      pack .mren.e -side left -fill both -expand 1
      bind .mren.e <Return> {MarkList_Rename $mark_rename_idx $mark_rename; MarkList_LeaveRename; break}
      bind .mren.e <Escape> {MarkList_LeaveRename; break}

      button .mren.b -text "X" -padx 2 -font $font_bold -command MarkList_LeaveRename
      pack .mren.b -side left

      .mren.e selection clear
      if {[regexp {^(FN:)?\d+ } $mark_rename match]} {
        set off [string length $match]
      } else {
        set off 0
      }
      .mren.e selection from $off
      .mren.e selection to end
      .mren.e icursor $off

      focus .mren.e
      grab .mren
    }
  }
}


#
# This function is bound to all events which signal an exit of the rename
# dialog window. The window is destroyed.
#
proc MarkList_LeaveRename {} {
  unset -nocomplain mark_rename mark_rename_idx
  catch {destroy .mren}
  catch {focus .dlg_mark.l}
}


# ----------------------------------------------------------------------------
#
# This function creates or raises the color highlighting tags list dialog.
# This dialog shows all currently defined tag assignments.
#
proc Tags_OpenDialog {} {
  global font_content col_bg_content col_fg_content
  global dlg_tags_shown dlg_tags_size

  if {![info exists dlg_tags_shown]} {
    toplevel .dlg_tags
    wm title .dlg_tags "Color highlights list"
    wm group .dlg_tags .

    frame .dlg_tags.f1
    listbox .dlg_tags.f1.l -width 1 -height 1 -cursor top_left_arrow -font $font_content \
                        -background $col_bg_content -foreground $col_fg_content \
                        -selectmode extended -exportselection false \
                        -yscrollcommand {.dlg_tags.f1.sb set}
    pack .dlg_tags.f1.l -side left -fill both -expand 1
    scrollbar .dlg_tags.f1.sb -orient vertical -command {.dlg_tags.f1.l yview} -takefocus 0
    pack .dlg_tags.f1.sb -side left -fill y
    frame .dlg_tags.f1.f11
    button .dlg_tags.f1.f11.b_up -image img_up -command TagsList_ShiftUp -state disabled
    button .dlg_tags.f1.f11.b_down -image img_down -command TagsList_ShiftDown -state disabled
    pack .dlg_tags.f1.f11.b_up .dlg_tags.f1.f11.b_down -side top -pady 2
    pack .dlg_tags.f1.f11 -side left -fill y -pady 15
    pack .dlg_tags.f1 -side top -fill both -expand 1

    bind .dlg_tags.f1.l <<ListboxSelect>> {TagsList_Selection; break}
    bind .dlg_tags.f1.l <ButtonRelease-3> {TagsList_ContextMenu %x %y; break}
    bind .dlg_tags.f1.l <Key-n> {Tags_Search 1; break}
    bind .dlg_tags.f1.l <Key-N> {Tags_Search 0; break}
    bind .dlg_tags.f1.l <Alt-Key-n> {Tags_Search 1; break}
    bind .dlg_tags.f1.l <Alt-Key-p> {Tags_Search 0; break}
    focus .dlg_tags.f1.l

    frame .dlg_tags.f2
    label .dlg_tags.f2.l -text "Find:"
    button .dlg_tags.f2.bn -text "Next" -command {Tags_Search 1} -underline 0 -state disabled -pady 2
    button .dlg_tags.f2.bp -text "Previous" -command {Tags_Search 0} -underline 0 -state disabled -pady 2
    pack .dlg_tags.f2.l .dlg_tags.f2.bn .dlg_tags.f2.bp -side left -pady 5 -padx 10
    pack .dlg_tags.f2 -side top

    menu .dlg_tags.ctxmen -tearoff 0

    set dlg_tags_shown 1
    bind .dlg_tags.f1.l <Destroy> {+ unset -nocomplain dlg_tags_shown}
    bind .dlg_tags <Configure> {TestCaseList_Resize %W .dlg_tags .dlg_tags dlg_tags_size}
    wm geometry .dlg_tags $dlg_tags_size

    TagsList_Fill

  } else {
    wm deiconify .dlg_tags
    raise .dlg_tags
  }
}


#
# This function pops up a context menu for the color tags list dialog.
#
proc TagsList_ContextMenu {xcoo ycoo} {
  global patlist tlb_find

  set idx [.dlg_tags.f1.l index "@$xcoo,$ycoo"]
  if {([llength $idx] > 0) && ($idx < [llength $patlist])} {
    .dlg_tags.f1.l selection clear 0 end
    .dlg_tags.f1.l selection set $idx
    TagsList_Selection

    if {$tlb_find ne ""} {
      set find_state normal
    } else {
      set find_state disabled
    }

    .dlg_tags.ctxmen delete 0 end
    .dlg_tags.ctxmen add command -label "Change background color" -command [list TagsList_EditColor $idx 0]
    .dlg_tags.ctxmen add command -label "Edit markup..." -command [list Markup_OpenDialog $idx]
    .dlg_tags.ctxmen add separator
    .dlg_tags.ctxmen add command -label "Add current search" -command {TagsList_AddSearch .dlg_tags} -state $find_state
    .dlg_tags.ctxmen add command -label "Copy to search field" -command [list TagsList_CopyToSearch $idx]
    .dlg_tags.ctxmen add command -label "Update from search field" -command [list TagsList_CopyFromSearch $idx] -state $find_state
    .dlg_tags.ctxmen add command -label "Remove this entry" -command [list TagsList_Remove $idx]

    set rootx [expr [winfo rootx .dlg_tags] + $xcoo]
    set rooty [expr [winfo rooty .dlg_tags] + $ycoo]
    tk_popup .dlg_tags.ctxmen $rootx $rooty 0
  }
}


#
# This function is bound to the "up" button next to the color highlight list.
# Each selected item (selection may be non-consecutive) is shifted up by one line.
#
proc TagsList_ShiftUp {} {
  global patlist

  set el [lsort -integer -increasing [.dlg_tags.f1.l curselection]]
  if {[lindex $el 0] > 0} {
    foreach index $el {
      # remove the item in the listbox widget above the shifted one
      .dlg_tags.f1.l delete [expr $index - 1]
      # re-insert the just removed item below the shifted one
      TagsList_Insert $index [expr $index - 1]

      # perform the same exchange in the associated list
      set patlist [lreplace $patlist [expr $index - 1] $index \
                            [lindex $patlist $index] \
                            [lindex $patlist [expr $index - 1]]]
    }
  }
}


#
# This function is bound to the "down" button next to the color highlight
# list.  Each selected item is shifted down by one line.
#
proc TagsList_ShiftDown {} {
  global patlist

  set el [lsort -integer -decreasing [.dlg_tags.f1.l curselection]]
  if {[lindex $el 0] < [llength $patlist] - 1} {
    foreach index $el {
      .dlg_tags.f1.l delete [expr $index + 1]
      TagsList_Insert $index [expr $index + 1]

      set patlist [lreplace $patlist $index [expr $index + 1] \
                            [lindex $patlist [expr $index + 1]] \
                            [lindex $patlist $index]]
    }
  }
}


#
# This function is bound to the next/prev buttons below the highlight tags
# list. When one of more list entries are selected, the function searches
# for the tag in the main window and makes the line visible.
#
proc Tags_Search {is_fwd} {
  global patlist

  set min_line -1
  foreach pat_idx [.dlg_tags.f1.l curselection] {

    set w [lindex $patlist $pat_idx]
    set tagnam [lindex $w 4]
    set start_pos [Search_GetBase $is_fwd 0]

    if {$is_fwd} {
      set pos12 [.f1.t tag nextrange $tagnam $start_pos]
    } else {
      set pos12 [.f1.t tag prevrange $tagnam $start_pos]
    }
    if {$pos12 ne ""} {
      scan [lindex $pos12 0] "%d.%d" line char
      if {($min_line == -1) ||
          ($is_fwd ? ($line < $min_line) : ($line > $min_line))} {
        set min_line $line
      }
    }
  }
  if {$min_line > 0} {
    Mark_JumpPos
    .f1.t mark set insert "$min_line.0"
    .f1.t see insert
    .f1.t tag remove sel 1.0 end
    .f1.t tag add sel "$min_line.0" "[expr $min_line + 1].0"
  }
}


#
# This function is bound to changes of the selection in the color tags list.
#
proc TagsList_Selection {} {

  set sel [.dlg_tags.f1.l curselection]
  if {[llength $sel] >= 0} {
    .dlg_tags.f2.bn configure -state normal
    .dlg_tags.f2.bp configure -state normal
    .dlg_tags.f1.f11.b_up configure -state normal
    .dlg_tags.f1.f11.b_down configure -state normal
  } else {
    .dlg_tags.f2.bn configure -state disabled
    .dlg_tags.f2.bp configure -state disabled
    .dlg_tags.f1.f11.b_up configure -state disabled
    .dlg_tags.f1.f11.b_down configure -state disabled
  }
}


#
# This function updates a color tag text in the listbox.
#
proc TagsList_Update {pat_idx} {
  global dlg_tags_shown patlist

  if [info exists dlg_tags_shown] {
    if {$pat_idx < [llength $patlist]} {
      set w [lindex $patlist $pat_idx]

      .dlg_tags.f1.l delete $pat_idx
      .dlg_tags.f1.l insert $pat_idx [lindex $w 0]
      .dlg_tags.f1.l itemconfigure $pat_idx -background [lindex $w 6] -foreground [lindex $w 7]
      .dlg_tags.f1.l see $pat_idx
    }
  }
}


#
# This function inserts a color tag text into the listbox and sets its
# foreground and background colors.
#
proc TagsList_Insert {pos pat_idx} {
  global patlist

  set w [lindex $patlist $pat_idx]

  .dlg_tags.f1.l insert $pos [lindex $w 0]
  .dlg_tags.f1.l itemconfigure $pos -background [lindex $w 6] -foreground [lindex $w 7]
}


#
# This function fills the color tags list dialog window with all color tags.
#
proc TagsList_Fill {} {
  global dlg_tags_shown patlist

  if [info exists dlg_tags_shown] {
    .dlg_tags.f1.l delete 0 end

    set idx 0
    foreach w $patlist {
      TagsList_Insert end $idx
      incr idx
    }
  }
}


#
# This function allows to edit a color assigned to a tags entry.
#
proc TagsList_EditColor {pat_idx is_fg} {
  global patlist

  .dlg_tags.f1.l see $pat_idx

  set cool [.dlg_tags.f1.l bbox $pat_idx]
  set rootx [expr [winfo rootx .dlg_tags] + [lindex $cool 0]]
  set rooty [expr [winfo rooty .dlg_tags] + [lindex $cool 1]]

  set w [lindex $patlist $pat_idx]
  set col_idx [expr $is_fg ? 7 : 6]
  set def_col [lindex $w $col_idx]

  PaletteMenu_Popup .dlg_tags $rootx $rooty \
                    [list TagsList_UpdateColor $pat_idx $is_fg] [lindex $w $col_idx]
}


#
# This function is invoked after a background color change via the context menu.
# The new color is saved in the highlight list and applied to the main window
# and the highlight dialog's list. NOTE: the color value my be an empty string
# (color "none" refers to the default fore- and background colors)
#
proc TagsList_UpdateColor {pat_idx is_fg col} {
  global patlist

  set w [lindex $patlist $pat_idx]
  set col_idx [expr $is_fg ? 7 : 6]

  set w [lreplace $w $col_idx $col_idx $col]
  if {[catch {.dlg_tags.f1.l itemconfigure $pat_idx -background [lindex $w 6] -foreground [lindex $w 7]}] == 0} {
    # clear selection so that the color becomes visible
    .dlg_tags.f1.l selection clear 0 end

    set patlist [lreplace $patlist $pat_idx $pat_idx $w]
    UpdateRcAfterIdle

    .f1.t tag configure [lindex $w 4] -background [lindex $w 6] -foreground [lindex $w 7]
  }
}


#
# This function is invoked by the "Add current search" entry in the highlight
# list's context menu.
#
proc TagsList_AddSearch {parent} {
  global tlb_find tlb_regexp tlb_case
  global col_bg_find col_bg_findinc
  global dlg_tags_shown patlist

  if {$tlb_find ne ""} {
    # search a free tag index
    set dup_idx -1
    set nam_idx 0
    set idx 0
    foreach w $patlist {
      scan [lindex $w 4] "tag%d" tag_idx
      if {$tag_idx >= $nam_idx} {
        set nam_idx [expr $tag_idx + 1]
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

      set w [list $tlb_find $tlb_regexp $tlb_case default "tag$nam_idx" {} \
                  $col_bg_find {} 0 0 0 {} {} {} 1 0]
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
    set opt [Search_GetOptions [lindex $w 1] [lindex $w 2]]
    HighlightAll [lindex $w 0] [lindex $w 4] $opt

    if [info exists dlg_tags_shown] {
      # insert the entry into the listbox
      TagsList_Insert end $pat_idx
      .dlg_tags.f1.l see $pat_idx
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
proc TagsList_CopyToSearch {pat_idx} {
  global patlist
  global tlb_find_focus tlb_find tlb_regexp tlb_case

  set w [lindex $patlist $pat_idx]

  # force focus into find entry field & suppress "Enter" event
  SearchInit
  set tlb_find_focus 1
  focus .f2.e

  SearchHighlightClear
  set tlb_regexp [lindex $w 1]
  set tlb_case [lindex $w 2]
  set tlb_find [lindex $w 0]
}


#
# This function is invoked by the "Update from search field" command in the
# highlight list's context menu.
#
proc TagsList_CopyFromSearch {pat_idx} {
  global tlb_find_focus tlb_find tlb_regexp tlb_case
  global patlist

  set answer [tk_messageBox -type okcancel -icon question -parent .dlg_tags \
                -message "Please confirm overwriting the search pattern for this entry? This cannot be undone"]
  if {$answer eq "ok"} {
    set w [lindex $patlist $pat_idx]
    set w [lreplace $w 0 2 $tlb_find $tlb_regexp $tlb_case]
    set patlist [lreplace $patlist $pat_idx $pat_idx $w]
    UpdateRcAfterIdle

    # update tag in the main window
    set cfg [HighlightConfigure $w]
    eval [linsert $cfg 0 .f1.t tag configure [lindex $w 4]]

    TagsList_Fill
    .dlg_tags.f1.l see $pat_idx
  }
}


#
# This function is invoked by the "Remove entry" command in the highlight
# list's context menu.
#
proc TagsList_Remove {pat_idx} {
  global patlist

  set answer [tk_messageBox -type yesno -icon question -parent .dlg_tags \
                -message "Really remove this entry? This cannot be undone"]
  if {$answer eq "yes"} {
    set w [lindex $patlist $pat_idx]
    set patlist [lreplace $patlist $pat_idx $pat_idx]
    UpdateRcAfterIdle

    # remove the highlight in the main window
    .f1.t tag delete [lindex $w 4]

    # remove the entry in the listbox
    TagsList_Fill
  }
}


# ----------------------------------------------------------------------------
#
# This function creates or raises the font selection dialog.
#
proc FontList_OpenDialog {} {
  global font_normal font_content dlg_font_shown
  global dlg_font_fams dlg_font_size dlg_font_bold

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
    set dlg_font_bold [expr ![string equal [font actual $font_content -weight] "normal"]]
    set dlg_font_size [font actual $font_content -size]
    set idx [lsearch -exact $dlg_font_fams [font actual $font_content -family]]
    if {$idx >= 0} {
      .dlg_font.f1.fams selection set $idx
      .dlg_font.f1.fams see $idx
    }

  } else {
    wm deiconify .dlg_font
    raise .dlg_font
  }
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
    if $dlg_font_bold {
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

  if $do_store {
    set sel [.dlg_font.f1.fams curselection]
    if {([llength $sel] == 1) && ($sel < [llength $dlg_font_fams])} {
      set name [list [lindex $dlg_font_fams $sel]]
      lappend name $dlg_font_size
      if $dlg_font_bold {
        lappend name bold
      }
      if {[catch {.f1.t configure -font $name} cerr] != 0} {
        tk_messageBox -type ok -icon error -parent .dlg_font -message "Selected font is unavailable: $cerr"
        return
      }

      # save to rc
      set font_content $name
      UpdateRcAfterIdle

    } else {
      tk_messageBox -type ok -icon error -parent .dlg_font -message "No font is selected - Use \"Abort\" to leave without selection"
      return
    }
  }
  unset -nocomplain dlg_font_fams dlg_font_size dlg_font_bold
  destroy .dlg_font
}


# ----------------------------------------------------------------------------
#
# This function creates or raises the color palette dialog.
#
proc Palette_OpenDialog {} {
  global font_normal dlg_cols_shown dlg_cols_palette dlg_cols_cid
  global col_palette

  if {![info exists dlg_cols_shown]} {
    toplevel .dlg_cols
    wm title .dlg_cols "Color palette"
    wm group .dlg_cols .

    label .dlg_cols.lab_hd -text "Pre-define a color palette for quick\nselection when adding patterns:" \
                           -font $font_normal -justify left
    pack .dlg_cols.lab_hd -side top -anchor w -pady 5 -padx 5

    set w [expr 10*15 + 4]
    canvas .dlg_cols.c -width $w -height 100 -background [.dlg_cols cget -background] \
                       -cursor top_left_arrow
    pack .dlg_cols.c -side top -fill both -expand 1 -padx 10 -pady 10

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
proc Palette_Fill {wid pal {sel_cmd {}}} {
  global dlg_cols_cid

  $wid delete all
  set dlg_cols_cid {}

  set x 2
  set y 2
  set col_idx 0
  set idx 0
  foreach col $pal {
    set cid [$wid create rect $x $y [expr $x + 15] [expr $y + 15] \
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

    incr x 15
    incr col_idx 1
    if {$col_idx >= 10} {
      incr y 15
      set x 2
      set col_idx 0
    }
    incr idx
  }

  $wid configure -height [expr (int([llength $pal]+9) / 10) * 15 + 2+2]
}


#
# This function is bound to right mouse clicks on color items.
#
proc Palette_ContextMenu {xcoo ycoo} {
  global dlg_cols_palette dlg_cols_cid

  set cid [.dlg_cols.c find closest $xcoo $ycoo]
  if {$cid ne ""} {
    set idx [lsearch -integer $dlg_cols_cid $cid]
    if {$idx != -1} {

      .dlg_cols.ctxmen delete 0 end
      .dlg_cols.ctxmen add command -label "" -background [lindex $dlg_cols_palette $idx] -state disabled
      .dlg_cols.ctxmen add separator
      .dlg_cols.ctxmen add command -label "Change this color..." -command [list Palette_EditColor $idx $cid]
      .dlg_cols.ctxmen add command -label "Duplicate this color" -command [list Palette_DuplicateColor $idx $cid]
      .dlg_cols.ctxmen add command -label "Insert new color (white)" -command [list Palette_InsertColor $idx $cid]
      .dlg_cols.ctxmen add separator
      .dlg_cols.ctxmen add command -label "Remove this color" -command [list Palette_RemoveColor $idx]

      set rootx [expr [winfo rootx .dlg_cols] + $xcoo]
      set rooty [expr [winfo rooty .dlg_cols] + $ycoo]
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
  .dlg_cols.c raise $cid
  .dlg_cols.c coords $cid [expr $xcoo - 7] [expr $ycoo - 7] [expr $xcoo + 8] [expr $ycoo + 8]
}


#
# This function is bound to the mouse button release event on color palette
# entries. It's used to change the order of colors by drag-and-drop.
#
proc Palette_MoveColorEnd {idx cid xcoo ycoo} {
  global dlg_cols_palette

  set col_idx [expr ($xcoo < 2+7) ? 0 : (($xcoo - (2+7)) / 15)]
  set row_idx [expr ($ycoo < 2+7) ? 0 : (($ycoo - (2+7)) / 15)]

  set new_idx [expr ($row_idx * 10) + $col_idx]
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

  if $do_save {
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
  wm group .colsel $parent
  wm transient .colsel $parent
  wm geometry .colsel "+$rootx+$rooty"

  set w [expr 10*15 + 4]
  canvas .colsel.c -width $w -height 100 -background [.colsel cget -background] \
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

  Palette_Fill .colsel.c $col_palette $cmd

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
# This function creates or raises the color color highlight edit dialog.
#
proc Markup_OpenDialog {pat_idx} {
  global font_normal font_content col_bg_content col_fg_content
  global dlg_fmt_shown dlg_fmt
  global patlist col_palette

  if {![info exists dlg_fmt_shown]} {
    toplevel .dlg_fmt
    wm title .dlg_fmt "Markup editor"
    wm group .dlg_fmt .dlg_tags

    label .dlg_fmt.head -textvariable dlg_fmt(pat)
    pack .dlg_fmt.head -side top -anchor c
    text .dlg_fmt.sample -height 5 -width 35 -font $font_content -wrap none \
                         -foreground $col_fg_content -background $col_bg_content \
                         -relief sunken -borderwidth 2 -takefocus 0 -highlightthickness 0 \
                         -exportselection 0 -insertofftime 0
    pack .dlg_fmt.sample -side top -padx 5 -pady 6
    bindtags .dlg_fmt.sample {.dlg_fmt.sample TextReadOnly .dlg_fmt all}

    set lh [font metrics $font_content -linespace]
    .dlg_fmt.sample tag configure spacing -spacing1 $lh
    .dlg_fmt.sample tag configure margin -lmargin1 17
    .dlg_fmt.sample tag configure sel -bgstipple gray50
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
    label   .dlg_fmt.mb.lsp2_lab -text "Dist.:" -font $font_normal
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
    button .dlg_fmt.f2.abort -text "Abort" -command {Markup_Save 0}
    button .dlg_fmt.f2.ok -text "Ok" -default active -command {Markup_Save 1}
    pack .dlg_fmt.f2.abort .dlg_fmt.f2.ok -side left -padx 10 -pady 4
    pack .dlg_fmt.f2 -side top

    bind .dlg_fmt.mb <Destroy> {+ unset -nocomplain dlg_fmt_shown}
    set dlg_fmt_shown 1

  } else {
    wm deiconify .dlg_fmt
    raise .dlg_fmt
  }

  Markup_InitConfig $pat_idx
  Markup_UpdateFormat
}


#
# This function is called when the mark-up dialog is opened to copy the
# format parameters from the global patlist into the dialog's has array.
#
proc Markup_InitConfig {pat_idx} {
  global patlist dlg_fmt

  set w [lindex $patlist $pat_idx]
  set dlg_fmt(pat) [lindex $w 0]
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
proc Markup_Save {do_save} {
  global dlg_fmt patlist

  if $do_save {
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
    if [info exists pat_idx] {
      set w [Markup_GetConfig $pat_idx]
      set patlist [lreplace $patlist $pat_idx $pat_idx $w]
      UpdateRcAfterIdle

      # update hightlight color listbox
      TagsList_Update $pat_idx

      # update tag in the main window
      set cfg [HighlightConfigure $w]
      eval [linsert $cfg 0 .f1.t tag configure $tagnam]
    } else {
      tk_messageBox -type ok -icon error -parent .dlg_fmt \
                    -message "This element has already been deleted."
      return
    }
  }
  unset -nocomplain dlg_fmt
  destroy .dlg_fmt
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
  set spc [expr $lh - $dlg_fmt(spacing)]
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

  frame ${wid} -relief sunken -borderwidth 1
  canvas ${wid}.c -width [expr [image width img_dropdown] + 4] -height [image height img_dropdown] \
                 -highlightthickness 0 -takefocus 0 -borderwidth 0
  pack ${wid}.c -fill both -expand 1 -side left
  button ${wid}.b -image img_dropdown -highlightthickness 0 -borderwidth 1 -relief raised
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
  set rooty [expr [winfo rooty $wid] + [winfo height $wid]]
  PaletteMenu_Popup .dlg_fmt $rootx $rooty [list Markup_UpdateColor $type] $dlg_fmt($type)
}


#
# This helper function is invoked when the "drop down" button is pressed
# on a pattern selction widget: it opens the associated menu directly
# below the widget.
#
proc Markup_PopupPatternMenu {wid} {
  set rootx [winfo rootx $wid]
  set rooty [expr [winfo rooty $wid] + [winfo height $wid]]
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
# This functions creates the "About" dialog with copyleft info
#
proc OpenAboutDialog {} {
  global dlg_about_shown font_normal

  if {![info exists dlg_about_shown]} {
    toplevel .about
    wm title .about "About"
    wm group .about .
    wm transient .about .

    label .about.name -text "Trace Browser"
    pack .about.name -side top -pady 8

    label .about.copyr1 -text "Copyright (C) 2007 by Thorsten Zörner" -font $font_normal
    pack .about.copyr1 -side top

    message .about.m -font $font_normal -text {
This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License Version 2 as published by the Free Software Foundation at http://www.fsf.org/

THIS PROGRAM IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL, BUT WITHOUT ANY WARRANTY; WITHOUT EVEN THE IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
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
  }
}


# ----------------------------------------------------------------------------
#
# This function loads a trace from a file or stdin (i.e. filename "-")
# Afterwards color highlighting is initiated.
#
proc LoadFile {filename} {
  global cur_filename load_size_limit

  if {$filename eq "-"} {
    if {[catch {
      while 1 {
        set data [read stdin]
        if {[string length $data] == 0} {
          break
        }
        .f1.t insert end $data
      }
      .f1.t see end
      set cur_filename ""
      set result 1
    } cerr] != 0} {
      tk_messageBox -type ok -icon error -message "Read error on STDIN: $cerr"
      set result 0
    }
  } else {
    if {[catch {
      set file [open $filename r]
      # TODO apply file length limit
      file stat $filename sta
      if {$sta(size) > $load_size_limit} {
        seek $file [expr 0 - $load_size_limit] end
      }
      .f1.t insert end [read $file]
      .f1.t see end
      close $file
      set cur_filename $filename
      set result 1
    } cerr] != 0} {
      tk_messageBox -type ok -icon error -message "Failed to load \"$filename\": $cerr"
      set result 0
    }
  }

  if $result {
    global tid_search_inc tid_search_hall tid_high_init
    global dlg_mark_shown

    after cancel $tid_high_init
    after cancel $tid_search_inc
    after cancel $tid_search_hall

    if {$cur_filename ne ""} {
      wm title . "$cur_filename - Trace browser"
      if [info exists dlg_mark_shown] {
        wm title .dlg_mark "Bookmark list [$cur_filename]"
      }
      .menubar.ctrl entryconfigure "Reload*" -state normal
    } else {
      wm title . "Trace browser"
    }
    Mark_JumpPos
    HighlightInit
    Mark_ReadFileAuto
  }
}


#
# This function is bound to the "Reload current file" menu command.
#
proc MenuCmd_Reload {} {
  global cur_filename

  if {$cur_filename ne ""} {
    after idle [list LoadFile $cur_filename]
  }
}


#
# This function is bound to the "Load file" menu command.  The function
# allows to specify a file from which a new trace is read. The current browser
# contents are discarded and all bookmarks are cleared.
#
proc MenuCmd_OpenFile {} {
  global patlist

  # offer to save old bookmarks before discarding them below
  Mark_OfferSave

  set filename [tk_getOpenFile -parent . -filetypes {{"trace" {out.*}} {all {*.*}}}]
  if {$filename ne ""} {
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

    global mark_list mark_list_modified
    array unset mark_list
    set mark_list_modified 0
    MarkList_Fill
    SearchReset

    after idle [list LoadFile $filename]
  }
}


#
# This function installed as callback for destroy requests on the main window.
#
proc UserQuit {} {
  UpdateRcFile
  Mark_OfferSave
  destroy .
}


#
# This functions reads configuration variables from the rc file.
# The function is called once during start-up.
#
proc LoadRcFile {isDefault} {
  global tlb_hist tlb_hist_maxlen tlb_case tlb_regexp tlb_hall
  global dlg_mark_size dlg_tags_size main_win_geom
  global patlist col_palette font_content load_size_limit
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
}

#
# This functions writes configuration variables into the rc file
#
proc UpdateRcFile {} {
  global argv0 myrcfile rcfile_compat rcfile_version
  global tid_update_rc_sec tid_update_rc_min
  global dlg_mark_size dlg_tags_size main_win_geom
  global patlist col_palette font_content load_size_limit
  global tlb_hist tlb_hist_maxlen tlb_case tlb_regexp tlb_hall

  after cancel $tid_update_rc_sec
  after cancel $tid_update_rc_min
  set tid_update_rc_min {}

  set tmpfile ${myrcfile}.tmp

  if {[catch {set rcfile [open $tmpfile "w"]} errstr] == 0} {
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

    # dump search history
    puts $rcfile [list set tlb_hist {}]
    foreach val $tlb_hist {
      puts $rcfile [list lappend tlb_hist $val]
    }

    # dump search settings
    puts $rcfile [list set tlb_case $tlb_case]
    puts $rcfile [list set tlb_regexp $tlb_regexp]
    puts $rcfile [list set tlb_hall $tlb_hall]
    puts $rcfile [list set tlb_hist_maxlen $tlb_hist_maxlen]

    # dialog sizes
    if [info exists dlg_mark_size] {
      puts $rcfile [list set dlg_mark_size $dlg_mark_size]
    }
    if [info exists dlg_tags_size] {
      puts $rcfile [list set dlg_tags_size $dlg_tags_size]
    }
    puts $rcfile [list set main_win_geom $main_win_geom]

    # font setting
    puts $rcfile [list set font_content $font_content]

    # misc
    puts $rcfile [list set load_size_limit $load_size_limit]

    close $rcfile

    # move the new file over the old one
    if {[catch {file rename -force $tmpfile ${myrcfile}} errstr] != 0} {
      tk_messageBox -type ok -default ok -icon error -message "Could not replace rc file $myrcfile\n$errstr"
    }

  } else {
    tk_messageBox -type ok -default ok -icon error -message "Could not create temporary rc file $tmpfile\n$errstr"
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
# Global variables
#
# This variable contains the search string in the "find" entry field, i.e.
# it's automatically updated when the user modifies the text in the widget.
# A variable trace is used to trigger incremental searches after each change.
set tlb_find {}

# This variable is a cache of the search string for which the last
# "highlight all" color highlighting was done. It's used to avoid unnecessarily
# repeating the search when the string is unchanged, because the search can
# take some time. The variable is empty when no highlights are shown.
set tlb_last_hall {}

# This variable contains a list of prio search texts.
set tlb_hist {}

# This variable defines the maximum length of the search history list.
set tlb_hist_maxlen 20

# These variables contain search options which can be set by checkboxes.
set tlb_case 1
set tlb_regexp 1
set tlb_hall 0

# This variable stores the search direction: 0:=backwards, 1:=forwards
set tlb_last_dir 1

# This variable indicates if the search entry field has keyboard focus.
set tlb_find_focus 0

# These variables are used while keyboard focus is in the search entry field.
#unset tlb_inc_base
#unset tlb_inc_yview
#unset tlb_hist_pos
#unset tlb_hist_base

# This variable is used to parse multi-key command sequences
set last_key_char {}

# This variable remembers the last position in the trace before a "large jump",
# i.e. a search or goto. The variable is a list of line number and yview.
set last_jump_orig {}

# This hash array stores the bookmark list. It's indices are text line numbers.
array set mark_list {}

# This variable tracks if the marker list was changed since the last save.
# It's used to offer automatic save upon quit
set mark_list_modified 0

# This variable contains the limit for file load
set load_size_limit 2000000

# These variables hold IDs of timers (i.e. scripts delayed by "after")
# They are used to cancel the scripts when necessary
set tid_search_inc {}
set tid_search_hall {}
set tid_high_init {}
set tid_update_rc_sec {}
set tid_update_rc_min {}
set tid_status_line {}

# These variable holds the font and color selections for the main text content.
set font_content {helvetica 9 normal}
set col_bg_content {#dcdcdf}
set col_fg_content {#000}
set col_bg_find {#faee0a}
set col_bg_findinc {#c8ff00}

# These variables store the geometry of the main and dialog windows.
set main_win_geom "640x480"
set dlg_mark_size "500x250"
set dlg_tags_size "400x300"

# This variable stores a list of pre-defined colors.
set col_palette [list \
  {#000000} \
  {#4acbb5} \
  {#94ff80} \
  {#b3beff} \
  {#b3fff3} \
  {#b4f9b4} \
  {#bfffb3} \
  {#c180ff} \
  {#ccffff} \
  {#dab3d9} \
  {#dab3ff} \
  {#e6b3d9} \
  {#e6b3ff} \
  {#e6ccff} \
  {#e73c39} \
  {#e7b3ff} \
  {#e9ff80} \
  {#efbf80} \
  {#f0b0a0} \
  {#f2ffb3} \
  {#ffb3be} \
  {#ffbf80} \
  {#ffd9b3} \
  {#ff6600} \
  {#ffffff}]

# This list stores text patterns and associated colors for color-highlighting
# in the main text window. Each list entry is again a list:
# 0: sub-string or regular expression
# 1: reg.exp. yes/no:=1/0
# 2: match case yes/no:=1/0
# 3: group tag
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
  {{TDMA TICK} 0 1 default tag0 {} #FFFFFF #000020 0 0 0 {} {} {} 1 0}
  {l1_brs_dsp-frame_tick 0 1 default tag1 {} #E6B3D9 #000000 0 0 0 {} {} {} 1 0}
  {l1d_brs_dsp-send_cmd 0 1 default tag2 {} #E6B3FF #000000 0 0 0 {} {} {} 1 0}
  {l1d_brs-mcu_task_exec 0 1 default tag3 {} #EFBF80 #000000 0 0 0 {} {} {} 1 0}
  {l1cd-air_if_avail 0 1 default tag4 {} #F0B0A0 #000000 0 0 0 {} {} {} 1 0}
  {BRS___DSP-REQUEST 0 1 default tag5 {} #DAB3FF #000000 0 0 0 {} {} {} 1 0}
  {BRS____DSP-RESULT 0 1 default tag6 {} #DAB3D9 #000000 0 0 0 {} {} {} 1 0}
  {SDL-Signal 0 1 default tag7 {} #FF6600 #000000 0 0 0 {} {} {} 1 0}
  {SDL-Loop 0 1 default tag8 {} #FF6600 #000000 0 0 0 {} {} {} 1 0}
  {###### 0 1 default tag9 {} #FF6600 #000000 0 0 0 {} {} {} 1 0}
  {BRS____CONFIG-SET 0 1 default tag10 {} #FF6600 #000000 0 0 0 {} {} {} 1 0}
}

# These variables are used by the bookmark list dialog.
#set dlg_mark_list {}
#unset dlg_mark_shown

# define limit for forwards compatibility
set rcfile_compat 0x01000000
set rcfile_version 0x01000000
set myrcfile "~/.l1trowserc"

#
# Main
#
if {[catch {tk appname "trowser"}]} {
  # this error occurs when the display connection is refused etc.
  puts stderr "Tk initialization failed"
  exit
}
LoadRcFile 1
InitResources
CreateMainWindow
HighlightCreateTags
update

if {$argc == 1} {
  LoadFile [lindex $argv 0]
} else {
  puts stderr "Usage: $argv0 <file>"
}

# done - all following actions are event-driven
# the application exits when the main window is closed
