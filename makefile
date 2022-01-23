CXX = g++

server: main.cpp config.cpp webserver.cpp ./timer/timer.cpp ./mysqlconnect/connect_pool.cpp ./log/log.cpp ./http/http_connect.cpp ./util/utils.cpp
	$(CXX) -g -o server $^ -lpthread -lmysqlclient

clean:
	rm  -r server