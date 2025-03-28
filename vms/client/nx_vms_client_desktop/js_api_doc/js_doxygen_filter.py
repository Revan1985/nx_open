#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import re

comment = re.compile("^\s+\*")
array = re.compile("QList<(\w+)[&*]?>")
args = re.compile("(const )?([\w]+)[&*] ")
optional = re.compile("std::optional<(.*?)>")
function = re.compile("([^\s]+\s[^\s]+\(.*\))( const;|;)")
defaultValue = re.compile(" = [^;()]+;")

replaceMap = {
    "QJsonValue": "Any",
    "QString": "String",
    "QJsonObject": "Object",
    "QVariantMap": "Object",
    "ResourceUniqueId": "String",
    "QUuid": "String",
    "nx::Uuid": "String",
    "Uuid": "String",
    "Q_INVOKABLE": "",
    "double": "Number",
    "int": "Number",
    "void": "",
    "std::chrono::milliseconds": "Number",
}

def transform(line):
    if comment.search(line):
        return line

    result = array.sub("Array<\\1>", line)
    result = args.sub("\\2 ", result)
    result = optional.sub("\\1", result)
    result = defaultValue.sub(";", result)
    result = function.sub("async \\1;", result)

    for item in replaceMap:
        result = re.sub(f"(^|\s+|\W){item}(\s)", f"\\1{replaceMap[item]}\\2", result)

    return result

def filter(fileName):
    skip = not (fileName.endswith(".h") or fileName.endswith(".cpp"))

    with open(fileName, "r", encoding="utf8") as file:
        for line in file:
            print(line if skip else transform(line), end="")

# Reads a C++ file and outputs it with some transformations so doxygen can use it as a filter to
# make syntax in the documentation look like JavaScript.
def main():
    parser = argparse.ArgumentParser(description="Doxygen JavaScript filter")
    parser.add_argument("file")
    args = parser.parse_args()
    filter(args.file)

if __name__ == "__main__":
    main()
