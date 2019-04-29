TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_LDFLAGS_Linux +=-L/usr/lib/x86_64-linux-gnu
#USR_LDFLAGS_Linux +=-lpthread

#=============================
# Build the IOC application

PROD_IOC = jfrx
# jfrx.dbd will be created and installed
DBD += jfrx.dbd

# jfrx.dbd will be made up from these files:
jfrx_DBD += base.dbd

# Include dbd files from all support applications:
jfrx_DBD += jfrxsub.dbd

# Add all the support libraries needed by this IOC
jfrx_LIBS += zmq
zmq_DIR = /usr/lib/x86_64-linux-gnu

jfrx_LIBS += pthread
pthread_DIR = /usr/lib/x86_64-linux-gnu

# jfrx_registerRecordDeviceDriver.cpp derives from jfrx.dbd
jfrx_SRCS += jfrx_registerRecordDeviceDriver.cpp
jfrx_SRCS += jfrxsub.c

# Build the main IOC entry point on workstation OSs.
jfrx_SRCS_DEFAULT += jfrxMain.cpp
jfrx_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#jfrx_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
jfrx_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
