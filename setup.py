#!/usr/bin/env python3

#from distutils.core import setup
from setuptools import setup
import os
import subprocess
import sys

if os.path.exists('doc/trowser.pod'):
    proc_rst = subprocess.run(['tools/pod2help.py',
                               "-rst",
                               'doc/trowser.pod'],
                              stdout=subprocess.PIPE, text=True, check=True)
    long_description = proc_rst.stdout
else:
    long_description = None

if os.path.exists("trowser.py"):
    if not os.path.exists("bin"):
        os.mkdir("bin")

    with open("trowser.py", "r") as fin:
        with open("bin/trowser.py", "w") as fout:
            line = fin.readline()
            print("#!pythonw", file=fout)
            for line in fin:
                print(line, file=fout, end="")
    os.chmod("bin/trowser.py", 0o755)

setup(
    name='trowser',
    version='2.1.0',
    scripts=['bin/trowser.py'],
    packages=[],

    author='T. Zoerner',
    author_email='tomzox@gmail.com',

    url='https://github.com/tomzox/trowser',

    description='Trowser is a graphical browser for large line-oriented text files ' \
                'with sytax highlighting and search facilities tailored for analysis ' \
                'of debug log files.',

    long_description=long_description,
    long_description_content_type='text/x-rst',

    classifiers=[
          'Topic :: Software Development :: Testing',
          'Development Status :: 5 - Production/Stable',
          'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
          'Programming Language :: Python :: 3',
          'Operating System :: POSIX',
          'Operating System :: Microsoft :: Windows',
          'Environment :: X11 Applications',
          'Environment :: Win32 (MS Windows)',
          'Intended Audience :: Developers',
          ],
    keywords=['color-highlighter', 'text-search', 'text-browser', 'tkinter-gui', 'GUI'],
    platforms=['posix', 'win32'],
)
