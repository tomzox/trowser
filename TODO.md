
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

- search list:
  + view adaption no longer implemented (i.e. align yview with main window)
  + allow multiple instances of the search list
- allow definition of stack of search patterns for search list?
  + to allow quick folding/unfolding in search result list
  + to replace find in highlight pattern dialog
- search history should remember & display reg.exp. and case options
- highlight color not updated in bookmarks and search results dialog after edit
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

