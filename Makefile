CXX = clang++
CXXFLAGS = -std=c++17 -O3 -march=x86-64 -ffast-math -fopenmp -m64 -DGLEW_STATIC -s

ifeq ($(OS), Windows_NT)
    LIBS = -lglew32 -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lpthread -static-libgcc -static-libstdc++
    EXT = .exe
    RM = del /f /q
else
    UNAME_S := $(shell uname -s)
    EXT = 
    RM = rm -f
    ifeq ($(UNAME_S), Linux)
        LIBS = -lGLEW -lglfw -lGL -lX11 -lpthread -ldl
    endif
    ifeq ($(UNAME_S), Darwin)
        LIBS = -lGLEW -lglfw -framework OpenGL
    endif
endif

BH_RAYTRACE = bh_raytrace$(EXT)
BH_CPU = bh_raytraceCPU$(EXT)
LS2D = ls2d_sim$(EXT)
TRACER = tracer$(EXT)

ALL_EXES = $(BH_RAYTRACE) $(BH_CPU) $(LS2D) $(TRACER)

all: $(ALL_EXES)

$(BH_RAYTRACE): black_hole.cpp
	$(CXX) $(CXXFLAGS) black_hole.cpp -o $(BH_RAYTRACE) $(LIBS)

$(BH_CPU): CPU_geodesic_calc.cpp
	$(CXX) $(CXXFLAGS) CPU_geodesic_calc.cpp -o $(BH_CPU) $(LIBS)

$(LS2D): lensing_2d.cpp
	$(CXX) $(CXXFLAGS) lensing_2d.cpp -o $(LS2D) $(LIBS)

$(TRACER): raytracing.cpp
	$(CXX) $(CXXFLAGS) raytracing.cpp -o $(TRACER) $(LIBS)

clean:
	$(RM) $(ALL_EXES)