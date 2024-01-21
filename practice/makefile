mysql_test:mysql_test.c
	gcc $^ -o $@ -L/usr/lib64/mysql/ -lmysqlclient
ws_server_test:ws_server_test.cpp
	g++ -std=c++11 $^ -o $@ -lpthread -lboost_system
