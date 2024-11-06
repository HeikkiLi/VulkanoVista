# Compiler and linker settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LIBS = -lvulkan -lSDL2
TARGET = SDL2VulkanGame

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Main target: build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile each .cpp file to an object file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Clean target: remove object files and executable
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
