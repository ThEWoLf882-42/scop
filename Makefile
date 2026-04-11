NAME := scop
SHELL := /bin/bash
CXX := c++
UNAME_S := $(shell uname -s)
USER_NAME := $(shell id -un)

SRC_DIR := src
OBJ_DIR := build/obj

SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/App.cpp \
	$(SRC_DIR)/Math.cpp \
	$(SRC_DIR)/ObjLoader.cpp \
	$(SRC_DIR)/TextureLoader.cpp \
	$(SRC_DIR)/FileUtils.cpp

OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

VERT_SHADER := shaders/mesh.vert
FRAG_SHADER := shaders/mesh.frag
VERT_SPV := shaders/mesh.vert.spv
FRAG_SPV := shaders/mesh.frag.spv

WARN_FLAGS := -Wall -Wextra -Werror
STD_FLAGS ?= -std=c++2a
OPT_FLAGS ?= -O2
DBG_FLAGS ?=

CPPFLAGS := -Iinclude
CXXFLAGS := $(WARN_FLAGS) $(STD_FLAGS) $(OPT_FLAGS) $(DBG_FLAGS)
LDFLAGS :=
LDLIBS :=

SDK_FROM_GOINFRE := $(lastword $(sort $(wildcard /goinfre/$(USER_NAME)/VulkanSDK/*)))
ifeq ($(origin VULKAN_SDK), undefined)
ifneq ($(strip $(SDK_FROM_GOINFRE)),)
VULKAN_SDK := $(SDK_FROM_GOINFRE)
endif
endif

REQUESTED_GOALS := $(strip $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all))

GLFW_PKG_OK := $(shell pkg-config --exists glfw3 2>/dev/null && echo 1)
GLFW_BREW_OK := $(shell { [ -f "$$HOME/.brew/lib/libglfw.dylib" ] || [ -f "$$HOME/.brew/lib/libglfw.3.dylib" ] || [ -f "$$HOME/.brew/lib/libglfw3.a" ] || [ -f "/opt/homebrew/lib/libglfw.dylib" ] || [ -f "/opt/homebrew/lib/libglfw.3.dylib" ] || [ -f "/opt/homebrew/lib/libglfw3.a" ]; } && echo 1)

GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS := $(shell pkg-config --libs glfw3 2>/dev/null)

ifeq ($(strip $(GLFW_LIBS)),)
ifeq ($(UNAME_S),Darwin)
ifneq ($(wildcard $(HOME)/.brew/lib/libglfw.dylib),)
GLFW_CFLAGS := -I$(HOME)/.brew/include
GLFW_LIBS := -L$(HOME)/.brew/lib -lglfw
else ifneq ($(wildcard $(HOME)/.brew/lib/libglfw.3.dylib),)
GLFW_CFLAGS := -I$(HOME)/.brew/include
GLFW_LIBS := -L$(HOME)/.brew/lib -lglfw
else ifneq ($(wildcard /opt/homebrew/lib/libglfw.dylib),)
GLFW_CFLAGS := -I/opt/homebrew/include
GLFW_LIBS := -L/opt/homebrew/lib -lglfw
else ifneq ($(wildcard /opt/homebrew/lib/libglfw.3.dylib),)
GLFW_CFLAGS := -I/opt/homebrew/include
GLFW_LIBS := -L/opt/homebrew/lib -lglfw
endif
endif
endif

ifeq ($(BOOTSTRAP_DONE),1)

CPPFLAGS += $(GLFW_CFLAGS)

ifeq ($(UNAME_S),Darwin)
CPPFLAGS += -I$(VULKAN_SDK)/include
LDFLAGS += -L$(VULKAN_SDK)/lib -Wl,-rpath,$(VULKAN_SDK)/lib
LDLIBS += $(GLFW_LIBS) -lvulkan -framework Cocoa -framework IOKit -framework CoreVideo
GLSLC := $(firstword $(wildcard $(VULKAN_SDK)/bin/glslc) $(shell command -v glslc 2>/dev/null))
GLSLANG := $(firstword $(wildcard $(VULKAN_SDK)/bin/glslangValidator) $(shell command -v glslangValidator 2>/dev/null))
else
LDLIBS += $(GLFW_LIBS) -lvulkan -ldl -lpthread -lX11 -lXrandr -lXi
GLSLC := $(shell command -v glslc 2>/dev/null)
GLSLANG := $(shell command -v glslangValidator 2>/dev/null)
endif

ifeq ($(strip $(GLSLC)),)
ifeq ($(strip $(GLSLANG)),)
SHADER_COMPILE = @echo "Install glslc or glslangValidator to build shaders" && false
else
SHADER_COMPILE = $(GLSLANG) -V $< -o $@
endif
else
SHADER_COMPILE = $(GLSLC) $< -o $@
endif

all: check-env shaders $(NAME)

check-env:
ifeq ($(UNAME_S),Darwin)
	@if [[ -z "$(strip $(VULKAN_SDK))" || ! -d "$(VULKAN_SDK)" ]]; then \
		echo "VULKAN_SDK not found after bootstrap."; \
		exit 1; \
	fi
endif
	@if [[ -z "$(strip $(GLFW_LIBS))" ]]; then \
		echo "GLFW development files not found."; \
		exit 1; \
	fi

install-vulkan:
	./scripts/install_vulkan.sh

print-config:
	@echo "OS          : $(UNAME_S)"
	@echo "CXX         : $(CXX)"
	@echo "CXXFLAGS    : $(CXXFLAGS)"
	@echo "CPPFLAGS    : $(CPPFLAGS)"
	@echo "LDFLAGS     : $(LDFLAGS)"
	@echo "LDLIBS      : $(LDLIBS)"
	@echo "VULKAN_SDK  : $(VULKAN_SDK)"
	@echo "GLSLC       : $(GLSLC)"
	@echo "GLSLANG     : $(GLSLANG)"
	@echo "GLFW_CFLAGS : $(GLFW_CFLAGS)"
	@echo "GLFW_LIBS   : $(GLFW_LIBS)"

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

shaders: $(VERT_SPV) $(FRAG_SPV)

$(VERT_SPV): $(VERT_SHADER)
	$(SHADER_COMPILE)

$(FRAG_SPV): $(FRAG_SHADER)
	$(SHADER_COMPILE)

run: all
	./$(NAME) $(or $(MODEL),assets/demo_cube.obj) $(or $(TEXTURE),assets/pony.ppm)

clean:
	rm -rf build
	rm -f $(VERT_SPV) $(FRAG_SPV)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all check-env install-vulkan print-config shaders clean fclean re run

-include $(DEPS)

else

bootstrap:
	@set -e; \
	SDK="$(VULKAN_SDK)"; \
	if [ "$(UNAME_S)" = "Darwin" ] && [ -z "$(GLFW_PKG_OK)$(GLFW_BREW_OK)" ]; then \
		echo "GLFW not found. Running 42-brew and installing glfw..."; \
		if ! command -v brew >/dev/null 2>&1; then \
			42-brew; \
		fi; \
		eval "$$(brew shellenv 2>/dev/null || true)"; \
		brew install glfw; \
	fi; \
	if [ "$(UNAME_S)" = "Darwin" ] && { [ -z "$$SDK" ] || [ ! -d "$$SDK" ]; }; then \
		echo "VULKAN_SDK not found. Running scripts/install_vulkan.sh..."; \
		./scripts/install_vulkan.sh; \
		SDK="$$(ls -d /goinfre/$(USER_NAME)/VulkanSDK/* 2>/dev/null | sort | tail -n 1)"; \
	fi; \
	if [ "$(UNAME_S)" = "Darwin" ] && { [ -z "$$SDK" ] || [ ! -d "$$SDK" ]; }; then \
		echo "Unable to locate VULKAN_SDK after installer finished."; \
		exit 1; \
	fi; \
	$(MAKE) BOOTSTRAP_DONE=1 VULKAN_SDK="$$SDK" $(REQUESTED_GOALS)

all run print-config shaders $(NAME) re: bootstrap

install-vulkan clean fclean:
	@$(MAKE) BOOTSTRAP_DONE=1 $(REQUESTED_GOALS)

%:
	@:

.PHONY: bootstrap all run print-config shaders install-vulkan clean fclean re $(NAME)

endif