# Makefile for building embedded application.
# by Brian Fraser

# Edit this file to compile extra C files into their own programs.
VPATH = c_code
TARGET= beatbox

# Node variables
NODE_PROJECT =beatbox-server
NODE_DEPLOY_PATH = $(HOME)/cmpt433/public/node/$(NODE_PROJECT)-copy
NODE_FILES_PATH = nodejs

PUBDIR = $(HOME)/cmpt433/public/myApps
OUTDIR = $(PUBDIR)

CROSS_TOOL = arm-linux-gnueabihf-
CC_CPP = $(CROSS_TOOL)g++
CC_C = $(CROSS_TOOL)gcc

CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -Werror

# Asound process:
# get alibsound2 lib on target:
# 	# apt-get install libasound2
# Copy target's /usr/lib/arm-linux-gnueabihf/libasound.so.2.0.0 
#      to host  ~/public/asound_lib_BBB/libasound.so
# Copy to just base library:

LFLAGS = -L$(HOME)/cmpt433/public/asound_lib_BBB

# -pg for supporting gprof profiling.
#CFLAGS += -pg

all: beatbox deploy node_install
	cp $(TARGET) $(OUTDIR)/$(TARGET)

%.o : %.c
	$(CC_C) -c $(CFLAGS) $< $(LFLAGS) -lpthread -lasound

# Copy wave files to the shared folder
beatbox: main.o audioMixer.o zenController.o udpListener.o
	$(CC_C) $(CFLAGS) -o $(TARGET) main.o audioMixer.o zenController.o udpListener.o $(LFLAGS) -lpthread -lasound
	mkdir -p $(PUBDIR)/beatbox-wav-files/
	cp wave-files/* $(PUBDIR)/beatbox-wav-files/ 

clean:
	rm *.o
	rm -f $(TARGET)
	rm -f $(OUTDIR)/$(TARGET)

# Deploy this Node.js project to the public folder
deploy:
	@echo 'COPYING THE NODE.JS FILES TO $(NODE_DEPLOY_PATH)'
	@echo ''
	mkdir -p $(NODE_DEPLOY_PATH)
	chmod a+rwx $(NODE_DEPLOY_PATH)
	cp -r $(NODE_FILES_PATH)/* $(NODE_DEPLOY_PATH)
	@echo 'Do not edit any files in this folder; they are copied!' > $(NODE_DEPLOY_PATH)/DO_NOT_EDIT-FILES_COPIED.txt
	@echo ''
	@echo 'NOTE: On the host, in $(NODE_DEPLOY_PATH), it is best to run: npm install'
	@echo '      Or, just run the node_install target in this makefile.'

node_install:
	@echo ''
	@echo ''
	@echo 'INSTALLING REQUIRED NODE PACKAGES'
	@echo '(This may take some time)'
	@echo ''
	cd $(NODE_DEPLOY_PATH) && npm install