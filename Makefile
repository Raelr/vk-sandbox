ifneq ("$(wildcard .env)","")
    include .env
endif

# Set debugging build flags
DEBUG ?= 1
ifeq ($(DEBUG), 1)
	override CXXFLAGS += -g -DDEBUG
else
    override CXXFLAGS += -DNDEBUG
endif

# Set validation layer build flags
ifeq ($(ENABLE_VALIDATION_LAYERS), 1)
    PACKAGE_FLAGS := --include-validation-layers
endif


rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
platformpth = $(subst /,$(PATHSEP),$1)

libDir := $(abspath lib)
buildDir := $(abspath bin)
vendorDir := $(abspath vendor)
assetsDir := $(abspath assets)
scriptsDir := $(abspath scripts)
outputDir := $(abspath output)
executable := app
target := $(buildDir)/$(executable)
sources := $(call rwildcard,src/,*.cpp)
objects := $(patsubst src/%, $(buildDir)/%, $(patsubst %.cpp, %.o, $(sources)))
depends := $(patsubst %.o, %.d, $(objects))

includes = -I $(vendorDir)/vulkan/include -I $(vendorDir)/glfw/include -I $(vendorDir)/glm -I $(vendorDir)/tinyobjloader
linkFlags = -L $(libDir) -lglfw3
compileFlags := -std=c++17 $(includes)

glfwLib := $(libDir)/libglfw3.a

vertSources = $(call rwildcard,shaders/,*.vert)
vertObjFiles = $(patsubst %.vert,$(buildDir)/%.vert.spv,$(vertSources))
fragSources = $(call rwildcard,shaders/,*.frag)
fragObjFiles = $(patsubst %.frag,$(buildDir)/%.frag.spv,$(fragSources))

ifeq ($(OS),Windows_NT)
    platform := windows
    CXX ?= g++
	
    PATHSEP := \$(BLANK)
    MKDIR := $(call platformpth,$(CURDIR)/scripts/mkdir.bat)
    RM := rm -r -f

    volkDefines = VK_USE_PLATFORM_WIN32_KHR
    linkFlags += -Wl,--allow-multiple-definition -pthread -lopengl32 -lgdi32 -lwinmm -mwindows -static -static-libgcc -static-libstdc++
    COPY = $(call platformpth,$(CURDIR)/scripts/copy.bat) $1 $2 $3
    COPY_DIR = $(call platformpth,$(CURDIR)/scripts/copy.bat) --copy-directory $1 $2 

    glslangValidator := $(vendorDir)\glslang\build\install\bin\glslangValidator.exe
    packageScript := $(scriptsDir)/package.bat
else 
    UNAMEOS := $(shell uname)
	ifeq ($(UNAMEOS), Linux)

        platform := linux
        CXX ?= g++
        libSuffix = so
        volkDefines = VK_USE_PLATFORM_XLIB_KHR
        linkFlags += -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -no-pie
	endif
	ifeq ($(UNAMEOS),Darwin)
		
        platform := macos
        CXX ?= clang++
        volkDefines = VK_USE_PLATFORM_MACOS_MVK
        linkFlags += -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
	endif

    PATHSEP := /
    MKDIR := mkdir -p
    RM := rm -rf
    COPY = cp -r $1$(PATHSEP)$3 $2
    COPY_DIR = $(call COPY,$1,$2,$3)

    glslangValidator := $(vendorDir)/glslang/build/install/bin/glslangValidator
    packageScript := $(scriptsDir)/package.sh
endif

# Lists phony targets for Makefile
.PHONY: all app release clean

all: app release clean

app: $(target)

# Link the program and create the executable
$(target): $(objects) $(glfwLib) $(vertObjFiles) $(fragObjFiles) $(buildDir)/lib $(buildDir)/assets
	$(CXX) $(objects) -o $(target) $(linkFlags)

$(buildDir)/%.spv: % 
	$(MKDIR) $(call platformpth, $(@D))
	$(glslangValidator) $< -V -o $@

$(glfwLib):
	$(MKDIR) $(call platformpth,$(libDir))
	$(call COPY,$(call platformpth,$(vendorDir)/glfw/src),$(libDir),libglfw3.a)

$(buildDir)/lib:
	$(MKDIR) $(call platformpth,$(buildDir)/lib)
	$(call COPY_DIR,$(call platformpth,$(vendorDir)/vulkan/lib),$(call platformpth,$(buildDir)/lib))

$(buildDir)/assets:
	$(MKDIR) $(call platformpth,$(buildDir)/assets)
	$(call COPY_DIR,$(call platformpth,$(assetsDir)),$(call platformpth,$(buildDir)/assets))

# Add all rules from dependency files
-include $(depends)

# Compile objects to the build directory
$(buildDir)/%.o: src/%.cpp Makefile
	$(MKDIR) $(call platformpth,$(@D))
	$(CXX) -MMD -MP -c $(compileFlags) $< -o $@ $(CXXFLAGS) -D$(volkDefines)

package: app
	$(packageScript) "Snek" $(outputDir) $(buildDir) $(PACKAGE_FLAGS)

clean: 
	$(RM) $(call platformpth, $(buildDir))
	$(RM) $(call platformpth, $(outputDir))
	$(RM) $(call platformpth, $(libDir))
