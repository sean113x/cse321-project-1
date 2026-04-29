CXX = c++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra

TARGET = project1
SRC = src/main.cpp src/dataset_handler/dataset.cpp src/index_tree/btree.cpp src/index_tree/bstartree.cpp src/index_tree/bplustree.cpp src/experiment/experiment1.cpp src/experiment/experiment2.cpp src/experiment/experiment3.cpp src/experiment/experiment4.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
