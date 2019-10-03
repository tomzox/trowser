
- search list insert gets exponentially slower when many lines are in dialog
- search list undo for ALT-A: lines are fragmented; limit size of history
- during search in search list: lock manual remove/insert
- allow vim key bindings in filter window (at least for scrolling)
- newly added color highlight in main window: tags not added to filter win
- progress bar on ALT-f in search list window
  OR: optimize by searching for frame separators first
- show bookmarks in filter window
- pipe load: main menu "continue loading from STDIN" in tail mode doesn't
  give chance to switch to "head" mode

checked in:
+ allow undo for delete in search filter window
+ search n,N in search filter dialog should be restricted to dialog lines
+ CTRL-+/- doesn't change font size in highlights tags (bold text)
+ enable line-wrap in main window via ALT-w
+ don't raise search window upon "i" in main window
+ optimisation: temp. disable reg-exp for cur. search if pattern is plain text
+ bug: fast typing search expr. + hit RETURN -> match on partial text only
+ search filter dialog: new mode to display FN deltas instead of abs. FN
+ control menu command "discard text": display line count + percentage
+ ctrl-g: display number of characters
+ incremental search: view jiggling when appending chars to search expr.
+ Key: /,RETURN: should repeat search, but does nothing
+ search string not always copied to stack (e.g. All)
+ search history not updated when searching via search history dialog
+ reverse order of search history
+ "Save as" in filter dialog saves the main text instead of the filtered text
+ "Save as" in filter dialog: add option to save line numbers only
+ tcl error when hitting "i" twice in main window
+ suspend background tasks while other bg tasks are active


- allow multiple instances of the search list?
- allow definition of stack of search patterns for search list?
  + to allow quick folding/unfolding in search result list
  + to replace find in highlight pattern dialog
- search history should remember & display reg.exp. and case options
- highlight color not updated in bookmarks and search results dialog after edit
- initial highlight search has no line count limit -> too slow with reg-exp's
- make "next" search interruptible?
  + place grab on progress bar with cancel button
- storing: offer to update old bookmark file w/o save dialog & confirmation
- add info to bookmarks file to adjust bookmark position
  + use TDMA frame number
  + include buffer size limit and head/tail option

- "b" across line start: should jump to start of last word, not eol
- implement multi-key bindings in repeat dialog

- switching between multiple "personalities":
  - different highlight patterns & default fg/bg color (allow copy/paste of patterns)
  - different bookmark name rules: TDMA FN, backtrace cycle num
  - different search histories

- pipe: title missing
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

