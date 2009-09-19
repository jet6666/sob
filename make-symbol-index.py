import os
import re

p = re.compile('\\);">');
symbols = []

for filep in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
    file = "all-index-" + filep + ".html"
    print "*** Processing " + file + " ***"
    f = open(file)
    try:
        for line in f:
            r = p.search(line)
            if r != None:
                line = line[r.start()+4:]
                if line[0] != ' ':
                    line = line.replace("<i>", "");
                    line = line.replace("</i>", "");
                    line = line.replace("-", "");
                    line = line[:line.find("</a>")]
                    symbols.append(line)
    finally:
        f.close()

symbols.append('flash*')
symbols.append('http://*')
symbols.append('Boot_*')
symbols.append('void')
f = open("symbols.txt", "w")
symbols = sorted(set(symbols))
for symbol in symbols:
    f.write(symbol + '\n')
f.close();

print "Finished."

