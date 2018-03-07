README for ng-jackspa
=====================

ng-jackspa is a set of simple interfaces that host a LADSPA plugin, providing
JACK ports for its audio inputs and outputs, and dynamic setting of its control
inputs.

njackspa::
	Visual interface for the terminal written using ncurses.

gjackspa::
	GTK+ graphical interface for X11 written using gtkmm-2.4.

qjackspa::
	Qt graphical interface for X11 written using Qt 4.

jackspa-cli::
	Command-line interface for the terminal without dynamic setting of the
	control inputs.

ng-jackspa is a fork/continuation of
link:http://code.google.com/p/jackspa/[jackspa].  ng-jackspa can stand either
for next generation jackspa or for ncurses/GTK+ jackspa.


Resources
---------

* For a more detailed description see the manual page linkpage:ng-jackspa[1].

* The link:https://gna.org/projects/ngjackspa[official homepage] is at Gna!.
  There you might find up-to-date information and releases.

* Release notes listing important user-visible changes are available in a
  separate file link:NEWS.html['NEWS'].


Installation instructions
-------------------------
Download links, installation instructions and a list of dependencies can be
found in the file link:INSTALL.html['INSTALL'].


// Information about bugs and limitations can be found in the file 'BUGS'.
include::BUGS[]


Credits & License
-----------------
ng-jackspa has been written by G.raud Meyer (g_raud chez gna.org).  The
original project of which it is a forked continuation was written by Nick
Thomas.  The ncurses interface is based on njconnect by Pawel/Xj.

ng-jackspa is free software; you can redistribute and/or modify if under the
terms of the GNU General Public License version 2 as published by the Free
Software Foundation.  The full text of the license can be found in the root of
the project sources, in the file 'COPYING'.  Otherwise see
<http://www.gnu.org/licenses/>.
