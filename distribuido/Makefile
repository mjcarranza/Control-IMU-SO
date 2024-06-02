# Nombre del ejecutable
EXEC = node1

# Archivos fuente
SRC = node1.c

# Compilador y banderas de compilación
MPICC = mpicc
CFLAGS = -lssl -lcrypto

# Número de procesos
NP = 2

# Regla por defecto: compilar y ejecutar
all: $(EXEC)
	@echo "Ejecutando con $(NP) procesos..."
	mpirun -np $(NP) ./$(EXEC)

# Regla para compilar el ejecutable
$(EXEC): $(SRC)
	$(MPICC) -o $(EXEC) $(SRC) $(CFLAGS)

# Regla para limpiar archivos generados
clean:
	rm -f $(EXEC)

# Regla para forzar la recompilación
rebuild: clean all
