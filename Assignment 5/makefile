all: server.c client.c idle_client.c repeat_client.c terminate_b4_answer.c send_do_nothing.c master_client.c
	gcc server.c -o server
	gcc client.c -o client
	gcc idle_client.c -o idle
	gcc repeat_client.c -o repeat
	gcc terminate_b4_answer.c -o term
	gcc send_do_nothing.c -o send_do_nothing
	gcc master_client.c -o master
clean:
	rm -f server client idle repeat term send_do_nothing master