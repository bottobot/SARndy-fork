########################################################################
# Makefile for the Augmented Reality Sandbox.
# Copyright (c) 2012-2024 Oliver Kreylos
#
# This file is part of the WhyTools Build Environment.
# 
# The WhyTools Build Environment is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
# 
# The WhyTools Build Environment is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with the WhyTools Build Environment; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA
########################################################################

# Directory containing the Vrui build system. The directory below
# matches the default Vrui installation; if Vrui's installation
# directory was changed during Vrui's installation, the directory below
# must be adapted.
VRUI_MAKEDIR := /usr/local/share/Vrui-12.1/make

# Base installation directory for the Augmented Reality Sandbox. If this
# is set to the default of $(PWD), the Augmented Reality Sandbox does
# not have to be installed to be run. Created executables and resources
# will be installed in the bin and etc directories under the given base
# directory, respectively.
# Important note: Do not use ~ as an abbreviation for the user's home
# directory here; use $(HOME) instead.
INSTALLDIR := $(PWD)

########################################################################
# Everything below here should not have to be changed
########################################################################

# Version number for installation subdirectories. This is used to keep
# subsequent release versions of the Augmented Reality Sandbox from
# clobbering each other. The value should be identical to the
# major.minor version number found in VERSION in the root package
# directory.
VERSION = 3.2

# Set up resource directories: */
CONFIGDIR = etc/SARndbox-$(VERSION)
RESOURCEDIR = share/SARndbox-$(VERSION)

# Include definitions for the system environment and system-provided
# packages
include $(VRUI_MAKEDIR)/SystemDefinitions
include $(VRUI_MAKEDIR)/Packages.System
include $(VRUI_MAKEDIR)/Configuration.Vrui
include $(VRUI_MAKEDIR)/Packages.Vrui
include $(VRUI_MAKEDIR)/Configuration.Kinect
include $(VRUI_MAKEDIR)/Packages.Kinect

# Set installation directory structure:
EXECUTABLEINSTALLDIR := $(INSTALLDIR)/$(EXEDIR)
ETCINSTALLDIR := $(INSTALLDIR)/$(CONFIGDIR)
SHAREINSTALLDIR := $(INSTALLDIR)/$(RESOURCEDIR)
SHADERINSTALLDIR := $(SHAREINSTALLDIR)/Shaders

########################################################################
# Specify additional compiler and linker flags
########################################################################

########################################################################
# List common packages used by all components of this project
# (Supported packages can be found in $(VRUI_MAKEDIR)/Packages.*)
########################################################################

PACKAGES = MYVRUI MYGLGEOMETRY MYGEOMETRY MYMATH MYCOMM MYTHREADS MYMISC GL

########################################################################
# Specify all final targets
########################################################################

ALL = $(EXEDIR)/CalibrateProjector \
      $(EXEDIR)/SARndbox \
      $(EXEDIR)/SARndboxClient

PHONY: all
all: $(ALL)

########################################################################
# Pseudo-target to print configuration options
########################################################################

.PHONY: config
config: Configure-End

.PHONY: Configure-Begin
Configure-Begin:
	@cp Config.h Config.h.temp
	@$(call CONFIG_SETSTRINGVAR,Config.h.temp,CONFIG_CONFIGDIR,$(ETCINSTALLDIR))
	@$(call CONFIG_SETSTRINGVAR,Config.h.temp,CONFIG_SHADERDIR,$(SHADERINSTALLDIR))
	@if ! diff Config.h.temp Config.h > /dev/null ; then cp Config.h.temp Config.h ; fi
	@rm Config.h.temp

.PHONY: Configure-Install
Configure-Install: Configure-Begin
	@echo "---- SARndbox installation configuration ----"
	@echo "Root installation directory: $(INSTALLDIR)"
	@echo "Executable directory: $(EXECUTABLEINSTALLDIR)"
	@echo "Configuration data directory: $(ETCINSTALLDIR)"
	@echo "Resource data directory: $(SHAREINSTALLDIR)"
	@echo "Shader source code directory: $(SHADERINSTALLDIR)"

.PHONY: Configure-End
Configure-End: Configure-Install
	@echo "---- End of SARndbox configuration options: ----"

$(wildcard *.cpp): config

########################################################################
# Specify other actions to be performed on a `make clean'
########################################################################

.PHONY: extraclean
extraclean:

.PHONY: extrasqueakyclean
extrasqueakyclean:

# Include basic makefile
include $(VRUI_MAKEDIR)/BasicMakefile

########################################################################
# Specify build rules for executables
########################################################################

#
# Calibration utility for Kinect 3D camera and projector:
#

$(EXEDIR)/CalibrateProjector: PACKAGES += MYKINECT MYIO
$(EXEDIR)/CalibrateProjector: $(OBJDIR)/CalibrateProjector.o
.PHONY: CalibrateProjector
CalibrateProjector: $(EXEDIR)/CalibrateProjector

#
# The Augmented Reality Sandbox:
#

SARNDBOX_SOURCES = FrameFilter.cpp \
                   TextureTracker.cpp \
                   ShaderHelper.cpp \
                   Shader.cpp \
                   DepthImageRenderer.cpp \
                   ElevationColorMap.cpp \
                   SurfaceRenderer.cpp \
                   WaterTable2.cpp \
                   PropertyGridCreator.cpp \
                   WaterRenderer.cpp \
                   HandExtractor.cpp \
                   HuffmanBuilder.cpp \
                   IntraFrameCompressor.cpp \
                   InterFrameCompressor.cpp \
                   RemoteServer.cpp \
                   GlobalWaterTool.cpp \
                   LocalWaterTool.cpp \
                   DEM.cpp \
                   DEMTool.cpp \
                   BathymetrySaverTool.cpp \
                   Sandbox.cpp

$(EXEDIR)/SARndbox: PACKAGES += MYKINECT MYGLMOTIF MYIMAGES MYGLSUPPORT MYGLWRAPPERS MYIO TIFF
$(EXEDIR)/SARndbox: $(SARNDBOX_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: SARndbox
SARndbox: $(EXEDIR)/SARndbox

#
# The Augmented Reality Sandbox remote client application:
#

SARNDBOXCLIENT_SOURCES = HuffmanBuilder.cpp \
                         IntraFrameDecompressor.cpp \
                         InterFrameDecompressor.cpp \
                         TextureTracker.cpp \
                         Shader.cpp \
                         ElevationColorMap.cpp \
                         SandboxClient.cpp

$(EXEDIR)/SARndboxClient: PACKAGES += MYGLSUPPORT MYGLWRAPPERS
$(EXEDIR)/SARndboxClient: $(SARNDBOXCLIENT_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: SARndboxClient
SARndboxClient: $(EXEDIR)/SARndboxClient

########################################################################
# Specify installation rules
########################################################################

install: $(ALL)
	@echo Installing the Augmented Reality Sandbox in $(INSTALLDIR)...
	@install -d $(INSTALLDIR)
	@install -d $(EXECUTABLEINSTALLDIR)
	@install $(ALL) $(EXECUTABLEINSTALLDIR)
	@install -d $(ETCINSTALLDIR)
	@install -m u=rw,go=r $(CONFIGDIR)/* $(ETCINSTALLDIR)
	@install -d $(SHADERINSTALLDIR)
	@install -m u=rw,go=r $(RESOURCEDIR)/Shaders/* $(SHADERINSTALLDIR)
