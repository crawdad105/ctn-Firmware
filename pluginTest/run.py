# hexdump -v -e '16/1 "%02X " "\n"' asmCode/pluginTest/pluginTest.cplg

# sections:
#   arm-none-eabi-readelf -S asmCode/pluginTest/3ModPlugin.o
# reloc:
#   arm-none-eabi-objdump -r asmCode/pluginTest/3ModPlugin.o
# code:
#   arm-none-eabi-objdump -d asmCode/pluginTest/3ModPlugin.o
# raw dump:
#   arm-none-eabi-objdump -s asmCode/pluginTest/3ModPlugin.o

def Error(string):
    print(f"ERROR:\n  {string}")

import sys
if sys.argv.__len__() < 2:
    print(f"No file attached")
    exit(1)

import subprocess
# arm-none-eabi-gcc -march=armv6 -marm -O0 -fno-builtin -nostdlib -ffreestanding -Wl,--oformat=binary -fPIC -c asmCode/pluginTest/pluginTest.c -o asmCode/pluginTest/pluginTest.o

# arm-none-eabi-gcc
# -march=armv6
# -marm
# -O0
# -fno-builtin
# -nostdlib
# -ffreestanding
# -Wl,--oformat=binary
# -fPIC
# -c
# asmCode/pluginTest/pluginTest.c
# -o
# asmCode/pluginTest/pluginTest.o

file = sys.argv[1] # use first file as main
print(f"Compiling: {file}")
result = subprocess.run([
    "arm-none-eabi-gcc", 
    "-march=armv6", 
    "-marm", 
    "-O0", 
    "-fno-builtin", 
    "-nostdlib", 
    "-ffreestanding", 
    "-Wl,--oformat=binary", 
    #"-fPIC",
    "-no-pie", 
    "-static", 
    "-c", 
   f"{file if not file.endswith(".c") else file[0:len(file) - 2]}.c", 
    "-o", 
   f"{file}.o"  
], text=True)
if result.returncode != 0:
    print(f"Failed to compile: {file}")
    exit(1)

# arm-none-eabi-readelf -S {file}.o
elfOutput = subprocess.check_output(["arm-none-eabi-readelf", "-S", f"{file}.o"], text=True)

import re
pattern = re.compile(r"\[(.*)\] (.*?) +(.*?) +([a-fA-F0-9]+) ([a-fA-F0-9]+) ([a-fA-F0-9]+)")

validSections = [ ".text", ".data", ".bss", ".rodata" ]
sections = [ ]
for elm in validSections:
    sections.append([elm, 0, 0])

runtimeSize = 0
for line in elfOutput.splitlines():
    m = pattern.search(line)
    if m:
        name = m.group(2)
        offset = int(m.group(5), 16)
        size = int(m.group(6), 16)
        if size > 0:
            if validSections.__contains__(name):
                sections[validSections.index(name)] = [name, offset, size]
                print(f"{name}\n  offset: {hex(offset)}\n  size: {hex(size)}")
                runtimeSize += size
            elif name == ".data" or name == ".bss":
                Error(f"Invalid section: {name}. | Only {validSections} are supported")
                quit(1)

print(f"Runtime size: {hex(runtimeSize)}")
f = open(f"{file}.o", "rb")
bytes = f.read()
f.close()

sectionBytes = []
for name, offset, size in sections:
    sectionBytes.append([name, bytes[offset : offset + size]])

# arm-none-eabi-objdump -r {file}.o
relocOutput = subprocess.check_output(["arm-none-eabi-objdump", "-r", f"{file}.o"], text=True)
rolocPatern = re.compile(r"([a-fA-F0-9]{8}) (.*?) +(.*)")
relocSectionPatern = re.compile(r"RELOCATION RECORDS FOR \[(.*)\]:")
curRelocSection = ""
relocs = []
validRelocTypes = [ "R_ARM_REL32", "R_ARM_CALL", "R_ARM_ABS32" ] # DO NOT REORDER
symbols: list[str] = []
for line in relocOutput.splitlines():
    m = rolocPatern.search(line)
    if m:
        type = m.group(2)
        data = m.group(3)

        if type == "R_ARM_REL32" or type == "R_ARM_CALL" or type == "R_ARM_ABS32":
            if type == "R_ARM_CALL":
                validCalls = [ "memcpy", "memcmp", "memset" ]
                if validCalls.__contains__(data):
                    symbols.append(data)
                    # print(f"reloc: {m.group(1)} ({curRelocSection}, {data})")
                    _offset = int(m.group(1), 16)
                    _section = validSections.index(curRelocSection)
                    _reference = validCalls.index(data) # not used in decoding
                    _relocationType = validRelocTypes.index(type)
                    _isCall = 1
                    relocs.append([_offset, _section, _reference, _relocationType, _isCall])
                else:
                    Error(f"Invalid call symbol: {data}. | Only {validCalls} and supported")
                    quit(1)
            elif validSections.__contains__(data): # R_ARM_ABS32 can have a function (data could be a functions name), we didn't implement that
                # print(f"reloc: {m.group(1)} ({curRelocSection}, {data})")
                _offset = int(m.group(1), 16)
                _section = validSections.index(curRelocSection)
                _reference = validSections.index(data)
                _relocationType = validRelocTypes.index(type)
                _isCall = 0
                relocs.append([_offset, _section, _reference, _relocationType, _isCall])
            else:
                Error(f"Invalid relocation section: {data}. | Only {validSections} supported")
                quit(1)
        else:
            Error(f"Invalid relocation type: {type}. | Only \"R_ARM_REL32\" supported")
            quit(1)
    else:
        m = relocSectionPatern.search(line)
        if m:
            curRelocSection = m.group(1)

def writeInt(f, i):
    f.write(struct.pack("<I", i))
    # print(f"wrote int: {hex(i)}")
def writeArr(f, arr):
    f.write(arr)
    # print(f"wrote arr: {arr.__len__()} len")

import struct
outPath = file[0:len(file) - 2] if file.endswith(".c") else file
outPath += ".cplg"
with open(f"{outPath}", "wb") as f:
    writeInt(f, 1) # write version (does not match api version)
    for name, offset, size in sections:
        writeInt(f, size)
    for name, sec_data in sectionBytes:    
        writeArr(f, sec_data)
        align = 4
        if len(sec_data) % align != 0:
            for i in range((align - len(sec_data) % align)):
                writeArr(f, b"\xFF")
    writeInt(f, len(relocs))
    for offset, secId1, refId, relocId, isCall in relocs:
        writeInt(f, offset)
        writeInt(f, (secId1 + (refId << 8) + (relocId << 16) + (isCall << 24)))
    for symbol in symbols:
        writeArr(f, symbol.encode("ascii"))
        writeArr(f, b"\x00")

print(f"complete {outPath}")