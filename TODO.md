
- command line options to read bookmarks or line list
- search list: allow to pipe text through external command (grep -n)
- search list: limit size of undo history?
- search history: allow prev/next on multiple entries
- bookmarks list not updated after adding/editing highlight patterns
- allow vim key bindings in filter window (at least for scrolling)
- vim selection controls (ctrl-v et.al.)
- undo/redo in bookmark list (removals)
- pipe load: main menu "continue loading from STDIN" in tail mode doesn't
  give chance to switch to "head" mode

- allow multiple instances of the search list?
- allow definition of stack of search patterns for search list?
  + to allow quick folding/unfolding in search result list
  + to replace find in highlight pattern dialog
  + PROPOSAL:
    - like history dialog, but with checkbuttons
    - can "undo" also older entries (i.e. not only oldest)
    - each entry must contain all lines (including overlap)
    - manual removal: removes from all entries (undo only via "u", not stack)
- make "next" search interruptible?
  + place grab on progress bar with cancel button
  + use "ESC" key
  + or use option to limit "search all" (menu cmd. or a la "bottom 1E6 only")
- detect concurrent changes in RC file
  + merge search history (& others?) during saving
  + but: do not import search history
- add info to bookmarks file to adjust bookmark position
  + use TDMA frame number
  + include buffer size limit and head/tail option
- frame numbers: option for mode which works across cell change?
  + OR new FN mode: FN delta in-between each line?
  + would require to count each frame number change between search matches

- "b" across line start: should jump to start of last word, not eol
- implement multi-key bindings in repeat dialog

- switching between multiple "personalities":
  - different highlight patterns & default fg/bg color (allow copy/paste of patterns)
  - different bookmark name rules: TDMA FN, backtrace cycle num
  - different search histories

- pipe: title missing
  + page mode: load one buffer's size at a time, allow user to browse
  + remove grab while waiting for user, display some data?
  + allow to use grep to determine stop of input skip
  + allow to keep dialog open after text insert?
- display dialog for files which exceed buffer limit:
  + total size
  + spinbox for buffer size
  + radio: head, tail
  + also offer these options upon "reload" command

- options dialog
  + open windows upon start: bookmark/highlights/search-hist/search-list
  + rc: save {main window | dialog} {nil | size | size & position}
  + rc: save -head and -tail buffer limit OR: define default
  + rc: save search options yes/no OR: default
  + rc: search history max. length
  + define regexp for frame separators and frame numbers OR nil
  + define search highlight mark-up
  + main fore- and background color
  + default markup for new highlights (currently search)
  + min. no. lines before/after line matching search
  + cursor placement after start: top/bottom
  + allow to use vim compatible directions for search "n" / "N"
  + option to wrap-around search at file end
  + storing: option to update old bookmark file w/o save dialog & confirmation

Use balanced tree for search list?
- isempty
- get all elements as flat list
- clear tree
- get nth element (context menu)
- search element with value <= x
- insert element value x (after known n)
- remove elements at indices (n...m)

