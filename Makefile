all: analyze sysmon

analyze: src/analyze.c
    gcc src/analyze.c -o build/analyze

sysmon: src/sysmon.c
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/src modules
    cp src/sysmon.ko build/sysmon.ko

clean:
    rm -f build/analyze
    rm -f build/sysmon.ko
