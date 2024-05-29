obj-m+=ArduinoDriver.o

# Create the modules
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

# Unload and load the driver  # Use with sudo
install: default
	rmmod ArduinoDriver || true
	insmod ArduinoDriver.ko || true

# Clean all the generated files
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
