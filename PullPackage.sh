#!/bin/bash
########################################################################
# PullPackage.sh - Script to pull a package installation or update
# script from a software repository and execute it.
# Copyright (c) 2021-2024 Oliver Kreylos
########################################################################

# Set and display PullPackage's version number
export PULLPACKAGE_VERSION=12.0
echo "PullPackage version ${PULLPACKAGE_VERSION}"

# Check if user wants to force an update
FORCE=0
if [[ $1 == "--force" || $1 == "-f" ]]; then
	FORCE=1
	shift
fi

# Construct the name of the package script
PACKAGE=$1
if [[ $# -ge 2 ]]; then
	VERSION=$2
else
	VERSION=current
fi
PACKAGESCRIPT=Build${PACKAGE}-${VERSION}.sh

########################################################################
# Set up package build environment
########################################################################

# Default location of the package repository
export REPOSITORY=http://vroom.library.ucdavis.edu/Software

# Default root directory in which to build packages
export BUILDROOT=${HOME}/src/PullPackage

# Root directory in which to install packages
export ROOTINSTALLDIR=/opt

# Directory hierarchy for PullPackage installation
export PULLPACKAGEDIR=/opt/PullPackage
export SCRIPTSDIR=${PULLPACKAGEDIR}/Scripts
export CURRENTVERSIONSDIR=${PULLPACKAGEDIR}/CurrentVersions
export CONFIGSDIR=${PULLPACKAGEDIR}/Configs
export POSTINSTALLSDIR=${PULLPACKAGEDIR}/PostInstalls

# Directory to put symlinks for all created executables
export SYMLINKBINDIR=/usr/local/bin

# Prepare the user's build environment
mkdir -p "${BUILDROOT}"
if [[ $? -ne 0 ]]; then
	echo -e "\033[0;31mUnable to create build root directory ${BUILDROOT}\033[0m"
	#rm -r "${BUILDROOT}"
	exit 1
fi
cd "${BUILDROOT}"

# Check if some version of the requested package is already installed
export CURRENTVERSION=
if [[ -f "${CURRENTVERSIONSDIR}/${PACKAGE}" ]]; then
	export CURRENTVERSION=$(<"${CURRENTVERSIONSDIR}/${PACKAGE}")
fi

########################################################################
# Pull the requested package
########################################################################

# Check if update is forced or the requested version is different from the already-installed version
if [[ ${FORCE} != 0 || "${VERSION}" != "${CURRENTVERSION}" ]]; then
	# Check if BuildSARndbox-current.sh exists in the BUILDROOT directory
	if [[ "${PACKAGE}" == "SARndbox" && "${VERSION}" == "current" && -f "${BUILDROOT}/BuildSARndbox-current.sh" ]]; then
		# If it exists, use the local BuildSARndbox-current.sh instead of downloading
		PACKAGESCRIPT="BuildSARndbox-current.sh"
	else
		# If it doesn't exist, download the package script
		wget -q "${REPOSITORY}/PullPackages/${PACKAGESCRIPT}"
		if [[ $? -ne 0 ]]; then
			echo -e "\033[0;31mUnable to download build script for package ${PACKAGE}, version ${VERSION}\033[0m"
			exit 2
		fi
	fi
	
	# Ask for admin password now instead of later, so user can go have a cup of coffee
	echo -e "\033[0;32mPlease enter your administrator password to install package ${PACKAGE}, version ${VERSION}\033[0m"
	sudo /bin/bash -c 'date > /dev/null'
	
	# Execute the downloaded script
	/bin/bash "${PACKAGESCRIPT}"
	if [[ $? -ne 0 ]]; then
		echo -e "\033[0;31mUnable to install package ${PACKAGE}, version ${VERSION}\033[0m"
		#rm "${PACKAGESCRIPT}"
		exit 3
	fi
	
	# Find out which version was actually installed
	if [[ -f "${CURRENTVERSIONSDIR}/${PACKAGE}" ]]; then
		VERSION=$(<"${CURRENTVERSIONSDIR}/${PACKAGE}")
	fi
	
	# Execute the package's post-installation scripts
	if [[ -d "${POSTINSTALLSDIR}/${PACKAGE}" ]]; then
		shopt -s nullglob
		cd "${POSTINSTALLSDIR}/${PACKAGE}" ; POSTINSTALLSCRIPTS=(*) ; cd ../..
		for SCRIPT in "${POSTINSTALLSCRIPTS[@]}"; do
			/bin/bash "${POSTINSTALLSDIR}/${PACKAGE}/${SCRIPT}" "${CURRENTVERSION}" "${VERSION}"
			if [[ $? -ne 0 ]]; then
				echo -e "\033[0;31mUnable to execute ${PACKAGE}'s post-installation scripts\033[0m"
				#rm "${PACKAGESCRIPT}"
				exit 4
			fi
		done
	fi
	
	echo -e "\033[0;32mPackage ${PACKAGE}, version ${VERSION}, successfully installed\033[0m"
	
	# Clean up
	#rm "${PACKAGESCRIPT}"
else
	echo -e "\033[0;32mPackage ${PACKAGE}, version ${CURRENTVERSION}, already installed\033[0m"
fi
