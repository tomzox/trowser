#!/usr/bin/env python3
#
#  Script for converting POD manpage to help dialog text.
#  (Originally developed in Perl language for the "nxtvepg" project.)
#
#  Copyright (C) 1999-2011, 2020-2021, 2023 T. Zoerner
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License Version 2 as
#  published by the Free Software Foundation. You find a copy of this
#  license in the file COPYRIGHT in the root directory of this release.
#
#  THIS PROGRAM IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL,
#  BUT WITHOUT ANY WARRANTY; WITHOUT EVEN THE IMPLIED WARRANTY OF
#  MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#
#  Description:
#
#    Reads the manpage in POD format and creates a Python script defining texts
#    for the help dialog. The script will be included in the package. See the
#    Perl manual page 'perlpod' for details on the POD syntax.
#

import re
import sys

started = False
over = False
sectIndex = 0
subIndex = 0
helpSections = ""
helpIndex = ""
helpTexts = {}

docTitle = "trowser := Trace Browser"
rstOutput = docTitle + "\n" + ("=" * len(docTitle)) + "\n\n"
doRst = False

def ReplaceEntity(match):
   tag = match.group(1)

   if   tag == "lt"    : return "<"
   elif tag == "gt"    : return ">"
   elif tag == "auml"  : return "ae"
   elif tag == "eacute": return "e"
   else:
      print("Unknown entity E<%s>" % tag, file=sys.stderr)
      sys.exit(1)

def Escape(txt):
    if txt.endswith("'"):
        return txt[:-1] + r"\'"
    else:
        return txt

def PrintParagraph(astr, indent, bullet=False):
    global rstOutput

    if indent:
        rstOutput += "  "

    re.sub(r"E<([a-z]+)>", ReplaceEntity, astr)

    prev_end = 0
    for match in re.finditer(r'([STHIFCLBP])<("([\x00-\xff]*?)"|[^">][^>]*?)>', astr):
        if prev_end < match.start():
            txt = astr[prev_end : match.start()]
            helpTexts[sectIndex] += "('''%s''', '%s'), " % (Escape(txt), "indent" if indent else "")
            rstOutput += txt
        prev_end = match.end()

        tag = match.group(1)
        chunk = match.group(2)

        if chunk[0] == '"':
            chunk = chunk.strip('"')

        txt = chunk

        if tag == "B":
            fmt = "bold"
        elif tag == "I" or tag == "F":
            fmt = "underlined"
        elif tag == "C":
            fmt = "fixed"
        elif tag == "P":
            fmt = "pfixed"
        elif tag == "T":
            fmt = "title1"
        elif tag == "H":
            fmt = "title2"
        elif tag == "L":
            fmt = "href"
            match = re.match(r"(.*?)/(.*)", chunk)
            if match:
                txt = "%s: %s" % (match.group(1).capitalize(), match.group(2).capitalize())
            else:
                txt = chunk.capitalize()
        elif tag == "S":
            fmt = ""
        else:
            raise BaseException("tag error:" + tag)

        if indent:
            fmt = ("('%s', 'indent')" % fmt) if fmt else "'indent'"
        else:
            fmt = "'%s'" % fmt

        helpTexts[sectIndex] += "('''%s''', %s), " % (Escape(txt), fmt)

        # ----------------------------

        if tag != "S":
            if rstOutput and not rstOutput[-1].isspace():
                rstOutput += "\\ "

        if tag == "B":
            rstOutput += "**" + chunk + "**"
        elif tag == "I":
            rstOutput += "*" + chunk + "*"
        elif tag == "C" or tag == "F":
            rstOutput += "``" + chunk + "``"
        elif tag == "P":
            rstOutput += "::\n\n" + chunk
        elif tag == "T":
            rstOutput += chunk + "\n" + ("-" * len(chunk))
        elif tag == "H":
            rstOutput += chunk + "\n" + ("~" * len(chunk))
        elif tag == "L":
            if re.match(r"^\S+\([1-8][a-z]*\)$", chunk):  # UNIX man page
                rstOutput += "*" + chunk + "*"
            else:
                match = re.match(r"(.*?)/(.*)", chunk)
                if match:
                    link = match.group(2)
                else:
                    link = chunk
                rstOutput += "`" + link.capitalize() + "`_"
        elif tag == "S":
            rstOutput += chunk
        else:
            raise BaseException("tag error:" + tag)

    if prev_end < len(astr):
        txt = astr[prev_end:]
        helpTexts[sectIndex] += "('''%s''', '%s'), " % (Escape(txt), "indent" if indent else "")
        rstOutput += txt

    if not bullet:
        rstOutput += "\n"



