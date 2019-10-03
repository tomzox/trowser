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
  option add *Dialog.msg.font {helvetica -12 bold} userDefault

  # fonts for text and label widgets
  set font_normal {helvetica -12 normal}
  set font_bold {helvetica -12 bold}

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
                 <Key-Home> <Key-End> <Shift-Key-Home> <Shift-Key-End> \
                 <Control-Home> <Control-End> <Control-Key-slash>} {
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

  # note: default text widget background is #d9d9d9
  global patlist
  set patlist {
    {{TDMA TICK} {#FFFFFF} {#000020}}
    {l1_brs_dsp-frame_tick {#E6B3D9} {#000000}}
    {l1d_brs_dsp-send_cmd {#E6B3FF} {#000000}}
    {l1d_brs-mcu_task_exec {#EFBF80} {#000000}}
    {l1cd-air_if_avail {#F0B0A0} {#000000}}
    {BRS___DSP-REQUEST {#DAB3FF} {#000000}}
    {BRS____DSP-RESULT {#DAB3D9} {#000000}}
    {SDL-Signal {#FF6600} {#000000}}
    {SDL-Loop {#FF6600} {#000000}}
    {{######} {#FF6600} {#000000}}
    {BRS____CONFIG-SET {#FF6600} {#000000}}
  }
}


# ----------------------------------------------------------------------------
#
# This function creates the main window of the trace browser.
#
proc CreateLogBrowser {} {
  global font_normal tlb_find tlb_hall tlb_case

  # menubar at the top of the window
  menu .menubar -relief raised
  . config -menu .menubar
  .menubar add cascade -label "Control" -menu .menubar.ctrl -underline 0
  .menubar add cascade -label "Navigate" -menu .menubar.mark -underline 0
  menu .menubar.ctrl -tearoff 0
  .menubar.ctrl add command -label "Open file..." -command MenuOpenFile
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Read boorkarks from file..." -command Mark_ReadFile
  .menubar.ctrl add command -label "Save boorkarks to file..." -command Mark_SaveFile
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Edit color highlighting..." -command Tags_OpenDialog
  .menubar.ctrl add separator
  .menubar.ctrl add command -label "Quit" -command {destroy .; update}
  menu .menubar.mark -tearoff 0
  .menubar.mark add command -label "Toggle bookmark" -accelerator "B" -command Mark_ToggleAtInsert
  .menubar.mark add command -label "List bookmarks" -accelerator "L" -command MarkList_OpenDialog
  .menubar.mark add command -label "Delete all bookmarks" -command Mark_DeleteAll
  .menubar.mark add separator
  .menubar.mark add command -label "Goto line..." -accelerator "CTRL-g" -command Goto_OpenDialog

  # frame #1: text widget and scrollbar
  frame .f1
  text .f1.t -width 100 -height 50 -wrap none -undo 0 \
          -font $font_normal -cursor top_left_arrow -relief flat \
          -exportselection false \
          -yscrollcommand {.f1.sb set}
  pack .f1.t -side left -fill both -expand 1
  scrollbar .f1.sb -orient vertical -command {.f1.t yview} -takefocus 0
  pack .f1.sb -side left -fill y
  pack .f1 -side top -fill both -expand 1

  focus .f1.t
  .f1.t tag configure find -background {#faee0a}
  .f1.t tag configure findinc -background {#c8ff00}
  .f1.t tag raise sel
  bindtags .f1.t {.f1.t TextReadOnly . all}
  # Ctrl+Cursor and Ctrl-E/Y allow to shift the view up/down
  bind .f1.t <Control-b> {.f1.t yview scroll -1 pages}
  bind .f1.t <Control-f> {.f1.t yview scroll 1 pages}
  bind .f1.t <Control-Up> {.f1.t yview scroll -1 units}
  bind .f1.t <Control-Down> {.f1.t yview scroll 1 units}
  bind .f1.t <Control-e> {.f1.t yview scroll 1 units}
  bind .f1.t <Control-y> {.f1.t yview scroll -1 units}
  bind .f1.t <Control-g> {Goto_OpenDialog}
  bind .f1.t <Key-1><Key-G> {.f1.t mark set insert 1.0; .f1.t see insert}
  bind .f1.t <G> {.f1.t mark set insert end; .f1.t see insert}
  # search with "/", "?"; repeat search with n/N
  #bind .f1.t <Control-Key-f> {focus .f2.e; break}
  bind .f1.t <Key-slash> {set tlb_last_dir 1; focus .f2.e; break}
  bind .f1.t <Key-question> {set tlb_last_dir 0; focus .f2.e; break}
  bind .f1.t <Key-n> {Search 1 0}
  bind .f1.t <Key-p> {Search 0 0}
  bind .f1.t <Key-N> {Search 0 0}
  bind .f1.t <Key-ampersand> {SearchHighlightClear}
  bind .f1.t <Alt-Key-f> {focus .f2.e}
  bind .f1.t <Alt-Key-n> {Search 1 0}
  bind .f1.t <Alt-Key-p> {Search 0 0}
  bind .f1.t <Alt-Key-h> {set tlb_hall [expr !$tlb_hall]; SearchHighlight}
  # bookmarks
  bind .f1.t <Double-Button-1> {Mark_ToggleAtInsert; break}
  bind .f1.t <Key-B> {Mark_ToggleAtInsert}
  bind .f1.t <Key-L> {MarkList_OpenDialog}

  # frame #2: search controls
  frame .f2 -borderwidth 2 -relief raised
  label .f2.l -text "Find:" -underline 0
  entry .f2.e -width 20 -textvariable tlb_find -exportselection false
  menu .f2.mh -tearoff 0
  button .f2.bn -text "Find next" -command {Search 1 0} -underline 5
  button .f2.bp -text "Find previous" -command {Search 0 0} -underline 5
  checkbutton .f2.bh -text "Highlight all" -variable tlb_hall -command SearchHighlight -underline 0
  checkbutton .f2.cb -text "Match case" -variable tlb_case
  pack .f2.l .f2.e .f2.bn .f2.bp .f2.bh .f2.cb -side left -anchor w
  pack .f2 -side top -fill x

  bind .f2.e <Escape> {SearchAbort}
  bind .f2.e <Return> {SearchIncLeave; focus .f1.t}
  bind .f2.e <FocusIn> {SearchInit}
  bind .f2.e <FocusOut> {SearchIncLeave}
  bind .f2.e <Control-n> {Search 1 0}
  bind .f2.e <Control-N> {Search 0 0}
  bind .f2.e <Key-Up> {Search_BrowseHistory 1}
  bind .f2.e <Key-Down> {Search_BrowseHistory 0}
  bind .f2.e <Control-d> {Search_CompleteByHistory; break}
  trace add variable tlb_find write SearchVarTrace
}


#
# This function is called after loading a new text to apply the color
# highlighting to the complete text. Since this can take some time,
# the operation done separately for each pattern with a 10ms pause
# in-between to allow user-interaction. Additionally, a progress bar
# is shown.
#
proc HighlightInit {} {
  global patlist

  if {[llength $patlist] > 0} {
    toplevel .hipro
    wm transient .hipro .
    wm geometry .hipro "+[expr [winfo rootx .f1.t] + 1]+[expr [winfo rooty .f1.t] + 1]"

    canvas .hipro.c -width 100 -height 10 -borderwidth 2 -relief sunken
    pack .hipro.c
    set cid [.hipro.c create rect 2 2 2 12 -fill {#0b1ff7}]

    set tagidx 0
    foreach w $patlist {
      set pat [lindex $w 0]

      # create a tag for the text widget which defines text colors
      set tagnam "tag$tagidx"
      .f1.t tag configure $tagnam -background [lindex $w 1] -foreground [lindex $w 2]
      .f1.t tag lower $tagnam
      incr tagidx
    }

    # trigger highlighting for the 1st and following patterns
    after 10 HighlightInitBg 0 $cid
  }
}


#
# This function is a slave-function of proc HighlightInit. The function
# loops across all patterns to apply color highlights. The loop is broken
# up by means of a 10ms timer.
#
proc HighlightInitBg {tagidx cid} {
  global patlist

  if {$tagidx < [llength $patlist]} {
    set w [lindex $patlist $tagidx]
    set pat [lindex $w 0]
    set tagnam "tag$tagidx"

    # apply the tag to all matching lines of text
    HighlightAll $pat $tagnam {}

    # trigger next tag right away - or allow user interaction
    incr tagidx
    after 10 HighlightInitBg $tagidx $cid

    # update the progress bar
    catch {.hipro.c coords $cid 2 2 [expr int(100*$tagidx/[llength $patlist])+2] 12}

  } else {
    catch {destroy .hipro}
  }
}


#
# This function searches for all text lines which contain the given
# sub-string and marks these lines with the given tag.
#
proc HighlightAll {pat tagnam opt} {
  set pos [.f1.t index end]
  scan $pos "%d.%d" max_line char
  set line 1
  while {($line < $max_line) &&
         ([set pos [eval .f1.t search $opt -- {$pat} "$line.0" end]] ne {})} {
    scan $pos "%d.%d" line char
    .f1.t tag add $tagnam "$line.0" "[expr $line + 1].0"
    incr line
  }
}


#
# This is the main search function which is invoked when the user
# enters text in the "find" entry field or repeats a previous search.
#
proc Search {is_fwd is_changed {start_pos {}}} {
  global tlb_find tlb_hall tlb_case tlb_last_hall tlb_last_dir

  if {$tlb_find ne ""} {
    if {[catch {regexp -- $tlb_find ""} cerr]} {
      tk_messageBox -icon error -type ok -message "Invalid regular expression: $cerr"
      return
    }
    set tlb_last_dir $is_fwd
    set search_opt [Search_GetOptions 1]
    if {$start_pos eq {}} {
      set start_pos [Search_GetBase $is_fwd 0]
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
    }
    if $tlb_hall {
      after cancel SearchHighlight
      after 500 SearchHighlight
    }

  } else {
    SearchReset
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
      .f1.t mark set insert "@[expr [winfo width .t.f1] - 1],[expr [winfo height .t.f1] - 1]"
    }
    set start_pos insert
  } else {
    if $is_init {
      set start_pos insert
    } elseif $is_fwd {
      set start_pos [list insert lineend]
    } else {
      set start_pos [list insert linestart]
    }
  }
  return [.f1.t index $start_pos]
}


#
# This function translates user-options into search options for the text widget.
#
proc Search_GetOptions {with_dir} {
  global tlb_case tlb_last_dir

  set search_opt {-regexp}
  if {$tlb_case == 0} {
    lappend search_opt {-nocase}
  }
  if $with_dir {
    if $tlb_last_dir {
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
# This function is bound to all changes of the search text in the
# "find" entry field. It's called when the user enters new text and
# triggers an incremental search.
#
proc SearchVarTrace {name1 name2 op} {
  after cancel SearchIncrement
  after 10 SearchIncrement
}


#
# This function performs an so-called "incremental" search after the user
# has modified the search text. This means searches are started already
# while the user is typing.
#
proc SearchIncrement {} {
  global tlb_find tlb_last_dir tlb_inc_base tlb_hist tlb_hist_pos

  if {($tlb_find ne {}) &&
      ([catch {regexp -- $tlb_find ""}] == 0)} {

    if {![info exists tlb_inc_base]} {
      set tlb_inc_base [Search_GetBase $tlb_last_dir 1]
    }
    Search $tlb_last_dir 1 $tlb_inc_base

    if {[info exists tlb_hist_pos] &&
        ($tlb_find ne [lindex $tlb_hist $tlb_hist_pos])} {
      unset tlb_hist_pos
    }
  } else {
    SearchReset
  }
}


#
# This function is called when the "find" entry field receives keyboard focus
# to intialize the search state machine for a new search.
#
proc SearchInit {} {
  global tlb_find

  set tlb_find {}
  unset -nocomplain tlb_hist_pos
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
# This function is bound to the FocusOut event in the search entry field.
# It resets the incremental search state.
#
proc SearchIncLeave {} {
  global tlb_find tlb_inc_base tlb_inc_yview tlb_hist_pos

  unset -nocomplain tlb_inc_base tlb_inc_yview tlb_hist_pos
  .f1.t tag remove findinc 1.0 end
  Search_AddHistory $tlb_find
}


#
# This function clears the current search color highlighting without
# resetting the search string. It's bound to the "&" key, but also used
# during regular search reset.
#
proc SearchHighlightClear {} {
  global tlb_last_hall

  .f1.t tag remove find 1.0 end
  .f1.t tag remove findinc 1.0 end
  set tlb_last_hall {}
}


#
# This function triggers color highlighting of all lines of text which match
# the current search string.  The function is called when global highlighting
# is en-/disabled and when the search string is modified.  In the latter case
# the highlighting is delayed by a timer because the operation can take quite
# a while and should not disturb the user while entering text in the search
# entry field.
#
proc SearchHighlight {} {
  global tlb_find tlb_hall tlb_last_hall

  if {$tlb_find ne ""} {
    if $tlb_hall {
      if {$tlb_last_hall ne $tlb_find} {
        if {[catch {regexp -- $tlb_find ""} cerr]} {
          tk_messageBox -icon error -type ok -message "Invalid regular expression: $cerr"
          return
        }
        set tlb_last_hall $tlb_find
        HighlightAll $tlb_find find [Search_GetOptions 0]
      }
    } else {
      SearchHighlightClear
    }
  }
}


#
# This function add the given search string to the search histroy stack.
# If the string is already on the stack, it's moved to the top.
#
proc Search_AddHistory {txt} {
  global tlb_hist

  if {$txt ne ""} {
    set idx [lsearch -exact $tlb_hist $txt]
    if {$idx != -1} {
      set tlb_hist [lreplace $tlb_hist $idx $idx]
    }
    lappend tlb_hist $txt
  }
}


#
# This function is bound to the up/down cursor keys in the search entry
# field. The function is used to iterate through the search history stack.
#
proc Search_BrowseHistory {is_up} {
  global tlb_find tlb_hist tlb_hist_pos

  if {[llength $tlb_hist] > 0} {
    if {![info exists tlb_hist_pos]} {
      if {$is_up} {
        set tlb_hist_pos [expr [llength $tlb_hist] - 1]
      } else {
        set tlb_hist_pos 0
      }
    } elseif $is_up {
      incr tlb_hist_pos -1
    } else {
      incr tlb_hist_pos 1
    }
    if {$tlb_hist_pos >= [llength $tlb_hist]} {
      set tlb_hist_pos 0
    } elseif {$tlb_hist_pos < 0} {
      set tlb_hist_pos [expr [llength $tlb_hist] - 1]
    }

    set tlb_find [lindex $tlb_hist $tlb_hist_pos]
  }
}


#
# This function is bound to "CTRL-D" in the "Find" entry field and
# performs auto-completion of a search text by using the search history.
# 
proc Search_CompleteByHistory {} {
  global tlb_find tlb_hist

  if {($tlb_find ne "") && ([llength $tlb_hist] > 0)} {
    set len [expr [string length $tlb_find]]
    for {set idx [expr [llength $tlb_hist] - 1]} {$idx >= 0} {incr idx -1} {
      set hi [lindex $tlb_hist $idx]
      if {[string compare -length $len $tlb_find $hi] == 0} {
        set tlb_find $hi
        .f2.e icursor end
        break
      }
    }
  }
}


# ----------------------------------------------------------------------------
#
# This function opens a tiny "overlay" dialog which allows to enter a line
# number.  The dialog is placed into the upper left corner of the text
# widget in the main window.
#
proc Goto_OpenDialog {} {
  global goto_ent

  if {[llength [info commands .goto.e]] == 0} {
    toplevel .goto
    bind .goto <Leave> {destroy .goto}
    wm transient .goto .
    wm geometry .goto "+[winfo rootx .f1.t]+[winfo rooty .f1.t]"

    set goto_ent {}
    label .goto.l -text "Goto line:"
    pack .goto.l -side left -padx 5
    entry .goto.e -width 12 -textvariable goto_ent -exportselection false
    pack .goto.e -side left -padx 5
    bind .goto.e <Return> {Goto_Jump; break}
    bind .goto.e <Escape> {Goto_Leave; break}
    bind .goto.e <Leave> {Goto_Leave; break}
    bind .goto.e <FocusOut> {Goto_Leave; break}
    focus .goto.e
  }
}


#
# This function scrolls the text in the main window to the line
# entered in the "goto" dialog window and closes the dialog.
#
proc Goto_Jump {} {
  global goto_ent

  # check if the content is a valid line number
  if {[catch {expr $goto_ent + 1}] == 0} {
    # note: line range check not required, text widget does not complain
    .f1.t mark set insert "$goto_ent.0"
    .f1.t see insert
    Goto_Leave
  }
}


#
# This function is bound to all events which signal an exit of the goto
# dialog window. The window is destroyed.
#
proc Goto_Leave {} {
  focus .f1.t
  destroy .goto
  unset -nocomplain goto_ent
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
    set dump {}
    foreach {key val idx} [.f1.t dump -text $pos3 [list $pos3 + $match_len chars]] {
      append dump $val
    }
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
  global img_marker mark_list

  if {![info exists mark_list($line)]} {
    if {$txt eq ""} {
      set fn [ParseTdmaFn insert]
      set mark_list($line) "FN:$fn "
      foreach {key val idx} [.f1.t dump -text "$line.0" "$line.0 lineend"] {
        append mark_list($line) $val
      }
    } else {
      set mark_list($line) $txt
    }
    .f1.t image create "$line.0" -image $img_marker -padx 5
    MarkList_Add $line

  } else {
    unset mark_list($line)
    .f1.t delete "$line.0" "$line.1"
    MarkList_Delete $line
  }
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
    tk_messageBox -icon info -type ok -message "Your bookmark list is already empty."
  }
}


#
# This function reads a list of line numbers and tags from a file and
# adds them to the bookmark list.
#
proc Mark_ReadFile {} {
  set filename [tk_getOpenFile -parent . -filetypes {{all {*.*}}}]
  if {$filename ne ""} {
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
          tk_messageBox -icon error -type ok -message "Parse error in line $line_num: line is not starting with a digit: \"[string range $line 0 40]\"."
          set line_num -1
          break
        }
        incr line_num
      }
      close $file

      if {$line_num > 0} {
        global mark_list
        foreach {line txt} $bol {
          if {![info exists mark_list($line)]} {
            Mark_Toggle $line $txt
          }
        }
        MarkList_Fill
      }
    } else {
      tk_messageBox -icon error -type ok -message "Failed to read bookmarks file: $cerr"
    }
  }
}


#
# This function stores the bookmark list in a file.
#
proc Mark_SaveFile {} {
  global mark_list

  if {[array size mark_list] > 0} {
    set filename [tk_getSaveFile -parent . -filetypes {{all {*.*}}}]
    if {$filename ne ""} {
      if {[catch {set file [open $filename w]} cerr] == 0} {
        foreach {line txt} [array get mark_list] {
          puts $file "$line $txt"
        }
        close $file
      } else {
        tk_messageBox -icon error -type ok -message "Failed to save bookmarks: $cerr"
      }
    }
  } else {
    tk_messageBox -icon info -type ok -message "Your bookmark list is empty."
  }
}


# ----------------------------------------------------------------------------
#
# This function inserts a bookmark text into the listbox and matches
# the text against the color highlight patterns to assign foreground
# and background colors.
#
proc MarkList_Insert {pos mtext} {
  global patlist

  .dlg_mark.l insert $pos $mtext

  foreach w $patlist {
    set pat [lindex $w 0]
    if {[string first $pat $mtext)] >= 0} {
      .dlg_mark.l itemconfigure $pos -background [lindex $w 1] -foreground [lindex $w 2]
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

      MarkList_Insert end $mark_list($line)
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
    MarkList_Insert $idx $mark_list($line)
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
  global dlg_mark_shown dlg_mark_list mark_list

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    set line [lindex $dlg_mark_list $idx]

    set dlg_mark_list [lreplace $dlg_mark_list $idx $idx]
    .dlg_mark.l delete $idx

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
  global dlg_mark_shown dlg_mark_list mark_list

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list])} {

    set line [lindex $dlg_mark_list $idx]
    if {$txt ne ""} {
      set mark_list($line) $txt

      .dlg_mark.l delete $idx
      .dlg_mark.l insert $idx $txt
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
  global font_normal dlg_mark_shown dlg_mark_size

  if {![info exists dlg_mark_shown]} {
    toplevel .dlg_mark
    wm title .dlg_mark "Bookmark list"
    wm group .dlg_mark .

    listbox .dlg_mark.l -width 40 -height 10 -font $font_normal -exportselection false \
                        -cursor top_left_arrow -yscrollcommand {.dlg_mark.sb set} \
                        -selectmode browse
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
    bind .dlg_mark.l <Destroy> {+ unset -nocomplain dlg_mark_shown}
    bind .dlg_mark <Configure> {TestCaseList_Resize %W .dlg_mark dlg_mark_size}
    if [info exists dlg_mark_size] {
      wm geometry .dlg_mark $dlg_mark_size
    }

    MarkList_Fill

  } else {
    wm deiconify .dlg_mark
    raise .dlg_mark
  }
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
  global font_normal

  if {[info exists dlg_mark_shown] &&
      ($idx < [llength $dlg_mark_list]) &&
      ([llength [info commands .mren.e]] == 0)} {

    set coo [.dlg_mark.l bbox $idx]
    if {[llength $coo] > 0} {
      toplevel .mren
      bind .mren <Leave> {destroy .mren}
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
      pack .mren.e -fill both -expand 1
      bind .mren.e <Return> {MarkList_Rename $mark_rename_idx $mark_rename; MarkList_LeaveRename; break}
      bind .mren.e <Escape> {MarkList_LeaveRename; break}
      bind .mren.e <Leave> {MarkList_LeaveRename; break}
      bind .mren.e <FocusOut> {MarkList_LeaveRename; break}
      focus .mren.e

      .mren.e selection clear
      if {[regexp {^(FN:)?\d+ } $mark_rename match]} {
        set off [string length $match]
      } else {
        set off 0
      }
      .mren.e selection from $off
      .mren.e selection to end
      .mren.e icursor $off
    }
  }
}


#
# This function is bound to all events which signal an exit of the rename
# dialog window. The window is destroyed.
#
proc MarkList_LeaveRename {} {
  focus .dlg_mark.l
  destroy .mren
  unset -nocomplain mark_rename mark_rename_idx
}


# ----------------------------------------------------------------------------
#
# This helper function determines which line is selected in the tag list.
#
proc TagsList_GetSelectedIdx {} {
  global dlg_tags_list

  set sel [.dlg_tags.l curselection]
  if {([llength $sel] == 1) && ($sel < [llength $dlg_tags_list])} {
    set idx $sel
  } else {
    set idx -1
  }
  return $idx
}


#
# This function is bound to changes of the selection in the color tags list.
#
proc TagsList_Selection {} {
  global dlg_tags_list

  set idx [TagsList_GetSelectedIdx]
  if {$idx >= 0} {
    set line [lindex $dlg_tags_list $idx]
    # TODO
  }
}


#
# This function inserts a color tag text into the listbox and sets its
# foreground and background colors.
#
proc TagsList_Insert {pos tag_idx} {
  global patlist

  set w [lindex $patlist $tag_idx]

  .dlg_tags.l insert $pos [lindex $w 0]
  .dlg_tags.l itemconfigure $pos -background [lindex $w 1] -foreground [lindex $w 2]
}


#
# This function fills the color tags list dialog window with all color tags.
#
proc TagsList_Fill {} {
  global dlg_tags_shown dlg_tags_list patlist

  if [info exists dlg_tags_shown] {
    set dlg_tags_list {}
    .dlg_tags.l delete 0 end

    set tag_idx 0
    foreach w $patlist {
      lappend dlg_tags_list $tag_idx

      TagsList_Insert end $tag_idx
      incr tag_idx
    }
  }
}


#
# This function allows to edit a color assigned to a tags entry.
#
proc TagsList_EditColor {tag_idx is_fg} {
  global dlg_tags_list patlist

  set w [lindex $patlist $tag_idx]
  if $is_fg {
    set col_idx 2
    set what foreground
  } else {
    set col_idx 1
    set what background
  }

  set col [lindex $w $col_idx]
  set col [tk_chooseColor -initialcolor $col \
                          -parent .dlg_tags -title "Select $what color"]

  if {$col ne ""} {
    set w [lreplace $w $col_idx $col_idx $col]
    if {[catch {.dlg_tags.l itemconfigure $tag_idx -background [lindex $w 1] -foreground [lindex $w 2]}] == 0} {

      set patlist [lreplace $patlist $tag_idx $tag_idx $w]

      set tagnam "tag$tag_idx"
      .f1.t tag configure $tagnam -background [lindex $w 1] -foreground [lindex $w 2]
    }
  }
}


#
# This function pops up a context menu for the color tags list dialog.
#
proc TagsList_ContextMenu {xcoo ycoo} {
  global dlg_tags_list

  set idx [.dlg_tags.l index "@$xcoo,$ycoo"]
  if {([llength $idx] > 0) && ($idx < [llength $dlg_tags_list])} {
    .dlg_tags.l selection clear 0 end
    .dlg_tags.l selection set $idx

    .dlg_tags.ctxmen delete 0 end
    .dlg_tags.ctxmen add command -label "Edit background color" -command [list TagsList_EditColor $idx 0]
    .dlg_tags.ctxmen add command -label "Edit foreground color" -command [list TagsList_EditColor $idx 1]
    .dlg_tags.ctxmen add command -label "Edit pattern" -state disabled
    .dlg_tags.ctxmen add separator
    .dlg_tags.ctxmen add command -label "Remove this entry" -state disabled
    .dlg_tags.ctxmen add command -label "Add new entry" -state disabled

    set rootx [expr [winfo rootx .dlg_tags] + $xcoo]
    set rooty [expr [winfo rooty .dlg_tags] + $ycoo]
    tk_popup .dlg_tags.ctxmen $rootx $rooty 0
  }
}


#
# This function creates or raises the color highlighting tags list dialog.
# This dialog shows all currently defined tag assignments.
#
proc Tags_OpenDialog {} {
  global font_normal dlg_tags_shown dlg_tags_size

  if {![info exists dlg_tags_shown]} {
    toplevel .dlg_tags
    wm title .dlg_tags "Color highlights list"
    wm group .dlg_tags .

    listbox .dlg_tags.l -width 40 -height 10 -font $font_normal -exportselection false \
                        -cursor top_left_arrow -yscrollcommand {.dlg_tags.sb set} \
                        -selectmode browse
    pack .dlg_tags.l -side left -fill both -expand 1
    scrollbar .dlg_tags.sb -orient vertical -command {.dlg_tags.l yview} -takefocus 0
    pack .dlg_tags.sb -side left -fill y

    bind .dlg_tags.l <<ListboxSelect>> {TagsList_Selection; break}
    #bind .dlg_tags.l <Delete> {TagsList_DeleteSelection; break}
    bind .dlg_tags.l <ButtonRelease-3> {TagsList_ContextMenu %x %y}
    focus .dlg_tags.l

    menu .dlg_tags.ctxmen -tearoff 0

    set dlg_tags_shown 1
    bind .dlg_tags.l <Destroy> {+ unset -nocomplain dlg_tags_shown}
    bind .dlg_tags <Configure> {TestCaseList_Resize %W .dlg_tags dlg_tags_size}
    if [info exists dlg_tags_size] {
      wm geometry .dlg_tags $dlg_tags_size
    }

    TagsList_Fill

  } else {
    wm deiconify .dlg_tags
    raise .dlg_tags
  }
}


#
# This function is bound to configure events on dialog windows, i.e. called
# when the window size or stacking changes. The function stores the new size
# so that the same size can be used when the window is closed and re-opened.
#
proc TestCaseList_Resize {wid top var} {
  upvar $var size

  if {$wid eq $top} {
    set size "[winfo width $wid]x[winfo height $wid]"
  }
}


#
# This function loads a trace log via stdin. Afterwards color highlighting
# is initiated.
#
proc LoadStream {} {
  update
  while 1 {
    set data [read stdin]
    if {[string length $data] == 0} {
      break
    }
    .f1.t insert end $data
  }
  .f1.t see end
  update
  HighlightInit
}


#
# This function loads a trace from a file. Afterwards color highlighting
# is initiated.
#
proc LoadFile {filename} {
  set file [open $filename r]
  .f1.t insert end [read $file]
  .f1.t see end
  close $file

  update
  HighlightInit
}


#
# This function loads a new text.
#
proc MenuOpenFile {} {
  #-initialfile [file tail $dumpdb_filename]
  #-initialdir [file dirname $dumpdb_filename]
  set filename [tk_getOpenFile -parent . -filetypes {{"trace" {out.*}} {all {*.*}}}]
  if {$filename ne ""} {
    #.f1.t delete 1.0 end
    # FIXME delete with 1000s of tags is extremely slow
    destroy .f1.t
    text .f1.t -width 100 -height 50 -wrap none -undo 0 \
            -font $::font_normal -cursor top_left_arrow -relief flat \
            -exportselection false \
            -yscrollcommand {.f1.sb set}
    pack .f1.t -side left -fill both -expand 1 -before .f1.sb
    # MAJOR FIXME: need to re-assign all bindings
    wm title . "$filename - Trace browser"
    update
    SearchReset
    array unset ::mark_list

    LoadFile $filename
  }
}


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

# These variables contain search options which can be set by checkboxes.
set tlb_case 0
set tlb_hall 0

# This variable stores the search direction: 0:=backwards, 1:=forwards
set tlb_last_dir 1

# These variables are used while keyboard focus is in the search entry field.
#unset tlb_hist_pos
#unset tlb_inc_base
#unset tlb_inc_yview

# This hash array stors the bookmark list. It's indices are text line numbers.
array set mark_list {}

# This list stores text patterns and associated colors for color-highlighting
# in the main text window. Each list entry is again a list:
# 0: sub-string (case sensitive)
# 1: foreground color
# 2: background color
set patlist {}

# These variables are used by the bookmark list dialog.
#set dlg_mark_list {}
#unset dlg_mark_shown
#unset dlg_mark_size


#
# Main
#
if {[catch {tk appname "trowser"}]} {
  # this error occurs when the display connection is refused etc.
  puts stderr "Tk initialization failed"
  exit
}
InitResources
CreateLogBrowser

if {$argc == 0} {
  LoadStream
  wm title . "Trace browser"
} elseif {$argc == 1} {
  LoadFile [lindex $argv 0]
  wm title . "[lindex $argv 0] - Trace browser"
} else {
  puts stderr "Usage: $argv0 <file>"
}

# done - all following actions are event-driven
# the application exits when the main window is closed
