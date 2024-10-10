#!/bin/bash
########################################################################
# BuildSARndbox.sh - Script to build and install the AR Sandbox.
# Copyright (c) 2022-2024 Oliver Kreylos
########################################################################

# Package name
PACKAGE=SARndbox

# Package version
VERSION=3.2

# Define common functions
if [[ ! -f "${SCRIPTSDIR}/Common-3.0.sh" ]]; then
	echo -e "\033[0;31mPullPackage is outdated; please run \"PullPackage PullPackage\" first, then try again\033[0m"
	exit 1
fi
source "${SCRIPTSDIR}/Common-3.0.sh"

# Full package name (tarball, build directory, ...)
FULLPKGNAME=${PACKAGE}-${VERSION}

# Installation directories
INSTALLDIR=${ROOTINSTALLDIR}/${FULLPKGNAME}
ETCINSTALLDIR=${INSTALLDIR}/etc
SHAREINSTALLDIR=${INSTALLDIR}/share

echo -e "\033[0;32mInstalling package $FULLPKGNAME in $INSTALLDIR\033[0m"

# Get the current Vrui and Kinect versions and fail if either one is not installed or outdated
VRUIVERSION=$(GetPackageVersion Vrui)
ExitIfOutdated Vrui "${VRUIVERSION}" "12.1"
VRUI_MAKEDIR=${ROOTINSTALLDIR}/Vrui-${VRUIVERSION}/share/make
KINECTVERSION=$(GetPackageVersion Kinect)
ExitIfOutdated Kinect "${KINECTVERSION}" "4.1"

########################################################################
# Download, build, and install the package
########################################################################

# Download and unpack the package tarball
wget -O - "${REPOSITORY}/${PACKAGE}/${FULLPKGNAME}.tar.gz" | tar xfz -
if [ $? -ne 0 ]; then
	echo -e "\033[0;31mUnable to download ${FULLPKGNAME}\033[0m"
	exit 2
fi

# Enter the package directory
cd "${FULLPKGNAME}"

# Build the make command line
MAKEARGS=(VRUI_MAKEDIR="${VRUI_MAKEDIR}")
MAKEARGS+=(INSTALLDIR="${INSTALLDIR}")
MAKEARGS+=(ETCINSTALLDIR="${ETCINSTALLDIR}")
MAKEARGS+=(SHAREINSTALLDIR="${SHAREINSTALLDIR}")

# Build the package
make "${MAKEARGS[@]}" -j${NUM_CPUS}
if [ $? -ne 0 ]; then
	echo -e "\033[0;31mUnable to build ${FULLPKGNAME}\033[0m"
	cd ..
	exit 3
fi