# read the complete paragraph, i.e. until an empty line
def ReadParagraph(f):
    line = ""
    while True:
        chunk = f.readline()
        if not chunk:
            return

        chunk = chunk.rstrip()
        if len(chunk) == 0 and line:
            break

        line += chunk

        # insert whitespace to separate lines inside the paragraph,
        # except for pre-formatted paragraphs in which the newline is kept
        if re.match(r"^\s+\S", line):
            line += "\n"
        elif line:
            line += " "

    # remove white space at line end
    return line.rstrip()


if (len(sys.argv) != 3) or (sys.argv[1] != "-help" and sys.argv[1] != "-rst"):
    print("Usage: %s [-help|-rst] input" % sys.argv[0], file=sys.stderr)
    sys.exit(1)

if sys.argv[1] == "-rst":
    doRst = True

f = open(sys.argv[2])

# process every text line of the manpage
while True:
    line = ReadParagraph(f)

    if not line:
        print("Ran into EOF - where's section 'DESCRIPTION'?", file=sys.stderr)
        sys.exit(1)

    # check for command paragraphs and process its command
    if line.startswith("=head1 "):
        title = line.partition(" ")[2].strip()

        # skip UNIX manpage specific until 'DESCRIPTION' chapter
        if started or (title == "DESCRIPTION"):
            if started:
                # close the string of the previous chapter
                helpTexts[sectIndex] += ")\n"
                sectIndex += 1

            # skip the last chapters
            if (title == "AUTHOR") or (title == "SEE ALSO"):
                break

            # initialize new chapter
            started = True
            title = title.capitalize()
            subIndex = 0

            # build array of chapter names for access from help buttons in popups
            helpIndex += "helpIndex['%s'] = %s\n" % (title, sectIndex)

            # put chapter heading at the front of the chapter
            helpTexts[sectIndex] = "helpTexts[%d] = (" % sectIndex
            PrintParagraph("T<%s>\n" % title, False)

    elif started:
        if line.startswith("=head2 "):
            if over:
                print("Format error: =head2 within =over (without =back)", file=sys.stderr)
            title = line.partition(" ")[2].strip()
            subIndex += 1
            helpSections += "helpSections[(%d,%d)] = '''%s'''\n" % (sectIndex, subIndex, title)
            # sub-header: handle like a regular paragraph, just in bold
            PrintParagraph("H<%s>\n" % title, False)

        elif line.startswith("=over"):
            if over:
                print("Format error: =over nesting is unsupported", file=sys.stderr)
            # start of an indented paragraph or a list
            over = True

        elif line.startswith("=back"):
            # end of an indented paragraph or list
            over = False

        elif line.startswith("=item "):
            if not over:
                print("Format error: =item outside of =over/=back", file=sys.stderr)
            title = line.partition(" ")[2].strip()
            # start a new list item, with a bullet at start of line or a title
            if title != "*":
                PrintParagraph("%s\n" % title, False, True)
            else:
                rstOutput += "- "

        else:
            # this is a regular paragraph
            # check for a pre-formatted paragraph: starts with white-space
            if re.match(r"^\s+(\S.*)", line):
                # add space after backslashes before newlines
                # to prevent interpretation by Python
                line = re.sub(r"\\\n", r"\\ \n", line);
                PrintParagraph("P<\"%s\n\n\">" % line, over)

            else:
                # append text of an ordinary paragraph to the current chapter
                if line and not line.isspace():
                    PrintParagraph(line + "\n", over)

if not doRst:
    print("# This file is automatically generated - do not edit")
    print("# Generated by %s from %s\n" % (sys.argv[0], sys.argv[2]))

    print("helpIndex = {}")
    print(helpIndex)

    print("helpSections = {}")
    print(helpSections)

    print("helpTexts = {}")
    for idx in range(sectIndex):
        print(helpTexts[idx])

else:
    print(rstOutput)
