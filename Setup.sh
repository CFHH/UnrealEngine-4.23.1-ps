#!/bin/bash
# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

set -e

cd "`dirname "$0"`"

if [ ! -f Engine/Binaries/DotNET/GitDependencies.exe ]; then
	echo "GitSetup ERROR: This script does not appear to be located \
       in the root UE4 directory and must be run from there."
	exit 1
fi 

if [ "$(uname)" = "Darwin" ]; then
	# Setup the git hooks
	if [ -d .git/hooks ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		rm -f .git/hooks/post-checkout
		rm -f .git/hooks/post-merge
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh .git/hooks/post-checkout
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh .git/hooks/post-merge
	fi

	# Get the dependencies for the first time
	Engine/Build/BatchFiles/Mac/GitDependencies.sh --prompt $@
else
	# Setup the git hooks
	if [ -d .git/hooks ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		echo \#!/bin/sh >.git/hooks/post-checkout
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >>.git/hooks/post-checkout
		chmod +x .git/hooks/post-checkout

		echo \#!/bin/sh >.git/hooks/post-merge
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >>.git/hooks/post-merge
		chmod +x .git/hooks/post-merge
	fi

	# Get the dependencies for the first time
	Engine/Build/BatchFiles/Linux/GitDependencies.sh --prompt $@

	echo Register the engine installation...
	if [ -f Engine/Binaries/Linux/UnrealVersionSelector-Linux-Shipping ]; then
		pushd Engine/Binaries/Linux > /dev/null
		./UnrealVersionSelector-Linux-Shipping -register > /dev/null &
		popd > /dev/null
	fi

	pushd Engine/Build/BatchFiles/Linux > /dev/null
	./Setup.sh "$@"
	popd > /dev/null
	
	# Verify that either curl or wget are available
	if which curl 1>/dev/null; then
		DOWNLOAD_COMMAND="curl -L -o"
	elif which wget 1>/dev/null; then
		DOWNLOAD_COMMAND="wget -O"
	else 
		echo "Please install curl or wget"
		exit 1
	fi
	
	# Download and extract our custom-built libWebRTC binaries
	TEMPFILE='/tmp/webrtc.tar.gz'
	WEBRTC_DIR='Engine/Source/ThirdParty/WebRTC/sdk_trunk_linux'
	WEBRTC_URL='https://github.com/adamrehn/pixel-streaming-linux/releases/download/binaries/webrtc-4.23.tar.gz'
	rm -R -f "$WEBRTC_DIR" && mkdir -p "$WEBRTC_DIR" && $DOWNLOAD_COMMAND "$TEMPFILE" "$WEBRTC_URL" && tar -xvf "$TEMPFILE" -C "$WEBRTC_DIR" && rm "$TEMPFILE"
fi
