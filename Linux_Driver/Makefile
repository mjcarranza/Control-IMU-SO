obj-m+=ArduinoDriver.o

# Create the modules
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

# Unload and load the driver  # Use with sudo
install:
	sudo rmmod cdc_acm || true
	sudo rmmod ArduinoDriver || true
	sudo insmod ArduinoDriver.ko || true

# Unload the driver
rem:
	sudo rmmod ArduinoDriver || true

# Clean all the generated files
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
