SOB, The SWF Obfuscator
=======================

   This is a SWF obfuscator written in C.  It works by replacing the strings in
 the SWF/ABC string table with random strings.  Since both methods and regular
 strings are placed in this table, it will only replace strings found in its
 symbol list while ignoring all strings that are found but have a dash in front
 of them.  See the output of sob --help for more information.

   The supplied symbols.txt works mostly with haXe generated SWF files.  If
 you use a 3rd party library or non-standard SWF framework (such as Flex) you
 may need to modify it.  I do not have Flex or any of Adobe's IDEs (i only
 use haXe) so i cannot test it there (and i don't have much of an interest to
 be honest).

   Keep in mind that this is old code that i've just tested to see if it still
 works and uploaded it to GitHub at https://github.com/badsector/sob - it may
 or may not work for you.

 Kostas "Bad Sector" Michalopoulos

