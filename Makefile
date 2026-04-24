CXX = c++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra

TARGET = result
SRC = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