# Save the current installation's configuration files
if [ -n "${CURRENTVERSION}" ]; then
	echo -e "\033[0;32mSaving configuration files from ${PACKAGE}-${CURRENTVERSION}\033[0m"
	SOURCECFG=${ROOTINSTALLDIR}/${PACKAGE}-${CURRENTVERSION}/etc
	DESTCFG=SavedConfigs
	mkdir -p ${DESTCFG}
	cp "${SOURCECFG}"/*.cfg "${DESTCFG}/"
	cp "${SOURCECFG}"/ProjectorMatrix.dat "${DESTCFG}/"
fi

# Install the package
sudo make "${MAKEARGS[@]}" install
if [ $? -ne 0 ]; then
	echo -e "\033[0;31mUnable to install ${FULLPKGNAME} to ${INSTALLDIR}\033[0m"
	cd ..
	exit 4
fi

# Copy or restore the current installation's configuration files
if [ -n "${CURRENTVERSION}" ]; then
	echo -e "\033[0;32mRestoring configuration files to ${PACKAGE}-${VERSION}\033[0m"
	SOURCECFG=SavedConfigs
	DESTCFG=${ETCINSTALLDIR}
	sudo cp "${SOURCECFG}"/*.cfg "${DESTCFG}/"
	sudo cp "${SOURCECFG}"/ProjectorMatrix.dat "${DESTCFG}/"
fi

# Create symbolic links to all created executables
CreateSymLinks ${INSTALLDIR}/bin ${FULLPKGNAME} bin/*

# Make the configuration files user-writable
sudo chmod a+w "${ETCINSTALLDIR}/SARndbox.cfg" "${ETCINSTALLDIR}/BoxLayout.txt" "${ETCINSTALLDIR}/HeightColorMap.cpt"

# Create a dummy projector calibration file to make it user-writable
PROJECTORMATRIXFILE=${ETCINSTALLDIR}/ProjectorMatrix.dat
if [[ ! -e "${PROJECTORMATRIXFILE}" ]]; then
	sudo dd if=/dev/zero bs=1 count=128 "of=${PROJECTORMATRIXFILE}"
fi
sudo chmod a+rw "${PROJECTORMATRIXFILE}"

# Create a command pipe and make it user-writable
SARNDBOX_COMMANDPIPE=${SHAREINSTALLDIR}/SARndboxCommandPipe.fifo
if [[ ! -e "${SARNDBOX_COMMANDPIPE}" ]]; then
	sudo mkfifo "${SARNDBOX_COMMANDPIPE}"
elif [[ ! -p "${SARNDBOX_COMMANDPIPE}" ]]; then
	echo -e "\033[0;31mCannot create SARndbox command pipe because ${SARNDBOX_COMMANDPIPE} is in the way\033[0m"
fi
if [[ -p "${SARNDBOX_COMMANDPIPE}" ]]; then
	sudo chmod a+rw "${SARNDBOX_COMMANDPIPE}"
fi

########################################################################
# Set up per-user configuration
########################################################################

SARNDBOXCONFIGDIR=${HOME}/.config/${FULLPKGNAME}
VRUIAPPCONFIGDIR=${HOME}/.config/Vrui-${VRUIVERSION}/Applications

# Create the Vrui application configuration directory just in case
mkdir -p "${VRUIAPPCONFIGDIR}"

# Create a Vrui configuration file for the projector calibration utility
CALIBRATEPROJECTOR_CFG=${VRUIAPPCONFIGDIR}/CalibrateProjector.cfg
if [[ ! -f "${CALIBRATEPROJECTOR_CFG}" ]]; then
	echo -e "\033[0;32mCreating projector calibration configuration file ${CALIBRATEPROJECTOR_CFG}\033[0m"
	CALIBRATEPROJECTORCFG_SOURCE="# Configuration file for CalibrateProjector utility
# Installed by \"PullPackage SARndbox\"

section Vrui
	section Desktop
		# Disable the screen saver while the calibration is running:
		inhibitScreenSaver true
		
		section Window
			# Force the application's window to full-screen mode:
			windowFullscreen true
		endsection
		
		section Tools
			# Don't show the tool bucket:
			killZoneRender false
			
			section DefaultTools
				# Bind a tie point capture tool to the \"1\" and \"2\" keys:
				section CalibrationTool
					toolClass CaptureTool
					bindings ((Mouse, 1, 2))
				endsection
			endsection
		endsection
	endsection
endsection"
	tee "${CALIBRATEPROJECTOR_CFG}" > /dev/null <<< "${CALIBRATEPROJECTORCFG_SOURCE}"
fi

# Create a Vrui configuration file for the main SARndbox application
SARNDBOX_CFG=${VRUIAPPCONFIGDIR}/SARndbox.cfg
if [[ ! -f "${SARNDBOX_CFG}" ]]; then
	echo -e "\033[0;32mCreating SARndbox configuration file ${SARNDBOX_CFG}\033[0m"
	SARNDBOXCFG_SOURCE="# Configuration file for main SARndbox application
# Installed by \"PullPackage SARndbox\"

section Vrui
	section Desktop
		# Disable the screen saver while the AR Sandbox is running:
		inhibitScreenSaver true
		
		section MouseAdapter
			# Hide the mouse cursor after 5 seconds of inactivity:
			mouseIdleTimeout 5.0
		endsection
		
		section Window
			# Force the application's window to full-screen mode:
			windowFullscreen true
		endsection
		
		section Tools
			# Don't show the tool bucket:
			killZoneRender false
			
			section DefaultTools
				# Bind a global rain/dry tool to the \"1\" and \"2\" keys:
				section GlobalWaterTool
					toolClass GlobalWaterTool
					bindings ((Mouse, 1, 2))
				endsection
			endsection
		endsection
	endsection
endsection"
	tee "${SARNDBOX_CFG}" > /dev/null <<< "${SARNDBOXCFG_SOURCE}"
fi

# Check if this package version is different than the current one
if [[ -n "${CURRENTVERSION}" && "${CURRENTVERSION}" != "${VERSION}" ]]; then
	if [[ -d "${HOME}/.config/${PACKAGE}-${CURRENTVERSION}" ]]; then
		# Copy the entire per-user configuration directory to the new version
		cp -R "${HOME}/.config/${PACKAGE}-${CURRENTVERSION}" "${SARNDBOXCONFIGDIR}"
	fi
fi

# Create the per-user configuration directory
mkdir -p "${SARNDBOXCONFIGDIR}"

# Create a configuration file for the main SARndbox application if there isn't one already
SARNDBOX_CONF=${SARNDBOXCONFIGDIR}/SARndbox.conf
if [[ ! -f "${SARNDBOX_CONF}" ]]; then
	echo -e "\033[0;32mCreating per-user configuration file ${SARNDBOX_CONF}\033[0m"
	SARNDBOXCONF_SOURCE="# Run-time configuration options for SARndbox
# Installed by \"PullPackage SARndbox\"

# Settings for per-pixel input filter:
NUMAVERAGINGSLOTS=30 # 30 slots means 1s delay
MINNUMSAMPLES=10
MAXVARIANCE=2
HYSTERESIS=0.1

# Scale settings:
SCALE=100.0 # 100.0 means 100:1 scale; 1cm in the sandbox is 1m in reality

# Rendering settings:
HEIGHTCOLORMAP=${ETCINSTALLDIR}/HeightColorMap.cpt
CONTOURLINESPACING=0.75
WATEROPACITY=2.0

# Settings for water management:
WATERSPEED=1.0
WATERMAXSTEPS=30
RAINSTRENGTH=0.25
EVAPORATIONRATE=0.0"
	tee "${SARNDBOX_CONF}" > /dev/null <<< "${SARNDBOXCONF_SOURCE}"
fi

# Create a start-up script for the main AR Sandbox application
SARNDBOX_SCRIPT=${INSTALLDIR}/bin/RunSARndbox.sh
SARNDBOX_SOURCE="#!/bin/bash
# Wrapper script for SARndbox that reads common options from a per-user
# configuration file.
# Installed by \"PullPackage SARndbox\"

# Set up default SARndbox options
NUMAVERAGINGSLOTS=30
MINNUMSAMPLES=10
MAXVARIANCE=2
HYSTERESIS=0.1
SCALE=100.0
HEIGHTCOLORMAP=${ETCINSTALLDIR}/HeightColorMap.cpt
CONTOURLINESPACING=0.75
WATEROPACITY=2.0
WATERSPEED=1.0
WATERMAXSTEPS=30
RAINSTRENGTH=0.25
EVAPORATIONRATE=0.0

# Include the user configuration file if it exists:
[ -f \"${SARNDBOX_CONF}\" ] && source \"${SARNDBOX_CONF}\"

# Assemble the SARndbox command line:
SARNDBOX_ARGS=(-nas \${NUMAVERAGINGSLOTS})
SARNDBOX_ARGS+=(-sp \${MINNUMSAMPLES} \${MAXVARIANCE})
SARNDBOX_ARGS+=(-he \${HYSTERESIS})
SARNDBOX_ARGS+=(-s \${SCALE})
SARNDBOX_ARGS+=(-uhm \${HEIGHTCOLORMAP})
SARNDBOX_ARGS+=(-ucl \${CONTOURLINESPACING})
SARNDBOX_ARGS+=(-rwt -wo \${WATEROPACITY})
SARNDBOX_ARGS+=(-ws \${WATERSPEED} \${WATERMAXSTEPS})
SARNDBOX_ARGS+=(-rs \${RAINSTRENGTH})
SARNDBOX_ARGS+=(-evr \${EVAPORATIONRATE})
SARNDBOX_ARGS+=(-fpv)
if [[ -p \"${SARNDBOX_COMMANDPIPE}\" ]]; then
	SARNDBOX_ARGS+=(-cp ${SARNDBOX_COMMANDPIPE})
fi

# Start the SARndbox application:
${SYMLINKBINDIR}/SARndbox \"\${SARNDBOX_ARGS[@]}\"" "$@"
sudo tee "${SARNDBOX_SCRIPT}" > /dev/null <<< "${SARNDBOX_SOURCE}"
sudo chmod a+x "${SARNDBOX_SCRIPT}"
sudo ln -sf "${SARNDBOX_SCRIPT}" "${SYMLINKBINDIR}/RunSARndbox.sh"

# Get the launcher creator (only to get the icon directory)
source "${SCRIPTSDIR}/CreateVRLauncher-2.0.sh"

# Install the SARndbox application icon image
echo -e "\033[0;32mInstalling ARSandbox icon ${ICONDIR}/SARndbox.png\033[0m"
sudo cp "share/${FULLPKGNAME}/SARndbox.png" "${ICONDIR}/"

# Make a custom launcher creator for now; will be rolled into Vrui next:
CreateAppLauncher()
	{
	# Retrieve function arguments
	local LAUNCHERDIR="$1"
	shift
	local APPNAME="$1"
	shift
	local COMMENT="$1"
	shift
	local ICONNAME="$1"
	shift
	local CMDLINE="$*"
	
	# Create the launcher file
	local LAUNCHERFILE=${HOME}/${LAUNCHERDIR}/${APPNAME}.desktop
	echo "#!/usr/bin/env xdg-open" > "${LAUNCHERFILE}"
	echo "" >> "${LAUNCHERFILE}"
	echo "[Desktop Entry]" >> "${LAUNCHERFILE}"
	echo "Version=1.0" >> "${LAUNCHERFILE}"
	echo "Type=Application" >> "${LAUNCHERFILE}"
	echo "Icon=${ICONDIR}/${ICONNAME}" >> "${LAUNCHERFILE}"
	echo "Terminal=false" >> "${LAUNCHERFILE}"
	echo "Name=${APPNAME}" >> "${LAUNCHERFILE}"
	echo "Comment=${COMMENT}" >> "${LAUNCHERFILE}"
	echo "Exec=${CMDLINE}" >> "${LAUNCHERFILE}"
	
	# Make the launcher file executable
	chmod a+x "${LAUNCHERFILE}"
	
	# Mark the launcher file as trusted
	gio set "${LAUNCHERFILE}" "metadata::trusted" true
	gio set "${LAUNCHERFILE}" "metadata::caja-trusted-launcher" true
	}

# Create a launcher to edit the box layout:
CreateAppLauncher "Desktop" "Edit Box Layout" "Edits the AR Sandbox box layout" "SARndbox.png" "${EDITOR} \"${ETCINSTALLDIR}/BoxLayout.txt\""

# Create a
