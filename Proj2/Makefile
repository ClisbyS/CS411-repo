PUBLICH = macros.h list.h
PRIVATEH = privatestructs.h
SCHEDULE = schedule.c schedule.h

.PHONY: default
default: clear app

.PHONY: clear
clear:
	clear; clear
	
.PHONY: clean
clean:
	rm -f *.o
	rm -f *.gch
	rm -f vmsched
	rm -f *.a

.PHONY: lib
lib: cpu.o cpuinit.o
	ar rcs libvm.a cpu.o cpuinit.o

app: cpu.o cpuinit.o  schedule.o
	gcc -o vmsched cpuinit.o cpu.o schedule.o

cpu.o: cpu.c $(SCHEDULE) $(PUBLICH) $(PRIVATEH)
	gcc -c cpu.c

cpuinit.o: cpuinit.c $(SCHEDULE) $(PUBLICH) $(PRIVATEH)
	gcc -c cpuinit.c

schedule.o: $(SCHEDULE) $(PUBLICH)
	gcc -c schedule.c
