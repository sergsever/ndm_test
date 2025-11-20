CXX = g++
CXXFLAGS = -Wall -g
TARGET= ndm_test
SRCS = ndm_test.cpp
OBJS = $(SRCS:.cpp=.o)
#link
all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

#compile

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm $(TARGET) $(OBJS)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/$(TARGET)
	sudo cp $(TARGET).service /etc/systemd/system/$(TARGET).service

