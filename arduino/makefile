# Nombre del ejecutable
EXEC = codigo-Ard

# Archivos fuente
SRC = codigo-Ard.c

# Compilador y banderas de compilación
MPICC = mpicc
CFLAGS = -lssl -lcrypto

# Número de procesos
NP = 2

# Regla por defecto: compilar y ejecutar
all: $(EXEC)
	@echo "Ejecutando con $(NP) procesos..."
	mpirun -np $(NP) --hostfile /etc/hosts $(EXEC)

# Regla para compilar el ejecutable
$(EXEC): $(SRC)
	$(MPICC) -o $@ $@.c $(CFLAGS)

# Regla para limpiar archivos generados
clean:
	rm -f $(EXEC)

# Regla para forzar la recompilación
rebuild: clean all

client:
	gcc -o client client.c driverLib.c -lssl -lcrypto

run:
	sudo ./client