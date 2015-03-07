###############################################################################
## List of subdirectories
###############################################################################
FORCE   = 0
VERBOSE = 0
EXPECTED = '*'

ifeq ($(VERBOSE), 1)
	VERBOSE = --verbose
else
	VERBOSE = 
endif

ifeq ($(FORCE), 1)
	FORCE = --force
else
	FORCE = 
endif

ifeq ($(OS),Windows_NT)
	PORT = COM1
else
	PORT = /dev/ttyUSB0
endif

TOOL = ./tools/transfer.py

transfer:
	$(TOOL) --port $(PORT) --src $(PWD)/base $(VERBOSE) $(FORCE) --expected=$(EXPECTED)
	
input:
	$(TOOL) --port $(PORT) --src ./ --input 10 --verbose