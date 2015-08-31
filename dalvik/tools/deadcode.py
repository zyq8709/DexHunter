#!/usr/bin/env python

import os
import re
import sys

def SplitSections(buffer):
    """Spin through the input buffer looking for section header lines.
    When found, the name of the section is extracted.  The entire contents
    of that section is added to a result hashmap with the section name
    as the key"""

    # Match lines like
    #              |section_name:
    # capturing section_name
    headerPattern = re.compile(r'^\s+\|([a-z _]+)\:$', re.MULTILINE)

    sections = {}
    start = 0
    anchor = -1
    sectionName = ''

    while True:
        # Look for a section header
        result = headerPattern.search(buffer, start)

        # If there are no more, add a section from the last header to EOF
        if result is None:
            if anchor is not -1:
                sections[sectionName] = buffer[anchor]
            return sections

        # Add the lines from the last header, to this one to the sections
        # map indexed by the section name
        if anchor is not -1:
            sections[sectionName] = buffer[anchor:result.start()]

        sectionName = result.group(1)
        start = result.end()
        anchor = start

    return sections

def FindMethods(section):
    """Spin through the 'method code index' section and extract all
    method signatures.  When found, they are added to a result list."""

    # Match lines like:
    #             |[abcd] com/example/app/Class.method:(args)return
    # capturing the method signature
    methodPattern = re.compile(r'^\s+\|\[\w{4}\] (.*)$', re.MULTILINE)

    start = 0
    methods = []

    while True:
        # Look for a method name
        result = methodPattern.search(section, start)

        if result is None:
            return methods

        # Add the captured signature to the method list
        methods.append(result.group(1))
        start = result.end()

def CallsMethod(codes, method):
    """Spin through all the input method signatures.  For each one, return
    whether or not there is method invokation line in the codes section that
    lists the method as the target."""

    start = 0

    while True:
        # Find the next reference to the method signature
        match = codes.find(method, start)

        if match is -1:
            break;

        # Find the beginning of the line the method reference is on
        startOfLine = codes.rfind("\n", 0, match) + 1

        # If the word 'invoke' comes between the beginning of the line
        # and the method reference, then it is a call to that method rather
        # than the beginning of the code section for that method.
        if codes.find("invoke", startOfLine, match) is not -1:
            return True

        start = match + len(method)

    return False



def main():
    if len(sys.argv) is not 2 or not sys.argv[1].endswith(".jar"):
        print "Usage:", sys.argv[0], "<filename.jar>"
        sys.exit()

    command = 'dx --dex --dump-width=1000 --dump-to=-"" "%s"' % sys.argv[1]

    pipe = os.popen(command)

    # Read the whole dump file into memory
    data = pipe.read()
    sections = SplitSections(data)

    pipe.close()
    del(data)

    methods = FindMethods(sections['method code index'])
    codes = sections['codes']
    del(sections)

    print "Dead Methods:"
    count = 0

    for method in methods:
        if not CallsMethod(codes, method):
            print "\t", method
            count += 1

    if count is 0:
        print "\tNone"

if __name__ == '__main__':
    main()
