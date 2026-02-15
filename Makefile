NAME := scop.app

CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2

INCLUDES := -Iinclude

# --- Auto collect all .cpp files under src/ (recursive) ---
SRC := $(shell find src -type f -name "*.cpp" | sort)
OBJ := $(SRC:.cpp=.o)

UNAME_S := $(shell uname -s)

# --- Vulkan SDK must be set ---
ifeq ($(VULKAN_SDK),)
$(error VULKAN_SDK is not set. Run: source /goinfre/$$USER/VulkanSDK/1.3.296.0/setup-env.sh)
endif

VULKAN_INC := -I$(VULKAN_SDK)/include
VULKAN_LIB := -L$(VULKAN_SDK)/lib
VULKAN_LNK := -lvulkan

# --- GLFW from pkg-config (homebrew/42) ---
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS   := $(shell pkg-config --libs glfw3 2>/dev/null)
ifeq ($(GLFW_LIBS),)
  GLFW_LIBS = -lglfw
endif

# --- macOS frameworks + rpath for VulkanSDK ---
ifeq ($(UNAME_S),Darwin)
  SYS_LIBS := -framework Cocoa -framework IOKit -framework CoreVideo
  RPATH := -Wl,-rpath,$(VULKAN_SDK)/lib
else
  SYS_LIBS :=
  RPATH :=
endif

LDFLAGS := $(VULKAN_LIB) $(RPATH) $(GLFW_LIBS) $(VULKAN_LNK) $(SYS_LIBS)

# --- Shader compilation ---
GLSLC := $(VULKAN_SDK)/bin/glslc
SHADERS := $(shell find shaders -type f \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" \) 2>/dev/null | sort)
SHADERS_SPV := $(patsubst %,%.spv,$(SHADERS))

all: $(NAME)

$(NAME): $(OBJ) $(SHADERS_SPV)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)

# Build object files; ensure directory exists for .o output path
%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(VULKAN_INC) $(GLFW_CFLAGS) -c $< -o $@

# Compile each shader file to .spv next to it
%.spv: %
	@mkdir -p $(dir $@)
	$(GLSLC) $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME) $(SHADERS_SPV)

re: fclean all

.PHONY: all clean fclean re
