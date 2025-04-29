phony: doencfileserver.c retrieveencfileclient.c
	gcc doencfileserver.c -o doencfileserver
	gcc retrieveencfileclient.c -o retrieveencfileclient
server: phony
	./doencfileserver
client: phony
	./retrieveencfileclient
clean:
	rm -f doencfileserver retrieveencfileclient