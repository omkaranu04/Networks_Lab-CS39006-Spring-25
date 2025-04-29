all: cldp_server.c cldp_client.c
	gcc -o cldp_server cldp_server.c
	gcc -o cldp_client cldp_client.c
server: cldp_server
	sudo ./cldp_server
client: cldp_client
	sudo ./cldp_client
clean:
	rm -f cldp_server cldp_client